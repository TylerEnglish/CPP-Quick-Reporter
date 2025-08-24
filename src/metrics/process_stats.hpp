#pragma once
#include <cstdint>

#if defined(_WIN32)
  #include <windows.h>
  #include <psapi.h>
  inline double process_rss_mb() {
      PROCESS_MEMORY_COUNTERS_EX pmc{};
      if (GetProcessMemoryInfo(GetCurrentProcess(),
                               reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                               sizeof(pmc))) {
          return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
      }
      return 0.0;
  }
#else
  #include <unistd.h>
  #include <fstream>
  inline double process_rss_mb() {
      long rss_pages = 0L;
      std::ifstream f("/proc/self/statm");
      long ignore = 0L;
      if (f) {
          f >> ignore >> rss_pages;
      }
      const long page = sysconf(_SC_PAGESIZE);
      return (rss_pages > 0 && page > 0)
          ? (static_cast<double>(rss_pages) * static_cast<double>(page)) / (1024.0 * 1024.0)
          : 0.0;
  }
#endif

// Placeholder for future CPU utilization if you want it:
inline double process_cpu_pct() { return 0.0; }
