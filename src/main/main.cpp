#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <string>

#include "../cli/cli_options.hpp"
#include "../io/file_stats.hpp"
#include "../metrics/timers.hpp"
#include "../report/emit_run_json.hpp"
#include "../report/emit_profile_json.hpp"
#include "../report/emit_dag_json.hpp"
#include "../report/render_report.hpp"
#include "../csv/csv_count.hpp"

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

int main(int argc, char** argv) try {
    auto opt = parse_cli(argc, argv);
    if (opt.project_id.empty())
        opt.project_id = gen_project_id();

    namespace fs = std::filesystem;
    fs::path input_path = opt.input;
    if (!fs::exists(input_path)) {
        fmt::print(stderr, "ERROR: input not found: {}\n", input_path.string());
        return 2; // IO error
    }

    WallTimer wt; wt.start();
    const auto started_iso = now_iso_utc();

    // --- fast row/column count (minimal RFC4180) ---
    auto first_char_or = [](const std::string& s, char fallback) -> char {
        return s.empty() ? fallback : s[0];
    };

    const char delim_char = first_char_or(opt.delimiter, ',');
    const char quote_char = first_char_or(opt.quote, '"');
    const bool header     = opt.has_header;

    const CsvCounts counts = csv_count_rows_cols(
        input_path.string(),                 // csv_count_* expects std::string
        delim_char,
        quote_char,
        static_cast<size_t>(opt.chunk_bytes),
        header
    );

    const auto bytes = file_size_bytes(input_path);

    wt.stop();
    const auto ended_iso = now_iso_utc();
    const double wall_ms = wt.ms();

    // Prepare artifacts dir and write files
    const fs::path out_dir       = ensure_artifacts_dir(opt.output_root, opt.project_id);
    const fs::path run_json      = out_dir / "run.json";
    const fs::path profile_json  = out_dir / "profile.json";
    const fs::path dag_json      = out_dir / "dag.json";
    const fs::path report_html   = out_dir / "report.html";
    const fs::path report_tmpl   = fs::path("templates") / "report.mustache";

    // Emit JSON artifacts (use real row count & bytes)
    emit_run_json(run_json.string(), started_iso, ended_iso, wall_ms, bytes, counts.rows);
    emit_profile_json(profile_json.string(), input_path.string(), counts.rows, counts.columns, header);
    emit_dag_json(dag_json.string());

    // Render HTML report by embedding the actual JSON blobs (what the template expects)
    try {
        csvqr::render_report(report_tmpl, profile_json, run_json, dag_json, report_html);
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
