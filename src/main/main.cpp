#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <string>
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <system_error>

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
    WallTimer wt; wt.start();
    const auto started_iso = now_iso_utc();

    // --- fast row/column count (minimal RFC4180)
    auto first_char_or = [](const std::string& s, char fallback) -> char {
        return s.empty() ? fallback : s[0];
    };

    const char delim_char = first_char_or(opt.delimiter, ',');
    const char quote_char = first_char_or(opt.quote, '"');
    const bool header     = opt.has_header;

    const CsvCounts counts = csv_count_rows_cols(
        input_path.string(),
        delim_char,
        quote_char,
        static_cast<size_t>(opt.chunk_bytes),
        header
    );

    const auto bytes = file_size_bytes(input_path);

    wt.stop();
    const auto ended_iso = now_iso_utc();
    const double wall_ms = wt.ms();
    const double rss_end = process_rss_mb();
    const double rss_peak = std::max(rss_start, rss_end); // simple peak for now

    // --- minimal samples (start / mid / end)
    std::vector<RunSample> samples;
    const std::uint64_t t0 = 0;
    const std::uint64_t t1 = static_cast<std::uint64_t>(wall_ms * 0.5);
    const std::uint64_t t2 = static_cast<std::uint64_t>(std::max(1.0, wall_ms));
    const std::uint64_t half = static_cast<std::uint64_t>(bytes / 2);

    samples.push_back(RunSample{ t0, 0,    0, rss_start, 0.0 });
    samples.push_back(RunSample{ t1, half, 0, (rss_start + rss_end) * 0.5, 0.0 });
    samples.push_back(RunSample{ t2, static_cast<std::uint64_t>(bytes), 0, rss_end, 0.0 });

    // --- per-stage (single stage for MVP)
    std::vector<RunStage> stages;
    stages.push_back(RunStage{ "read_chunks", 1, wall_ms, wall_ms });

    // --- artifacts
    const fs::path out_dir       = ensure_artifacts_dir(opt.output_root, opt.project_id);
    const fs::path run_json      = out_dir / "run.json";
    const fs::path profile_json  = out_dir / "profile.json";
    const fs::path dag_json      = out_dir / "dag.json";
    const fs::path report_html   = out_dir / "report.html";

    // --- emit JSON artifacts
    emit_run_json(run_json.string(), started_iso, ended_iso, wall_ms, bytes, counts.rows,
                  stages, samples, /*rss_peak_mb*/ rss_peak, /*cpu_user_pct*/ 0.0, /*cpu_sys_pct*/ 0.0);

    emit_profile_json(profile_json.string(), input_path.string(), counts.rows, counts.columns, header);
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

    // --- render report (the template expects {{{run_json}}}, {{{profile_json}}}, {{{dag_json}}})
    // The template itself references local ./assets/app.css & ./assets/*.js
    try {
        const fs::path tmpl = fs::path("templates") / "report.mustache"; // render_report resolves via exe_dir/cwd too
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
