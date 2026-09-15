#!/usr/bin/env python3
"""Experiment 1: writeback insertion RRPV. Writes plot_3.png in the repo root."""
import json, os
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
LRU = {"456.hmmer-191B": 25.94, "429.mcf-22B": 64.82}
r = json.load(open(os.path.join(HERE, "results.json")))
red = lambda b, k: 100 * (LRU[b] - r[f"{k}|{b}.champsimtrace.xz"]["miss_rate"]) / LRU[b]

fig, ax = plt.subplots(figsize=(7, 4.4))
for b, marker in zip(LRU, ["o", "s"]):
    ax.plot(range(8), [red(b, f"wb{i}_assignment") for i in range(8)],
            marker=marker, lw=2, ms=7, label=b)
ax.axhline(0, color="black", lw=0.8)
ax.set_xticks(range(8))
ax.set_xlabel("RRPV assigned to a writeback fill")
ax.set_ylabel("LLC miss-rate reduction over LRU (%)")
ax.set_title("Effect of the writeback insertion RRPV\n(2048 sets $\\times$ 16 ways)")
ax.grid(True, alpha=0.3); ax.legend()
fig.tight_layout(); fig.savefig(os.path.join(ROOT, "plot_3.png"), dpi=150)
print("wrote plot_3.png")
