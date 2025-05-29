.SUFFIXES: .d .h .cc .o
VPATH= .. ../RavelCAPI

UNITTESTOBJS=main.o testTensorOps.o testTensorVal.o testXVector.o

CIVITAOBJS=$(filter-out ../tclmain.o,$(wildcard ../*.o))
FLAGS=-g -I.. -I../RavelCAPI
LIBS+=-L.. -lcivita -lboost_date_time -lboost_thread -lboost_system -lpthread -lUnitTest++ 
CPLUSPLUS=g++

ifdef GCOV
FLAGS+=-fprofile-arcs -ftest-coverage
endif

all: unittests 

unittests: $(UNITTESTOBJS) $(CIVITAOBJS) 
	$(CPLUSPLUS) $(FLAGS)  -o $@ $(UNITTESTOBJS) $(CIVITAOBJS) $(LIBS)

.cc.d: 
	$(CPLUSPLUS) $(FLAGS)  -MM -MG $< >$@

.cc.o: 
	$(CPLUSPLUS) -c $(FLAGS) $(OPT) -o $@ $<

ifneq ($(MAKECMDGOALS),clean)
include $(UNITTESTOBJS:.o=.d)
endif

BASIC_CLEAN=rm -rf *.o *~ "\#*\#" core *.d *.cd *.xcd *.gcda *.gcno

clean:
	-$(BASIC_CLEAN) unittests
