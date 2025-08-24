#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <string>
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <system_error>
#include <cstdint>
#include <chrono>

#if defined(_WIN32)
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif
  #include <windows.h>
  #include <processthreadsapi.h>
#else
  #include <sys/resource.h>
  #include <sys/time.h>
  #include <unistd.h>
#endif

#include "../cli/cli_options.hpp"
#include "../io/file_stats.hpp"
#include "../metrics/timers.hpp"
#include "../metrics/process_stats.hpp"
#include "../report/emit_run_json.hpp"
#include "../report/emit_profile_json.hpp"
#include "../report/emit_dag_json.hpp"
#include "../report/render_report.hpp"
#include "../csv/csv_count.hpp"

namespace fs = std::filesystem;

// ---------- small helpers ----------
static std::string now_iso_utc() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

static std::string gen_project_id() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "quick-reporter-%Y%m%d-%H%M%S", &tm);
    return std::string(buf);
}

// Copy directory recursively (overwrite existing files)
static bool copy_dir_tree(const fs::path& src, const fs::path& dst, std::string* err = nullptr) {
    std::error_code ec;
    if (!fs::exists(src, ec) || !fs::is_directory(src, ec)) {
        if (err) *err = "source does not exist or is not a directory: " + src.string();
        return false;
    }
    fs::create_directories(dst, ec);
    if (ec) {
        if (err) *err = "failed to create destination: " + dst.string() + " (" + ec.message() + ")";
        return false;
    }
    for (auto const& entry : fs::recursive_directory_iterator(src, ec)) {
        if (ec) {
            if (err) *err = "iter error at: " + src.string() + " (" + ec.message() + ")";
            return false;
        }
        const auto rel = fs::relative(entry.path(), src, ec);
        const auto out = dst / rel;
        if (entry.is_directory()) {
            fs::create_directories(out, ec);
            if (ec) {
                if (err) *err = "failed to mkdir: " + out.string() + " (" + ec.message() + ")";
                return false;
            }
        } else if (entry.is_regular_file()) {
            fs::create_directories(out.parent_path(), ec);
            fs::copy_file(entry.path(), out, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                if (err) *err = "copy failed: " + entry.path().string() + " -> " + out.string() + " (" + ec.message() + ")";
                return false;
            }
        }
    }
    return true;
}

// heuristics to locate the source assets folder that contains app.js/app.css/vendor/*
static fs::path find_assets_src() {
    auto has_marker = [](const fs::path& root) -> bool {
        std::error_code ec;
        return fs::exists(root / "app.js", ec)
            && fs::exists(root / "app.css", ec)
            && fs::exists(root / "vendor" / "vega.min.js", ec)
            && fs::exists(root / "vendor" / "vega-lite.min.js", ec)
            && fs::exists(root / "vendor" / "vega-embed.min.js", ec);
    };

    // 1) explicit env var
    if (const char* env = std::getenv("CSVQR_ASSETS_DIR")) {
        fs::path p(env);
        if (has_marker(p)) return p;
    }

    // 2) ./templates/assets (cwd)
    fs::path cwd_assets = fs::current_path() / "templates" / "assets";
    if (has_marker(cwd_assets)) return cwd_assets;

    // 3) next to executable: <exe>/templates/assets
    fs::path exe_assets = csvqr::exe_dir() / "templates" / "assets";
    if (has_marker(exe_assets)) return exe_assets;

    // 4) build-staged location: <build>/assets (often parent of exe dir)
    fs::path staged = csvqr::exe_dir().parent_path() / "assets";
    if (has_marker(staged)) return staged;

    // 5) fallback: <exe>/assets
    fs::path exe_assets_flat = csvqr::exe_dir() / "assets";
    if (has_marker(exe_assets_flat)) return exe_assets_flat;

    return {}; // not found
}

// ---------- StageTimer (tiny helper for stages[] ----------
struct StageTimer {
    std::string   name;
    std::uint64_t calls = 0;   // ← was int
    WallTimer     wt{};
    double        last_ms = 0.0;

    explicit StageTimer(const char* n) : name(n ? n : "(stage)") {}
    void start() { wt.start(); }
    void stop()  { wt.stop(); last_ms = wt.ms(); ++calls; }

