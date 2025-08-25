#pragma once
#include <CLI/CLI.hpp>
#include <string>
#include <filesystem>
#include <stdexcept>

struct AppOptions {
    // Required/paths
    std::string input;
    std::string config = "config/config.toml";
    std::string project_id;
    std::string output_root = "artifacts";

    // Perf
    int64_t     chunk_bytes = 262144;   // 256 KiB default
    double      sample_frac = 0.10;     // 0..1

    // CSV parsing
    std::string delimiter = ",";        // single char, e.g. ","
    std::string quote     = "\"";       // single char, e.g. "\""
    std::string escape    = "\\";       // single char, e.g. "\\"
    bool        has_header = true;      // header row present?
};

inline AppOptions parse_cli(int argc, char** argv) {
    AppOptions opt;
    CLI::App app{"CSV → Quick Reporter"};
    app.set_version_flag("--version", "0.1.0");

    // Required/basic
    app.add_option("--input",       opt.input,      "Path to input CSV")->required();
    app.add_option("--config",      opt.config,     "Path to config.toml");
    app.add_option("--project-id",  opt.project_id, "Project/run identifier");
    app.add_option("--output-root", opt.output_root,"Artifacts output root");

    // Perf
    app.add_option("--chunk-bytes", opt.chunk_bytes,"Chunk size (bytes)");
    app.add_option("--sample-frac", opt.sample_frac,"Typed sample fraction (0..1)");

    // CSV parsing
    app.add_option("-d,--delimiter", opt.delimiter,
                   "CSV delimiter (single character, default ',')")->default_val(",");
    app.add_option("-q,--quote",     opt.quote,
                   "CSV quote (single character, default '\"')")->default_val("\"");
    app.add_option("-e,--escape",    opt.escape,
                   "CSV escape (single character, default '\\')")->default_val("\\");
    app.add_option("--has-header",   opt.has_header,
                   "CSV has a header row (true/false)")->default_val(true);

    app.allow_windows_style_options();
    app.parse(argc, argv);

    // --- Validation ---
    auto one_char = [](const std::string& s, const char* name){
        if (s.size() != 1)
            throw CLI::ValidationError{name, "must be a single character"};
    };
    one_char(opt.delimiter, "delimiter");
    one_char(opt.quote,     "quote");
    one_char(opt.escape,    "escape");

    if (opt.sample_frac < 0.0 || opt.sample_frac > 1.0)
        throw CLI::ValidationError{"sample-frac", "must be in [0, 1]"};
    if (opt.chunk_bytes <= 0)
        throw CLI::ValidationError{"chunk-bytes", "must be > 0"};

    return opt;
}

inline std::filesystem::path ensure_artifacts_dir(const std::string& root, const std::string& project_id) {
    namespace fs = std::filesystem;
    fs::path dir = fs::path(root) / project_id;
    fs::create_directories(dir);
    return dir;
}
