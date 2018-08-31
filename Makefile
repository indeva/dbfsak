CPP=gcc

VERSION=`cat VERSION`

CFLAGS= -Wall -O -I. -DVERSION=\"$(VERSION)\"

DESTDIR=/usr/local/bin
MANDIR=/usr/local/share/man

HEADERS=dbf.h
TARGETS=dbfsak
DUMPOBJ=dbf.o dbfmemo.o dbfsak.o
SQLOBJ=dbf.o

FILES=dbfsak.c \
	dbf.c \
	dbf.h \
	dbfmemo.c \
	Makefile \
	dbfsak.1 \
	LICENSE \
	README \
	CREDITS \
	MANIFEST \
        CHANGES \
	VERSION

.SUFFIXES:.o.c

.c.o	:
	$(CPP) -c $(CFLAGS) $<

all: $(HEADERS) $(TARGETS)

clean:
	rm -f *.o core $(TARGETS)

install:
	cp dbfsak $(DESTDIR)
	cp dbfsak.1 $(MANDIR)

cleanpkg:
	rm -rf ../dbfsak-$(VERSION)
	rm dbfsak-$(VERSION).tar.gz

package:
	mkdir dbfsak-$(VERSION)
	cp `cat MANIFEST` dbfsak-$(VERSION)
	tar czvf dbfsak-$(VERSION).src.tar.gz dbfsak-$(VERSION)
	rm -r dbfsak-$(VERSION)
	rmdir dbfsak-$(VERSION)

# Main targets

dbfsak : $(DUMPOBJ)
	$(CPP) -o $@ $(DUMPOBJ)

# Dependencies

dbfsak : dbfsak.o dbfmemo.o dbf.o

dbf.o : dbf.c

dbfsak.o : dbfsak.c Makefile

dbfmemo.o : dbfmemo.c
