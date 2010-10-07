
PREFIX = /usr/local

SRCDIR = src
INCDIR = include
OBJDIR = objs
BINDIR = bin
LIBDIR = lib

AR = ar cq
RANLIB = ranlib

INCS = -I$(INCDIR) -I/usr/include/libxml2

CFLAGSPROD = $(INCS) -g -O2 -Wall -Wunused -Werror -Wuninitialized -D_REENTRANT -fPIC $(EXTRA_CFLAGS)
CFLAGSDEBUG = $(INCS) -g -Wall -Wunused -Werror -D_REENTRANT -fPIC -DDROPLET_DEBUG $(EXTRA_CFLAGS)
CFLAGSVALGRIND = $(INCS) -g -O2 -fno-inline -Wall -Wunused -Werror -D_REENTRANT -fPIC $(EXTRA_CFLAGS)

CC  = gcc $(CFLAGS)
DO_PROTO = ../build-tools/do_proto

LIBDROPLET=$(LIBDIR)/libdroplet.a

PLATFORM ?= $(shell uname -o)
ifeq ($(PLATFORM),Solaris)
EXTRA_OBJS := 
endif

OBJS = \
$(OBJDIR)/droplet_conn.o \
$(OBJDIR)/droplet_vec.o \
$(OBJDIR)/droplet_dict.o \
$(OBJDIR)/droplet_converters.o \
$(OBJDIR)/droplet_reqbuilder.o \
$(OBJDIR)/droplet_httpreply.o \
$(OBJDIR)/droplet_replyparser.o \
$(OBJDIR)/droplet_utils.o \
$(OBJDIR)/droplet_conven.o \
$(OBJDIR)/droplet_profile.o \
$(OBJDIR)/droplet_pricing.o \
$(OBJDIR)/droplet_vdir.o \
$(OBJDIR)/droplet_vfile.o \
$(OBJDIR)/droplet.o

all: prod

prod: CFLAGS=$(CFLAGSPROD)
prod: compile

prod32: CFLAGS=$(CFLAGSPROD) -m32
prod32: compile

debug: CFLAGS=$(CFLAGSDEBUG)
debug: compile

valgrind: CFLAGS=$(CFLAGSVALGRIND)
valgrind: compile

compile: $(LIBDROPLET)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -o $@ -c $<

$(LIBDROPLET): $(OBJS) $(EXTRA_OBJS) 
	rm -f $(LIBDROPLET)
	$(AR) $(LIBDROPLET) $(OBJS) $(EXTRA_OBJS)
	$(RANLIB) $(LIBDROPLET)

install:
	mkdir -p $(PREFIX)/include/droplet
	install -c $(INCDIR)/* $(PREFIX)/include/droplet
	install -c $(LIBDROPLET) $(PREFIX)/lib

clean:
	rm -f $(OBJS) $(EXTRA_OBJS) $(LIBDROPLET)

fproto:
	touch $(SRCDIR)/*.c
	$(DO_PROTO) $(INCS) -P$(SRCDIR) $(INCDIR)/*.h 
