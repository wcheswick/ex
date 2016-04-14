TARGET?=${HOME}/../ex
BIN?=${TARGET}/bin
FONTS?=${TARGET}/lib/fonts
LIB?=${TARGET}/lib/aud

TEXT_FONT=courB14
BUTTON_FONT=helvB12
TITLE_FONT=helvB24
#SUBTITLE_FONT=helvBO18
SUBTITLE_FONT=helvB18

XFONTDIR?=/usr/local/lib/X11/fonts/75dpi

INC=-I.. -I/usr/local/include
CFLAGS=-O3 -g -Wall -pthread ${INC} -static
CFLAGS+=-DIN_W=${IN_W} -DIN_H=${IN_H}

BUTDEFS=-DTF='"${TEXT_FONT}.bdf"' -DBF='"${BUTTON_FONT}.bdf"'
BUTDEFS+=-DSTF='"${SUBTITLE_FONT}.bdf"'

LDFLAGS=-pthread -g -L/usr/local/lib

OBJS=aud.o audio.o

DLIBS=../libutil/libutil.a ../libio/libio.a 
LIBS=-lfftw3 -lkvm -laudiofile -lm -ljpeg # -lao
XLIBS=-lglut -lGLU

XOUT=../libio/xout.o

SOUNDS=vic-cricks2 \
	treefrogs3 \
	munich-crowd \
	munich-chimes \
	munich-glock \
	gulls \
	yearlinggulls \
	bbdove \
	bbrooster \
	bville-bird

GENSOUNDS=sin-440 \
	sin-880 \
	sq-440

all::	${BIN}/aud sounds fonts

${BIN}/aud:	${OBJS} ${XOUT} ${DLIBS}
	${CC} -o $@ ${LDFLAGS} ${OBJS} ${XOUT} ${XLIBS} ${DLIBS} ${LIBS}

audio.o:	aud.h

aud.o:		aud.h ../libio/io.h # ${FONTS}
	${CC} ${CFLAGS} ${BUTDEFS} -c aud.c 

fonts::		${FONTS}/${TEXT_FONT}.bdf ${FONTS}/${BUTTON_FONT}.bdf \
		${FONTS}/${TITLE_FONT}.bdf ${FONTS}/${SUBTITLE_FONT}.bdf

${FONTS}/${TEXT_FONT}.bdf:	${XFONTDIR}/${TEXT_FONT}.pcf.gz
	zcat -f $> | pcf2bdf -o $@ /dev/stdin

${FONTS}/${BUTTON_FONT}.bdf:	${XFONTDIR}/${BUTTON_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${BUTTON_FONT}.pcf* | pcf2bdf -o $@ /dev/stdin

${FONTS}/${TITLE_FONT}.bdf:	${XFONTDIR}/${TITLE_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${TITLE_FONT}.pcf* | pcf2bdf -o $@ /dev/stdin

${FONTS}/${SUBTITLE_FONT}.bdf:	${XFONTDIR}/${SUBTITLE_FONT}.pcf.gz Makefile
	zcat -f ${XFONTDIR}/${SUBTITLE_FONT}.pcf* | pcf2bdf -o $@ /dev/stdin

clean::
	rm -f *.o *.core aud

.for i in ${SOUNDS} ${GENSOUNDS}
sounds::	${LIB}/$i.wav
.endfor

.for i in ${SOUNDS}
${LIB}/$i.wav:	sounds/$i.wav
	cp $> $@
.endfor

${LIB}/sin-440.wav:
	sox sounds/bville-bird.wav $@ synth sine 440 vol 0.4

${LIB}/sin-880.wav:
	sox sounds/bville-bird.wav $@ synth sine 880 vol 0.4

${LIB}/sq-440.wav:
	sox sounds/bville-bird.wav $@ synth square 440 vol 0.2

${LIB}/sinx.wav:
	sox sounds/bville-bird.wav $@ synth sine 10100 vol 0.1
