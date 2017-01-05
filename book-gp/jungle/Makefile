

all: jungle

jungle: jungle.o jungle-board.o jungle-search.o
	gcc -o jungle jungle.o jungle-board.o jungle-search.o -lm

jungle-board.o: jungle-board.c jungle-board.h
	gcc -Wall -O3 -c jungle-board.c -o jungle-board.o

jungle-search.o: jungle-search.c jungle-board.h jungle-search.h
	gcc -Wall -O3 -c jungle-search.c -o jungle-search.o

jungle.o: jungle.c jungle-board.h jungle-search.h
	gcc -Wall -O3 -c jungle.c -o jungle.o
