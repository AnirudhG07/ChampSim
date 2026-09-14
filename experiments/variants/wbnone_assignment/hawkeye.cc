// hawkeye.cc
#include "hawkeye.h"

#include <algorithm>

#include "cache.h"
// TODO: implement find_victim / replacement_cache_fill / update_replacement_state here, delegating to optgen / predictor / rrip.h as described in hawkeye.h
// above.

hawkeye::hawkeye(CACHE* cache) : hawkeye(cache, cache->NUM_SET, cache->NUM_WAY) {}

hawkeye::hawkeye(CACHE* cache, size_t sets, size_t ways)
    : replacement(cache), NUM_WAYS(ways), rrpv(sets, vector<int>(ways, 7)), predictor(8192, 3), optgen(sets, ways), history_len(ways * 8),
      addr_seq(sets, vector<uint64_t>(ways * 8, 0)), pc_seq(sets, vector<uint64_t>(ways * 8, 0)), pc_time(sets, 0)
{
}

/*
Parameter meanings (from inc/cache.h's dispatch):
- triggering_cpu: index of the core whose access caused this call; always 0 in single-core runs
- instr_id:       unique id of the instruction that generated the access, used for ordering and debug
- set:            index of the cache set being accessed
- way:            index of the way within that set (the line being filled or hit)
- current_set:    pointer to way 0 of this set; the NUM_WAY blocks are contiguous, so you can scan them for invalid ways
- ip:             PC of the instruction that caused the access -> this is what feeds the predictor
- full_addr:      full byte address of the access -> convert to a block address before giving it to OPTgen
- victim_addr:    address of the line being evicted; empty (champsim::address{}) when hit is true
- type:           LOAD / RFO / PREFETCH / WRITE / TRANSLATION
- hit:            whether this access hit in the cache
*/

long hawkeye::find_victim(uint64_t triggering_cpu, uint64_t instr_id, size_t set, const champsim::cache_block* current_set, champsim::address ip,
                          champsim::address full_addr, access_type type)
{
  // find initial victim from rrpv for that set
  size_t victim_way = ::find_victim(rrpv[set]);
  return static_cast<long>(victim_way);
}

void hawkeye::replacement_cache_fill(uint64_t triggering_cpu, size_t set, size_t way, champsim::address full_addr, champsim::address ip,
                                     champsim::address victim_addr, access_type type)
{
  // impl_replacement_cache_fill(fill.cpu, get_set_index(fill.address), way_idx, module_address(fill), fill.ip, evicting_address, fill.type);
  // So Cache miss happened, replace with new block
  Classification cls;

  if (access_type{type} == access_type::WRITE) // for writeback hits
  {
    rrpv[set][way] = 7;
    return;
  }

  // get prediction from predictor
  bool _pred = predictor.predict(ip.to<uint64_t>());
  cls = _pred ? Classification::CACHE_FRIENDLY : Classification::CACHE_AVERSE;

  // Update the RRPV
  ::update_rrpv(rrpv[set], static_cast<std::size_t>(way), cls, false);
}

void hawkeye::update_replacement_state(uint64_t triggering_cpu, size_t set, size_t way, champsim::address full_addr, champsim::address ip,
                                       champsim::address victim_addr, access_type type, bool hit)
{

  // get cache block of the previous address
  auto block = champsim::block_number{full_addr}.to<uint64_t>();

  if (access_type{type} == access_type::WRITE) // for writeback hits
  {
    return;
  }

  size_t t = pc_time[set];
  size_t steps = std::min(t, history_len);
  bool found_prev = false;
  uint64_t prev_pc = 0;

  for (size_t s = 1; s <= steps; ++s) {
    size_t i = (t - s) % history_len;
    if (addr_seq[set][i] == block) {
      prev_pc = pc_seq[set][i];
      found_prev = true;
      break;
    }
  }

  // get the optgen cache hit/miss
  bool optgen_hit = optgen.access(set, block);

  // train the PC that last accessed this block, not the current one. If it is
  // not in the window there is nothing to attribute the verdict to.
  if (found_prev) {
    predictor.train(prev_pc, optgen_hit);
  }

  // record this access at the current step
  addr_seq[set][t % history_len] = block;
  pc_seq[set][t % history_len] = ip.to<uint64_t>();
  pc_time[set] = t + 1;

  // if it misses, it will automatically go to replacement_cache_fill

  if (hit) {
    // update the rrpv for a hit = true
    bool _pred = predictor.predict(ip.to<uint64_t>());
    Classification cls = _pred ? Classification::CACHE_FRIENDLY : Classification::CACHE_AVERSE;
    ::update_rrpv(rrpv[set], static_cast<std::size_t>(way), cls, true);
  }
}