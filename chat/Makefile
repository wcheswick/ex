TARGET?=${HOME}/../ex
BIN?=${TARGET}/bin
FONTS?=${TARGET}/lib/fonts
LIB?=${TARGET}/lib/chat

TITLE_FONT=helvBO24
BUTTON_FONT=helvB24

INC=-I/usr/local/include -I../libio -I../libtrans -I../libutil
CFLAGS+=-O0 -g -Wall -Wno-unused ${INC} -static
CFLAGS+=-DTITLE_FONT='"${TITLE_FONT}.bdf"' -DBUTTON_FONT='"${BUTTON_FONT}.bdf"'
CFLAGS+=-DIN_W=${IN_W} -DIN_H=${IN_H}

LDFLAGS+=-pthread -g

#XFONTDIR=/usr/X11R6/lib/X11/fonts/75dpi
XFONTDIR=/usr/local/lib/X11/fonts/75dpi
XFONTDIR=/usr/share/fonts/X11/100dpi	# linux

OBJS=chat.o

DLIBS=../libtrans/libtrans.a ../libio/libio.a ../libutil/libutil.a
LIBS=-L/usr/local/lib ${DLIBS} -lm -ljpeg
ULIBS=-lnetpbm 

XLIBS=-lglut -lGLU

TIN=../libio/tin.o
LIN=../libio/lin.o
HIN=../libio/hin.o
UIN=../libio/uin.o
WIN=../libio/win.o
XOUT=../libio/xout.o

.for in in ${IN_DEVS}
PROGS+=${BIN}/chat_${in}
.endfor

all::	${PROGS}

${BIN}/chat_u:	${OBJS} ${UIN} ${XOUT} ${DLIBS}
	${CC} -o ${.TARGET} ${LDFLAGS} ${OBJS} ${UIN} ${ULIBS} ${XOUT} ${XLIBS} ${LIBS}
	
${BIN}/chat_h:	${OBJS} ${HIN} ${XOUT} ${DLIBS}
	${CC} -o ${.TARGET} ${LDFLAGS} ${OBJS} ${HIN} ${XOUT} ${XLIBS} ${LIBS}
	
${BIN}/chat_t:	${OBJS} ${TIN} ${XOUT} ${DLIBS}
	${CC} -o ${.TARGET} ${LDFLAGS} ${OBJS} ${TIN} ${XOUT} ${XLIBS} ${LIBS}
	
${BIN}/chat_l:	${OBJS} ${LIN} ${XOUT} ${DLIBS}
	${CC} -o ${.TARGET} ${LDFLAGS} ${OBJS} ${LIN} ${XOUT} ${XLIBS} ${LIBS}

${OBJS}:	${FONTS}/${TITLE_FONT}.bdf ../libio/io.h ../libtrans/trans.h \
		../libutil/util.h Makefile\
		${FONTS}/${BUTTON_FONT}.bdf

${FONTS}/${TITLE_FONT}.bdf:	${FONTS} ${XFONTDIR}/${TITLE_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${TITLE_FONT}.pcf.gz | pcf2bdf -o $@ /dev/stdin

${FONTS}/${BUTTON_FONT}.bdf:	${FONTS} ${XFONTDIR}/${BUTTON_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${BUTTON_FONT}.pcf.gz | pcf2bdf -o $@ /dev/stdin

${FONTS}:
	-mkdir ${FONTS}


clean:
	rm -f *.o *.core
