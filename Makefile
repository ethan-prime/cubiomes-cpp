#CC      = gcc
CXX     ?= g++
#AR      = ar
ARFLAGS = cr
override LDFLAGS = -lm
override CFLAGS += -Wall -Wextra -fwrapv
override CXXFLAGS += -std=c++23 -Wall -Wextra -fwrapv
override CPPFLAGS += -Iinclude

ifeq ($(OS),Windows_NT)
	override CFLAGS += -D_WIN32
	override CXXFLAGS += -D_WIN32
	CC = gcc
	CXX = g++
	RM = del
else
	override LDFLAGS += -pthread
	override LDFLAGS += -Wl,-rpath,/usr/local/gcc-14.1.0/lib64
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

finders.o: src/finders.cpp include/finders.hpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

generator.o: src/generator.cpp include/generator.hpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

biomenoise.o: src/biomenoise.cpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

biometree.o: biometree.c
	$(CC) -c $(CFLAGS) $<

layers.o: src/layers.cpp include/layers.hpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

biomes.o: src/biomes.cpp include/biomes.hpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

noise.o: src/noise.cpp include/noise.hpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

util.o: src/util.cpp include/util.hpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

quadbase.o: src/quadbase.cpp include/quadbase.hpp
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

clean:
	$(RM) *.o *.a
