# Hawkeye Cache Replacement in ChampSim

## Build and run the policy

Clone the repository:
```bash
git clone https://github.com/ChampSim/ChampSim
cd Champsim
```

Run the below commands to run(these are from the assignment instructions)

```bash
./config.sh hawkeye_config.json
make -j$(nproc)
bin/champsim --warmup_instructions 20000000 --simulation_instructions 50000000 \
    traces/456.hmmer-191B.champsimtrace.xz
```

## Plot 1 and Plot 2

```bash
uv run python plots.py -j 10        # run the simulations parallely
```
There are plotting options in the file, but primarily you can run this.

## Experiment 1 and 2 — writeback insertion RRPV, and find_victim variants

```bash
uv run python experiments/sweep/run_sweep.py        # ~4 h, writes experiments/sweep/results.json
uv run python experiments/sweep/plot_writeback.py   # writes plot_3.png
uv run python experiments/sweep/plot_find_victim.py # writes plot_4.png
```

`results.json` is committed, so the two plot scripts can be run without re-simulating.

## Experiment 3 — policy-internal counters

```bash
uv run python experiments/instrumentation/run_instrumented.py
```

Writes `experiments/instrumentation/logs/*.log` and prints one `HAWKEYE_DIAG` line per
benchmark. Restores the original sources when it finishes.

## Report

```bash
cd report && latexmk -pdf report.tex
```
