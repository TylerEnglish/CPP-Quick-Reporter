#pragma once
#include <fmt/format.h>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>

struct RunStage {
    std::string name;
    std::uint64_t calls = 0;
    double p50_ms = 0.0;  // optional
    double p95_ms = 0.0;  // optional
    // (bytes/rows in/out omitted for now)
};

struct RunSample {
    std::uint64_t ts_ms = 0;
    std::uint64_t bytes_in = 0;
    std::uint64_t bytes_out = 0;
    double rss_mb = 0.0;
    double cpu_pct = 0.0;
};

// Writes run.json (schema v1).
inline void emit_run_json(const std::string& out_path,
                          const std::string& started_iso,
                          const std::string& ended_iso,
                          double wall_ms,
                          std::uintmax_t input_bytes,
                          std::uint64_t rows,
                          const std::vector<RunStage>& stages,
                          const std::vector<RunSample>& samples,
                          double rss_peak_mb = 0.0,
                          double cpu_user_pct = 0.0,
                          double cpu_sys_pct = 0.0)
{
    const double mb   = static_cast<double>(input_bytes) / (1024.0 * 1024.0);
    const double secs = wall_ms / 1000.0;
    const double mbps = secs > 0.0 ? (mb / secs) : 0.0;

    std::ofstream f(out_path, std::ios::binary);
    if (!f) return;

    // header
    f << "{\n";
    f << R"(  "version":"1",)"
      << "\n  " << fmt::format(R"("started_at":"{}",)", started_iso)
      << "\n  " << fmt::format(R"("ended_at":"{}",)", ended_iso)
      << "\n  " << fmt::format(R"("wall_time_ms":{},)", wall_ms)
      << "\n  " << fmt::format(R"("rows":{},)", rows)
      << "\n  " << fmt::format(R"("input_bytes":{},)", input_bytes)
      << "\n  " << fmt::format(R"("throughput_input_mb_s":{},)", mbps)
      << "\n  " << fmt::format(R"("rss_peak_mb":{},)", rss_peak_mb)
      << "\n  " << fmt::format(R"("cpu_user_pct":{},)", cpu_user_pct)
      << "\n  " << fmt::format(R"("cpu_sys_pct":{},)", cpu_sys_pct)
      << "\n  " << R"("errors":0,)"
      << "\n  " << R"("cache_hit_pct":null,)"
      << "\n  " << R"("build":{"type":"Debug","flags":""},)"
      << "\n  " << R"("host":{"os":"windows","arch":"x86_64"},)";

    // stages
    f << "\n  \"stages\":[\n";
    for (size_t i = 0; i < stages.size(); ++i) {
        const auto& s = stages[i];
        f << "    {"
          << fmt::format(R"("name":"{}","calls":{})", s.name, s.calls);
        if (s.p50_ms > 0.0) f << fmt::format(R"(,"p50_ms":{})", s.p50_ms);
        if (s.p95_ms > 0.0) f << fmt::format(R"(,"p95_ms":{})", s.p95_ms);
        f << "}";
        if (i + 1 < stages.size()) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    // samples
    f << "  \"samples\":[\n";
    for (size_t i = 0; i < samples.size(); ++i) {
        const auto& s = samples[i];
        f << "    {"
          << fmt::format(R"("ts_ms":{},"bytes_in":{},"rss_mb":{})", s.ts_ms, s.bytes_in, s.rss_mb);
        if (s.bytes_out > 0) f << fmt::format(R"(,"bytes_out":{})", s.bytes_out);
        if (s.cpu_pct > 0.0) f << fmt::format(R"(,"cpu_pct":{})", s.cpu_pct);
        f << "}";
        if (i + 1 < samples.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n";

    f << "}\n";
}
