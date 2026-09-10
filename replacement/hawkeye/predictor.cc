#include "predictor.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <algorithm>

using namespace std;

HawkeyePredictor::HawkeyePredictor(size_t num_entries, int counter_bits)
{
  // initialize the predictor constructor
  n_entries = num_entries;
  max_counter = (1 << counter_bits) - 1;
  init_counter = 4;
  predictor_table.assign(n_entries, init_counter);
}

uint64_t HawkeyePredictor::hash_pc(uint64_t pc) const
{
  // hash function
  pc = pc ^ (pc >> 12);
  // get lower 13 bits, so 1<<log2(n_entries) = n_entries, direct usage
  pc = pc & (n_entries - 1);
  return pc;
}

void HawkeyePredictor::train(uint64_t pc, bool opt_hit)
{
  uint64_t hashed_pc = hash_pc(pc);
  // if opt_hit is true, it is trained positively
  if (opt_hit) {
    predictor_table[hashed_pc] = std::min<int>(predictor_table[hashed_pc] + 1, max_counter);
  } else {
    predictor_table[hashed_pc] = std::max<int>(predictor_table[hashed_pc] - 1, 0);
  }
}

bool HawkeyePredictor::predict(uint64_t pc) const
{
  // first bit is 1 (like 100), then it is predicted as a hit, else miss
  if (predictor_table[hash_pc(pc)] >= init_counter) {
    return true;
  }
  return false;
}

int HawkeyePredictor::get_counter(uint64_t pc) const { return predictor_table[hash_pc(pc)]; }