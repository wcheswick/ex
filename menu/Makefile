TARGET?=${HOME}/../ex
BIN?=${TARGET}/bin
FONTS?=${TARGET}/lib/fonts
LIB?=${TARGET}/lib/lsc

TEXT_FONT=courB14
BUTTON_FONT=helvB18
TITLE_FONT=helvB24
SUBTITLE_FONT=helvBO18		# oblique is broken?
#SUBTITLE_FONT=helvB18

#XFONTDIR=/usr/X11R6/lib/X11/fonts/75dpi
XFONTDIR=/usr/local/lib/X11/fonts/75dpi
FONTS=../lib/fonts

INC=-I../libio -I../libutil
CFLAGS+=-O3 -g -Wall -Wno-unused -pthread ${INC} -static\
	-DTEXT_FONT='"${TEXT_FONT}.bdf"' -DBUTTON_FONT='"${BUTTON_FONT}.bdf"' \
	-DTITLE_FONT='"${TITLE_FONT}.bdf"' -DSUBTITLE_FONT='"${SUBTITLE_FONT}.bdf"'

LDFLAGS+=-pthread -g

OBJS=menu.o

DLIBS=../libtrans/libtrans.a ../libutil/libutil.a ../libio/libio.a
LIBS=-L/usr/local/lib ${DLIBS} -lm -ljpeg

XLIBS=-lglut -lGLU
VLIBS= # ../libio/libvgl.a

XOUT=../libio/xout.o

all::	menu

menu:	${OBJS} ${XOUT} ../libio/libio.a ../libutil/libutil.a
	${CC} -o $@ ${LDFLAGS} ${OBJS} ${XOUT} ${XLIBS} ${LIBS}

${OBJS}:	../libio/io.h \
		${FONTS}/${TEXT_FONT}.bdf \
		${FONTS}/${BUTTON_FONT}.bdf \
		${FONTS}/${TITLE_FONT}.bdf \
		${FONTS}/${SUBTITLE_FONT}.bdf

${FONTS}/${TEXT_FONT}.bdf:	${XFONTDIR}/${TEXT_FONT}.pcf.gz
	zcat -f $> | pcf2bdf -o $@ /dev/stdin

${FONTS}/${BUTTON_FONT}.bdf:	${XFONTDIR}/${BUTTON_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${BUTTON_FONT}.pcf* | pcf2bdf -o $@ /dev/stdin

${FONTS}/${TITLE_FONT}.bdf:	${XFONTDIR}/${TITLE_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${TITLE_FONT}.pcf* | pcf2bdf -o $@ /dev/stdin

${FONTS}/${SUBTITLE_FONT}.bdf:	${XFONTDIR}/${SUBTITLE_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${SUBTITLE_FONT}.pcf* | pcf2bdf -o $@ /dev/stdin


install::	${BIN}/menu ${BIN}/xmenu

${BIN}/menu:	menu
	cp $> $@


clean:
	rm -f *.o *.core menu
