
CC=gcc

CCOPTS=-Wall -Wextra -O3 -I.
##CCOPTS=-Wall -Wextra -g -I. -pthread
#CCOPTS=-Wall -Wextra -g -I.
#CCOPTS=-Wall -Wextra -g -p -pg -I.

##LNOPTS=-mthread
LNOPTS=-mthreads
##LNOPTS=-pthread -mthread


all: c-s chupacabra

clean:
	@rm -f *.o *.exe c-s chupacabra *.stackdump

fresh: clean all

c-s: board.o evaluate.o main.o player.o search.o td-lambda.o util.o wb.o
	${CC} ${CCOPTS} ${LNOPTS} $+ -o $@ -lm

chupacabra: board.o evaluate.o main-wb.o player.o search.o td-lambda.o util.o wb.o
	${CC} ${CCOPTS} ${LNOPTS} $+ -o $@ -lm

board.o: board.c board.h evaluate.h search.h util.h util-bits.h
	${CC} ${CCOPTS} -c board.c

evaluate.o: evaluate.c evaluate.h board.h util.h util-bits.h
	${CC} ${CCOPTS} -c evaluate.c

main.o: main.c board.h evaluate.h player.h search.h util.h wb.h
	${CC} ${CCOPTS} -c main.c

main-wb.o: main.c board.h evaluate.h player.h search.h util.h wb.h
	${CC} ${CCOPTS} -DWINBOARD_ONLY=1 -c main.c -o main-wb.o

player.o: player.c player.h board.h evaluate.h search.h
	${CC} ${CCOPTS} -c player.c

search.o: search.c search.h board.h evaluate.h player.h util.h
	${CC} ${CCOPTS} -c search.c

td-lambda.o: td-lambda.c td-lambda.h board.h evaluate.h player.h
	${CC} ${CCOPTS} -c td-lambda.c

util.o: util.c util.h
	${CC} ${CCOPTS} -c util.c

wb.o: wb.c wb.h board.h evaluate.h player.h search.h
	${CC} ${CCOPTS} -c wb.c
