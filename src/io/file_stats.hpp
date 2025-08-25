#pragma once
#include <filesystem>
#include <cstdint>

inline std::uintmax_t file_size_bytes(const std::filesystem::path& p) {
    std::error_code ec;
    auto sz = std::filesystem::file_size(p, ec);
    return ec ? 0u : static_cast<std::uintmax_t>(sz);
}
