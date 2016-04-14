TARGET?=/home/ex
LIB=${TARGET}/lib

INC=-I/usr/local/include -I..
CFLAGS=-O3 -g -Wall ${INC} -pthread -static
CFLAGS+=-DIN_W=${IN_W} -DIN_H=${IN_H}

# TIN=tin.o	# test: file only, no live input
# HIN=hin.o	# FreeBSD BT848 frame capture board
# LIN=lin.o	# linux v4l2 input (ubuntu)
# UIN=uin.o	# Crappy ancient FreeBSD USB input for ov511
# WIN=win.o	# Webcam net input (under construction)

XOUT=xout.o	# X window system GLUT driver
#VOUT=vout.o	# deprecated, FreeBSD VGA driver

IOOBJS=${XOUT}
.for IN in ${IN_DEVS}
IOOBJS+=${IN}in.o
.endfor

# IOOBJS=${LIN} ${TIN} ${XOUT} # ${WIN} ${HIN} ${UIN}

OBJS=fileio.o

lib::		${IOOBJS} libio.a ${LIB}/sample.jpeg

${LIB}/sample.jpeg:	sample.jpeg
	cp ${.ALLSRC} ${.TARGET}

libio.a:	${OBJS}
	ar r libio.a ${OBJS}
	ranlib libio.a

${IOOBJS}:	io.h ../libutil/util.h
${OBJS}:	io.h ../libutil/util.h

clean:
	rm -f *.o *.a *.core

test:	win_debug.o
	${CC} -g win_debug.o -L/usr/local/lib libio.a ../libutil/libutil.a -ljpeg
	a.out

win_debug.o:	win.c io.h ../libutil/util.h
	${CC} -c win.c ${CFLAGS} -DWINDEBUG=1 -o win_debug.o
