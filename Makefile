CFLAGS ?= -Wall -g
CFLAGS += -I/usr/include/mariadb -I../include -fPIC
LDFLAGS = -g
LIBS = ./ecc256/alsarec.o ./ecc256/sha256.o -lmariadb -lasound

.PHONY: rnda release clean

rnda: rnda.o mariadb.o
	$(LINK.o) $^ $(LIBS) -o $@

release: rnda

release: CFLAGS += -O2

release: LDFLAGS += -Wl,-O2

clean:
	rm -rf rnda *.o
