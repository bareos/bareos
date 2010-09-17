
PREFIX = /usr/local

DO_PROTO = ../build-tools/do_proto
RM = rm -f

INCS = -I../libdroplets/include

PLATFORM ?= $(shell uname -o)
ifeq ($(PLATFORM),Solaris)
EXTRA_LIBS = -lsocket -lnsl
endif

LIBS = -L../libdroplets/lib -ldroplets -lm -lz -lssl -lcrypto -lpthread -lxml2 -lreadline $(EXTRA_LIBS)
DEPENDS = ../libdroplets/lib/libdroplets.a

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
dpl_box \
dpl_sh

DPL_BOX_LINKS = \
dpl_la \
dpl_ls \
dpl_mb \
dpl_get \
dpl_head \
dpl_put \
dpl_bput \
dpl_bget \
dpl_delete

COMMON_OBJS = \
usage.o \
xfuncs.o

DPL_BOX_OBJS = \
$(COMMON_OBJS) \
dpl_box.o 

DPL_SH_OBJS = \
$(COMMON_OBJS) \
shell.o \
cmds.o \
vars.o \
dpl_sh.o \
cmd_cd.o \
cmd_lcd.o \
cmd_pwd.o \
cmd_set.o \
cmd_unset.o \
cmd_ls.o \
cmd_mkdir.o

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

compile: $(PROGS) $(DPL_BOX_LINKS)

dpl_box: $(DPL_BOX_OBJS) $(DEPENDS)
	$(CC) -o dpl_box $(DPL_BOX_OBJS) $(LDFLAGS)

$(DPL_BOX_LINKS): dpl_box
	ln -sf dpl_box $@

dpl_sh: $(DPL_SH_OBJS) $(DEPENDS)
	$(CC) -o dpl_sh $(DPL_SH_OBJS) $(LDFLAGS)
	
install:
	install -m755 -c $(PROGS) $(PREFIX)/bin
	cd $(PREFIX)/bin; \
	ln -sf dpl_box dpl_la; \
	ln -sf dpl_box dpl_ls; \
	ln -sf dpl_box dpl_mb; \
	ln -sf dpl_box dpl_get; \
	ln -sf dpl_box dpl_head; \
	ln -sf dpl_box dpl_put; \
	ln -sf dpl_box dpl_bput; \
	ln -sf dpl_box dpl_delete;

clean:
	$(RM) *.o $(PROGS) $(DPL_BOX_LINKS)

fproto:
	touch *.c
	$(DO_PROTO) $(INCS) *.h 
