#pragma once
#include <mustache.hpp>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <stdexcept>

namespace csvqr {

inline std::string slurp(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("Failed to read: " + p.string());
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

// Renders report.html by embedding the three JSON blobs into Mustache context.
// The template should reference them with triple braces: {{{run_json}}}, {{{profile_json}}}, {{{dag_json}}}
inline void render_report(const std::filesystem::path& template_path,
                          const std::filesystem::path& profile_json,
                          const std::filesystem::path& run_json,
                          const std::filesystem::path& dag_json,
                          const std::filesystem::path& out_html) {
    const auto tmpl = slurp(template_path);
    kainjow::mustache::mustache m{tmpl};

    kainjow::mustache::data ctx;
    // triple braces in template prevent HTML-escaping, so we pass raw JSON strings
    ctx.set("profile_json", slurp(profile_json));
    ctx.set("run_json",     slurp(run_json));
    ctx.set("dag_json",     slurp(dag_json));

    const auto rendered = m.render(ctx);

    std::ofstream out(out_html, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to write: " + out_html.string());
    out << rendered;
}

}