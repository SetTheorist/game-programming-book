#CC=gcc
#CC=g++ -fpermissive
#CCOPTS=-Wall -O

#CC=quantify gcc
#CCOPTS=-Wall -g

#CC=purify gcc
#CCOPTS=-Wall -g

CC=gcc
CCOPTS=-Wall -g

all: cng

clean:
	rm -f cng cng.o cng-b.o cng-s.o

cng: cng.o cng-b.o cng-s.o
	${CC} ${CCOPTS} -o cng cng.o cng-b.o cng-s.o -lm

cng-b.o: cng-b.c cng-b.h
	${CC} ${CCOPTS} -c cng-b.c -o cng-b.o

cng-s.o: cng-s.c cng-b.h cng-s.h
	${CC} ${CCOPTS} -c cng-s.c -o cng-s.o

cng.o: cng.c cng-b.h cng-s.h
	${CC} ${CCOPTS} -c cng.c -o cng.o
