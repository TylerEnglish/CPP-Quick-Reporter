# CSV → Quick Reporter

A tiny, fast C++ tool that scans a CSV and emits a self-contained HTML report plus machine-readable JSON artifacts. The report shows KPIs, per-stage timings, memory/CPU timelines, and per-column quality (e.g., null ratios). All charts are rendered client-side with offline-bundled Vega/Vega-Lite.

---

## What & Why

**What:**
`csv_quick_report` reads a CSV (optionally headered), computes counts/metrics, and writes:

* `report.html` — an interactive, standalone HTML report
* `run.json` — run/session metrics (wall time, throughput, CPU/RSS samples, stage timings)
* `profile.json` — dataset profile (rows, columns, null counts, types, simple stats)
* `dag.json` — a coarse DAG of pipeline stages (for context)

**Why:**

* Quick, no-frills quality checks on huge CSVs
* Artifacts are portable (open `report.html` without a web server)
* JSON outputs plug into CI, Airflow, or downstream analysis

---

## Quickstart

### Prereqs

* CMake **≥ 3.24**
* C++20 compiler (MSVC 17/VS2022, Clang, or GCC)
* (Optional) **Ninja** for a simpler build tree
* (Optional) Python 3.8+ to run the schema validator

### Build & run (Windows, PowerShell – Visual Studio)

```powershell
$CMake = "C:\Program Files\CMake\bin\cmake.exe"
$CTest = "C:\Program Files\CMake\bin\ctest.exe"
$Bld   = "build\debug"

# Configure
& $CMake -S . -B $Bld -G "Visual Studio 17 2022" -A x64

# Build (Debug or Release)
& $CMake --build $Bld --config Release -- /m

# Generate a report from the sample CSV
.\build\debug\Release\csv_quick_report.exe `
  --input .\data\samples\simple.csv `
  --output-root .\artifacts `
  --project-id sample_simple `
  --chunk-bytes 1048576 `
  --has-header true
```

Open the generated `artifacts\sample_simple\report.html` in your browser.

### Build & run (macOS/Linux, bash)

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

./build/csv_quick_report `
  --input ./data/samples/simple.csv `
  --output-root ./artifacts `
  --project-id sample_simple `
  --chunk-bytes 1048576 `
  --has-header true
```

> Charts are fully offline. We bundle pinned versions of Vega (5.x), Vega-Lite (5.x), and vega-embed into `templates/assets/vendor/`.

---

## CLI Usage

```
csv_quick_report
  --input <path/to.csv>                [required]
  --output-root <dir>                  (default: ./artifacts)
  --project-id <string>                (default: quick-reporter-YYYYMMDD-HHMMSS)
  --chunk-bytes <N>                    bytes per IO chunk (default: 1 MiB)
  --has-header <true|false>            input has a header row? (default: true)
  --delimiter <char>                   CSV delimiter (default: ',')
  --quote <char>                       CSV quote char (default: '"')
```

**Examples**

Generate with custom delimiter and no header:

```bash
csv_quick_report --input data/nohdr.tsv `
  --output-root artifacts --project-id tsv_run `
  --delimiter $'\t' --has-header false
```

---

## Config Schema

Artifacts are validated against JSON Schemas under `schemas/`.

### `profile.schema.json` (Dataset Profile)

Key fields:

```json
{
  "version": "1",
  "dataset": {
    "rows": 123,
    "columns": 6,
    "header_present": true,
    "source_path": "path/to.csv"
  },
  "columns": [
    {
      "name": "id",
      "logical_type": "int64",     // one of: int64, float64, bool, date, datetime, string
      "null_count": 0,
      "non_null_count": 123
      // optional stats:
      // "min", "max", "mean", "median", "stddev",
      // "quantiles": { "0.25": 1.2, "0.5": 2.3, ... },
      // "cardinality": 100,
      // "topk": [ { "value":"A", "count":10 }, ... ],
      // "histogram": { "bins": 10, "edges":[...], "counts":[...] },
      // "null_ratio": 0.01
    }
  ]
}
```

### `run.schema.json` (Pipeline Run Metrics)

```json
{
  "version": "1",
  "started_at": "2025-08-25T19:22:18Z",
  "ended_at":   "2025-08-25T19:22:18Z",
  "wall_time_ms": 382.01,
  "rows": 1000001,
  "input_bytes": 62597744,
  "throughput_input_mb_s": 156.27,
  "rss_peak_mb": 12.79,
  "cpu_user_pct": 0,
  "cpu_sys_pct": 0,
  "errors": 0,
  "build": { "type": "Release", "flags": "" },
  "host":  { "os": "windows", "arch": "x86_64" },
  "stages": [
    { "name": "count_rows_cols", "calls": 1, "p50_ms": 270.19, "p95_ms": 270.19 }
  ],
  "samples": [
    { "ts_ms": 12, "rss_mb": 13.8, "cpu_pct": 24.8, "bytes_in": 2097152, "bytes_out": 0 }
  ]
}
```

### `dag.schema.json` (Pipeline DAG)

```json
{
  "version": "1",
  "nodes": [
    { "id": "n1", "label": "read_chunks", "type": "io", "duration_ms": 0.0 }
  ],
  "edges": [
    { "from": "n1", "to": "n2" }
  ]
}
```

**Validate** (optional):

```bash
# requires Python and jsonschema (pip)
python scripts/validate_artifacts.py `
  --artifacts-dir ./artifacts `
  --schemas-dir ./schemas `
  --projects sample_simple
```

