CPP := g++
CPPFLAGS := -std=c++14 -Wall -g

LGSL := $(shell gsl-config --libs)
IGSL := $(shell gsl-config --cflags)

LBLAS := 
IBLAS := 

LIB := -L./
INC := -I./

LDLIBS := -lgsl -lcblas

SRCS := $(wildcard BlockSim/*.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

STRAT_SRCS := $(wildcard StratSim/*.cpp)
STRAT_OBJS := $(patsubst %.cpp,%.o,$(STRAT_SRCS))

all: strat selfish scheduled-strat scheduled-strat-b5 scheduled-strat-b5-gamma scheduled-strat-b5-no-abandon epoch-tracking

test-scheduler: tests/test_strategy_scheduler.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

strat: $(STRAT_SRCS) $(OBJS)
	$(CPP) $(CPPFLAGS) $(INC) $(IBLAS) $(IGSL) $(LDLIBS) $(LGSL) $(LBLAS) -o $@ $^

selfish: SelfishSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

scheduled-strat: ScheduledStratSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

scheduled-strat-b5: ScheduledStratSimB5/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

scheduled-strat-b5-gamma: ScheduledStratSimB5Gamma/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

scheduled-strat-b5-no-abandon: ScheduledStratSimB5NoAbandon/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

epoch-tracking: EpochTrackingSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

publishN: PublishNSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

selfish-petty: SelfishSimPetty/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

stubborn-trail: StubbornTrailSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)
stubborn-fork: StubbornForkSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

stubborn-lead: StubbornLeadSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)
# Combos 
stubborn-lead-fork: StubbornLeadForkSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

stubborn-lead-trail: StubbornLeadTrailSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

stubborn-trail-fork: StubbornTrailForkSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

stubborn-lead-trail-fork: StubbornLeadTrailForkSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

# Rational
rational: SingleStratRationalSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)


# Incentivzed
incentive-selfish: IncentiveSelfishSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

# Multiple Miners
double-strat-rational: DoubleStratRationalSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

double-strat: DoubleStratSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

# TTP 3-Miner Scheduled Simulation (with incentive strategy support)
ttp-scheduler-3miner: TTP_Scheduled_3Miner_Sim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

%.o: %.cpp
	$(CPP) $(CPPFLAGS) $(INC) $(IGSL) $(IBLAS) $(LGSL) $(LBLAS) $(LDLIBS) -o $@ -c $<

clean:
	rm -rf BlockSim/*.o *.o strat selfish stubborn-fork stubborn-trail stubborn-lead scheduled-strat scheduled-strat-b5 scheduled-strat-b5-gamma

.PHONY: all clean

