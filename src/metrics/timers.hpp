#pragma once
#include <chrono>

struct WallTimer {
    using clock = std::chrono::steady_clock;
    clock::time_point t0, t1;
    void start() { t0 = clock::now(); }
    void stop()  { t1 = clock::now(); }
    double ms() const { return std::chrono::duration<double,std::milli>(t1 - t0).count(); }
};
