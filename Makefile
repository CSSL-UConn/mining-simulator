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

selfish-bv: SelfishBlockValueSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

selfish-cm: SelfishCostMiningSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

selfish-nexp: SelfishNetworkExpSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

selfish-nlin: SelfishNetworkLinearSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

selfish-npoiss: SelfishNetworkPoissonSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

selfish-nuni: SelfishNetworkUniformSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

selfish-comp: SelfishClassicClever/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

selfish: SingleSelfishSim/main.cpp $(OBJS)
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

selfish-double: DoubleSelfishSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

selfish-triple: TripleSelfishSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

double-strat: DoubleStratSim/main.cpp $(OBJS)
	$(CPP) $(CPPFLAGS)  $(INC) $(IGSL) $(IBLAS)  -o $@ $^ $(LBLAS) $(LGSL)

plot-nuni: 
	python3 plot.py "Profitability of Selfish Mining in Presence of Uni-Dist Network Delay with α = 0.${ALPHA}" "Selfish Network Delay - Honest Network Delay (sec)" selfish-nuni_500_${ALPHA}_0.txt selfish-nuni_500_${ALPHA}_1.txt 

plot-npoiss: 
	python3 plot.py "Profitability of Selfish Mining in Presence of Poisson-Dist Network Delay with α = 0.${ALPHA}" "Selfish Network Delay - Honest Network Delay (sec)" selfish-npoiss_500_${ALPHA}_0.txt selfish-nuni_500_${ALPHA}_1.txt 

plot-nlin: 
	python3 plot.py "Profitability of Selfish Mining in Presence of Linear Network Delay" "Selfish Network Delay (sec)" selfish-nlin_500_12.txt  selfish-nlin_500_25.txt  selfish-nlin_500_38.txt  selfish-nlin_500_40.txt  selfish-nlin_500_50.txt  

plot-nexp: 
	python3 plot.py "Profitability of Selfish Mining in Presence of Exponential Network Delay" "Selfish Network Delay (sec)" selfish-nexp_25_12.txt  selfish-nexp_25_25.txt selfish-nexp_25_38.txt selfish-nexp_25_40.txt selfish-nexp_25_50.txt    

plot-cm: 
	python3 plot.py "Profitability of Selfish Mining in Presence of Mining Cost per Sec" "Mining Cost/Sec (BTC)" selfish-cm_500_12.txt  selfish-cm_500_25.txt selfish-cm_500_38.txt selfish-cm_500_40.txt selfish-cm_500_50.txt    

plot-bv:
	python3 plot.py "Profitability of Selfish Mining vs. Block Value with λ=$(shell awk 'BEGIN{print ${BV}/1000000}' )" "Block Value (BTC)" selfish-bv_500_12_${BV}.txt  selfish-bv_500_25_${BV}.txt selfish-bv_500_38_${BV}.txt selfish-bv_500_40_${BV}.txt selfish-bv_500_50_${BV}.txt

plot-comp:
	python3 plot.py "Profitability of Mining vs. Share of Classic Selfish α for γ=${GAMMA}%" "Classic Hash Power α %" selfish-comp_${GAMMA}.txt

%.o: %.cpp
	$(CPP) $(CPPFLAGS) $(INC) $(IGSL) $(IBLAS) $(LGSL) $(LBLAS) $(LDLIBS) -o $@ -c $<

clean:
	rm -rf BlockSim/*.o *.o strat selfish stubborn-fork stubborn-trail stubborn-lead

.PHONY: all clean

