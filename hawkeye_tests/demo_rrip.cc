// demo_rrip.cc -- NOT graded. Verbose exercise of the RRIP insertion/aging logic.
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "../replacement/hawkeye/rrip.h"

static int failures = 0;
static void check(const std::string& what, bool ok)
{
  std::cout << "  " << std::left << std::setw(60) << what << (ok ? "PASS" : "FAIL") << "\n";
  if (!ok)
    failures++;
}
static void banner(const std::string& s) { std::cout << "\n=== " << s << " ===\n"; }
static void show(const std::string& label, const std::vector<int>& v)
{
  std::cout << "  " << std::left << std::setw(34) << label << "[ ";
  for (int x : v)
    std::cout << x << " ";
  std::cout << "]\n";
}

int main()
{
  banner("Table 1 update rule, 8-way set");
  {
    std::vector<int> r(8, 3);
    show("start", r);

    update_rrpv(r, 0, Classification::CACHE_AVERSE, false);
    show("averse insert at way 0 -> 7", r);
    check("cache-averse insertion sets 7", r[0] == 7);

    update_rrpv(r, 1, Classification::CACHE_AVERSE, true);
    show("averse HIT at way 1 -> 7", r);
    check("cache-averse on a HIT also sets 7 (Table 1)", r[1] == 7);

    std::vector<int> r2(8, 3);
    update_rrpv(r2, 4, Classification::CACHE_FRIENDLY, false);
    show("friendly insert at way 4", r2);
    check("friendly insertion sets the new line to 0", r2[4] == 0);
    check("friendly insertion ages the others 3 -> 4", r2[0] == 4 && r2[7] == 4);

    std::vector<int> r3 = {5, 6, 7, 3};
    update_rrpv(r3, 3, Classification::CACHE_FRIENDLY, false);
    show("aging cap: {5,6,7,3} friendly@3", r3);
    check("6 stays 6 (cap: only RRPV < 6 ages)", r3[1] == 6);
    check("7 stays 7 (cache-averse line untouched)", r3[2] == 7);

    std::vector<int> r4 = {4, 4, 4, 4};
    update_rrpv(r4, 2, Classification::CACHE_FRIENDLY, true);
    show("friendly HIT at way 2 (no aging)", r4);
    check("friendly hit sets 0 and does NOT age others", r4[2] == 0 && r4[0] == 4);
  }

  banner("find_victim: search, then age if nothing is at 7");
  {
    struct C {
      std::vector<int> in;
      const char* note;
    };
    std::vector<C> cases = {{{7, 0, 3, 1}, "a 7 exists -> return it, vector unchanged"},
                            {{0, 5}, "no 7 -> age until one reaches 7"},
                            {{0, 0, 0, 0}, "all zero -> ages 7 times"},
                            {{6, 6, 6}, "all at the cap -> one aging pass"},
                            {{3, 1, 6, 2}, "highest (6) should win"}};
    for (auto& c : cases) {
      std::vector<int> v = c.in;
      std::size_t victim = find_victim(v);
      std::cout << "  ";
      std::cout << std::left << std::setw(44) << c.note;
      std::cout << "in [ ";
      for (int x : c.in)
        std::cout << x << " ";
      std::cout << "] -> victim " << victim << ", after [ ";
      for (int x : v)
        std::cout << x << " ";
      std::cout << "]\n";
    }
    std::vector<int> unchanged = {7, 0, 3, 1};
    std::vector<int> copy = unchanged;
    find_victim(copy);
    check("vector is NOT aged when a 7 already exists", copy == unchanged);

    // the victim must match "highest RRPV", which is what the paper's rule picks
    std::mt19937 rng(7);
    bool agrees = true;
    for (int t = 0; t < 200000; t++) {
      std::size_t n = 4 + (rng() % 13);
      std::vector<int> v(n);
      for (auto& x : v)
        x = (int)(rng() % 8);
      std::vector<int> copy2 = v;
      std::size_t got = find_victim(copy2);
      std::size_t want = 0;
      for (std::size_t i = 1; i < v.size(); i++)
        if (v[i] > v[want])
          want = i;
      if (got != want)
        agrees = false;
    }
    check("200000 random vectors: victim == highest-RRPV way (paper 3.4)", agrees);
  }

  banner("Cache-averse lines are evicted before cache-friendly ones");
  {
    // Fill an 8-way set: ways 0-5 friendly, ways 6-7 averse.
    std::vector<int> r(8, 7);
    for (std::size_t w = 0; w < 6; w++)
      update_rrpv(r, w, Classification::CACHE_FRIENDLY, false);
    for (std::size_t w = 6; w < 8; w++)
      update_rrpv(r, w, Classification::CACHE_AVERSE, false);
    show("6 friendly + 2 averse", r);
    std::size_t v = find_victim(r);
    std::cout << "  first victim: way " << v << " (want 6 or 7, an averse line)\n";
    check("an averse line is chosen first", v >= 6);
  }

  banner("Stress: 200000 random operations, 16-way");
  {
    std::mt19937 rng(99);
    std::vector<int> r(16, 7);
    long evictions = 0;
    bool in_range = true, valid_way = true;
    for (int i = 0; i < 200000; i++) {
      if (rng() % 3 == 0) {
        std::size_t v = find_victim(r);
        if (v >= r.size())
          valid_way = false;
        evictions++;
        update_rrpv(r, v, (rng() % 2) ? Classification::CACHE_FRIENDLY : Classification::CACHE_AVERSE, false);
      } else {
        std::size_t w = rng() % r.size();
        update_rrpv(r, w, (rng() % 2) ? Classification::CACHE_FRIENDLY : Classification::CACHE_AVERSE, true);
      }
      for (int x : r)
        if (x < 0 || x > 7)
          in_range = false;
    }
    std::cout << "  completed " << evictions << " evictions without hanging\n";
    show("final RRPV state", r);
    check("all RRPV values stayed within [0,7]", in_range);
    check("every victim index was a valid way", valid_way);
  }

  std::cout << "\n" << (failures == 0 ? "All checks passed." : std::to_string(failures) + " CHECK(S) FAILED.") << "\n";
  return failures == 0 ? 0 : 1;
}
