// src/profile/profile.hpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace csvqr {

// ---------- data model ----------
struct ColumnSummary {
    std::string  name;
    std::string  logical_type;      // "bool" | "int" | "float" | "date" | "string"
    std::uint64_t null_count      = 0;
    std::uint64_t non_null_count  = 0;
};

struct ProfileResult {
    std::vector<ColumnSummary> columns;
    std::uint64_t rows = 0;
};

// ---------- small helpers ----------
inline std::string ltrim(std::string s){
    auto it = std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); });
    s.erase(s.begin(), it); return s;
}
inline std::string rtrim(std::string s){
    auto it = std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); }).base();
    s.erase(it, s.end()); return s;
}
inline std::string trim(std::string s){ return rtrim(ltrim(std::move(s))); }

inline bool ieq(const std::string& a, const std::string& b){
    if (a.size() != b.size()) return false;
    for (size_t i=0;i<a.size();++i){
        unsigned char ca = static_cast<unsigned char>(a[i]);
        unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

// very small date detector: YYYY-MM-DD [T| ]HH:MM:SS(Z)? | YYYY-MM-DD | MM/DD/YYYY
inline bool is_date_like(const std::string& s){
    const std::string t = trim(s);
    if (t.size() >= 10) {
        // YYYY-MM-DD
        if (std::isdigit(t[0])&&std::isdigit(t[1])&&std::isdigit(t[2])&&std::isdigit(t[3]) &&
            t[4]=='-' &&
            std::isdigit(t[5])&&std::isdigit(t[6]) &&
            t[7]=='-' &&
            std::isdigit(t[8])&&std::isdigit(t[9])) {
            return true; // good enough for MVP (accept optional time suffix)
        }
    }
    if (t.size() >= 8) {
        // MM/DD/YYYY
        if (std::isdigit(t[0]) && (std::isdigit(t[1]) || t[1]=='/') ) {
            size_t p1 = t.find('/'); if (p1!=std::string::npos){
                size_t p2 = t.find('/', p1+1);
                if (p2!=std::string::npos && p2+5<=t.size() &&
                    std::isdigit(t[p2+1])&&std::isdigit(t[p2+2])&&
                    std::isdigit(t[p2+3])&&std::isdigit(t[p2+4])) {
                    return true;
                }
            }
        }
    }
    return false;
}

inline bool is_bool_like(const std::string& s){
    const std::string t = trim(s);
    static const char* k[] = {"true","false","1","0","yes","no"};
    for (auto* w: k) if (ieq(t, w)) return true;
    return false;
}
inline bool is_int64_like(const std::string& s){
    const std::string t = trim(s);
    if (t.empty()) return false;
    size_t i = (t[0]=='+'||t[0]=='-') ? 1 : 0;
    if (i>=t.size()) return false;
    for (; i<t.size(); ++i) if (!std::isdigit(static_cast<unsigned char>(t[i]))) return false;
    return true; // (range check omitted for MVP)
}
inline bool is_float_like(const std::string& s){
    // allow decimals and scientific notation; rely on strtod acceptance
    const std::string t = trim(s);
    if (t.empty()) return false;
    char* end = nullptr;
#if defined(_WIN32)
    double v = _strtod_l(t.c_str(), &end, nullptr);
#else
    double v = std::strtod(t.c_str(), &end);
#endif
    (void)v;
    return end && end != t.c_str() && *end == '\0';
}

// ---------- tiny CSV line parser (RFC4180-ish, covers quotes) ----------
inline std::vector<std::string> parse_csv_line(const std::string& line, char delim, char quote){
    std::vector<std::string> out;
    std::string cur;
    bool inq = false;

    for (size_t i=0;i<line.size();++i){
        char c = line[i];
        if (inq){
            if (c == quote){
                // double-quote escape -> append one quote, stay in quoted field
                if (i+1<line.size() && line[i+1]==quote){ cur.push_back(quote); ++i; }
                else { inq = false; }
            } else {
                cur.push_back(c);
            }
        } else {
            if (c == quote){ inq = true; }
            else if (c == delim){ out.push_back(cur); cur.clear(); }
            else { cur.push_back(c); }
        }
    }
    out.push_back(cur);
    return out;
}

// ---------- profile core ----------
inline ProfileResult profile_csv_file(const std::string& path,
                                      char delim,
                                      char quote,
                                      bool header_present,
                                      const std::vector<std::string>& null_tokens = {"", "NA", "N/A", "null", "NULL", "NaN"})
{
    std::ifstream is(path, std::ios::binary);
    ProfileResult pr{};
    if (!is) return pr;

    std::string line;
    std::vector<std::string> header;
    bool header_read = false;
    std::vector<std::string> names;

    // type trackers per column
    struct TState {
        bool all_bool  = true;
        bool all_int   = true;
        bool all_float = true;
        bool all_date  = true;
        std::uint64_t nulls = 0, non_nulls = 0;
    };
    std::vector<TState> states;

    auto is_null_like = [&](const std::string& v)->bool{
        const std::string t = trim(v);
        for (auto& tok : null_tokens){
            if (ieq(t, tok)) return true;
        }
        return false;
    };

    while (std::getline(is, line)){
        // handle CRLF
        if (!line.empty() && line.back()=='\r') line.pop_back();

        auto fields = parse_csv_line(line, delim, quote);
        if (!header_read){
            header_read = true;
            if (header_present){
                header = fields;
                if (header.empty()) continue;
                names = header;
            } else {
                // synthesize names from first row's width
                names.resize(fields.size());
                for (size_t i=0;i<fields.size();++i){
                    names[i] = "col" + std::to_string(i+1);
                }
                // and process this first line as data
                header = names;
                header_read = true;
            }
            states.resize(header.size());
            if (!header_present){
                // process data row (fields already parsed)
            } else {
                // continue to next line for data
                ++pr.rows; // count header row? Usually not; we won't. So revert:
                --pr.rows;
                continue;
            }
        }

        // data row
        ++pr.rows;
        if (states.empty()){
            names.resize(fields.size());
            for (size_t i=0;i<fields.size();++i) names[i] = "col" + std::to_string(i+1);
            states.resize(fields.size());
        }

        // normalize width (short/long rows)
        if (fields.size() < states.size()) fields.resize(states.size());
        if (fields.size() > states.size()){
            // expand states/names to fit widest row (rare but possible)
            size_t old = states.size();
            states.resize(fields.size());
            for (size_t i=old;i<fields.size();++i) names.push_back("col" + std::to_string(i+1));
        }

        for (size_t c=0;c<fields.size();++c){
            const std::string& raw = fields[c];
            if (is_null_like(raw)){ states[c].nulls++; continue; }
            states[c].non_nulls++;

            const std::string t = trim(raw);
            if (!is_bool_like(t))  states[c].all_bool  = false;
            if (!is_int64_like(t)) states[c].all_int   = false;
            if (!is_float_like(t)) states[c].all_float = false;
            if (!is_date_like(t))  states[c].all_date  = false;
        }
    }

    // finalize
    pr.columns.resize(states.size());
    for (size_t i=0;i<states.size(); ++i){
        auto& cs = pr.columns[i];
        cs.name = (i < names.size() && !names[i].empty()) ? names[i] : ("col"+std::to_string(i+1));
        cs.null_count = states[i].nulls;
        cs.non_null_count = states[i].non_nulls;

        // choose type by “all values are X” priority
        if (cs.non_null_count == 0)        cs.logical_type = "string";
        else if (states[i].all_bool)       cs.logical_type = "bool";
        else if (states[i].all_int)        cs.logical_type = "int";
        else if (states[i].all_float)      cs.logical_type = "float";
        else if (states[i].all_date)       cs.logical_type = "date";
        else                               cs.logical_type = "string";
    }
    return pr;
}

}
