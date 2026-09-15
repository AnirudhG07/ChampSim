#!/usr/bin/env python3
"""Experiment 2: find_victim, assignment vs paper. Writes plot_4.png in the repo root."""
import json, os
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
LRU = {"456.hmmer-191B": 25.94, "429.mcf-22B": 64.82}
r = json.load(open(os.path.join(HERE, "results.json")))
red = lambda b, k: 100 * (LRU[b] - r[f"{k}|{b}.champsimtrace.xz"]["miss_rate"]) / LRU[b]

fig, ax = plt.subplots(figsize=(7, 4.4))
x = np.arange(len(LRU)); w = 0.35
for i, (fv, lab) in enumerate([("assignment", "assignment: age the set"),
                               ("paper", "paper (Sec. 3.4): no aging")]):
    vals = [red(b, f"wb7_{fv}") for b in LRU]
    bars = ax.bar(x + (i - 0.5) * w, vals, w, label=lab)
    for bb, v in zip(bars, vals):
        ax.annotate(f"{v:+.2f}%", (bb.get_x() + bb.get_width() / 2, v),
                    textcoords="offset points", xytext=(0, 4 if v >= 0 else -13),
                    ha="center", fontsize=9)
ax.axhline(0, color="black", lw=0.8)
ax.set_xticks(x); ax.set_xticklabels(list(LRU))
ax.set_ylabel("LLC miss-rate reduction over LRU (%)")
ax.set_title("find_victim: aging the set vs leaving it untouched")
ax.grid(True, axis="y", alpha=0.3); ax.legend(fontsize=9)
fig.tight_layout(); fig.savefig(os.path.join(ROOT, "plot_4.png"), dpi=150)
print("wrote plot_4.png")
