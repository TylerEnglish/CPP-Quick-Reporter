// src/util/json_escape.hpp
#pragma once
#include <string>
#include <string_view>

namespace csvqr {

// Minimal JSON string escaper.
// Escapes: backslash, quote, control chars (< 0x20), and common whitespace.
inline std::string json_escape(std::string_view in) {
    std::string out;
    out.reserve(in.size() + 16);
    auto hex = [](unsigned v)->char {
        return (v < 10) ? char('0' + v) : char('a' + (v - 10));
    };

    for (unsigned char c : in) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    // \u00XX
                    out += "\\u00";
                    out += hex((c >> 4) & 0xF);
                    out += hex(c & 0xF);
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

}
