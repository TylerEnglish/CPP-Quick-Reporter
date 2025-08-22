#pragma once
#include <cstdint>
#include <chrono>

namespace csvqr {

struct proc_sample {
    std::int64_t ts_ms{0};
    double rss_mb{0.0};
    double cpu_pct{0.0};
    std::uint64_t bytes_in{0};
    std::uint64_t bytes_out{0};
};

// Cheap, portable “do nothing” sampler; wire up real Windows APIs later.
inline proc_sample sample_process_now() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    proc_sample s;
    s.ts_ms = static_cast<std::int64_t>(ms);
    return s;
}

}
