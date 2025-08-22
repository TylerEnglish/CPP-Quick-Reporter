#pragma once
#include <string_view>
#include <vector>
#include <cstddef>

namespace csvqr {

struct record_view {
    std::vector<std::string_view> fields;

    std::size_t size() const noexcept { return fields.size(); }
    std::string_view operator[](std::size_t i) const noexcept { return fields[i]; }
    void clear() noexcept { fields.clear(); }
    void push(std::string_view sv) { fields.push_back(sv); }
};

} // namespace csvqr
