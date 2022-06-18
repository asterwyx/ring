CFLAGS=
CC=g++
CXXFLAGS=-g
LDFLAGS=
SRCS=main.cpp ring.hpp 

test_ring : ${SRCS}
	${CC} ${CXXFLAGS} ${LDFLAGS} ${SRCS} -o $@