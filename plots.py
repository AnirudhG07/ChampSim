import argparse
import concurrent.futures
import fcntl
import json
import os
import re
import shutil
import subprocess
import sys
import time
from contextlib import contextmanager

import matplotlib

matplotlib.use("Agg")  # save to file; no display needed
import matplotlib.pyplot as plt
import numpy as np

POLICIES = ["lru", "hawkeye"]

WARMUP = "20000000"
SIMULATION = "50000000"

HMMER = "456.hmmer-191B.champsimtrace.xz"
MCF = "429.mcf-22B.champsimtrace.xz"
ASTAR = "473.astar-42B.champsimtrace.xz"

# Plot 1: LLC capacity fixed at 2MB with a 64B line, so sets = 2MB / (64B * ways)
PLOT1_GEOMETRIES = [(8192, 4), (4096, 8), (2048, 16)]

# Plot 2: the default LLC geometry
DEFAULT_SETS, DEFAULT_WAYS = 2048, 16
PLOT2_TRACES = [HMMER, MCF, ASTAR]

CACHE_FILE = "results.json"
BUILD_LOCK = ".build.lock"
CACHE_LOCK = ".cache.lock"


# Note, this is just a plotting file and running commands in parallel since the simulations are independent and take a while. This has been created with help of LLMs.

