INC=-I..
CFLAGS=-O3 -g -Wall ${INC} -pthread -static
CFLAGS+=-DIN_W=${IN_W} -DIN_H=${IN_H}

OBJS=util.o button.o fancy_button.o font.o

all:		libutil.a

libutil.a:	${OBJS}
	ar r $@ ${OBJS}
	ranlib $@

button.o:	button.h util.h font.h
fancy_button.o:	fancy_button.h util.h font.h
font.o:		util.h font.h
util.o:		util.h

clean:
	rm -f *.o *.a *.core

ttt:		font.h ttt.c libutil.a
	${CC} ${CFLAGS} -o ttt ttt.c libutil.a
