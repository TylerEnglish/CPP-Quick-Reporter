#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>

namespace csvqr {

struct numeric_stats {
    std::size_t null_count{0};
    std::size_t non_null_count{0};
    double min{0.0}, max{0.0};
    double mean{0.0}, m2{0.0}; // Welford
    std::vector<double> sample; // for median/quantiles
    void add_null() { ++null_count; }
    void add(double x) {
        ++non_null_count;
        if (non_null_count == 1) { min = max = x; mean = x; m2 = 0.0; }
        else {
            if (x < min) min = x;
            if (x > max) max = x;
            double delta = x - mean;
            mean += delta / static_cast<double>(non_null_count);
            m2 += delta * (x - mean);
        }
        sample.push_back(x);
    }
    double variance() const { return non_null_count > 1 ? m2 / (non_null_count - 1) : 0.0; }
    double stddev() const { return std::sqrt(variance()); }
    double quantile(double q) {
        if (sample.empty()) return 0.0;
        auto v = sample;
        std::sort(v.begin(), v.end());
        double pos = q * (v.size() - 1);
        std::size_t i = static_cast<std::size_t>(pos);
        double frac = pos - static_cast<double>(i);
        if (i + 1 < v.size()) return v[i] * (1.0 - frac) + v[i + 1] * frac;
        return v[i];
    }
};

struct categorical_stats {
    std::size_t null_count{0};
    std::size_t non_null_count{0};
    std::unordered_map<std::string, std::size_t> freq;
    void add_null() { ++null_count; }
    void add(const std::string& s) { ++non_null_count; ++freq[s]; }
};

}
