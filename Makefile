# Makefile for "dm", a (hex) dump program.

OPTIM = -O2 -Wall

CFLAGS = $(OPTIM)

DESTDIR =
prefix = $(HOME)
bindir = ${prefix}/bin

OBJ = main.o opt.o print.o utf8.o

dm: $(OBJ)
	$(CC) $(OPTIM) -o dm $(OBJ)

$(OBJ): dm.h

install: dm
	cp dm ${DESTDIR}${bindir}

shar: README makefile main.c opt.c print.c dm.h dm.nro
	shar $?

dm.tar.gz: README makefile main.c opt.c print.c dm.h dm.nro
	tar czf dm.tar.gz $^

clean:
	rm -f dm *.o
