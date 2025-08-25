"""
Validate CSV-Quick-Reporter artifacts:
- JSON Schema checks for run.json, profile.json, dag.json
- Extra invariants (non-empty arrays, monotonic timestamps, etc.)
- Optional golden comparisons (selected fields) if truth files exist

Usage:
  python scripts/validate_artifacts.py \
    --artifacts-dir artifacts \
    --schemas-dir schemas \
    --projects sample_simple sample_nulls sample_quoted sample_bigints \
    [--truth-dir tests/fixtures]
"""
from __future__ import annotations
import argparse, json, sys
from pathlib import Path

try:
    import jsonschema  # type: ignore
except Exception:
    jsonschema = None

def load_json(p: Path) -> dict:
    with p.open("rb") as f:
        return json.loads(f.read().decode("utf-8"))

def validate_schema(data: dict, schema_path: Path) -> list[str]:
    if jsonschema is None:
        return [f"jsonschema not installed; skipped schema check for {schema_path.name}"]
    schema = load_json(schema_path)
    try:
        jsonschema.validate(data, schema)
        return []
    except jsonschema.ValidationError as e:
        msg = f"{schema_path.name}: {e.message}"
        return [msg]

def assert_invariants(run: dict, profile: dict, dag: dict) -> list[str]:
    errs: list[str] = []

    # --- run.json sanity ---
    samples = run.get("samples", [])
    if not isinstance(samples, list) or not samples:
        errs.append("run.json: samples[] must be non-empty")
    else:
        # monotonic timestamps & last bytes == input_bytes
        last_ts = -1
        for i, s in enumerate(samples):
            ts = s.get("ts_ms", -1)
            if ts < last_ts:
                errs.append(f"run.json: samples[{i}].ts_ms not monotonic")
            last_ts = ts
            cpu = s.get("cpu_pct")
            if cpu is not None and not (0.0 <= float(cpu) <= 100.0):
                errs.append(f"run.json: samples[{i}].cpu_pct out of [0,100]")
        if "input_bytes" in run and samples[-1].get("bytes_in") is not None:
            if int(samples[-1]["bytes_in"]) != int(run["input_bytes"]):
                errs.append("run.json: last sample bytes_in != input_bytes")

    stages = run.get("stages", [])
    if not isinstance(stages, list) or not stages:
        errs.append("run.json: stages[] must be non-empty")

    # --- profile.json sanity ---
    ds = profile.get("dataset", {})
    if run.get("rows") is not None and ds.get("rows") is not None:
        if int(run["rows"]) != int(ds["rows"]):
            errs.append("profile.dataset.rows must equal run.rows")

    cols = profile.get("columns", [])
    if not isinstance(cols, list) or not cols:
        errs.append("profile.json: columns[] must be non-empty")
    else:
        rows_total = int(ds.get("rows", 0))
        for i, c in enumerate(cols):
            nn = int(c.get("non_null_count", 0))
            nul = int(c.get("null_count", 0))
            if nn < 0 or nul < 0:
                errs.append(f"profile.columns[{i}] counts must be >= 0")
            if rows_total and (nn + nul) != rows_total:
                errs.append(f"profile.columns[{i}] non_null+null != dataset.rows")

    # --- dag.json sanity ---
    if not isinstance(dag.get("nodes", []), list) or not dag["nodes"]:
        errs.append("dag.json: nodes[] must be non-empty")
    if not isinstance(dag.get("edges", []), list) or not dag["edges"]:
        errs.append("dag.json: edges[] must be non-empty")

    return errs

def deep_contains(sup: dict | list, sub: dict | list) -> bool:
    """Return True if sup 'contains' sub (all keys/values in sub appear in sup).
    Useful for 'golden snippets' that specify only critical fields."""
    if isinstance(sub, dict):
        if not isinstance(sup, dict): return False
        for k, v in sub.items():
            if k not in sup: return False
            if not deep_contains(sup[k], v): return False
        return True
    if isinstance(sub, list):
        if not isinstance(sup, list): return False
        # every item in sub must appear somewhere in sup (order-insensitive)
        for item in sub:
            if not any(deep_contains(s, item) for s in sup):
                return False
        return True
    return sup == sub

def maybe_check_truth(name: str, actual: dict, truth_path: Path, errors: list[str]) -> None:
    if not truth_path.exists():
        return
    truth = load_json(truth_path)
    if not deep_contains(actual, truth):
        errors.append(f"{name}: does not contain all fields from {truth_path.name}")

def validate_project(art_dir: Path, schema_dir: Path, truth_dir: Path | None) -> list[str]:
    errs: list[str] = []

    run_p     = art_dir / "run.json"
    profile_p = art_dir / "profile.json"
    dag_p     = art_dir / "dag.json"

    if not (run_p.exists() and profile_p.exists() and dag_p.exists()):
        return [f"missing artifacts in {art_dir} (need run.json, profile.json, dag.json)"]

    run     = load_json(run_p)
    profile = load_json(profile_p)
    dag     = load_json(dag_p)

    # schemas
    errs += validate_schema(run,     schema_dir / "run.schema.json")
    errs += validate_schema(profile, schema_dir / "profile.schema.json")
    errs += validate_schema(dag,     schema_dir / "dag.schema.json")

    # invariants
    errs += assert_invariants(run, profile, dag)

    # optional golden snippets
    if truth_dir:
        maybe_check_truth("run.json",     run,     truth_dir / "run_truth.json",     errs)
        maybe_check_truth("profile.json", profile, truth_dir / "profile_truth.json", errs)
        maybe_check_truth("dag.json",     dag,     truth_dir / "dag_truth.json",     errs)

    return errs

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--artifacts-dir", required=True)
    ap.add_argument("--schemas-dir",   required=True)
    ap.add_argument("--projects",      nargs="+", required=True,
                    help="artifact subdirs to validate (e.g. sample_simple sample_nulls)")
    ap.add_argument("--truth-dir",     default=None, help="optional tests/fixtures dir")
    args = ap.parse_args()

    root_art   = Path(args.artifacts_dir)
    schema_dir = Path(args.schemas_dir)
    truth_dir  = Path(args.truth_dir) if args.truth_dir else None

    all_errs: list[str] = []
    for pid in args.projects:
        errs = validate_project(root_art / pid, schema_dir, truth_dir)
        if errs:
            all_errs.append(f"[{pid}]")
            all_errs.extend("  " + e for e in errs)

    if all_errs:
        print("\n".join(all_errs), file=sys.stderr)
        sys.exit(1)
    print("OK: all artifacts validated")
    return 0

if __name__ == "__main__":
    main()
