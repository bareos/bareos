
PREFIX = /usr/local

RM = rm -f

INCS = -I../libdroplet/include

PLATFORM ?= $(shell uname -o)
ifeq ($(PLATFORM),Solaris)
EXTRA_LIBS = -lsocket -lnsl
endif

LIBS = -L../libdroplet/lib -ldroplet -lm -lz -lssl -lcrypto -lpthread -lxml2 $(EXTRA_LIBS)
DEPENDS = ../libdroplet/lib/libdroplet.a

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
dpl_perf_test \
test_req

DPL_PERF_TEST_OBJS = dpl_perf_test.o

TEST_REQ_OBJS = test_req.o

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

test_req: $(TEST_REQ_OBJS) $(COMMON_OBJS) $(DEPENDS)
	$(CC) -o test_req $(TEST_REQ_OBJS) $(COMMON_OBJS) $(LDFLAGS)

VALGRIND = valgrind --leak-check=full
DPLTESTBUCKET1=droplettest01
DPLTESTBUCKET2=droplettest02

test: $(DPLTOOLSDIR)/dplsh
	$(RM) dpltest.tmp dpltest.tmp2
	@echo make bucket
	$(VALGRIND) $(DPLTOOLSDIR)/dplsh -e "mb $(DPLTESTBUCKET1);$(DPLTESTBUCKET1):"
	@echo put file
	dd if=/dev/urandom of=dpltest.tmp bs=1M count=1
	$(VALGRIND) $(DPLTOOLSDIR)/dplsh -e "put -mfoo=bar dpltest.tmp"
	@echo get file
	$(VALGRIND) $(DPLTOOLSDIR)/dplsh -e "get dpltest.tmp dpltest.tmp2"
	diff dpltest.tmp dpltest.tmp2
	@echo list bucket
	$(VALGRIND) $(DPLTOOLSDIR)/dplsh -e "rm dpltest.tmp"
	@echo remove bucket
	$(VALGRIND) $(DPLTOOLSDIR)/dplsh -e "rb $(DPLTESTBUCKET1)"

install:

clean:
	$(RM) *.o $(TESTS) dpltest.tmp dpltest.tmp2

fproto:
