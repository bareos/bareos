
PREFIX = /usr/local

DO_PROTO = ../build-tools/do_proto
RM = rm -f

INCS = -I../libdroplet/include

PLATFORM ?= $(shell uname -o)
ifeq ($(PLATFORM),Solaris)
EXTRA_LIBS = -lsocket -lnsl
endif

LIBS = -L../libdroplet/lib -ldroplet -lm -lz -lssl -lcrypto -lpthread -lxml2 -lreadline $(EXTRA_LIBS)
DEPENDS = ../libdroplet/lib/libdroplet.a

CFLAGSPROD = $(INCS) -O2 -Wall -Wunused -Werror -Wuninitialized -D_REENTRANT -fPIC
CFLAGSDEBUG = $(INCS) -g -Wall -Wunused -Werror -D_REENTRANT -fPIC -DASDK_DEBUG
CFLAGSVALGRIND = $(INCS) -O2 -fno-inline -Wall -Wunused -Werror -D_REENTRANT -fPIC

LDFLAGSPROD = $(LIBS)
LDFLAGSDEBUG = $(LIBS)
LDFLAGVALGRIND = $(LIBS)

# not used by prod builds
CFLAGS=$(CFLAGSDEBUG)
LDFLAGS=$(LDFLAGSDEBUG)

PROGS = \
recurse

RECURSE_OBJS = \
recurse.o

all: prod

prod: CFLAGS=$(CFLAGSPROD)
prod: LDFLAGS=$(LDFLAGSPROD)
prod: compile

prod32: CFLAGS=$(CFLAGSPROD) -m32
prod32: LDFLAGS=$(LDFLAGSPROD) -m32
prod32: compile

debug: CFLAGS=$(CFLAGSDEBUG)
debug: LDFLAGS=$(LDFLAGSDEBUG)
debug: compile

valgrind: CFLAGS=$(CFLAGSVALGRIND)
valgrind: LDFLAGS=$(LDFLAGSVALGRIND)
valgrind: compile

compile: $(PROGS) $(RECURSE_LINKS)

recurse: $(RECURSE_OBJS) $(DEPENDS)
	$(CC) -o recurse $(RECURSE_OBJS) $(LDFLAGS)
	
install:
	install -m755 -c $(PROGS) $(PREFIX)/bin

clean:
	$(RM) *.o $(PROGS) $(RECURSE_LINKS)

fproto:
	touch *.c
	$(DO_PROTO) $(INCS) *.h 
