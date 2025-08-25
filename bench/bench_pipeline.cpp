#include <fmt/format.h>
#include <string>
#include <filesystem>
#include "../metrics/timers.hpp"
#include "../io/file_stats.hpp"
#include "../csv/csv_count.hpp"

int main(int argc, char** argv){
  if (argc < 2){
    fmt::print(stderr, "usage: bench_pipeline <input.csv> [chunk_bytes=1048576]\n");
    return 2;
  }
  const std::string path = argv[1];
  const size_t chunk = (argc >= 3) ? static_cast<size_t>(std::stoull(argv[2])) : (1<<20);

  const auto bytes = file_size_bytes(path);
  if (bytes == 0){ fmt::print(stderr, "file empty or missing: {}\n", path); return 2; }

  WallTimer wt; wt.start();
  auto res = csv_count_rows_cols(path, ',', '"', chunk, /*header*/ true);
  wt.stop();

  const double secs = wt.ms()/1000.0;
  const double mb   = double(bytes)/(1024.0*1024.0);
  const double mbps = secs>0? (mb/secs) : 0.0;
  const double rps  = secs>0? (double(res.rows)/secs) : 0.0;

  fmt::print("bench_pipeline,file={},rows={},bytes={},sec={:.3f},MB/s={:.2f},rows/s={:.0f}\n",
             path, res.rows, bytes, secs, mbps, rps);
  return 0;
}
