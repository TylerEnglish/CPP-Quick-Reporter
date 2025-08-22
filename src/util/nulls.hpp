#pragma once
#include <string_view>
#include <string>
#include <vector>
#include <algorithm>

namespace csvqr {

inline bool is_null_like(std::string_view s, const std::vector<std::string>& nulls) {
    for (const auto& n : nulls) {
        if (s == n) return true;
    }
    return false;
}

}