@contextmanager
def file_lock(path):
    """Cross-process mutex, so `-p 1` and `-p 2` can run at the same time."""
    with open(path, "w") as f:
        fcntl.flock(f, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(f, fcntl.LOCK_UN)


def binary_for(policy, sets, ways):
    """Each geometry gets its own binary snapshot. Without this, two parallel
    problems would overwrite bin/champsim_* from under each other."""
    return f"bin/champsim_{policy}_{sets}x{ways}"


def load_cache():
    if os.path.exists(CACHE_FILE):
        with open(CACHE_FILE) as f:
            return json.load(f)
    return {}


def save_cache(cache):
    with open(CACHE_FILE, "w") as f:
        json.dump(cache, f, indent=2, sort_keys=True)


def record(key, stats):
    """Read-modify-write the shared cache under a lock, so a parallel problem
    writing its own result cannot clobber ours."""
    with file_lock(CACHE_LOCK):
        cache = load_cache()
        cache[key] = stats
        save_cache(cache)


def key_for(policy, trace, sets, ways):
    return f"{policy}|{trace}|{sets}x{ways}"


def write_configs(sets, ways):
    """One JSON per policy, each with its own executable_name."""
    for policy in POLICIES:
        cfg = {
            "executable_name": f"champsim_{policy}",
            "LLC": {"replacement": policy, "sets": sets, "ways": ways},
        }
        with open(f"plots_{policy}_config.json", "w") as f:
            json.dump(cfg, f, indent=2)


def configure_and_build(sets, ways):
    """Configure both policies at once and build, then snapshot the binaries
    under geometry-specific names. Held under BUILD_LOCK because config.sh and
    make share .csconfig across problems. Idempotent: a geometry already
    snapshotted is skipped, so re-runs cost nothing."""
    with file_lock(BUILD_LOCK):
        if all(os.path.exists(binary_for(p, sets, ways)) for p in POLICIES):
            return

        write_configs(sets, ways)
        print(f"  configuring + building for {sets} sets x {ways} ways ...", flush=True)

        cfg = subprocess.run(
            ["./config.sh", "--join", "chain", "plots_lru_config.json", "plots_hawkeye_config.json"],
            capture_output=True, text=True,
        )
        if cfg.returncode != 0:
            sys.exit(f"config.sh failed:\n{cfg.stdout}\n{cfg.stderr}")

        build = subprocess.run(
            ["make", "-j", str(os.cpu_count() or 4)], capture_output=True, text=True
        )
        if build.returncode != 0:
            sys.exit(f"make failed:\n{build.stdout[-3000:]}\n{build.stderr[-3000:]}")

        os.makedirs("bin", exist_ok=True)
        for policy in POLICIES:
            shutil.copy2(f"bin/champsim_{policy}", binary_for(policy, sets, ways))


LLC_RE = re.compile(r"LLC TOTAL\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)")
LLC_LOAD_RE = re.compile(r"LLC LOAD\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)")


def parse_llc(text):
    """ChampSim reports raw counts only, so the miss rate is derived here:
           LLC miss rate (%) = 100 * MISS / ACCESS
    The LOAD-only rate is kept alongside it -- that is the demand stream the
    replacement policy actually influences, and it is useful for diagnosis."""
    m = LLC_RE.search(text)
    if not m:
        return None
    access, hit, miss = (int(x) for x in m.groups())
    if access == 0:
        return None

    stats = {
        "access": access,
        "hit": hit,
        "miss": miss,
        "miss_rate": 100.0 * miss / access,
    }

    lm = LLC_LOAD_RE.search(text)
    if lm:
        l_access, l_hit, l_miss = (int(x) for x in lm.groups())
        stats["load_access"] = l_access
        stats["load_hit"] = l_hit
        stats["load_miss"] = l_miss
        stats["load_miss_rate"] = 100.0 * l_miss / l_access if l_access else None

    return stats


def run_sim(policy, trace, sets, ways, log_dir="logs"):
    os.makedirs(log_dir, exist_ok=True)
    binary = binary_for(policy, sets, ways)
    if not os.path.exists(binary):
        sys.exit(f"{binary} not found -- build step must have failed")

    print(f"  start  {policy:<8} {trace:<34} {sets}x{ways}", flush=True)
    start = time.time()
    out = subprocess.run(
        [binary,
         "--warmup_instructions", WARMUP,
         "--simulation_instructions", SIMULATION,
         f"traces/{trace}"],
        capture_output=True, text=True,
    )
    elapsed = time.time() - start

    log_path = os.path.join(log_dir, f"{policy}_{trace}_{sets}x{ways}.log")
    with open(log_path, "w") as f:
        f.write(out.stdout)

    stats = parse_llc(out.stdout)
    if stats is None:
        sys.exit(f"\ncould not parse LLC stats; see {log_path}\n{out.stderr[-2000:]}")

    print(f"  DONE   {policy:<8} {trace:<34} {sets}x{ways}  "
          f"miss rate {stats['miss_rate']:6.2f}%  ({elapsed/60:.1f} min)", flush=True)
    return stats


def ensure(cache, policy, trace, sets, ways, dry_run=False, force=False):
    """Return cached stats, running the simulation only if we do not have them.
    Re-reads the shared cache first, so a result produced by the other problem
    running in parallel is picked up instead of being recomputed."""
    k = key_for(policy, trace, sets, ways)
    if not force:
        if k in cache:
            return cache[k]

        with file_lock(CACHE_LOCK):
            fresh = load_cache()
        if k in fresh:
            cache[k] = fresh[k]
            return cache[k]

    if dry_run:
        print(f"  WOULD RUN {k}")
        return None

    stats = run_sim(policy, trace, sets, ways)
    cache[k] = stats
    record(k, stats)
    return stats


def plot_1(cache):
    ways = [w for _, w in PLOT1_GEOMETRIES]
    fig, ax = plt.subplots(figsize=(7, 4.5))

    for policy, marker in zip(POLICIES, ["o", "s"]):
        rates = [cache[key_for(policy, HMMER, s, w)]["miss_rate"] for s, w in PLOT1_GEOMETRIES]
        ax.plot(ways, rates, marker=marker, linewidth=2, markersize=8, label=policy.upper())
        for x, y in zip(ways, rates):
            ax.annotate(f"{y:.2f}", (x, y), textcoords="offset points",
                        xytext=(0, 8), ha="center", fontsize=8)

    ax.set_xlabel("LLC associativity (ways)")
    ax.set_ylabel("LLC miss rate (%)")
    ax.set_title("LLC miss rate vs associativity, 2MB LLC\n456.hmmer-191B")
    ax.set_xticks(ways)
    ax.set_xticklabels([str(w) for w in ways])
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig("plot_1.png", dpi=150)   # save BEFORE any show(); show() clears the figure
    print("wrote plot_1.png")


def plot_2(cache):
    names, reductions = [], []
    for trace in PLOT2_TRACES:
        lru = cache[key_for("lru", trace, DEFAULT_SETS, DEFAULT_WAYS)]["miss_rate"]
        hawk = cache[key_for("hawkeye", trace, DEFAULT_SETS, DEFAULT_WAYS)]["miss_rate"]
        names.append(trace.split(".")[1].split("-")[0])
        reductions.append(100.0 * (lru - hawk) / lru)

    fig, ax = plt.subplots(figsize=(7, 4.5))
    colors = ["tab:green" if r >= 0 else "tab:red" for r in reductions]
    bars = ax.bar(names, reductions, color=colors, width=0.55)
    for bar, r in zip(bars, reductions):
        ax.annotate(f"{r:.2f}%", (bar.get_x() + bar.get_width() / 2, r),
                    textcoords="offset points", xytext=(0, 4 if r >= 0 else -14),
                    ha="center", fontsize=9)

    ax.axhline(0, color="black", linewidth=0.8)
    ax.set_ylabel("LLC miss-rate reduction over LRU (%)")
    ax.set_title(f"Hawkeye vs LRU, {DEFAULT_SETS} sets x {DEFAULT_WAYS} ways\n"
                 "(missrate_LRU - missrate_Hawkeye) / missrate_LRU x 100")
    ax.grid(True, axis="y", alpha=0.3)
    fig.tight_layout()
    fig.savefig("plot_2.png", dpi=150)
    print("wrote plot_2.png")


def summary(cache):
    print("\n=== Plot 1: 456.hmmer, 2MB LLC ===")
    print(f"  {'ways':<8}{'sets':<8}{'LRU':>10}{'Hawkeye':>12}{'reduction':>12}")
    for sets, ways in PLOT1_GEOMETRIES:
        l = cache.get(key_for("lru", HMMER, sets, ways))
        h = cache.get(key_for("hawkeye", HMMER, sets, ways))
        if l and h:
            red = 100.0 * (l["miss_rate"] - h["miss_rate"]) / l["miss_rate"]
            print(f"  {ways:<8}{sets:<8}{l['miss_rate']:>9.2f}%{h['miss_rate']:>11.2f}%{red:>11.2f}%")

    print(f"\n=== Plot 2: {DEFAULT_SETS} sets x {DEFAULT_WAYS} ways ===")
    print(f"  {'benchmark':<14}{'LRU':>10}{'Hawkeye':>12}{'reduction':>12}")
    for trace in PLOT2_TRACES:
        l = cache.get(key_for("lru", trace, DEFAULT_SETS, DEFAULT_WAYS))
        h = cache.get(key_for("hawkeye", trace, DEFAULT_SETS, DEFAULT_WAYS))
        if l and h:
            red = 100.0 * (l["miss_rate"] - h["miss_rate"]) / l["miss_rate"]
            name = trace.split(".")[1].split("-")[0]
            print(f"  {name:<14}{l['miss_rate']:>9.2f}%{h['miss_rate']:>11.2f}%{red:>11.2f}%")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-p", "--problem", choices=["1", "2", "all"], default="all",
                    help="which plot to work on. Run '-p 1' and '-p 2' in two "
                         "terminals to do both at once; builds are locked and "
                         "results are shared through results.json.")
    ap.add_argument("--plot-only", action="store_true", help="replot from results.json, run nothing")
    ap.add_argument("--dry-run", action="store_true", help="list the runs that are missing")
    ap.add_argument("-j", "--jobs", type=int, default=1,
                    help="how many simulations to run concurrently (default 1). "
                         "Builds always happen first and are serialized.")
    ap.add_argument("--policy", choices=["lru", "hawkeye", "both"], default="both",
                    help="restrict to one policy. Useful when one policy's code "
                         "is still changing but the baseline is settled.")
    ap.add_argument("--force", action="store_true",
                    help="re-run simulations even when a cached result exists "
                         "(results are always written back to the cache)")
    args = ap.parse_args()

    do1 = args.problem in ("1", "all")
    do2 = args.problem in ("2", "all")
    policies = POLICIES if args.policy == "both" else [args.policy]
    cache = load_cache()

    if not args.plot_only:
        plan = []
        if do1:
            for sets, ways in PLOT1_GEOMETRIES:
                for policy in policies:
                    plan.append((policy, HMMER, sets, ways))
        if do2:
            for trace in PLOT2_TRACES:
                for policy in policies:
                    item = (policy, trace, DEFAULT_SETS, DEFAULT_WAYS)
                    if item not in plan:
                        plan.append(item)

        if not args.force:
            plan = [p for p in plan if key_for(*p) not in cache]

        if args.dry_run:
            for p in plan:
                print(f"  WOULD RUN {key_for(*p)}")
            return

        if not plan:
            print("nothing to run; every result is already cached")
        else:
            for sets, ways in sorted({(s, w) for _, _, s, w in plan}):
                configure_and_build(sets, ways)

            jobs = max(1, args.jobs)
            print(f"\nrunning {len(plan)} simulation(s), {jobs} at a time\n", flush=True)
            started = time.time()

            def work(item):
                policy, trace, sets, ways = item
                stats = run_sim(policy, trace, sets, ways)
                record(key_for(*item), stats)
                return item, stats

            with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
                for item, stats in pool.map(work, plan):
                    cache[key_for(*item)] = stats

            print(f"\nall runs finished in {(time.time()-started)/60:.1f} min")

    if args.dry_run:
        return

    with file_lock(CACHE_LOCK):
        cache = load_cache()

    def have(keys):
        return [k for k in keys if k not in cache]

    if do1:
        missing = have([key_for(p, HMMER, s, w) for s, w in PLOT1_GEOMETRIES for p in POLICIES])
        if missing:
            print("\nProblem 1 incomplete, skipping plot_1.png:")
            for k in missing:
                print("  " + k)
        else:
            plot_1(cache)

    if do2:
        missing = have([key_for(p, t, DEFAULT_SETS, DEFAULT_WAYS)
                        for t in PLOT2_TRACES for p in POLICIES])
        if missing:
            print("\nProblem 2 incomplete, skipping plot_2.png:")
            for k in missing:
                print("  " + k)
        else:
            plot_2(cache)

    summary(cache)


if __name__ == "__main__":
    main()
