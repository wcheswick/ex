TARGET?=/home/ex
LIB?=${TARGET}/lib/lsc

transformlist=bigpixels bleed blkwht blur blurry \
	brghtr colorize contrast crackle cylinder dimmer earthqke escher \
	fisheye flip focus kite mirror negative oilpaint outline \
	pinch raisedgray shower solar speckle twist wave zoom
transformtype=color look shape
actions= freeze startover undo

all::	dirs names buttons ${LIB}/background.pnm    # XXX  descriptions

dirs::
.for i in names buttons descriptions
	@mkdir -p ${LIB}/$i
.endfor

.for i in ${transformlist}
names::	${LIB}/names/$i.pnm

${LIB}/names/$i.pnm:	names/$i.gif
	anytopnm names/$i.gif >$@ 2>/dev/null
.endfor

.for i in ${transformlist}
descriptions::	${LIB}/descriptions/$i.pnm

${LIB}/descriptions/$i.pnm:	descriptions/$i.gif
	anytopnm descriptions/$i.gif >$@ 2>/dev/null
.endfor


.for i in ${actions} ${transformlist} ${transformtype}
buttons::	${LIB}/buttons/$i_up.pnm ${LIB}/buttons/$i_dwn.pnm

${LIB}/buttons/$i_up.pnm:	buttons/$i_up.gif
	anytopnm buttons/$i_up.gif >$@ 2>/dev/null

${LIB}/buttons/$i_dwn.pnm:	buttons/$i_dwn.gif
	anytopnm buttons/$i_dwn.gif >$@ 2>/dev/null
.endfor

${LIB}/background.pnm:	background-1680x1050.png
	anytopnm background-1680x1050.png >$@ 2>/dev/null