---

## Artifacts

When you run `csv_quick_report`, artifacts land in:

```
artifacts/
  <project-id>/
    run.json
    profile.json
    dag.json
    report.html
    assets/           # chart JS/CSS bundles (copied at build/run time)
```

Open `report.html` directly in your browser.
The report shows:

* **KPIs:** rows, errors, wall time, throughput (MB/s), peak RSS
* **Throughput timeline:** MB ingested over time (**line chart**)
* **Per-stage timings:** p95 per stage (bar chart)
* **Null ratios by column:** (bar chart)
* **Memory over time:** RSS MB (**line chart**)
* **CPU utilization over time:** percent (**line chart**)
* **DAG summary:** nodes/edges overview

> If you serve the report from a different location, set `CSVQR_ASSETS_DIR` so the app can find `templates/assets/` when copying/staging.

---

## Performance Targets

These are rough, hardware-dependent guidelines:

* **Throughput:** aim to be *I/O-bound* (hundreds of MB/s on SSD/NVMe)
* **Memory:** RSS should scale sublinearly with file size (primarily buffer-size bound)
* **CPU:** under sustained read/parse, expect moderate utilization (<100% on one core for simple counts)

Tuning knobs:

* `--chunk-bytes` (default 1 MiB). Larger chunks reduce syscalls; 4–16 MiB is often a sweet spot.
* `--delimiter`, `--quote` according to your data (mis-specified quoting can slow parsing).

---

## Testing & Benchmarks

### Unit tests (CMake/CTest)

```bash
# Ninja multi-config or single-config both work
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

### Micro/pipe benchmarks (Google Benchmark)

Two executables are built when `-DCSVQR_BUILD_BENCH=ON`:

* `csvqr_bench_tokenizer`
* `csvqr_bench_pipeline`

**Examples**

```bash
# Tokenizer microbench (default GB flags)
./build/csvqr_bench_tokenizer --benchmark_format=console

# Pipeline bench on a large CSV
./build/csvqr_bench_pipeline `
  --data ./data/large/quoted.csv `
  --chunk-bytes 1048576 `
  --has-header true
```

**Generate synthetic data** (simple utility):

```bash
# compile-time target: gen_synth_csv
./build/gen_synth_csv --rows 1000000 --output ./data/large/synthetic_1M.csv
```

> Google Benchmark flags you might like:
> `--benchmark_min_time=2.0`, `--benchmark_repetitions=5`, `--benchmark_display_aggregates_only=true`, `--benchmark_counters_tabular=true`

---

## Limitations & Next Steps

**Current limitations**

* CSV parser focuses on counting/typing for reporting; it’s not a full RFC-4180 engine.
* Quoted newlines are handled for counting, but timeline row estimates rely on simple `\n` scans.
* Type inference is basic; complex locale/format handling (e.g., thousands separators, custom datetime formats) is limited.
* CPU stats are sampled per-process and normalized; per-stage CPU isn’t attributed.
* The DAG is schematic (helpful for context) rather than a full execution trace.

**Planned/ideas**

* More robust type inference (configurable date/time patterns, decimals, null sentinels).
* Column stats: min/max/quantiles for strings & dates, top-K across larger domains.
* Optional Parquet/Arrow export of the profile.
* Pluggable readers (gzip/zstd streams).
* Richer DAG with bytes/rows lineage and stage-level resource metrics.
* First-class container & Airflow recipes (end-to-end orchestration and MinIO drop-folder ingestion).

