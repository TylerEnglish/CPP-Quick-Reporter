// src/report/emit_profile_json.hpp
#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include "../profile/profile.hpp"
#include "../util/json_escape.hpp"

namespace csvqr {

// Overload that accepts computed columns
inline void emit_profile_json(const std::string& out_path,
                              const std::string& source_path,
                              std::uint64_t rows,
                              bool header_present,
                              const std::vector<ColumnSummary>& cols)
{
    std::ofstream os(out_path, std::ios::binary);
    if (!os) return;

    os << "{\n";
    os << "  \"version\":\"1\",\n";
    os << "  \"dataset\":{";
    os << "\"rows\":" << rows << ",";
    os << "\"columns\":" << cols.size() << ",";
    os << "\"header_present\":" << (header_present ? "true" : "false") << ",";
    os << "\"source_path\":\"" << csvqr::json_escape(source_path) << "\"";
    os << "},\n";

    os << "  \"columns\":[";
    for (size_t i = 0; i < cols.size(); ++i) {
        const auto& c = cols[i];
        if (i) os << ",";
        os << "{";
        os << "\"name\":\""          << csvqr::json_escape(c.name)         << "\",";
        os << "\"logical_type\":\""  << csvqr::json_escape(c.logical_type) << "\",";
        os << "\"null_count\":"      << c.null_count                        << ",";
        os << "\"non_null_count\":"  << c.non_null_count;
        os << "}";
    }
    os << "]\n";
    os << "}\n";
}

// Convenience: compute + emit from file
inline void emit_profile_json_scan_file(const std::string& out_path,
                                        const std::string& source_path,
                                        std::uint64_t /*rows_hint*/,
                                        char delim,
                                        char quote,
                                        bool header_present)
{
    auto pr = csvqr::profile_csv_file(source_path, delim, quote, header_present);
    emit_profile_json(out_path, source_path, pr.rows, header_present, pr.columns);
}

}
