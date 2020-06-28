CFLAGS ?= -Wall -g
CFLAGS += -I/usr/include/mariadb -I../include -fPIC
LDFLAGS = -g
#LIBS = -lecc256 -lmariadb -lasound
vpath %.c ../elec_token

.PHONY: all release clean

all: rnda elec_dbinit dbtx

rnda: rnda.o mariadb.o
	$(LINK.o) $^ -L../lib -lecc256 -lmariadb -o $@

VPATH = ../ecc256:../elec_token
eccobj = sha256.o base64.o ripemd160.o rand32bytes.o ecc_secp256k1.o dscrc.o dsaes.o

elec_dbinit: elec_dbinit.o global_param.o toktx.o toktx_svr.o tok_block.o virtmach.o tokens.o $(eccobj)
	$(LINK.o) -pthread $^ -lmariadb -lasound -lgmp -o $@

dbtx: dbtx.o base64.o
	$(LINK.o) $^ -lmariadb -o $@

release: rnda elec_dbinit

release: CFLAGS += -O2

release: LDFLAGS += -Wl,-O2

clean:
	rm -rf rnda *.o
	rm -f rnda elec_dbinit
