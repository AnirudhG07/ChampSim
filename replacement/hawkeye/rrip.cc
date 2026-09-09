#include "rrip.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

using namespace std;

void update_rrpv(std::vector<int>& rrpv, std::size_t way, Classification cls, bool is_hit)
{
    if (cls == Classification::CACHE_AVERSE) {
        rrpv[way] = 7; // set 7 for hit/mess
        return;
    }

    if (!is_hit) { 
        // insertion: age every line
        for (size_t i = 0; i < rrpv.size(); i++)
            if (rrpv[i] < 6)
                rrpv[i]++;
    }

    rrpv[way] = 0; // applied last, so it stays 0
}

size_t find_victim(std::vector<int>& rrpv)
{
  if (rrpv.empty()) {
    return 0; // No ways to choose from
  }

  while (true) {
    for (size_t i = 0; i < rrpv.size(); i++) {
      if (rrpv[i] == 7) {
        return i; // Any way with rrpv = 7 will work
      }
      // age all lines, mentioned in assignment
      for (size_t i = 0; i < rrpv.size(); i++) {
        rrpv[i]++;
      }
    }
  }
}
