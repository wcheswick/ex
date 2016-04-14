INC=-I..
CFLAGS=-O3 -g -Wall ${INC} -pthread -static
CFLAGS+=-DIN_W=${IN_W} -DIN_H=${IN_H}

OBJS=trans.o point.o geom.o area.o # etc.o

libtrans.a:	${OBJS}
	ar cr $@ ${OBJS}
	ranlib $@

${OBJS}:	trans.h ../libio/io.h

clean:
	rm -f *.o *.a *.core
