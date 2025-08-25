
## start build:
```bash
Remove-Item -Recurse -Force build\debug
"C:\Program Files\CMake\bin\cmake.exe" -S . -B build\debug -G "Visual Studio 17 2022"
"C:\Program Files\CMake\bin\cmake.exe" --build build\debug --config Debug
"C:\Program Files\CMake\bin\ctest.exe" --test-dir build\debug -C Debug --output-on-failure
```

## for bench_pipeline:
```bash
"C:\Program Files\CMake\bin\cmake.exe" --build build\debug --config Release --target csvqr_bench_pipeline
```

### run bench_pipeline:

#### Basic
```bash
.\build\debug\Release\csvqr_bench_pipeline.exe .\data\large\quoted.csv 1048576
```

#### Flags (space-separated):
```bash
.\build\debug\Release\csvqr_bench_pipeline.exe `
  --data .\data\large\quoted.csv `
  --chunk-bytes 1048576 `
  --has-header true

```

### Generate Data and run bench

#### Generate a synthetic big CSV
```bash
# build the generator if needed
"C:\Program Files\CMake\bin\cmake.exe" --build build\debug --config Release --target gen_synth_csv

# make a ~1M-row CSV
.\build\debug\Release\gen_synth_csv.exe .\data\large\synthetic_1M.csv 1000000 --with-header

```

### Run bench against it:
```bash
.\build\debug\Release\csvqr_bench_pipeline.exe .\data\large\synthetic_1M.csv 1048576

```

## Generate Report
```bash
.\build\debug\Debug\csv_quick_report.exe `
  --input .\data\samples\simple.csv `
  --config .\config\config.toml `
  --project-id run001 `
  --output-root .\artifacts `
  --sample-frac 0.2 `
  --has-header true `
  --chunk-bytes 1048576

```
