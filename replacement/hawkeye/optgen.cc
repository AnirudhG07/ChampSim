#include "optgen.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

OPTgen::OPTgen(std::size_t num_sets, std::size_t associativity, std::size_t history_multiplier)
{
  capacity = associativity;
  history_length = associativity * history_multiplier;

  occupancy_vecs.assign(num_sets, vector<size_t>(history_length, 0));
  access_seqs.assign(num_sets, vector<uint64_t>(history_length, 0));
  current_times.assign(num_sets, 0);
}

bool OPTgen::access(std::size_t set_idx, uint64_t address)
{
  vector<size_t>& occupancy_vec = occupancy_vecs[set_idx];
  vector<uint64_t>& access_seq = access_seqs[set_idx];
  size_t current_time = current_times[set_idx];

  bool hit = false;
  size_t usage_interval = 0;
  // time can be >= 8W, but steps should be capped at 8W
  size_t steps = std::min(current_time, history_length);

  // find the previous access
  for (size_t s = 1; s <= steps; ++s) {
    size_t pos = (current_time - s) % history_length;
    // If any entry >= 8W, miss, else hit
    if (occupancy_vec[pos] >= capacity)
      break;
    if (access_seq[pos] == address) {
      hit = true;
      usage_interval = s;
      break;
    }
  }

  // If hit, increment all entries in the occ vector within the usage interval.
  if (hit) {
    for (size_t s = 1; s <= usage_interval; ++s)
      occupancy_vec[(current_time - s) % history_length]++;
  }

  // The new address that came must be added to the access sequence
  access_seq[current_time % history_length] = address;
  // newest address is always 0 for the occ vector
  occupancy_vec[current_time % history_length] = 0;
  current_times[set_idx]++;

  return hit;
}
