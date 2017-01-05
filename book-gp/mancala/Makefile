

all: mancala mm

mancala: mancala.o
	gcc -o mancala mancala.o

mancala.o: mancala.c
	gcc -Wall -O3 -c mancala.c -o mancala.o


CC=gcc
//CCOPTS=-Wall -Wextra -g -I. -I../generic
//CCOPTS=-Wall -Wextra -O -I. -I../generic
CCOPTS=-Wall -Wextra -O3 -I. -I../generic

mm: board.o main.o search.o util.o
	${CC} ${CCOPTS} $+ -o $@ -lm

board.o: board.c board.h
	${CC} ${CCOPTS} -c board.c

main.o: main.c board.h ../generic/search.h ../generic/util.h
	${CC} ${CCOPTS} -c main.c

search.o: ../generic/search.c ../generic/search.h board.h
	${CC} ${CCOPTS} -c ../generic/search.c

util.o: ../generic/util.c ../generic/util.h
	${CC} ${CCOPTS} -c ../generic/util.c
