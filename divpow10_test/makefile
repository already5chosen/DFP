CC = clang
CPP = clang++
COPT = -Wall -O2

all: divpow10_test.exe

main.o: main.cpp divide_pow10_reference.h divide_pow10.h
	${CPP} -c $<

divide_pow10_reference.o: divide_pow10_reference.c divide_pow10_reference.h
	${CC} ${COPT} -c $<

divide_pow10.o: divide_pow10.c divide_pow10.h
	${CC} ${COPT} -c $<

divpow10_test.exe : main.o divide_pow10_reference.o divide_pow10.o
	${CPP} $+ -o $@