#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <random>
#include <fstream>
#include <iostream>

int main(int argc, char** argv){
  if (argc < 4){
    std::cerr << "usage: gen_synth_csv <out.csv> <rows> <quoted:0|1>\n";
    return 2;
  }
  const std::string out = argv[1];
  const std::uint64_t rows = std::strtoull(argv[2], nullptr, 10);
  const bool quoted = std::string(argv[3]) == "1";

  std::ofstream f(out, std::ios::binary);
  if (!f){ std::cerr << "open failed: " << out << "\n"; return 2; }

  // header
  f << "id,int_col,float_col,bool_col,date_col,str_col\n";

  std::mt19937_64 rng(42);
  std::uniform_int_distribution<long long> di(-100000, 100000);
  std::uniform_real_distribution<double> df(-1e4, 1e4);
  const char* words[] = {"alpha","bravo","charlie","delta","echo","foxtrot"};
  for (std::uint64_t i=1;i<=rows;++i){
    long long iv = di(rng);
    double fv = df(rng);
    bool bv = (i % 3) == 0;
    // yyyy-mm-dd — keep it simple
    int y = 2023 + int(i % 3), m = 1 + int(i % 12), d = 1 + int(i % 28);
    char datebuf[32];
    std::snprintf(datebuf, sizeof(datebuf), "%04d-%02d-%02d", y, m, d);
    std::string s = words[i % 6];

    auto emit = [&](const std::string& x){
      if (!quoted) { f << x; return; }
      // quote & escape inner quotes
      f << '"';
      for (char c: x){ if (c=='"') f << "\"\""; else f << c; }
      f << '"';
    };

    emit(std::to_string(i)); f << ",";
    emit(std::to_string(iv)); f << ",";
    emit(std::to_string(fv)); f << ",";
    emit(bv ? "true" : "false"); f << ",";
    emit(datebuf); f << ",";
    // Add some commas/quotes/newlines occasionally when quoted mode is on
    if (quoted && (i % 17 == 0)) s += ", said \"hi\"";
    emit(s);
    f << "\n";
  }
  std::cerr << "wrote " << rows << " rows to " << out << "\n";
  return 0;
}
