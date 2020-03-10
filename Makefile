CFLAGS ?= -Wall -g
CFLAGS += -I/usr/include/mariadb -I../include -fPIC
LDFLAGS = -g
#LIBS = -lecc256 -lmariadb -lasound
vpath %.c ../elec_token

.PHONY: all release clean

all: rnda elec_dbinit

rnda: rnda.o mariadb.o
	$(LINK.o) $^ -L../lib -lecc256 -lmariadb -o $@

elec_dbinit: elec_dbinit.o tok_block.o
	$(LINK.o) -pthread $^ -L../lib -lecc256 -lmariadb -o $@

release: rnda elec_dbinit

release: CFLAGS += -O2

release: LDFLAGS += -Wl,-O2

clean:
	rm -rf rnda *.o
	rm -f rnda elec_dbinit