    RunStage as_stage() const {
        return RunStage{ name, calls, last_ms, last_ms };
    }
};

// ---------- CPU meter (process % normalized by logical CPUs) ----------
struct CpuMeter {
    int ncpu = 1;
    std::chrono::steady_clock::time_point last_wall;
    double last_proc_s = 0.0;   // user+kernel seconds
    double last_pct = 0.0;

    CpuMeter() {
#if defined(_WIN32)
        DWORD cnt = 0;
        #if defined(PROCESSOR_NUMBER) || _WIN32_WINNT >= 0x0601
            cnt = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        #endif
        if (cnt == 0) {
            SYSTEM_INFO si{};
            GetSystemInfo(&si);
            cnt = si.dwNumberOfProcessors ? si.dwNumberOfProcessors : 1;
        }
        ncpu = static_cast<int>(cnt);
#else
        long cnt = ::sysconf(_SC_NPROCESSORS_ONLN);
        if (cnt < 1) cnt = 1;
        ncpu = static_cast<int>(cnt);
#endif
    }

    static double proc_seconds_now() {
#if defined(_WIN32)
        FILETIME ftCreate{}, ftExit{}, ftKernel{}, ftUser{};
        if (!GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, &ftKernel, &ftUser)) {
            return 0.0;
        }
        ULARGE_INTEGER k{}, u{};
        k.LowPart = ftKernel.dwLowDateTime;  k.HighPart = ftKernel.dwHighDateTime;
        u.LowPart = ftUser.dwLowDateTime;    u.HighPart = ftUser.dwHighDateTime;
        return (static_cast<double>(k.QuadPart + u.QuadPart)) * 1e-7; // 100ns -> s
#else
        rusage ru{};
        if (getrusage(RUSAGE_SELF, &ru) != 0) return 0.0;
        double us = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec * 1e-6;
        double ss = ru.ru_stime.tv_sec + ru.ru_stime.tv_usec * 1e-6;
        return us + ss;
#endif
    }

    void begin() {
        last_wall   = std::chrono::steady_clock::now();
        last_proc_s = proc_seconds_now();
        last_pct    = 0.0;
    }

    double sample() {
        const auto now  = std::chrono::steady_clock::now();
        const double ps = proc_seconds_now();

        const double wall_dt = std::chrono::duration<double>(now - last_wall).count();
        const double proc_dt = ps - last_proc_s;

        double pct = last_pct;
        if (wall_dt > 1e-6 && ncpu > 0) {
            pct = (proc_dt / (wall_dt * static_cast<double>(ncpu))) * 100.0;
            if (pct < 0.0) pct = 0.0;
            if (pct > 100.0) pct = 100.0;
        }

        last_wall   = now;
        last_proc_s = ps;
        last_pct    = pct;
        return pct;
    }
};

