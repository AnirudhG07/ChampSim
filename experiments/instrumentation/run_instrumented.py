#!/usr/bin/env python3
"""
Experiment 3: policy-internal counters.

Temporarily adds five counters to struct hawkeye and prints them once per run
through ChampSim's replacement_final_stats() hook, runs the three benchmarks at
the default LLC geometry, then restores the original sources.

Logs land in experiments/instrumentation/logs/.
"""
import os, re, shutil, subprocess, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = os.path.join(ROOT, "replacement", "hawkeye")
HERE = os.path.dirname(os.path.abspath(__file__))
LOGS = os.path.join(HERE, "logs")
TRACES = ["456.hmmer-191B", "429.mcf-22B", "473.astar-42B"]


def instrument():
    h = open(os.path.join(SRC, "hawkeye.h")).read()
    h = h.replace("  vector<vector<uint64_t>> pc_seq;",
                  "  long diag_accesses = 0, diag_opt_hits = 0, diag_prev_found = 0;\n"
                  "  long diag_fills = 0, diag_fills_friendly = 0;\n\n"
                  "  vector<vector<uint64_t>> pc_seq;")
    a = "access_type type, bool hit);"
    i = h.index(a) + len(a)
    h = h[:i] + "\n\n  void replacement_final_stats();" + h[i:]
    open(os.path.join(SRC, "hawkeye.h"), "w").write(h)

    c = open(os.path.join(SRC, "hawkeye.cc")).read()
    c = c.replace("#include <algorithm>", "#include <algorithm>\n#include <iostream>")
    c = c.replace("  if (d > 0) {",
                  "  diag_accesses++;\n"
                  "  diag_opt_hits += optgen_hit ? 1 : 0;\n"
                  "  diag_prev_found += (d > 0) ? 1 : 0;\n\n"
                  "  if (d > 0) {")
    c = c.replace("  cls = _pred ? Classification::CACHE_FRIENDLY : Classification::CACHE_AVERSE;",
                  "  cls = _pred ? Classification::CACHE_FRIENDLY : Classification::CACHE_AVERSE;\n"
                  "  diag_fills++;\n  diag_fills_friendly += _pred ? 1 : 0;", 1)
    c += '''
void hawkeye::replacement_final_stats()
{
  auto pct = [](long a, long b) { return b ? 100.0 * (double)a / (double)b : 0.0; };
  std::cout << "HAWKEYE_DIAG"
            << " accesses=" << diag_accesses
            << " opt_hit_rate=" << pct(diag_opt_hits, diag_accesses)
            << " prev_found_rate=" << pct(diag_prev_found, diag_accesses)
            << " friendly_fill_rate=" << pct(diag_fills_friendly, diag_fills) << "\\n";
}
'''
    open(os.path.join(SRC, "hawkeye.cc"), "w").write(c)


def build():
    subprocess.run(["./config.sh", "hawkeye_config.json"], cwd=ROOT, check=True,
                   capture_output=True, text=True)
    for f in os.listdir(SRC):
        os.utime(os.path.join(SRC, f), None)
    b = subprocess.run(["make", "-j", str(os.cpu_count() or 4)], cwd=ROOT,
                       capture_output=True, text=True)
    if b.returncode != 0:
        sys.exit(b.stdout[-3000:] + b.stderr[-3000:])


def main():
    os.makedirs(LOGS, exist_ok=True)
    backup = {f: open(os.path.join(SRC, f)).read() for f in ("hawkeye.h", "hawkeye.cc")}
    try:
        instrument()
        build()
        procs = []
        for t in TRACES:
            log = open(os.path.join(LOGS, f"{t}.log"), "w")
            procs.append((t, log, subprocess.Popen(
                [os.path.join(ROOT, "bin", "champsim"),
                 "--warmup_instructions", "20000000",
                 "--simulation_instructions", "50000000",
                 os.path.join(ROOT, "traces", f"{t}.champsimtrace.xz")],
                stdout=log, stderr=subprocess.STDOUT, cwd=ROOT)))
        for t, log, p in procs:
            p.wait(); log.close()
    finally:
        for f, text in backup.items():
            open(os.path.join(SRC, f), "w").write(text)
            os.utime(os.path.join(SRC, f), None)
        build()

    for t in TRACES:
        txt = open(os.path.join(LOGS, f"{t}.log")).read()
        m = re.search(r"HAWKEYE_DIAG.*", txt)
        print(f"{t:<16} {m.group(0) if m else 'NO DIAG LINE'}")


if __name__ == "__main__":
    main()
