#pragma once
#include <mustache.hpp>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <stdexcept>

#if defined(_WIN32)
  #include <windows.h>
#elif defined(__APPLE__)
  #include <mach-o/dyld.h>
#endif

namespace csvqr {

// ---------- utils ----------
inline std::string read_file(const std::filesystem::path& p, bool* ok = nullptr) {
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        if (ok) *ok = false;
        // Return minimal JSON so the template still renders something visible
        std::ostringstream err;
        err << R"({"_error":"Failed to read )" << p.string() << R"("})";
        return err.str();
    }
    if (ok) *ok = true;
    std::ostringstream oss; oss << f.rdbuf();
    return oss.str();
}

// Prevent "</script>" from prematurely closing the script tag in HTML
inline std::string sanitize_for_script(std::string s) {
    std::string::size_type pos = 0;
    const std::string needle = "</script>";
    const std::string repl   = "<\\/script>";
    while ((pos = s.find(needle, pos)) != std::string::npos) {
        s.replace(pos, needle.size(), repl);
        pos += repl.size();
    }
    return s;
}

inline std::filesystem::path exe_dir() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return std::filesystem::current_path();
    return std::filesystem::path(buf).parent_path();
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string tmp(size, '\0');
    if (_NSGetExecutablePath(tmp.data(), &size) != 0) return std::filesystem::current_path();
    std::error_code ec;
    auto p = std::filesystem::weakly_canonical(std::filesystem::path(tmp), ec);
    if (ec) p = std::filesystem::path(tmp);
    return p.parent_path();
#else
    std::error_code ec;
    auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) return std::filesystem::current_path();
    return p.parent_path();
#endif
}

inline std::filesystem::path first_existing(const std::vector<std::filesystem::path>& candidates,
                                            std::string* tried = nullptr) {
    for (const auto& c : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(c, ec) && std::filesystem::is_regular_file(c, ec)) {
            return c;
        }
        if (tried) *tried += "  - " + c.string() + "\n";
    }
    return {};
}

// ---------- main ----------
/**
 * Renders report.html by embedding the three JSON blobs into Mustache context.
 * Template must use triple braces: {{{run_json}}}, {{{profile_json}}}, {{{dag_json}}}
 *
 * @param template_path  Exact path or "report.mustache". If not found, tries:
 *                       <exe_dir>/templates/<name>, then <cwd>/templates/<name>.
 */
inline void render_report(const std::filesystem::path& template_path,
                          const std::filesystem::path& profile_json_path,
                          const std::filesystem::path& run_json_path,
                          const std::filesystem::path& dag_json_path,
                          const std::filesystem::path& out_html) {
    namespace fs = std::filesystem;

    // Resolve template
    fs::path resolved = template_path;
    std::string tried;
    std::error_code ec;
    if (!(fs::exists(resolved, ec) && fs::is_regular_file(resolved, ec))) {
        const fs::path tpl_name = template_path.filename();
        const auto exedir = exe_dir();
        const auto cwdir  = fs::current_path();
        std::vector<fs::path> candidates = {
            exedir / "templates" / tpl_name,
            cwdir  / "templates" / tpl_name
        };
        resolved = first_existing(candidates, &tried);
        if (resolved.empty()) {
            std::ostringstream msg;
            msg << "Template not found. Looked at:\n" << tried
                << "Original requested path: " << template_path.string();
            throw std::runtime_error(msg.str());
        }
    }

    // Load template
    std::ifstream tf(resolved, std::ios::binary);
    if (!tf) throw std::runtime_error("Failed to read template: " + resolved.string());
    std::ostringstream tss; tss << tf.rdbuf();
    const std::string tmpl = tss.str();

    // Load JSON payloads
    bool ok_profile=false, ok_run=false, ok_dag=false;
    std::string profile_blob = read_file(profile_json_path, &ok_profile);
    std::string run_blob     = read_file(run_json_path, &ok_run);
    std::string dag_blob     = read_file(dag_json_path, &ok_dag);

    // Optional tiny sentinel so it's obvious they aren't empty (harmless in JSON)
    if (!profile_blob.empty() && profile_blob.front() != '{') profile_blob = "{}";
    if (!run_blob.empty()     && run_blob.front()     != '{') run_blob     = "{}";
    if (!dag_blob.empty()     && dag_blob.front()     != '{') dag_blob     = "{}";

    // Sanitize for embedding in <script type="application/json">
    profile_blob = sanitize_for_script(std::move(profile_blob));
    run_blob     = sanitize_for_script(std::move(run_blob));
    dag_blob     = sanitize_for_script(std::move(dag_blob));

    // Mustache render
    kainjow::mustache::mustache m{tmpl};
    if (!m.is_valid()) throw std::runtime_error("Mustache template parse error");

    kainjow::mustache::data ctx;
    // Be explicit about value types to avoid odd overload resolution issues
    ctx.set("profile_json", kainjow::mustache::data(profile_blob));
    ctx.set("run_json",     kainjow::mustache::data(run_blob));
    ctx.set("dag_json",     kainjow::mustache::data(dag_blob));

    const std::string rendered = m.render(ctx);

    std::ofstream out(out_html, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to write: " + out_html.string());
    out << rendered;
}

}
