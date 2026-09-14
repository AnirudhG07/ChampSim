# Demo/diagnostic tests for the Hawkeye components. NOT part of the ChampSim build,
# and NOT the graded tests -- those are optgen_test.cc / predictor_test.cc / rrip_test.cc.
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
SRC       = ../replacement/hawkeye

DEMOS = demo_optgen demo_predictor demo_rrip
GRADED = optgen_test predictor_test rrip_test

.PHONY: all demos graded run clean
all: demos graded

demos: $(DEMOS)
graded: $(GRADED)

demo_optgen:    demo_optgen.cc    $(SRC)/optgen.cc
	$(CXX) $(CXXFLAGS) $^ -o $@
demo_predictor: demo_predictor.cc $(SRC)/predictor.cc
	$(CXX) $(CXXFLAGS) $^ -o $@
demo_rrip:      demo_rrip.cc      $(SRC)/rrip.cc
	$(CXX) $(CXXFLAGS) $^ -o $@

optgen_test:    optgen_test.cc    $(SRC)/optgen.cc
	$(CXX) $(CXXFLAGS) $^ -o $@
predictor_test: predictor_test.cc $(SRC)/predictor.cc
	$(CXX) $(CXXFLAGS) $^ -o $@
rrip_test:      rrip_test.cc      $(SRC)/rrip.cc
	$(CXX) $(CXXFLAGS) $^ -o $@

run: demos
	@./demo_optgen    && ./demo_predictor && ./demo_rrip

clean:
	rm -f $(DEMOS) $(GRADED)
