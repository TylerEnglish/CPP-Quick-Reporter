#pragma once
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

namespace csvqr {

class chunk_reader {
public:
    chunk_reader(const std::filesystem::path& p, std::size_t chunk_bytes)
        : path_(p), buf_(chunk_bytes)
    {
        if (chunk_bytes == 0) throw std::invalid_argument("chunk_bytes == 0");
        in_.open(path_, std::ios::binary);
        if (!in_) throw std::runtime_error("Failed to open file: " + path_.string());
    }

    // Returns number of bytes read; 0 = EOF
    std::size_t next(std::string& out) {
        if (!in_) return 0;
        in_.read(reinterpret_cast<char*>(buf_.data()),
                 static_cast<std::streamsize>(buf_.size()));
        auto got = static_cast<std::size_t>(in_.gcount());
        out.assign(reinterpret_cast<const char*>(buf_.data()), got);
        return got;
    }

    // non-const: std::istream::peek() is non-const
    bool eof() { return in_.peek() == std::char_traits<char>::eof(); }

private:
    std::filesystem::path path_;
    std::ifstream in_;
    std::vector<unsigned char> buf_;
};

}