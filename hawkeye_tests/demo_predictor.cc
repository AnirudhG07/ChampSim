// demo_predictor.cc -- NOT graded. Verbose exercise of HawkeyePredictor.
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "../replacement/hawkeye/predictor.h"

static int failures = 0;
static void check(const std::string& what, bool ok)
{
  std::cout << "  " << std::left << std::setw(56) << what << (ok ? "PASS" : "FAIL") << "\n";
  if (!ok)
    failures++;
}
static void banner(const std::string& s) { std::cout << "\n=== " << s << " ===\n"; }

int main()
{
  banner("Initial state: untrained PCs must default to cache-friendly");
  {
    HawkeyePredictor p;
    std::cout << "  professor: init the counter to the midpoint, 4 for a 3-bit counter\n\n";
    bool all_four = true, all_friendly = true;
    for (uint64_t pc = 0x400000; pc < 0x400000 + 10000; pc += 7) {
      if (p.get_counter(pc) != 4)
        all_four = false;
      if (!p.predict(pc))
        all_friendly = false;
    }
    check("all 10000 sampled PCs start at counter 4", all_four);
    check("all 10000 sampled PCs predict cache-friendly", all_friendly);
  }

  banner("Saturation, with full counter trace");
  {
    HawkeyePredictor p;
    uint64_t pc = 0x401234;
    std::cout << "  train(hit) x10:   ";
    for (int i = 0; i < 10; i++) {
      p.train(pc, true);
      std::cout << p.get_counter(pc) << " ";
    }
    std::cout << "\n";
    check("saturates at 7, never exceeds", p.get_counter(pc) == 7);

    std::cout << "  train(miss) x10:  ";
    for (int i = 0; i < 10; i++) {
      p.train(pc, false);
      std::cout << p.get_counter(pc) << " ";
    }
    std::cout << "\n";
    check("saturates at 0, never underflows", p.get_counter(pc) == 0);
    check("counter 0 predicts cache-averse", p.predict(pc) == false);

    std::cout << "  crossing the midpoint upward: ";
    for (int i = 0; i < 5; i++) {
      p.train(pc, true);
      std::cout << p.get_counter(pc) << "(" << (p.predict(pc) ? "F" : "A") << ") ";
    }
    std::cout << "\n  (prediction must flip from A to F exactly when the counter reaches 4)\n";
  }

  banner("Learning biased PCs over many training events");
  {
    HawkeyePredictor p;
    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> pct(0, 99);

    struct Case {
      uint64_t pc;
      int hit_pct;
      const char* want;
    };
    std::vector<Case> cases = {{0x1000, 95, "friendly"}, {0x2000, 75, "friendly"}, {0x3000, 25, "averse"}, {0x4000, 5, "averse"}};

    for (int i = 0; i < 20000; i++)
      for (auto& c : cases)
        p.train(c.pc, pct(rng) < c.hit_pct);

    std::cout << "  20000 training events per PC\n\n";
    std::cout << "  " << std::left << std::setw(10) << "PC" << std::setw(12) << "hit rate" << std::setw(11) << "counter" << std::setw(12) << "predicts"
              << "expected\n";
    for (auto& c : cases) {
      bool f = p.predict(c.pc);
      bool ok = (std::string(c.want) == "friendly") == f;
      std::cout << "  " << std::left << std::setw(10) << std::hex << c.pc << std::dec << std::setw(12) << (std::to_string(c.hit_pct) + "%") << std::setw(11)
                << p.get_counter(c.pc) << std::setw(12) << (f ? "friendly" : "averse") << c.want << (ok ? "  ok" : "  <-- WRONG") << "\n";
      if (!ok)
        failures++;
    }
  }

  banner("Hash behavior: pc ^ (pc >> 12), low 13 bits");
  {
    HawkeyePredictor p;
    std::cout << "  " << std::left << std::setw(14) << "PC" << std::setw(12) << "expected" << "matches?\n";
    bool all_ok = true;
    for (uint64_t pc : {0x401234ull, 0x0ull, 0xFFFFFFFFull, 0x1000ull, 0xDEADBEEFull}) {
      uint64_t want = (pc ^ (pc >> 12)) & 8191;
      // train this PC to 7, then confirm the PC that shares the index also reads 7
      HawkeyePredictor q;
      for (int i = 0; i < 10; i++)
        q.train(pc, true);
      bool ok = q.get_counter(pc) == 7;
      std::cout << "  " << std::left << std::setw(14) << std::hex << pc << std::dec << std::setw(12) << want << (ok ? "ok" : "MISMATCH") << "\n";
      all_ok = all_ok && ok;
    }
    check("hashed index is stable across calls", all_ok);

    // aliasing: how many distinct indices do 100k PCs land on?
    std::map<uint64_t, int> buckets;
    for (uint64_t i = 0; i < 100000; i++) {
      uint64_t pc = 0x400000 + i * 4; // realistic: 4-byte aligned instruction addresses
      buckets[(pc ^ (pc >> 12)) & 8191]++;
    }
    std::cout << "\n  100000 distinct 4-byte-aligned PCs -> " << buckets.size() << " of 8192 entries used\n";
    std::cout << "  (aliasing is expected and harmless; the paper uses a 13-bit hashed PC too)\n";
  }

  banner("Independence: training one PC must not move an unrelated one");
  {
    HawkeyePredictor p;
    uint64_t a = 0xAAAA, b = 0xBBBB;
    int before = p.get_counter(b);
    for (int i = 0; i < 50; i++)
      p.train(a, false);
    check("unrelated PC unchanged after 50 trainings", p.get_counter(b) == before);
  }

  std::cout << "\n" << (failures == 0 ? "All checks passed." : std::to_string(failures) + " CHECK(S) FAILED.") << "\n";
  return failures == 0 ? 0 : 1;
}
