#pragma once
#include <vector>
#include <limits>
#include <cstddef>
#include <algorithm>
#include <stdexcept>

namespace csvqr {

struct histogram {
    int bins{0};
    std::vector<double> edges;   // size = bins+1
    std::vector<std::size_t> counts; // size = bins
};

inline histogram make_histogram(const std::vector<double>& values, int bins) {
    if (bins <= 0) throw std::invalid_argument("bins must be > 0");
    histogram h;
    h.bins = bins;
    if (values.empty()) {
        h.edges.assign(bins + 1, 0.0);
        h.counts.assign(bins, 0);
        return h;
    }
    auto [mn_it, mx_it] = std::minmax_element(values.begin(), values.end());
    double mn = *mn_it, mx = *mx_it;
    if (!(mx > mn)) {
        h.edges.assign(bins + 1, mn);
        h.counts.assign(bins, 0);
        h.counts[0] = values.size();
        return h;
    }
    h.edges.resize(bins + 1);
    double step = (mx - mn) / static_cast<double>(bins);
    for (int i = 0; i <= bins; ++i) h.edges[i] = mn + step * i;
    h.counts.assign(bins, 0);
    for (double v : values) {
        int idx = static_cast<int>((v - mn) / step);
        if (idx == bins) idx = bins - 1; // right-closed
        if (idx >= 0 && idx < bins) ++h.counts[static_cast<std::size_t>(idx)];
    }
    return h;
}

} 
