#pragma once
#include <fmt/format.h>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

// Minimal JSON string escaper for paths/short strings.
inline std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '\"': out += "\\\""; break;
            default:   out += c;      break;
        }
    }
    return out;
}

// Writes profile.json (schema v1).
// MVP: emit N synthetic columns (col1..colN) with non_null_count=rows (refine when profiling is wired).
inline void emit_profile_json(const std::string& out_path,
                              const std::string& source_path,
                              std::uint64_t rows,
                              std::uint32_t columns,
                              bool header_present)
{
    std::ofstream f(out_path, std::ios::binary);
    if (!f) throw std::runtime_error("Failed to open for write: " + out_path);

    const std::string escaped_path = json_escape(source_path);
    const std::uint64_t non_null = rows;

    f << fmt::format(
R"({{
  "version":"1",
  "dataset":{{"rows":{},"columns":{},"header_present":{},"source_path":"{}"}},
  "columns":[)",
        rows,
        columns,
        header_present ? "true" : "false",
        escaped_path
    );

    if (columns == 0) {
        // schema requires minItems=1
        f << R"({"name":"_unknown","logical_type":"string","null_count":0,"non_null_count":0})";
    } else {
        for (std::uint32_t i = 0; i < columns; ++i) {
            const std::string name = "col" + std::to_string(i+1);
            f << fmt::format(
                R"({{"name":"{}","logical_type":"string","null_count":0,"non_null_count":{}}})",
                name, non_null
            );
            if (i + 1 < columns) f << ",";
        }
    }

    f << "]\n}\n";
}
