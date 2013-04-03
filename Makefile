CXXFLAGS=-std=c++0x -g -Ipgsql_src/include/server -Wall
CXX=g++

all: page_dumper

page_dumper: main.o
	${CXX} ${CXXFLAGS} $^ -o $@
