CFLAGS+=-O3 -pipe -g -Wall -I. -DLIBVGL_DEBUG -static

OBJS=main.o simple.o bitmap.o mouse.o keyboard.o #  text.o

../libvgl.a:	${OBJS}
	ar cr $@ ${OBJS}

clean:
	rm -f *.o
