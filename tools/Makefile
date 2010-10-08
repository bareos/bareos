
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
dplsh

DPLSH_OBJS = \
usage.o \
xfuncs.o \
shell.o \
cmds.o \
vars.o \
utils.o \
dplsh.o \
filecompl.o \
cmd_cd.o \
cmd_lcd.o \
cmd_pwd.o \
cmd_set.o \
cmd_unset.o \
cmd_la.o \
cmd_put.o \
cmd_get.o \
cmd_rm.o \
cmd_getattr.o \
cmd_mb.o \
cmd_rb.o \
cmd_ls.o \
cmd_mkdir.o \
cmd_rmdir.o \
cmd_genurl.o \
cmd_setattr.o \
cmd_cp.o

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

compile: $(PROGS) $(DPLSH_LINKS)

dplsh: $(DPLSH_OBJS) $(DEPENDS)
	$(CC) -o dplsh $(DPLSH_OBJS) $(LDFLAGS)

install:
	install -m755 -c $(PROGS) $(PREFIX)/bin

clean:
	$(RM) *.o $(PROGS) $(DPLSH_LINKS)

fproto:
	touch *.c
	$(DO_PROTO) $(INCS) *.h 
