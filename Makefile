CFLAGS ?= -Wall -g
CFLAGS += -I/usr/include/mariadb -I../include -fPIC
LDFLAGS = -g
#LIBS = -lecc256 -lmariadb -lasound
vpath %.c ../elec_token

.PHONY: all release clean

all: elec_dbinit dbtx

VPATH = ../ecc256:../elec_token ../ezini
eccobj = sha256.o base64.o ripemd160.o rand32bytes.o ecc_secp256k1.o dscrc.o dsaes.o

elec_dbinit: elec_dbinit.o global_param.o toktx.o tok_block.o virtmach.o tokens.o ezini.o $(eccobj)
	$(LINK.o) -pthread $^ -lmariadb -lasound -lgmp -o $@

dbtx: dbtx.o base64.o
	$(LINK.o) $^ -lmariadb -o $@

release: rnda elec_dbinit

release: CFLAGS += -O2

release: LDFLAGS += -Wl,-O2

clean:
	rm -rf rnda *.o
	rm -f rnda elec_dbinit
