#include "predictor.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

using namespace std;

HawkeyePredictor::HawkeyePredictor(size_t num_entries = 8192, int counter_bits = 3){};

void HawkeyePredictor::train(uint64_t pc, bool opt_hit){
    
};

bool HawkeyePredictor::predict(uint64_t pc) const{};

int HawkeyePredictor::get_counter(uint64_t pc) const{

};