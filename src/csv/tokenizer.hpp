#pragma once
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>

// Count rows and columns without materializing cells.
// Handles RFC4180 quotes, CRLF, delimiters only when not in quotes.
// Columns = token count in the first logical row (header or first data row).
struct CsvCounts {
    uint64_t rows = 0;        // logical rows (line breaks not in quotes)
    uint64_t columns = 0;     // tokens in first row
};

inline CsvCounts csv_count_rows_cols(std::string_view path,
                                     char delimiter = ',',
                                     char quote = '"',
                                     size_t chunk_bytes = 262144,
                                     bool has_header = true)
{
    std::ifstream is(std::string(path), std::ios::binary);
    if (!is) throw std::runtime_error("csv_count_rows_cols: cannot open file");

    std::string buf;
    buf.resize(chunk_bytes);

    bool in_quotes = false;
    bool first_row_done = false;
    uint64_t rows = 0;
    uint64_t first_row_cols = 0;
    uint64_t cur_cols = 1; // columns = delimiters + 1, when not in quotes
    bool at_line_start = true;

    // Track if previous char was CR to swallow CRLF as one newline
    bool prev_was_cr = false;

    auto finish_row = [&]() {
        rows++;
        if (!first_row_done) {
            first_row_cols = cur_cols;
            first_row_done = true;
        }
        cur_cols = 1;
        at_line_start = true;
    };

    while (is) {
        is.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        std::streamsize n = is.gcount();
        if (n <= 0) break;

        for (std::streamsize i = 0; i < n; ++i) {
            char c = buf[static_cast<size_t>(i)];

            if (prev_was_cr) {
                // If LF follows CR, treat CRLF as a single newline.
                prev_was_cr = false;
                if (c == '\n') {
                    continue; // already finished row on CR
                }
            }

            if (c == quote) {
                // Double quote inside quotes -> escaped quote; advance over the next quote
                if (in_quotes) {
                    // Lookahead: if the next char is also a quote, it's an escaped quote
                    if (i + 1 < n && buf[static_cast<size_t>(i + 1)] == quote) {
                        ++i; // consume the escaped quote
                    } else {
                        in_quotes = false;
                    }
                } else {
                    in_quotes = true;
                }
                at_line_start = false;
            } else if (!in_quotes && c == delimiter) {
                ++cur_cols;
                at_line_start = false;
            } else if (!in_quotes && (c == '\n' || c == '\r')) {
                finish_row();
                if (c == '\r') prev_was_cr = true; // swallow following \n
            } else {
                at_line_start = false;
            }
        }
    }

    // Handle last line if file didn’t end with newline (and we saw any non-newline chars)
    if (!at_line_start) {
        finish_row();
    }

    // If header declared, treat rows as data rows (minus header if present)
    if (has_header && rows > 0) {
        rows -= 1;
    }

    CsvCounts out;
    out.rows = rows;
    out.columns = first_row_cols > 0 ? first_row_cols : 0;
    return out;
}
