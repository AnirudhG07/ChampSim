#include "rrip.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

using namespace std;

void update_rrpv(std::vector<int>& rrpv, std::size_t way, Classification cls, bool is_hit)
{
  // `way` is cache associative index
  // Value inside rrpv vector is the RRPV value for each way
  if (cls == Classification::CACHE_AVERSE) {
    rrpv[way] = 7; // set 7 for hit/mess
    return;
  }
  // CACHE_FRIENDLY
  if (!is_hit) {
    // age every line
    for (size_t i = 0; i < rrpv.size(); i++)
      if (rrpv[i] < 6)
        rrpv[i]++;
    rrpv[way] = 0; // set 0 for that way
  } else {
    rrpv[way] = 0; // set 0 for hit
  }
  return;
}

size_t find_victim(std::vector<int>& rrpv)
{
  if (rrpv.empty()) {
    return 0; // No ways to choose from
  }

  // loop till you find a victim
  //
  // If we want to find the maximum RRIP if no 7 is found(mentioned in paper)
  // int max_rrpv = rrpv[0];
  // int idx = 0;
  // for (size_t i = 0; i < rrpv.size(); i++) {
  //   if (rrpv[i] > max_rrpv) {
  //     max_rrpv = rrpv[i];
  //     idx = i;
  //   }
  //   if (rrpv[i] == 7) {
  //     return i; // Any way with rrpv = 7 will work
  //   }
  // }
  // return idx;

  // Age all lines directly by the distance between the maximum and 7.
  // find maximum RRPV value and its index
  int max_rrpv = rrpv[0];
  size_t victim = 0;
  for (size_t i = 1; i < rrpv.size(); i++) {
    if (rrpv[i] > max_rrpv) {
      max_rrpv = rrpv[i];
      victim = i;
    }
  }

  const int distance = 7 - max_rrpv;
  if (distance > 0) {
    for (size_t i = 0; i < rrpv.size(); i++) {
      rrpv[i] += distance;
    }
  }

  return victim;
}
