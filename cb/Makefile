TARGET?=/home/ex
BIN?=${TARGET}/bin
FONTS?=${TARGET}/lib/fonts
LIB?=${TARGET}/lib/cb

#TEXT_FONT=helvBO14
TEXT_FONT=helvB14
BUTTON_FONT=helvB12
BIG_BUTTON_FONT=helvR14
TITLE_FONT=helvB24

INC=-I..
CFLAGS+=-g -Wall ${INC} -static -pthread
CFLAGS+=-DTF='"${TEXT_FONT}.bdf"' -DBF='"${BIG_BUTTON_FONT}.bdf"'
CFLAGS+=-DTIF='"${TITLE_FONT}.bdf"' 
CFLAGS+=-DIN_W=${IN_W} -DIN_H=${IN_H}

LDFLAGS=-g -pthread

#XFONTDIR=/usr/X11R6/lib/X11/fonts/75dpi
XFONTDIR=/usr/local/lib/X11/fonts/75dpi
XFONTDIR=/usr/share/fonts/X11/100dpi	# linux

OBJS=cb.o colorblind.o

DLIBS=../libio/libio.a ../libutil/libutil.a
LIBS=-L/usr/local/lib ${DLIBS} -lm -ljpeg
ULIBS=-lnetpbm 

XLIBS=-lglut -lGLU

TIN=../libio/tin.o
LIN=../libio/lin.o
HIN=../libio/hin.o
UIN=../libio/uin.o
XOUT=../libio/xout.o


.for in in ${IN_DEVS}
PROGS+=${BIN}/cb_${in}
.endfor

all::	images ${PROGS}

images::
	(cd images; ${MAKE} TARGET=${TARGET} IN_W=${IN_W} IN_H=${IN_H})

${BIN}/cb_u:	${OBJS} ${UIN} ${XOUT} ${DLIBS}
	${CC} -o ${.TARGET} ${LDFLAGS} ${OBJS} ${UIN} ${ULIBS} ${XOUT} ${XLIBS} ${LIBS}
	
${BIN}/cb_h:	${OBJS} ${HIN} ${XOUT} ${DLIBS}
	${CC} -o ${.TARGET} ${LDFLAGS} ${OBJS} ${HIN} ${XOUT} ${XLIBS} ${LIBS}
	
${BIN}/cb_t:	${OBJS} ${TIN} ${XOUT} ${DLIBS}
	${CC} -o ${.TARGET} ${LDFLAGS} ${OBJS} ${TIN} ${XOUT} ${XLIBS} ${LIBS}
	
${BIN}/cb_l:	${OBJS} ${LIN} ${XOUT} ${DLIBS}
	${CC} -o ${.TARGET} ${LDFLAGS} ${OBJS} ${LIN} ${XOUT} ${XLIBS} ${LIBS}

${OBJS}:	${FONTS}/${TEXT_FONT}.bdf ../libio/io.h \
		../libutil/util.h Makefile\
		${FONTS}/${BUTTON_FONT}.bdf

${OBJS}:	colorblind.h

${FONTS}/${TEXT_FONT}.bdf:	${XFONTDIR}/${TEXT_FONT}.pcf.gz Makefile
	zcat -f $> | pcf2bdf -o $@ /dev/stdin

${FONTS}/${BUTTON_FONT}.bdf:	${XFONTDIR}/${BUTTON_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${BUTTON_FONT}.pcf* | pcf2bdf -o $@ /dev/stdin

clean::
	rm -f *.o *.core

