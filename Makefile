# Simple Makefile for Stage 3 C++ build
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Iinclude

all: dottalkpp

build:
	mkdir -p build

src/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

xbase.o: src/xbase.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

cli.o: src/cli.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

dottalkpp: xbase.o cli.o
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -f *.o dottalkpp
