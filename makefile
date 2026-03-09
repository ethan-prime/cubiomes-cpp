#CC      = gcc
CXX     ?= g++
#AR      = ar
ARFLAGS = cr
override LDFLAGS = -lm
override CFLAGS += -Wall -Wextra -fwrapv
override CXXFLAGS += -std=c++23 -Wall -Wextra -fwrapv

ifeq ($(OS),Windows_NT)
	override CFLAGS += -D_WIN32
	override CXXFLAGS += -D_WIN32
	CC = gcc
	CXX = g++
	RM = del
else
	override LDFLAGS += -pthread
	#RM = rm
endif

.PHONY : all debug release native libcubiomes clean

all: release

debug: CFLAGS += -DDEBUG -O0 -ggdb3
debug: CXXFLAGS += -DDEBUG -O0 -ggdb3
debug: libcubiomes
release: CFLAGS += -O3
release: CXXFLAGS += -O3
release: libcubiomes
native: CFLAGS += -O3 -march=native -ffast-math
native: CXXFLAGS += -O3 -march=native -ffast-math
native: libcubiomes

ifneq ($(OS),Windows_NT)
release: CFLAGS += -fPIC
release: CXXFLAGS += -fPIC
#debug: CFLAGS += -fsanitize=undefined
endif


libcubiomes: noise.o biomes.o layers.o biomenoise.o generator.o finders.o util.o quadbase.o
	$(AR) $(ARFLAGS) libcubiomes.a $^

finders.o: finders.cpp finders.h
	$(CXX) -c $(CXXFLAGS) $<

generator.o: generator.cpp generator.h
	$(CXX) -c $(CXXFLAGS) $<

biomenoise.o: biomenoise.cpp
	$(CXX) -c $(CXXFLAGS) $<

biometree.o: biometree.c
	$(CC) -c $(CFLAGS) $<

layers.o: layers.cpp layers.h
	$(CXX) -c $(CXXFLAGS) $<

biomes.o: biomes.cpp biomes.h
	$(CXX) -c $(CXXFLAGS) $<

noise.o: noise.cpp noise.h
	$(CXX) -c $(CXXFLAGS) $<

util.o: util.cpp util.h
	$(CXX) -c $(CXXFLAGS) util.cpp -o util.o

quadbase.o: quadbase.cpp quadbase.h
	$(CXX) -c $(CXXFLAGS) $<

clean:
	$(RM) *.o *.a
