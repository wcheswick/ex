LIB?=../../lib/images

translations::	${LIB}/protanope.pnm ${LIB}/deuteranope.pnm ${LIB}/tritanope.pnm

${LIB}/protanope.pnm:	protanope.png
	pngtopnm $> $@

${LIB}/deuteranope.pnm:	deuteranope.png
	pngtopnm $> $@

${LIB}/tritanope.pnm:	tritanope.png
	pngtopnm $> $@

all:	spectrum.tiff

spectrum.pbm:
	mkpbm >$@

spectrum.tiff:	spectrum.pbm
	pngtopnm $> $@
