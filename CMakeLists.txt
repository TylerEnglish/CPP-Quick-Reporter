cmake_minimum_required(VERSION 3.24)

project(CSVQuickReporter
  VERSION 0.1.0
  DESCRIPTION "CSV → Quick Reporter"
  LANGUAGES CXX)

option(CSVQR_BUILD_TESTS "Build unit tests" ON)
option(CSVQR_BUILD_BENCH "Build benchmarks" ON)
option(CSVQR_ENABLE_SANITIZERS "Enable sanitizers (Linux/macOS)" OFF)
option(CSVQR_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

function(fetch_pkg name GIT_REPO GIT_TAG)
  FetchContent_Declare(${name}
    GIT_REPOSITORY ${GIT_REPO}
    GIT_TAG        ${GIT_TAG}
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(${name})
endfunction()

# ---- version pins ----
set(V_CLI11        v2.4.2)
set(V_FMT          11.0.2)
set(V_TOMLPLUS     v3.4.0)
set(V_FASTFLOAT    v6.1.5)
set(V_DATE         v3.0.1)
set(V_GTEST        v1.15.2)
set(V_GBENCH       v1.8.4)
set(V_MUSTACHE     v4.1)

# ---- normal deps ----
fetch_pkg(CLI11        https://github.com/CLIUtils/CLI11.git            ${V_CLI11})
fetch_pkg(fmt          https://github.com/fmtlib/fmt.git                ${V_FMT})
fetch_pkg(tomlplusplus https://github.com/marzer/tomlplusplus.git       ${V_TOMLPLUS})
fetch_pkg(date         https://github.com/HowardHinnant/date.git        ${V_DATE})

# --- fast_float: header-only shim ---
FetchContent_Declare(fast_float_src
  GIT_REPOSITORY https://github.com/fastfloat/fast_float.git
  GIT_TAG        ${V_FASTFLOAT}
  GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(fast_float_src)
if(NOT fast_float_src_POPULATED)
  FetchContent_Populate(fast_float_src)
endif()
add_library(fast_float INTERFACE)
add_library(fast_float::fast_float ALIAS fast_float)
target_include_directories(fast_float INTERFACE "${fast_float_src_SOURCE_DIR}/include")

# --- Mustache: header-only shim ---
FetchContent_Declare(mustache_src
  GIT_REPOSITORY https://github.com/kainjow/Mustache.git
  GIT_TAG        ${V_MUSTACHE}
  GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(mustache_src)
if(NOT mustache_src_POPULATED)
  FetchContent_Populate(mustache_src)
endif()

add_library(mustache_headers INTERFACE)
# header lives at repo root
target_include_directories(mustache_headers INTERFACE "${mustache_src_SOURCE_DIR}")
add_library(kainjow::mustache ALIAS mustache_headers)
add_library(mustache::mustache  ALIAS mustache_headers)

# ---- internal interface libs ----
add_library(csvqr_core INTERFACE)
target_include_directories(csvqr_core INTERFACE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(csvqr_core INTERFACE
  CLI11::CLI11
  fmt::fmt
  tomlplusplus::tomlplusplus
  fast_float::fast_float
  date::date
)

add_library(csvqr_report INTERFACE)
target_link_libraries(csvqr_report INTERFACE
  mustache::mustache
  fmt::fmt
)

# ---- app ----
add_executable(csv_quick_report
  src/main/main.cpp
  src/cli/cli_options.hpp
  src/io/file_stats.hpp
  src/metrics/timers.hpp
  src/report/emit_run_json.hpp
  src/report/emit_profile_json.hpp
  src/report/emit_dag_json.hpp
  src/report/render_report.hpp
  src/csv/tokenizer.hpp
  src/csv/csv_count.hpp
)

# ensure mustache headers are visible during compile
target_link_libraries(csv_quick_report PRIVATE
  csvqr_core
  csvqr_report
  fmt::fmt
  CLI11::CLI11
  mustache::mustache
)
target_include_directories(csv_quick_report PRIVATE
  "${mustache_src_SOURCE_DIR}/include"
)

# ---- warnings ----
if(MSVC)
  add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:/permissive->"
    "$<$<COMPILE_LANGUAGE:CXX>:/W4>"
    "$<$<COMPILE_LANGUAGE:CXX>:/bigobj>"
  )
  if(CSVQR_WARNINGS_AS_ERRORS)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/WX>")
  endif()
else()
  add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:-Wall>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wextra>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wconversion>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wshadow>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>"
    "$<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>"
  )
  if(CSVQR_WARNINGS_AS_ERRORS)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Werror>")
  endif()
endif()

# ---- tests ----
if(CSVQR_BUILD_TESTS)
  fetch_pkg(googletest https://github.com/google/googletest.git ${V_GTEST})
  enable_testing()
  add_executable(csvqr_tests
    tests/unit/test_tokenizer.cpp
    tests/unit/test_infer.cpp
    tests/unit/test_profiler.cpp
    tests/unit/test_cli.cpp
  )
  target_link_libraries(csvqr_tests PRIVATE
    csvqr_core
    GTest::gtest GTest::gtest_main
    fmt::fmt
  )
  add_test(NAME csvqr_tests COMMAND csvqr_tests)
endif()

# ---- bench ----
if(CSVQR_BUILD_BENCH)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
  fetch_pkg(benchmark https://github.com/google/benchmark.git ${V_GBENCH})
  add_executable(csvqr_bench
    bench/bench_tokenizer.cpp
    bench/bench_pipeline.cpp
  )
  target_link_libraries(csvqr_bench PRIVATE
    csvqr_core
    benchmark::benchmark
    fmt::fmt
  )
endif()

# ---- install (templates & sample config) ----
include(GNUInstallDirs)
install(DIRECTORY templates/ DESTINATION ${CMAKE_INSTALL_DATADIR}/csvqr/templates)
install(FILES config/config.toml DESTINATION ${CMAKE_INSTALL_DATADIR}/csvqr/config)
