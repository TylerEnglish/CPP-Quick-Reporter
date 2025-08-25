```
CPP-Quick-Reporter/
├─ .github
│  └─ workflows/ 
│     └─ ci.yml 
├─ CMakeLists.txt
├─ CMakePresents.json
├─ .gitignore
├─ .dockerignore
├─ docker-compose.yml
├─ README.MD
├─ config/
│  └─ config.toml
├─ data/
│  └─ large/ 
│  └─ samples/
│    ├─ bigints.csv 
│    ├─ nulls.csv
│    ├─ quoted_edges.csv
│    └─ simple.csv 
├─ schemas/ 
│  ├─ dag.schema.json
│  ├─ profile.schema.json
│  └─ run.schema.json
├─ docker/
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
│  │  ├─ profiler.hpp
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
│  └─ unit/
│     ├─ test_tokenizer.cpp
│     ├─ test_infer.cpp
│     ├─ test_profiler.cpp
│     └─ test_cli.cpp
├─ bench/
│  ├─ bench_tokenizer.cpp
│  └─ bench_pipeline.cpp
├─ artifacts/
└─ scripts/


```