int main(int argc, char** argv) try {
    auto opt = parse_cli(argc, argv);
    if (opt.project_id.empty())
        opt.project_id = gen_project_id();

    fs::path input_path = opt.input;
    if (!fs::exists(input_path)) {
        fmt::print(stderr, "ERROR: input not found: {}\n", input_path.string());
        return 2; // IO error
    }

    // --- timing + baseline RSS
    const double rss_start = process_rss_mb();
    WallTimer wt_all; wt_all.start();
    const auto started_iso = now_iso_utc();

    // --- stage: count_rows_cols (accurate final counts)
    auto first_char_or = [](const std::string& s, char fallback) -> char {
        return s.empty() ? fallback : s[0];
    };
    const char delim_char = first_char_or(opt.delimiter, ',');
    const char quote_char = first_char_or(opt.quote, '"');
    const bool header     = opt.has_header;

    std::vector<RunStage> stages;

    StageTimer st_count("count_rows_cols");
    st_count.start();

    const CsvCounts counts = csv_count_rows_cols(
        input_path.string(),
        delim_char,
        quote_char,
        static_cast<size_t>(opt.chunk_bytes),
        header
    );

    st_count.stop();
    stages.push_back(st_count.as_stage());

    // --- stage: scan_chunks (timeline samples w/ CPU%)
    StageTimer st_scan("scan_chunks");
    st_scan.start();

    std::vector<RunSample> samples;
    const std::uint64_t file_bytes = file_size_bytes(input_path);

    std::ifstream in(input_path, std::ios::binary);
    if (in) {
        const size_t chunk = std::max<size_t>(1, static_cast<size_t>(opt.chunk_bytes > 0 ? opt.chunk_bytes : (1 << 20)));
        std::vector<char> buf(chunk);
        std::uint64_t bytes_in = 0;
        std::uint64_t rows_in  = 0;

        WallTimer wt_scan_clock; wt_scan_clock.start();
        CpuMeter cpu; cpu.begin();

        while (in) {
            in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
            const std::streamsize got = in.gcount();
            if (got <= 0) break;

            bytes_in += static_cast<std::uint64_t>(got);

            // quick newline-based row approximation for timeline only
            const char* p = buf.data();
            for (std::streamsize i = 0; i < got; ++i) {
                if (p[i] == '\n') ++rows_in;
            }

            const std::uint64_t ts_ms = static_cast<std::uint64_t>(wt_scan_clock.ms());
            const double rss_now = process_rss_mb();
            const double cpu_pct = cpu.sample();

            samples.push_back(RunSample{
                ts_ms,
                bytes_in,
                rows_in,
                rss_now,
                cpu_pct
            });
        }

        // Ensure a final sample at EOF
        if (!samples.empty() && samples.back().bytes_in < file_bytes) {
            const std::uint64_t ts_ms = static_cast<std::uint64_t>(wt_scan_clock.ms());
            const double rss_now = process_rss_mb();
            const double cpu_pct_final = cpu.sample();  // <— get a reading
            samples.push_back(RunSample{ ts_ms, file_bytes, rows_in, rss_now, cpu_pct_final });
        }
    }

    st_scan.stop();
    stages.push_back(st_scan.as_stage());

    // --- finalize run stats
    wt_all.stop();
    const auto ended_iso = now_iso_utc();
    const double wall_ms = wt_all.ms();
    const double rss_end = process_rss_mb();
    const double rss_peak = std::max(rss_start, rss_end); // safe: NOMINMAX prevents macro collision

    // --- artifacts
    const fs::path out_dir       = ensure_artifacts_dir(opt.output_root, opt.project_id);
    const fs::path run_json      = out_dir / "run.json";
    const fs::path profile_json  = out_dir / "profile.json";
    const fs::path dag_json      = out_dir / "dag.json";
    const fs::path report_html   = out_dir / "report.html";

    // --- emit JSON artifacts
    emit_run_json(run_json.string(), started_iso, ended_iso, wall_ms, file_bytes, counts.rows,
                  stages, samples, /*rss_peak_mb*/ rss_peak, /*cpu_user_pct*/ 0.0, /*cpu_sys_pct*/ 0.0);

    // keep profile minimal for now; step 2 will fill real column data
    csvqr::emit_profile_json_scan_file(
        profile_json.string(),
        input_path.string(),
        /*rows_hint*/ counts.rows,
        delim_char,
        quote_char,
        header
    );
    emit_dag_json(dag_json.string());

    // --- copy report assets (JS/CSS/vendor) next to report.html
    {
        const fs::path assets_dst = out_dir / "assets";
        const fs::path assets_src = find_assets_src();
        if (assets_src.empty()) {
            fmt::print(stderr,
                "WARN: could not find report assets (app.js/app.css/vendor/*). "
                "Set CSVQR_ASSETS_DIR or ensure templates/assets/ exists next to the exe or in the build dir.\n");
        } else {
            std::string err;
            if (!copy_dir_tree(assets_src, assets_dst, &err)) {
                fmt::print(stderr, "WARN: failed to copy assets: {}\n", err);
            }
        }
    }

    // --- render report (template references local ./assets/*)
    try {
        const fs::path tmpl = fs::path("templates") / "report.mustache";
        csvqr::render_report(tmpl, profile_json, run_json, dag_json, report_html);
    } catch (const std::exception& re) {
        fmt::print(stderr, "WARN: report render failed: {}\n", re.what());
    }

    fmt::print("OK {}\n", out_dir.string());
    return 0;
}
catch (const CLI::ParseError&) {
    return 1; // bad args already printed by CLI11
}
catch (const std::exception& e) {
    fmt::print(stderr, "ERROR: {}\n", e.what());
    return 4; // internal error
}
