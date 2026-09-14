// hawkeye.h
#ifndef HAWKEYE_H
#define HAWKEYE_H
#include <vector>

#include "modules.h"
#include "optgen.h"
#include "predictor.h"
#include "rrip.h"

struct hawkeye : public champsim::modules::replacement {
  // TODO: instantiate different modules
  // TODO: Add any new data structures or functions to connect each of the modules
  // TODO: Complete the definitions for the following functions that are
  // required across all replacement policies. You can use the other replacement policies as a reference. Each should be implemented primarily by calling
  // optgen.access(...), predictor.train(...)/predict(...), and update_rrpv(...)/find_victim(...) from rrip.h, do not re-implement OPTgen/predictor/RRIP logic
  // here.
  //
  // find_victim (args);
  // replacement_cache_fill (args);
  // update_replacement_state (args);

  int NUM_WAYS;
  vector<vector<int>> rrpv; // RRPV values = 7 since initially any line can be evicted

  HawkeyePredictor predictor; // Predictor instance (constructed with 8192, 3 in the ctor init list)
  OPTgen optgen;              // OPTgen instance

  size_t history_len;              // 8W, same horizon OPTgen uses
  vector<vector<uint64_t>> pc_seq; // PC at each step, indexed by OPTgen's clock

public:
  explicit hawkeye(CACHE* cache);

  hawkeye(CACHE* cache, size_t sets, size_t ways);

  // constructors similar to lru file but with ours data type
  long find_victim(uint64_t triggering_cpu, uint64_t instr_id, size_t set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);

  void replacement_cache_fill(uint64_t triggering_cpu, size_t set, size_t way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type);

  void update_replacement_state(uint64_t triggering_cpu, size_t set, size_t way, champsim::address full_addr, champsim::address ip,
                                champsim::address victim_addr, access_type type, bool hit);
};

#endif