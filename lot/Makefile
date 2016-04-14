TARGET?=/home/ex
BIN?=${TARGET}/bin
FONTS?=${TARGET}/lib/fonts
LIB?=${TARGET}/lib/lot

TEXT_FONT=courB14
BUTTON_FONT=helvB18
TITLE_FONT=helvB24
SUBTITLE_FONT=helvBO18		# oblique is broken
#SUBTITLE_FONT=helvB18

#XFONTDIR=/usr/X11R6/lib/X11/fonts/75dpi
XFONTDIR=/usr/local/lib/X11/fonts/100dpi	# FreeBSD
XFONTDIR=/usr/share/fonts/X11/100dpi

INC=-I..
CFLAGS+=-O3 -g -Wall -pthread ${INC} -static
CFLAGS+=-DTF='"${TEXT_FONT}.bdf"' -DBF='"${BUTTON_FONT}.bdf"'
CFLAGS+=-DTIF='"${TITLE_FONT}.bdf"' -DSTIF='"${SUBTITLE_FONT}.bdf"'
CFLAGS+=-DIN_W=${IN_W} -DIN_H=${IN_H}

LDFLAGS+=-pthread -g

OBJS=lot.o

DLIBS=../libutil/libutil.a ../libio/libio.a 
LIBS=-L/usr/local/lib -lm -ljpeg
XLIBS=-lglut -lGLU
XOUT=../libio/xout.o


all::	${BIN}/lot data

data::	${LIB}/lotdata

${LIB}:
	-mkdir -p ${.TARGET}

${LIB}/lotdata:	${LIB} pick6.dat
	grep -v Unknown pick6.dat | cut -f5-10,12-17 -d% | \
		sed 's/\.00//g' | sed 's/,//g' | \
		sed 's/%/ /g' >${.TARGET}

${BIN}/lot:	${OBJS} ${XOUT} ../libio/libio.a ../libutil/libutil.a
	${CC} -o $@ ${LDFLAGS} ${OBJS} ${XOUT} ${XLIBS} ${DLIBS} ${LIBS}

${OBJS}:	../libio/io.h \
		${FONTS}/${TEXT_FONT}.bdf \
		${FONTS}/${BUTTON_FONT}.bdf \
		${FONTS}/${TITLE_FONT}.bdf \
		${FONTS}/${SUBTITLE_FONT}.bdf


${FONTS}/${TEXT_FONT}.bdf:	${XFONTDIR}/${TEXT_FONT}.pcf.gz
	zcat -f ${XFONTDIR}/${TEXT_FONT}.pcf.gz | pcf2bdf -o ${.TARGET} /dev/stdin

${FONTS}/${BUTTON_FONT}.bdf:	${XFONTDIR}/${BUTTON_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${BUTTON_FONT}.pcf* | pcf2bdf -o ${.TARGET} /dev/stdin

${FONTS}/${TITLE_FONT}.bdf:	${XFONTDIR}/${TITLE_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${TITLE_FONT}.pcf* | pcf2bdf -o ${.TARGET} /dev/stdin

${FONTS}/${SUBTITLE_FONT}.bdf:	${XFONTDIR}/${SUBTITLE_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${SUBTITLE_FONT}.pcf* | pcf2bdf -o ${.TARGET} /dev/stdin


clean:
	rm -f *.o *.core lot lotdata
