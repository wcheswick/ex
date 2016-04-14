TARGET?=/home/ex
BIN?=${TARGET}/bin
FONTS?=${TARGET}/lib/fonts
LIB?=${TARGET}/lib/cb

IMAGES=cube.jpeg rainbow.gif ishihara25.jpeg

all::	images

images::	${LIB}/cube.pnm \
		${LIB}/rainbow.pnm \
		${LIB}/ishihara9.pnm

${LIB}/cube.pnm:	cube.jpeg
	(anytopnm cube.jpeg 2>/dev/null | pnmscale -ysize ${IN_H} -xsize ${IN_W} >$@)

${LIB}/rainbow.pnm:	rainbow.gif
	anytopnm rainbow.gif | pnmscale -ysize ${IN_H} -xsize ${IN_W} 2>/dev/null >$@

${LIB}/ishihara9.pnm:	ishihara9.gif
	anytopnm ishihara9.gif | pnmscale -ysize ${IN_H} -xsize ${IN_W} 2>/dev/null >$@
