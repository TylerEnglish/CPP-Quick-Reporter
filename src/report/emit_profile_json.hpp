#pragma once
#include <fmt/format.h>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

// Minimal JSON string escaper for paths/short strings.
// Escapes backslash and double quote.
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

// Writes profile.json (schema v1) with valid fields: rows, columns, header, source_path.
inline void emit_profile_json(const std::string& out_path,
                              const std::string& source_path,
                              std::uint64_t rows,
                              std::uint32_t columns,
                              bool header_present)
{
    std::ofstream f(out_path, std::ios::binary);
    if (!f) throw std::runtime_error("Failed to open for write: " + out_path);

    const std::string escaped_path = json_escape(source_path);

    // Minimal "columns" array to satisfy schema; you can replace with real columns later.
    f << fmt::format(
        R"({{
  "version":"1",
  "dataset":{{"rows":{},"columns":{},"header_present":{},"source_path":"{}"}},
  "columns":[
    {{"name":"_unknown","logical_type":"string","null_count":0,"non_null_count":0}}
  ]
}})",
        rows,
        columns,
        header_present ? "true" : "false",
        escaped_path
    );
}
