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
  // optgen.access(...), predictor.train(...)/predict(...), and update_rrpv(...)/find_victim(...) from rrip.h, do not re-implement OPTgen/predictor/RRIP logic here.
  //
  // find_victim (args);
  // replacement_cache_fill (args);
  // update_replacement_state (args);

  explicit hawkeye(CACHE* cache);
};
#endif