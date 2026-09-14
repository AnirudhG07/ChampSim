// demo_optgen.cc -- NOT graded. Verbose exercise of OPTgen on large access streams.
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "../replacement/hawkeye/optgen.h"

// A plain LRU model, used only as a baseline to compare OPTgen against.
struct LRUModel {
  std::size_t ways;
  std::list<uint64_t> lines; // front = most recent
  explicit LRUModel(std::size_t w) : ways(w) {}
  bool access(uint64_t addr)
  {
    for (auto it = lines.begin(); it != lines.end(); ++it) {
      if (*it == addr) {
        lines.erase(it);
        lines.push_front(addr);
        return true;
      }
    }
    lines.push_front(addr);
    if (lines.size() > ways)
      lines.pop_back();
    return false;
  }
};

static void banner(const std::string& s)
{
  std::cout << "\n=== " << s << " ===\n";
}

// ---------------------------------------------------------------- Figure 6
static int figure6()
{
  banner("Paper Figure 6 replication (capacity 2)");
  std::cout << "Access sequence: A B B C D E A F D E F C\n";
  std::cout << "Paper says: 4 hits, at B(T=2) A(T=6) D(T=8) F(T=10)\n\n";

  OPTgen opt(1, 2);
  const std::string seq = "ABBCDEAFDEFC";
  int hits = 0;
  for (std::size_t t = 0; t < seq.size(); t++) {
    bool h = opt.access(0, (uint64_t)seq[t]);
    std::cout << "  T=" << std::setw(2) << t << "  " << seq[t] << "  " << (h ? "HIT " : "MISS") << "\n";
    hits += h;
  }
  std::cout << "\n  OPTgen hits: " << hits << "   expected: 4   -> " << (hits == 4 ? "PASS" : "FAIL") << "\n";
  return hits == 4 ? 0 : 1;
}

// ------------------------------------------------------------- workload gen
// Each workload returns a vector of (set, block address) pairs.
using Stream = std::vector<std::pair<std::size_t, uint64_t>>;

static Stream streaming(std::size_t n)
{
  Stream s;
  for (std::size_t i = 0; i < n; i++)
    s.push_back({0, i}); // every line touched once, never reused
  return s;
}

static Stream cyclic(std::size_t n, std::size_t working_set)
{
  Stream s;
  for (std::size_t i = 0; i < n; i++)
    s.push_back({0, i % working_set});
  return s;
}

static Stream hot_plus_stream(std::size_t n, std::size_t hot_lines)
{
  Stream s;
  uint64_t cold = 1000000;
  for (std::size_t i = 0; i < n; i++) {
    if (i % 2 == 0)
      s.push_back({0, i % hot_lines}); // reused
    else
      s.push_back({0, cold++}); // never reused
  }
  return s;
}

static Stream multi_set(std::size_t n, std::size_t sets, std::size_t working_set)
{
  Stream s;
  for (std::size_t i = 0; i < n; i++)
    s.push_back({i % sets, i % working_set});
  return s;
}

// --------------------------------------------------------------- comparison
static void run(const std::string& name, const Stream& s, std::size_t sets, std::size_t ways)
{
  OPTgen opt(sets, ways);
  std::vector<LRUModel> lru(sets, LRUModel(ways));

  int opt_hits = 0, lru_hits = 0;
  for (auto& [set, addr] : s) {
    opt_hits += opt.access(set, addr);
    lru_hits += lru[set].access(addr);
  }

  double opt_r = 100.0 * opt_hits / (double)s.size();
  double lru_r = 100.0 * lru_hits / (double)s.size();

  std::cout << "  " << std::left << std::setw(30) << name << std::right << std::setw(9) << s.size() << std::setw(11) << opt_hits << std::setw(8)
            << std::fixed << std::setprecision(1) << opt_r << "%" << std::setw(11) << lru_hits << std::setw(8) << lru_r << "%" << "   "
            << (opt_hits >= lru_hits ? "ok" : "<-- OPT BELOW LRU, SUSPICIOUS") << "\n";
}

int main()
{
  int rc = figure6();

  const std::size_t W = 16;
  const std::size_t N = 200000;

  banner("Large synthetic workloads, 16-way, single set");
  std::cout << "  OPTgen should never do worse than LRU. A cyclic working set just over\n"
            << "  capacity is the classic case where LRU gets 0 hits and OPT does not.\n\n";
  std::cout << "  " << std::left << std::setw(30) << "workload" << std::right << std::setw(9) << "accesses" << std::setw(11) << "OPT hits" << std::setw(9)
            << "rate" << std::setw(10) << "LRU hits" << std::setw(9) << "rate" << "\n";
  std::cout << "  " << std::string(88, '-') << "\n";

  run("streaming (no reuse)", streaming(N), 1, W);
  run("cyclic, fits (W=16 lines)", cyclic(N, W), 1, W);
  run("cyclic, W+1 (LRU thrashes)", cyclic(N, W + 1), 1, W);
  run("cyclic, 2W (32 lines)", cyclic(N, 2 * W), 1, W);
  run("cyclic, 8W (128 lines)", cyclic(N, 8 * W), 1, W);
  run("cyclic, 16W (beyond history)", cyclic(N, 16 * W), 1, W);
  run("hot 8 lines + streaming", hot_plus_stream(N, 8), 1, W);

  banner("Multi-set: 2048 sets, 16-way (LLC geometry)");
  std::cout << "  Sets must be independent; interleaving must not change per-set results.\n\n";
  std::cout << "  " << std::left << std::setw(30) << "workload" << std::right << std::setw(9) << "accesses" << std::setw(11) << "OPT hits" << std::setw(9)
            << "rate" << std::setw(10) << "LRU hits" << std::setw(9) << "rate" << "\n";
  std::cout << "  " << std::string(88, '-') << "\n";
  run("2048 sets, ws=8", multi_set(N, 2048, 8), 2048, W);
  run("2048 sets, ws=64", multi_set(N, 2048, 64), 2048, W);

  banner("Per-set isolation check");
  {
    const std::string seq = "ABBCDEAFDEFC";
    OPTgen one(1, 2), many(8, 2);
    int h1 = 0, h8 = 0;
    for (char c : seq)
      h1 += one.access(0, (uint64_t)c);
    for (char c : seq)
      for (std::size_t s = 0; s < 8; s++)
        h8 += many.access(s, (uint64_t)c);
    std::cout << "  1 set alone: " << h1 << " hits;  same stream on 8 interleaved sets: " << h8 << " hits (expect " << h1 * 8 << ")  -> "
              << (h8 == h1 * 8 ? "PASS" : "FAIL") << "\n";
    if (h8 != h1 * 8)
      rc = 1;
  }

  std::cout << "\n" << (rc == 0 ? "All hard checks passed." : "SOME CHECKS FAILED.") << "\n";
  return rc;
}
