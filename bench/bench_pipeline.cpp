#include "metrics/timers.hpp"
#include "io/file_stats.hpp"
#include "csv/csv_count.hpp"
#include <fmt/format.h>
#include <string>
#include <string_view>
#include <filesystem>
#include <cctype>

using std::string;
namespace fs = std::filesystem;

static bool ieq(std::string_view a, std::string_view b){
  if (a.size()!=b.size()) return false;
  for (size_t i=0;i<a.size();++i){
    if (std::tolower((unsigned char)a[i])!=std::tolower((unsigned char)b[i])) return false;
  }
  return true;
}

int main(int argc, char** argv){
  // Defaults
  string dataPath;
  size_t chunkBytes = (1u<<20);  // 1 MiB
  bool   hasHeader  = true;

  // Parse flags (supports both flag and positional styles)
  // Supported:
  //   --data <file>         | --data=<file>
  //   --chunk-bytes <N>     | --chunk-bytes=<N>
  //   --has-header true|false|1|0 | --has-header=<...>
  // Fallback positional: <input.csv> [chunk_bytes]
  for (int i=1;i<argc;++i){
    std::string_view a(argv[i]);

    auto eat_next_str = [&](std::string_view name, string* out)->bool{
      if (i+1<argc){ *out = argv[++i]; return true; }
      fmt::print(stderr, "missing value for {}\n", name);
      return false;
    };
    auto eat_next_size = [&](std::string_view name, size_t* out)->bool{
      if (i+1<argc){ *out = static_cast<size_t>(std::stoull(argv[++i])); return true; }
      fmt::print(stderr, "missing value for {}\n", name);
      return false;
    };

    if (a.rfind("--data=",0)==0) {
      dataPath = string(a.substr(7));
    } else if (a == "--data") {
      if (!eat_next_str("--data", &dataPath)) return 2;
    } else if (a.rfind("--chunk-bytes=",0)==0) {
      chunkBytes = static_cast<size_t>(std::stoull(string(a.substr(14))));
    } else if (a == "--chunk-bytes") {
      if (!eat_next_size("--chunk-bytes", &chunkBytes)) return 2;
    } else if (a.rfind("--has-header=",0)==0) {
      string v = string(a.substr(13));
      hasHeader = !(ieq(v,"false") || v=="0" || ieq(v,"no"));
    } else if (a == "--has-header") {
      if (i+1<argc){
        string v = argv[++i];
        hasHeader = !(ieq(v,"false") || v=="0" || ieq(v,"no"));
      } else {
        fmt::print(stderr, "missing value for --has-header\n");
        return 2;
      }
    } else if (!dataPath.size() && a.size() && a[0] != '-') {
      // positional file
      dataPath = string(a);
      // optional positional chunk bytes
      if (i+1<argc && argv[i+1][0] != '-') {
        ++i;
        chunkBytes = static_cast<size_t>(std::stoull(argv[i]));
      }
    } else {
      // ignore unknown flags so you can pass extra args without breaking
    }
  }

  if (dataPath.empty()){
    fmt::print(stderr,
      "usage:\n"
      "  csvqr_bench_pipeline <input.csv> [chunk_bytes]\n"
      "  csvqr_bench_pipeline --data <input.csv> [--chunk-bytes N] [--has-header true|false]\n");
    return 2;
  }

  const auto bytes = file_size_bytes(dataPath);
  if (bytes == 0){
    fmt::print(stderr, "file empty or missing: {}\n", dataPath);
    return 2;
  }

  WallTimer wt; wt.start();
  auto res = csv_count_rows_cols(dataPath, ',', '"', chunkBytes, /*header*/ hasHeader);
  wt.stop();

  const double secs = wt.ms()/1000.0;
  const double mb   = double(bytes)/(1024.0*1024.0);
  const double mbps = secs>0? (mb/secs) : 0.0;
  const double rps  = secs>0? (double(res.rows)/secs) : 0.0;

  fmt::print("bench_pipeline,file={},rows={},bytes={},sec={:.3f},MB/s={:.2f},rows/s={:.0f}\n",
             dataPath, res.rows, bytes, secs, mbps, rps);
  return 0;
}
