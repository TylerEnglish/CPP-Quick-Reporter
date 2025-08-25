#pragma once
#include <string_view>
#include <string>
#include <cctype>
#include <optional>

namespace csvqr {

enum class logical_type { int64_, float64_, boolean_, date_, datetime_, string_ };

inline bool is_int64(std::string_view s) {
    if (s.empty()) return false;
    std::size_t i = (s[0] == '+' || s[0] == '-') ? 1 : 0;
    if (i >= s.size()) return false;
    for (; i < s.size(); ++i) if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    return true;
}
inline bool is_float64(std::string_view s) {
    if (s.empty()) return false;
    bool dot = false, exp = false, digit = false;
    std::size_t i = (s[0] == '+' || s[0] == '-') ? 1 : 0;
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (std::isdigit(static_cast<unsigned char>(c))) { digit = true; continue; }
        if (c == '.' && !dot) { dot = true; continue; }
        if ((c == 'e' || c == 'E') && !exp) {
            exp = true;
            if (i + 1 < s.size() && (s[i + 1] == '+' || s[i + 1] == '-')) ++i;
            continue;
        }
        return false;
    }
    return digit;
}
inline bool is_bool(std::string_view s) {
    return s == "true" || s == "false" || s == "TRUE" || s == "FALSE" ||
           s == "1" || s == "0";
}

inline logical_type infer_type(std::string_view s) {
    if (is_int64(s))   return logical_type::int64_;
    if (is_float64(s)) return logical_type::float64_;
    if (is_bool(s))    return logical_type::boolean_;
    // date/datetime will be handled by parse_date formats later
    return logical_type::string_;
}

inline const char* to_string(logical_type t) {
    switch (t) {
        case logical_type::int64_:   return "int64";
        case logical_type::float64_: return "float64";
        case logical_type::boolean_: return "bool";
        case logical_type::date_:    return "date";
        case logical_type::datetime_:return "datetime";
        default:                     return "string";
    }
}

}
