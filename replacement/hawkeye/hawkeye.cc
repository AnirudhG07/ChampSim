// hawkeye.cc
#include "hawkeye.h"

#include <algorithm>
// TODO: implement find_victim / replacement_cache_fill / update_replacement_state here, delegating to optgen / predictor / rrip.h as described in hawkeye.h
// above.

hawkeye::hawkeye(CACHE* cache) : replacement(cache) {}