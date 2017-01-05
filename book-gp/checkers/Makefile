
CC=gcc
CCOPTS=-Wall -Wextra -O -I. -I../generic

all: ch

ch: board.o main.o search.o util.o
	${CC} ${CCOPTS} $+ -o $@ -lm

board.o: board.c board.h
	${CC} ${CCOPTS} -c board.c

main.o: main.c board.h ../generic/search.h ../generic/util.h
	${CC} ${CCOPTS} -c main.c

search.o: ../generic/search.c ../generic/search.h board.h
	${CC} ${CCOPTS} -c ../generic/search.c

util.o: ../generic/util.c ../generic/util.h
	${CC} ${CCOPTS} -c ../generic/util.c
