CPP := g++
CPPFLAGS := -std=c++14 -Wall -g

LGSL := $(shell gsl-config --libs)
IGSL := $(shell gsl-config --cflags)

LBLAS := -L/opt/homebrew/opt/openblas/lib
IBLAS := -I/opt/homebrew/opt/openblas/include

LIB := -L./
INC := -I./

LDLIBS := -lgsl -lcblas

SRCS := $(wildcard BlockSim/*.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

STRAT_SRCS := $(wildcard StratSim/*.cpp)
STRAT_OBJS := $(patsubst %.cpp,%.o,$(STRAT_SRCS))

all: strat selfish

strat: $(STRAT_SRCS) $(OBJS)
	$(CPP) $(CPPFLAGS) $(INC) $(IBLAS) $(IGSL) $(LDLIBS) $(LGSL) $(LBLAS) -o $@ $^

selfish: SelfishSim/main.cpp $(OBJS)
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

%.o: %.cpp
	$(CPP) $(CPPFLAGS) $(INC) $(IGSL) $(IBLAS) $(LGSL) $(LBLAS) $(LDLIBS) -o $@ -c $<

clean:
	rm -rf BlockSim/*.o *.o strat selfish stubborn-fork stubborn-trail stubborn-lead

.PHONY: all clean

