#ifndef REPLACEMENT_HAWKEYE_OPTGEN_H
#define REPLACEMENT_HAWKEYE_OPTGEN_H

#include <cstddef>
#include <cstdint>
#include <vector>

using namespace std;

class OPTgen
{
public:
  // num_sets: number of cache sets tracked independently
  // associativity: W, the cache associativity (occupancy vector cap)
  // history_multiplier: length of tracked history, in units of the set's capacity (paper uses 8x; see Figure 2). Default 8.
  OPTgen(std::size_t num_sets, std::size_t associativity, std::size_t history_multiplier = 8);
  // Processes one access to `address`, mapped to set `set_idx`, per Section 3.1.
  bool access(std::size_t set_idx, uint64_t address);

private:
  size_t capacity;       // W, the cache associativity
  size_t history_length; // length of history tracked per set = 8W

  vector<vector<size_t>> occupancy_vecs; // occupancy vectors, one per set
  vector<vector<uint64_t>> access_seqs;  // address accessed at each step, one ring per set
  vector<size_t> current_times;          // accesses seen so far, one per set
};

#endif