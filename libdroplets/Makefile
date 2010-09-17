
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
CFLAGSDEBUG = $(INCS) -g -Wall -Wunused -Werror -D_REENTRANT -fPIC -DDROPLETS_DEBUG $(EXTRA_CFLAGS)
CFLAGSVALGRIND = $(INCS) -g -O2 -fno-inline -Wall -Wunused -Werror -D_REENTRANT -fPIC $(EXTRA_CFLAGS)

CC  = gcc $(CFLAGS)
DO_PROTO = ../build-tools/do_proto

LIBDROPLETS=$(LIBDIR)/libdroplets.a

PLATFORM ?= $(shell uname -o)
ifeq ($(PLATFORM),Solaris)
EXTRA_OBJS := 
endif

OBJS = \
$(OBJDIR)/droplets_conn.o \
$(OBJDIR)/droplets_vec.o \
$(OBJDIR)/droplets_dict.o \
$(OBJDIR)/droplets_httpreq.o \
$(OBJDIR)/droplets_s3req.o \
$(OBJDIR)/droplets_httpreply.o \
$(OBJDIR)/droplets_utils.o \
$(OBJDIR)/droplets_req_listallmybuckets.o \
$(OBJDIR)/droplets_req_listbucket.o \
$(OBJDIR)/droplets_req_makebucket.o \
$(OBJDIR)/droplets_req_put.o \
$(OBJDIR)/droplets_req_get.o \
$(OBJDIR)/droplets_req_head.o \
$(OBJDIR)/droplets_req_delete.o \
$(OBJDIR)/droplets_profile.o \
$(OBJDIR)/droplets_pricing.o \
$(OBJDIR)/droplets_vdir.o \
$(OBJDIR)/droplets_vfile.o \
$(OBJDIR)/droplets.o

all: prod

prod: CFLAGS=$(CFLAGSPROD)
prod: compile

prod32: CFLAGS=$(CFLAGSPROD) -m32
prod32: compile

debug: CFLAGS=$(CFLAGSDEBUG)
debug: compile

valgrind: CFLAGS=$(CFLAGSVALGRIND)
valgrind: compile

compile: $(LIBDROPLETS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -o $@ -c $<

$(LIBDROPLETS): $(OBJS) $(EXTRA_OBJS) 
	rm -f $(LIBDROPLETS)
	$(AR) $(LIBDROPLETS) $(OBJS) $(EXTRA_OBJS)
	$(RANLIB) $(LIBDROPLETS)

install:
	mkdir -p $(PREFIX)/include/droplets
	install -c $(INCDIR)/* $(PREFIX)/include/droplets
	install -c $(LIBDROPLETS) $(PREFIX)/lib

clean:
	rm -f $(OBJS) $(EXTRA_OBJS) $(LIBDROPLETS)

fproto:
	touch $(SRCDIR)/*.c
	$(DO_PROTO) $(INCS) -P$(SRCDIR) $(INCDIR)/*.h 
