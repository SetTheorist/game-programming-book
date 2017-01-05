

all: simp

simp: simple-shogi.o
	gcc -o simp simple-shogi.o

simple-shogi.o: simple-shogi.c
	gcc -Wall -O -c simple-shogi.c -o simple-shogi.o
