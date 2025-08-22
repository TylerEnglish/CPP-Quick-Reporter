#pragma once
#include <fmt/format.h>
#include <fstream>
#include <string>
#include <cstdint>

// Writes run.json (schema v1).
inline void emit_run_json(const std::string& out_path,
                          const std::string& started_iso,
                          const std::string& ended_iso,
                          double wall_ms,
                          std::uintmax_t input_bytes,
                          std::uint64_t rows)
{
    double mb = static_cast<double>(input_bytes) / (1024.0 * 1024.0);
    double secs = wall_ms / 1000.0;
    double mbps = secs > 0.0 ? (mb / secs) : 0.0;

    std::ofstream f(out_path, std::ios::binary);
    f << fmt::format(
        R"({{
  "version":"1",
  "started_at":"{}",
  "ended_at":"{}",
  "wall_time_ms":{},
  "rows":{},
  "input_bytes":{},
  "throughput_input_mb_s":{},
  "rss_peak_mb":0.0,
  "cpu_user_pct":0.0,
  "cpu_sys_pct":0.0,
  "errors":0,
  "cache_hit_pct":null,
  "build":{{"type":"Debug","flags":""}},
  "host":{{"os":"windows","arch":"x86_64"}},
  "stages":[
    {{"name":"read_chunks","calls":0}},
    {{"name":"tokenize_csv","calls":0}},
    {{"name":"type_infer","calls":0}},
    {{"name":"profile_columns","calls":0}}
  ],
  "samples":[]
}})",
        started_iso, ended_iso, wall_ms, rows, input_bytes, mbps);
}
