CC = clang
CPP = clang++
COPT = -Wall -O2

all: divpow10_test.exe divpow10branchless_test.exe

main.o: main.cpp divide_pow10_reference.h divide_pow10.h multiprec_ut.h
	${CPP} ${COPT} -c $<

divide_pow10_reference.o: divide_pow10_reference.c divide_pow10_reference.h
	${CC} ${COPT} -c $<

divide_pow10.o: divide_pow10.c divide_pow10.h
	${CC} ${COPT} -c $<

multiprec_ut.o: multiprec_ut.cpp multiprec_ut.h
	${CPP} ${COPT} -c $<

divpow10_test.exe : main.o divide_pow10_reference.o divide_pow10.o multiprec_ut.o
	${CPP} $+ -o $@

divide_pow10branchless.o: divide_pow10branchless.c divide_pow10.h
	${CC} ${COPT} -c $<

divpow10branchless_test.exe : main.o divide_pow10_reference.o divide_pow10branchless.o multiprec_ut.o
	${CPP} $+ -o $@
