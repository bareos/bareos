
PREFIX = /usr/local

RM = rm -f

INCS = -I../libdroplets/include

PLATFORM ?= $(shell uname -o)
ifeq ($(PLATFORM),Solaris)
EXTRA_LIBS = -lsocket -lnsl
endif

LIBS = -L../libdroplets/lib -ldroplets -lm -lz -lssl -lcrypto -lpthread $(EXTRA_LIBS)
DEPENDS = ../libdroplets/lib/libdroplets.a

DPLTOOLSDIR=../tools

CFLAGSPROD = $(INCS) -O2 -Wall -Wunused -Werror -Wuninitialized -D_REENTRANT -fPIC
CFLAGSDEBUG = $(INCS) -g -Wall -Wunused -Werror -D_REENTRANT -fPIC -DASDK_DEBUG
CFLAGSVALGRIND = $(INCS) -O2 -fno-inline -Wall -Wunused -Werror -D_REENTRANT -fPIC

LDFLAGSPROD = $(LIBS)
LDFLAGSDEBUG = $(LIBS)
LDFLAGVALGRIND = $(LIBS)

# not used by prod builds
CFLAGS=$(CFLAGSDEBUG)
LDFLAGS=$(LDFLAGSDEBUG)

TESTS = \
dpl_perf_test

DPL_PERF_TEST_OBJS = dpl_perf_test.o

COMMON_OBJS = gendata.o

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

compile: $(TESTS)

dpl_perf_test: $(DPL_PERF_TEST_OBJS) $(COMMON_OBJS) $(DEPENDS)
	$(CC) -o dpl_perf_test $(DPL_PERF_TEST_OBJS) $(COMMON_OBJS) $(LDFLAGS)

VALGRIND = valgrind --leak-check=full --suppressions=valgrind.supp
DPLTESTBUCKET1=dropletstest01
DPLTESTBUCKET2=dropletstest02

test: $(DPLTOOLSDIR)/dpl_la $(DPLTOOLSDIR)/dpl_ls $(DPLTOOLSDIR)/dpl_mb $(DPLTOOLSDIR)/dpl_put $(DPLTOOLSDIR)/dpl_get $(DPLTOOLSDIR)/dpl_delete
	@echo make bucket
	$(VALGRIND) $(DPLTOOLSDIR)/dpl_mb $(DPLTESTBUCKET1)
	@echo put file
	dd if=/dev/urandom of=dpltest.tmp bs=1M count=1
	$(VALGRIND) $(DPLTOOLSDIR)/dpl_put dpltest.tmp $(DPLTESTBUCKET1): -mfoo=bar
	@echo get file
	$(VALGRIND) $(DPLTOOLSDIR)/dpl_get $(DPLTESTBUCKET1):dpltest.tmp > dpltest.tmp2
	diff dpltest.tmp dpltest.tmp2
	@echo list bucket
	$(VALGRIND) $(DPLTOOLSDIR)/dpl_delete -i $(DPLTESTBUCKET1):dpltest.tmp
	@echo list bucket
	$(VALGRIND) $(DPLTOOLSDIR)/dpl_ls $(DPLTESTBUCKET1)
	@echo remove bucket
	$(VALGRIND) $(DPLTOOLSDIR)/dpl_delete -i $(DPLTESTBUCKET1)

test2: $(DPLTOOLSDIR)/dpl_la $(DPLTOOLSDIR)/dpl_ls $(DPLTOOLSDIR)/dpl_mb $(DPLTOOLSDIR)/dpl_put $(DPLTOOLSDIR)/dpl_get $(DPLTOOLSDIR)/dpl_delete
	@echo put file encrypted
	dd if=/dev/urandom of=dpltest.tmp bs=1M count=1
	$(DPLTOOLSDIR)/dpl_bput -C dpltest.tmp $(DPLTESTBUCKET2): -mfoo=bar

test4: dpl_perf_test $(DPLTOOLSDIR)/dpl_mb $(DPLTOOLSDIR)/dpl_delete
	@echo make bucket
	./dpl_perf_test -N30 -n100 -b1000000 -B dropletstest04

install:

clean:
	$(RM) *.o $(TESTS) dpltest.tmp dpltest.tmp2
