CFLAGS=-g -Ipgsql_src/include/server -Wall
CXXFLAGS=-std=c++0x ${CFLAGS}
CXX=g++
CC=gcc

all: table_sample toast_dumper

table_sample: main.o table_sample_user.o page_dumper.o pg_lzcompress.o
	${CXX} ${CXXFLAGS} $^ -o $@

toast_dumper: main.o toast_dumper_user.o page_dumper.o pg_lzcompress.o
	${CXX} ${CXXFLAGS} $^ -o $@

page_dumper.o: page_dumper.h page_dumper.cpp
main.o: main.cpp page_dumper.h

table_sample_user.o: table_sample_user.cpp page_dumper.h
toast_dumper_user.o: toast_dumper_user.cpp page_dumper.h

pg_lzcompress.o: pgsql_src/pg_lzcompress.c
	${CC} ${CFLAGS} -c $^ -o $@

clean:
	rm -f *.o
