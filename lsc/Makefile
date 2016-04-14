TARGET?=${HOME}/../nex
BIN?=${TARGET}/bin
FONTS?=${TARGET}/lib/fonts
LIB?=${TARGET}/lib/lsc

#TEXT_FONT=helvBO12
TEXT_FONT=helvB12
BUTTON_FONT=helvB10
BIG_BUTTON_FONT=helvB18
BIG_BUTTON_FONT=helvR14

XFONTDIR=/usr/local/lib/X11/fonts/100dpi	# FreeBSD
XFONTDIR=/usr/share/fonts/X11/100dpi

INC=-I../libio -I../libtrans -I../libutil -I/usr/local/include
CFLAGS+=-g -Wall -Wno-unused -pthread ${INC} -static # -O3 
CFLAGS+=-DTEXT_FONT='"${TEXT_FONT}.bdf"' -DBUTTON_FONT='"${BUTTON_FONT}.bdf"'
CFLAGS+=-DBIG_BUTTON_FONT='"${BIG_BUTTON_FONT}.bdf"'
CFLAGS+=-DIN_W=${IN_W} -DIN_H=${IN_H}

LDFLAGS+=-pthread -g

OBJS=lsc.o

LIBS=-L/usr/local/lib ../libtrans/libtrans.a ../libio/libio.a \
	../libutil/libutil.a -lm -ljpeg
#ULIBS=-lnetpbm 

XLIBS=-lglut -lGLU

TIN=../libio/tin.o
HIN=../libio/hin.o
LIN=../libio/lin.o
UIN=../libio/uin.o
WIN=../libio/win.o
XOUT=../libio/xout.o

.for in in ${IN_DEVS}
PROGS+=${BIN}/lsc_${in}
.endfor

# PROGS=lsc_t lsc_l # lsc_w # lsc_u lsc_h 


all::	lib ${PROGS} fonts graphics 

lib::
	@mkdir -p ${LIB}

graphics::
	(cd graphics; ${MAKE} TARGET=${TARGET})

	
${BIN}/lsc_l:	${OBJS} ${LIN} ${XOUT} ../libtrans/libtrans.a ../libio/libio.a ../libutil/libutil.a
	${CC} -o ${.TARGET} ${LDFLAGS} ${OBJS} ${LIN} ${XOUT} ${XLIBS} ${LIBS}

${BIN}/lsc_w:	${OBJS} ${WIN} ${XOUT} ../libtrans/libtrans.a ../libio/libio.a ../libutil/libutil.a
	${CC} -o ${.TARGET} ${LDFLAGS} ${OBJS} ${WIN} ${ULIBS} ${XOUT} ${XLIBS} ${LIBS}

${BIN}/lsc_u:	${OBJS} ${UIN} ${XOUT} ../libtrans/libtrans.a ../libio/libio.a ../libutil/libutil.a
	${CC} -o ${.TARGET} ${LDFLAGS} ${OBJS} ${UIN} ${ULIBS} ${XOUT} ${XLIBS} ${LIBS}
	
${BIN}/lsc_h:	${OBJS} ${HIN} ${XOUT} ../libtrans/libtrans.a ../libio/libio.a ../libutil/libutil.a
	${CC} -o ${.TARGET} ${LDFLAGS} ${OBJS} ${HIN} ${XOUT} ${XLIBS} ${LIBS}
	
${BIN}/lsc_t:	${OBJS} ${TIN} ${XOUT} ../libtrans/libtrans.a ../libio/libio.a ../libutil/libutil.a
	${CC} -o ${.TARGET} ${LDFLAGS} ${OBJS} ${TIN} ${XOUT} ${XLIBS} ${LIBS}

${OBJS}:	../libio/io.h ../libutil/button.h


fonts::		${FONTS}/${TEXT_FONT}.bdf ${FONTS}/${BUTTON_FONT}.bdf \
		${FONTS}/${BIG_BUTTON_FONT}.bdf

${FONTS}/${TEXT_FONT}.bdf:	${XFONTDIR}/${TEXT_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${TEXT_FONT}.pcf.gz | pcf2bdf -o $@ /dev/stdin

${FONTS}/${BUTTON_FONT}.bdf:	${XFONTDIR}/${BUTTON_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${BUTTON_FONT}.pcf.gz | pcf2bdf -o $@ /dev/stdin

${FONTS}/${BIG_BUTTON_FONT}.bdf:	${XFONTDIR}/${BIG_BUTTON_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${BIG_BUTTON_FONT}.pcf.gz | pcf2bdf -o $@ /dev/stdin
	

clean:
	rm -f *.o *.core ${PROGS}
