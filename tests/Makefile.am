noinst_PROGRAMS = dpl_perf_test test_req

INCLUDES = -I$(top_srcdir)/libdroplet/include
LIBS = $(top_builddir)/libdroplet/libdroplet.la
dpl_perf_test_LIBS = $(PTHREAD_LIBS)
dpl_perf_test_CFLAGS = $(PTHREAD_CFLAGS)
dpl_perf_test_SOURCES = gendata.c dpl_perf_test.c
test_req_SOURCES = gendata.c test_req.c

noinst_HEADERS = gendata.h

VALGRIND = valgrind --leak-check=full
DPLTESTBUCKET1=droplettest01
DPLTESTBUCKET2=droplettest02

test: $(top_builddir)/tools/dplsh
	$(RM) dpltest.tmp dpltest.tmp2
	@echo make bucket
	$(VALGRIND) $(top_builddir)/tools/dplsh -e "mb $(DPLTESTBUCKET1)"
	@echo put file
	dd if=/dev/urandom of=dpltest.tmp bs=1M count=1
	$(VALGRIND) $(top_builddir)/tools/dplsh -e "put -mfoo=bar dpltest.tmp $(DPLTESTBUCKET1):"
	@echo get file
	$(VALGRIND) $(top_builddir)/tools/dplsh -e "get $(DPLTESTBUCKET1):dpltest.tmp dpltest.tmp2"
	diff dpltest.tmp dpltest.tmp2
	@echo list bucket
	$(VALGRIND) $(top_builddir)/tools/dplsh -e "rm $(DPLTESTBUCKET1):dpltest.tmp"
	@echo remove bucket
	$(VALGRIND) $(top_builddir)/tools/dplsh -e "rb $(DPLTESTBUCKET1)"

