TARGET?=$$HOME/nex
BIN?=${TARGET}/bin
LIB?=${TARGET}/lib
FONTS?=${LIB}/fonts
BUTTONS?=${TARGET}/lib/buttons
XFONTDIR=/usr/share/fonts/X11/100dpi

IN_DEVS="t l"	# legacy: h, u, w
OUT_DEVS="x"	# X is the only output now.  Used to include VGA drivers

IN_W=640
IN_H=360

EXHIBITS=lsc lot cb chat # aud 	// aud need linux sound I/O
AUXPROGS=demo menu exrc #  hw start run_exhibit

LIBS=libio libtrans libutil

all::	dirs ${LIBS} ${EXHIBITS} aux

dirs::
	test -d ${BIN} || mkdir -p ${BIN}
	test -d ${LIB} || mkdir -p ${LIB}
	test -d ${FONTS} || mkdir -p ${FONTS}


libio::
	(cd $@; ${MAKE} TARGET=${TARGET} IN_DEVS=${IN_DEVS} IN_W=${IN_W} IN_H=${IN_H})


libtrans::
	(cd $@; ${MAKE} TARGET=${TARGET} IN_W=${IN_W} IN_H=${IN_H})


libutil::
	(cd $@; ${MAKE} TARGET=${TARGET} IN_W=${IN_W} IN_H=${IN_H})


aud::
	test -d ${LIB}/$@ || mkdir -p ${LIB}/$@
	(cd $@; ${MAKE} TARGET=${TARGET} IN_DEVS=${IN_DEVS} IN_W=${IN_W} IN_H=${IN_H})

lot::
	test -d ${LIB}/$@ || mkdir -p ${LIB}/$@
	(cd $@; ${MAKE} TARGET=${TARGET} IN_DEVS=${IN_DEVS} IN_W=${IN_W} IN_H=${IN_H})

cb::
	test -d ${LIB}/$@ || mkdir -p ${LIB}/$@
	(cd $@; ${MAKE} TARGET=${TARGET} IN_DEVS=${IN_DEVS} IN_W=${IN_W} IN_H=${IN_H})

chat::
	test -d ${LIB}/$@ || mkdir -p ${LIB}/$@
	(cd $@; ${MAKE} TARGET=${TARGET} IN_DEVS=${IN_DEVS} IN_W=${IN_W} IN_H=${IN_H})

lsc::
	test -d ${LIB}/$@ || mkdir -p ${LIB}/$@
	(cd lsc; ${MAKE} TARGET=${TARGET} IN_DEVS=${IN_DEVS} IN_W=${IN_W} IN_H=${IN_H})


aux::
	rsync -av --delete  bin/[a-z]* ${BIN}


clean::
	(cd libtrans;	${MAKE} clean)
	(cd libutil;	${MAKE} clean)
	(cd libio;	${MAKE} clean)
	(cd aud; ${MAKE} clean)
	(cd cb; ${MAKE} clean)
	(cd lsc; ${MAKE} clean)
	(cd chat; ${MAKE} clean)
	(cd lot; ${MAKE} clean)
	rm -f web/ex.tgz

cleanex::
	rm -rf ~ex/*
	
web::
	rsync -a -v --delete --exclude *CVS* web/* h.cheswick.com.:/web/pages/ex 

src::	clean
	tar cf - --exclude lib/sounds --exclude web/ex.tgz * | gzip >web/ex.tgz

# must be run as root.  Set up for FreeBSD 7.2

root::	acct ports

acct::
	grep '^ex:' /etc/passwd >/dev/null || (echo "ex::::::exhibits:::e" | adduser -f -; chown -R ches ~ex)

ports::
	pkg_info | grep freeglut >/dev/null || (cd /usr/ports/graphics/freeglut; make; ${MAKE} install)
	pkg_info | grep netpbm >/dev/null || (cd /usr/ports/graphics/netpbm; make; ${MAKE} install)
	pkg_info | grep libaudiofile >/dev/null || (cd /usr/ports/audio/libaudiofile; make; ${MAKE} install)
	pkg_info | grep pcf2bdf >/dev/null || (cd /usr/ports/x11-fonts/pcf2bdf; make; ${MAKE} install)
	# select Lame MP3 decoding
	pkg_info | grep sox >/dev/null || (cd /usr/ports/audio/sox; make; ${MAKE} install)
	pkg_info | grep rsync >/dev/null || (cd /usr/ports/net/rsync; make; ${MAKE} install)
	pkg_info | grep jpeg >/dev/null || (cd /usr/ports/graphics/jpeg; make; ${MAKE} install)
	# don't select the FORTRAN shim interface option: we don't need it, and it takes forever
	pkg_info | grep fftw >/dev/null || (cd /usr/ports/math/fftw3; make; ${MAKE} install)
	pkg_info | grep jhead >/dev/null || (cd /usr/ports/graphics/jhead; make; ${MAKE} install)
	pkg_info | grep xorg-fonts-75dpi >/dev/null || (cd /usr/ports/x11-fonts/xorg-fonts-75dpi; make; ${MAKE} install)
	pkg_info | grep xorg-fonts-100dpi >/dev/null || (cd /usr/ports/x11-fonts/xorg-fonts-100dpi; make; ${MAKE} install)
	# mesa:
#	pkg_info | grep libGL >/dev/null || (cd /usr/ports/graphics/libGL; make; ${MAKE} install)


burncd::	iso
	burncd -e -s 12 data /usr/obj/FreeSBIE.iso fixate

iso::	/usr/obj/FreeSBIE.iso

/usr/obj/FreeSBIE.iso:	inspkg
	echo "ex" >/var/tmp/pkglist
	(cd ~ches/src/freesbie2; PKGFILE=/var/tmp/pkglist ${MAKE} iso)

inspkg::	pkg
	-pkg_info | grep "^ex[ \t]" >/dev/null && pkg_delete ex 2>/dev/null
	pkg_add -v - </var/tmp/ex.tbz

pkg::		/var/tmp/ex.tbz

/var/tmp/ex.tbz:	bin/mkpackage # ${TARGET}/ex
	bin/mkpackage ex $@
	-rm /usr/obj/mhome/ches/src/freesbie2/.done_pkginstall 2>/dev/null

${TARGET}/ex:	${TARGET}/.installed
	${MAKE} install
	touch $@

