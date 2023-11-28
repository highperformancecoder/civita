.SUFFIXES: .d .h .cc .o .jcd

HOST=$(shell hostname)
OS=$(shell uname)

ifdef DEBUG
OPT=-g
else
OPT=-O3 -DNDEBUG
endif

ifdef FPIC
OPT+=-fPIC
endif

ifdef GCOV
FLAGS+=-fprofile-arcs -ftest-coverage
endif

ifdef MXE
MXE_32bit=$(shell if which i686-w64-mingw32.static-g++>&/dev/null; then echo 1; fi)

MXE_64bit_static=$(shell if which x86_64-w64-mingw32.static-g++>&/dev/null; then echo 1; fi)
MXE_64bit_shared=$(shell if which x86_64-w64-mingw32.shared-g++>&/dev/null; then echo 1; fi)

ifeq ($(MXE_32bit),1)
  MXE_PREFIX=i686-w64-mingw32.static
else
  ifeq ($(MXE_64bit_static),1)  # prefer static to shared
    MXE_PREFIX=x86_64-w64-mingw32.static
  else
    ifeq ($(MXE_64bit_shared),1)
      MXE_PREFIX=x86_64-w64-mingw32.shared
    else
      $(error "MXE compiler not found")
    endif
  endif
endif
CC=$(MXE_PREFIX)-gcc
CPLUSPLUS=$(MXE_PREFIX)-g++
FLAGS=-DWIN32
else
CC=gcc
CPLUSPLUS=g++
FLAGS+=-isystem /usr/local/include -isystem /opt/local/include
endif

OBJS=hypercube.o index.o interpolateHypercube.o tensorOp.o xvector.o
FLAGS+=-I. $(EXTRA_FLAGS) -I$(HOME)/usr/include -I/usr/local/include

ifeq ($(OS),Darwin)
FLAGS+=-std=c++17
endif

ifndef CLASSDESC
ifeq ($(shell if which classdesc>&/dev/null; then echo 1; fi),1)
CLASSDESC=$(shell which classdesc)
endif
endif

ifdef CLASSDESC
FLAGS+=-DCLASSDESC -I$(dir $(CLASSDESC))/../include
endif

ifdef AEGIS
FLAGS+=-Werror -Wall -Wno-unused-local-typedefs -Wno-unused-variable -Wno-unused-function -Wno-unknown-pragmas
ifeq ($(CPLUSPLUS),g++)
FLAGS+=-Wno-restrict
endif
endif

ifdef GCOV
FLAGS+=-fprofile-arcs -ftest-coverage
endif

libcivita.a: $(OBJS)
	ar r $@ $^

.cc.d: 
	$(CPLUSPLUS) $(FLAGS)  -MM -MG $< >$@

.cc.o: 
	$(CPLUSPLUS) -c $(FLAGS) $(OPT) -o $@ $<

.h.jcd:
	$(CLASSDESC) -typeName -nodef -respect_private -i $< json_pack json_unpack >$@

ifneq ($(MAKECMDGOALS),clean)
include $(OBJS:.o=.d)
endif

tests: libcivita.a
	cd test; $(MAKE)

sure: tests
	test/unittests

lcov:
	$(MAKE) clean
	-$(MAKE) GCOV=1 tests
	lcov -i -c -d . --no-external -o lcovi.info
	-$(MAKE) GCOV=1 sure
	lcov -c -d .  --no-external -o lcovt.info
	lcov -a lcovi.info -a lcovt.info -o lcov.info
	genhtml -o coverage lcov.info

BASIC_CLEAN=rm -rf *.o *~ "\#*\#" core *.d *.cd *.jcd *.gcda *.gcno

clean:
	-$(BASIC_CLEAN) libcivita.a
	cd test; $(MAKE) clean
