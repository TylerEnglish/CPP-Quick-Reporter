```
CPP-Quick-Reporter/
├─ .github
│  └─ workflows/ 
│     └─ ci.yml 
├─ CMakeLists.txt
├─ CMakePresents.json
├─ README.MD

├─ config/
│  └─ config.toml
├─ data/
│  └─ large/  # empty

│  └─ samples/
│    ├─ bigints.csv     # empty
│    ├─ nulls.csv        # empty
│    ├─ quoted_edges.csv     # empty

│    └─ simple.csv 
├─ schemas/ 
│  ├─ dag.schema.json
│  ├─ profile.schema.json
│  └─ run.schema.json
├─ docker/ 
│  ├─ Dockerfile     #basically empty
│  └─ entrypoint.sh  #empty
├─ src/ 
│  ├─ main/main.cpp
│  ├─ cli/cli_options.hpp
│  ├─ csv/
│  │  ├─ tokenizer.hpp
│  │  ├─ record_view.hpp 
│  │  └─ csv_count.hpp
│  ├─ io/
│  │  ├─ chunk_reader.hpp 
│  │  └─ file_stats.hpp 
│  ├─ metrics/
│  │  ├─ process_stats.hpp 
│  │  └─ timers.hpp 
│  ├─ profile/
│  │  ├─ profile.hpp
│  │  └─ histogram.hpp  
│  ├─ report/
│  │  ├─ emit_run_json.hpp
│  │  ├─ emit_profile_json.hpp
│  │  ├─ emit_dag_json.hpp
│  │  └─ render_report.hpp
│  ├─ types
│  │  ├─ infer.hpp
│  │  └─ parse_date.hpp 
│  └─ util/
│     ├─ json_escape.hpp
│     └─ nulls.hpp 
├─ templates/
│  ├─ vega-lite/
│  │  ├─ cpu_over_time.json 
│  │  ├─ memory_over_time
│  │  ├─ null_heatmap.json
│  │  ├─ stage_latencies.json
│  │  └─ throughput.json
│  └─ report.mustache
├─ tests/
│  └─ fixtures/  
│     ├─ dag_truth.json 
│     ├─ profile_truth.json 
│     └─ run_truth.json
│  └─ property/ 
│     └─ test_csv_edges.cpp 
│  └─ unit/
│     ├─ test_tokenizer.cpp
│     ├─ test_infer.cpp
│     ├─ test_profiler.cpp
│     └─ test_cli.cpp
├─ bench/
│  ├─ bench_tokenizer.cpp
│  └─ bench_pipeline.cpp
├─ artifacts/             ← outputs here (runXXX dirs)
└─ scripts/


```