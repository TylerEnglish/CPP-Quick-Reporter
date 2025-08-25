#pragma once
#include <string>
#include <string_view>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <optional>
#include <vector>
namespace csvqr {

inline std::optional<std::tm> parse_date_any(std::string_view s,
                                             const std::vector<std::string>& fmts) {
    for (const auto& fmt : fmts) {
        std::tm tm{};
        std::istringstream iss(std::string{s});
        iss >> std::get_time(&tm, fmt.c_str());
        if (!iss.fail()) return tm;
    }
    return std::nullopt;
}

}
