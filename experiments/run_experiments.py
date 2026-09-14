#!/usr/bin/env python3
"""
Factorial experiment over two Hawkeye design choices.

  writeback fill policy (replacement_cache_fill):
      none : no special case -- the predictor decides, using ip == 0
      6    : insert writeback fills at RRPV 6
      7    : insert writeback fills at RRPV 7 (immediate eviction candidate)

  find_victim (rrip.cc):
      assignment : return the max-RRPV way AND age every line by (7 - max)
      paper      : return the max-RRPV way, leave the vector untouched (SS3.4)

Both find_victim variants select the same victim; they differ only in the side
effect on the RRPV vector, which is exactly what we want to isolate.

Source files under replacement/hawkeye/ are patched, built, snapshotted, and
then restored from experiments/variants/_original/ in a finally block.

Everything this script produces stays under experiments/.
"""

import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXP = os.path.join(ROOT, "experiments")
ORIG = os.path.join(EXP, "variants", "_original")
SRC = os.path.join(ROOT, "replacement", "hawkeye")

TRACES = ["429.mcf-22B.champsimtrace.xz", "456.hmmer-191B.champsimtrace.xz"]
SETS, WAYS = 2048, 16
WARMUP, SIM = "20000000", "50000000"

WB_MODES = ["0", "1", "2", "3", "4", "5", "6", "7", "none"]
FV_MODES = ["assignment", "paper"]

RESULTS = os.path.join(EXP, "results.json")

# --------------------------------------------------------------------- patches
WB_BLOCK = """  // EXPERIMENT: writeback fills carry no PC (cache.cc sets ip = {{}}), so the
  // predictor cannot classify them. Insert at a fixed RRPV instead.
  if (access_type{{type}} == access_type::WRITE) {{
    rrpv[set][static_cast<std::size_t>(way)] = {value};
    return;
  }}

"""

FV_PAPER = """size_t find_victim(std::vector<int>& rrpv)
{
  if (rrpv.empty()) {
    return 0;
  }

  // PAPER variant (Section 3.4): evict the line with the highest RRPV and do
  // NOT age the set.
  int max_rrpv = rrpv[0];
  size_t victim = 0;
  for (size_t i = 1; i < rrpv.size(); i++) {
    if (rrpv[i] > max_rrpv) {
      max_rrpv = rrpv[i];
      victim = i;
    }
  }

  return victim;
}
"""


def apply_variant(wb, fv):
    """Write the patched sources into replacement/hawkeye/."""
    cc = open(os.path.join(ORIG, "hawkeye.cc")).read()
    if wb != "none":
        anchor = "  Classification cls;"
        assert anchor in cc, "could not find the anchor in replacement_cache_fill"
        cc = cc.replace(anchor, WB_BLOCK.format(value=wb) + anchor, 1)
    open(os.path.join(SRC, "hawkeye.cc"), "w").write(cc)

    rr = open(os.path.join(ORIG, "rrip.cc")).read()
    if fv == "paper":
        start = rr.index("size_t find_victim")
        rr = rr[:start] + FV_PAPER
    open(os.path.join(SRC, "rrip.cc"), "w").write(rr)


def restore():
    for f in ("hawkeye.cc", "rrip.cc"):
        dst = os.path.join(SRC, f)
        shutil.copy(os.path.join(ORIG, f), dst)  # copy, not copy2: must NOT
        os.utime(dst, None)                      # preserve the old mtime, or make
                                                 # keeps the variant's stale .o


def build(name):
    """Configure + build, then snapshot the binary under experiments/bin/."""
    out = os.path.join(EXP, "bin", f"champsim_{name}")
    cfg = subprocess.run(["./config.sh", "hawkeye_config.json"],
                         cwd=ROOT, capture_output=True, text=True)
    if cfg.returncode != 0:
        sys.exit(f"config.sh failed for {name}:\n{cfg.stderr}")
    b = subprocess.run(["make", "-j", str(os.cpu_count() or 4)],
                       cwd=ROOT, capture_output=True, text=True)
    if b.returncode != 0:
        sys.exit(f"make failed for {name}:\n{b.stdout[-3000:]}{b.stderr[-3000:]}")
    shutil.copy2(os.path.join(ROOT, "bin", "champsim"), out)
    return out


LLC_RE = re.compile(r"LLC TOTAL\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)")
LOAD_RE = re.compile(r"LLC LOAD\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)\s+MISS:\s+(\d+)")


def simulate(name, binary, trace):
    started = time.time()
    out = subprocess.run(
        [binary, "--warmup_instructions", WARMUP, "--simulation_instructions", SIM,
         os.path.join(ROOT, "traces", trace)],
        capture_output=True, text=True, cwd=ROOT)
    log = os.path.join(EXP, "logs", f"{name}_{trace}.log")
    open(log, "w").write(out.stdout)

    m = LLC_RE.search(out.stdout)
    if not m:
        sys.exit(f"could not parse LLC stats for {name} / {trace}; see {log}")
    access, hit, miss = (int(x) for x in m.groups())
    stats = {"access": access, "hit": hit, "miss": miss,
             "miss_rate": 100.0 * miss / access}
    lm = LOAD_RE.search(out.stdout)
    if lm:
        la, lh, lmi = (int(x) for x in lm.groups())
        stats["load_hit"] = lh
        stats["load_miss_rate"] = 100.0 * lmi / la if la else None
    stats["minutes"] = (time.time() - started) / 60.0
    print(f"  DONE {name:<22} {trace:<34} miss {stats['miss_rate']:6.2f}%  "
          f"({stats['minutes']:.1f} min)", flush=True)
    return stats


def main():
    existing = json.load(open(RESULTS)) if os.path.exists(RESULTS) else {}

    variants = []
    for fv in FV_MODES:
        for wb in WB_MODES:
            name = f"wb{wb}_{fv}"
            if all(f"{name}|{t}" in existing for t in TRACES):
                continue          # already measured
            variants.append((wb, fv))

    if not variants:
        print("every variant already measured")
        return
    print(f"{len(variants)} variant(s) to build: "
          + ", ".join(f"wb{w}_{f}" for w, f in variants) + "\n")
    binaries = {}

    try:
        for wb, fv in variants:
            name = f"wb{wb}_{fv}"
            print(f"building {name} ...", flush=True)
            apply_variant(wb, fv)
            # keep a copy of exactly what was built, for the record
            vdir = os.path.join(EXP, "variants", name)
            os.makedirs(vdir, exist_ok=True)
            for f in ("hawkeye.cc", "rrip.cc"):
                shutil.copy2(os.path.join(SRC, f), os.path.join(vdir, f))
            binaries[name] = build(name)
    finally:
        restore()
        print("\nsource files restored from experiments/variants/_original/\n", flush=True)

    jobs = [(f"wb{wb}_{fv}", binaries[f"wb{wb}_{fv}"], t)
            for wb, fv in variants for t in TRACES]
    print(f"running {len(jobs)} simulations, 8 at a time\n", flush=True)

    results = dict(existing)
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        futs = {pool.submit(simulate, n, b, t): (n, t) for n, b, t in jobs}
        for fut in concurrent.futures.as_completed(futs):
            n, t = futs[fut]
            results[f"{n}|{t}"] = fut.result()

    json.dump(results, open(RESULTS, "w"), indent=2, sort_keys=True)
    print(f"\nwrote {RESULTS}")


if __name__ == "__main__":
    main()
