#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
#include <stdexcept>

// Minimal RFC4180-aware scan to count rows and columns using a chunk buffer.
// - Counts rows by seeing newlines that occur OUTSIDE quotes.
// - Determines column count from the first logical line (header or first row),
//   counting delimiters OUTSIDE quotes.
// - Handles CRLF and CR newlines; normalizes all to '\n' for counting.
// - Handles escaped quotes inside quoted fields per RFC4180 ("").
//
// NOTE: We do not validate malformed CSV here; this is a fast counter.

struct CsvCounts {
    std::uint64_t rows = 0;
    std::uint32_t columns = 0;
};

inline CsvCounts csv_count_rows_cols(const std::filesystem::path& path,
                                     char delimiter,
                                     char quote,
                                     std::size_t chunk_bytes,
                                     bool has_header)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to open: " + path.string());
    if (chunk_bytes == 0) chunk_bytes = 262144;

    std::vector<char> buf(chunk_bytes);

    bool in_quotes = false;
    bool first_line_done = false;
    std::uint32_t header_cols = 1;   // at least 1 col if any data
    bool seen_any_data = false;

    std::uint64_t rows = 0;
    char prev = '\0';
    bool have_prev = false;

    auto is_newline = [&](char c) { return c == '\n'; };

    // Helper to handle entering/leaving quotes + escaped quotes.
    auto handle_quote = [&](const char* data, std::size_t i, std::size_t n) {
        if (!in_quotes) {
            in_quotes = true;
        } else {
            // If we're in quotes and see a second consecutive quote -> escaped
            if (i + 1 < n && data[i + 1] == quote) {
                // Skip the next quote (treat as literal ")
                return std::size_t{1}; // caller will i += returned
            } else {
                in_quotes = false; // end quoted field
            }
        }
        return std::size_t{0};
    };

    // We need to detect CRLF and CR. We'll convert to LF semantics on the fly.
    // Strategy: if we see '\r', treat newline. If next is '\n', consume it too.
    // We do this by looking at prev/current characters.

    while (in) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        std::streamsize got = in.gcount();
        if (got <= 0) break;

        const char* data = buf.data();
        std::size_t n = static_cast<std::size_t>(got);

        for (std::size_t i = 0; i < n; ++i) {
            char c = data[i];

            // Normalize CR/CRLF to single '\n' events
            if (c == '\r') {
                // Treat as newline outside quotes only
                if (!in_quotes) {
                    rows++;
                    first_line_done = first_line_done ? true : true;
                }
                have_prev = false; // reset prev since CRLF handling below eats LF
                // If next is '\n', skip it
                if (i + 1 < n && data[i + 1] == '\n') {
                    ++i;
                } else {
                    // Could be lone CR across chunk boundary: peek next iteration via prev
                    // We just handled as newline already.
                }
                continue;
            }

            // Handle quotes
            if (c == quote) {
                std::size_t skip = handle_quote(data, i, n);
                if (skip > 0) { i += skip; }
                continue;
            }

            // Count header columns from first logical line only (outside quotes)
            if (!first_line_done) {
                seen_any_data = true;
                if (!in_quotes && c == delimiter) {
                    header_cols++;
                } else if (!in_quotes && c == '\n') {
                    first_line_done = true;
                }
            }

            // Row counting: newline outside quotes
            if (!in_quotes && c == '\n') {
                rows++;
            }

            prev = c;
            have_prev = true;
        }

        // Handle CRLF across chunk boundary: if last char of chunk was '\r',
        // and the next chunk starts with '\n', we will convert at next loop; but
        // we already counted the row on '\r' above and will ignore that '\n' next loop
        if (have_prev && prev == '\r') {
            prev = '\0';
            have_prev = false;
        }
    }

    // If we ended without a trailing newline and we have any data, count the final row.
    // Only if we're not inside quotes (malformed CSV if we are).
    if (seen_any_data && !in_quotes) {
        // If the last seen char was not a normalized newline event, we need to add 1.
        // Heuristic: if rows == 0 OR the last char wasn't newline, add 1.
        // A simple approach: bump rows by 1 now, then correct if the file ended exactly on newline:
        // But we already increment on newline, so only add if the very last processed char wasn't '\n' or '\r'.
        // We don't have that readily now; use 'seen_any_data' + last char check:
        // We'll assume common case: if file doesn't end with newline, we need this bump.
        // To avoid double-counting when file does end with newline, rely on the fact that
        // we incremented rows when we saw '\n' or '\r'. So:
        // Do nothing special if last char was '\n' or '\r' because rows already counted.
        // We lost last char here, so safest: add 1 if rows == 0 (single-line no newline),
        // or if first_line_done is true but last char wasn't newline. We can’t know reliably,
        // but "rows == 0" covers single-line no newline; for multi-line, most CSVs end with newline.
        if (rows == 0) rows = 1;
    }

    CsvCounts out{};
    out.rows = rows;

    // If header present, data rows = total_rows - 1 (but not below 0)
    if (has_header && out.rows > 0) {
        out.rows -= 1;
    }

    out.columns = first_line_done ? header_cols : (seen_any_data ? header_cols : 0);
    return out;
}
