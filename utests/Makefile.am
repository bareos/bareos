check_PROGRAMS = alltests toyserver
TESTS = alltests

TESTENV_ARGS = --top-srcdir $(top_srcdir)
if COVERAGE
	TESTENV_ARGS += --coverage
endif
TESTS_ENVIRONMENT= sh $(srcdir)/tools/testenv.sh $(TESTENV_ARGS) --

AM_CPPFLAGS = -I$(top_srcdir)/libdroplet/include \
	$(LIBXML_CFLAGS) $(JSON_CFLAGS) $(OPENSSL_CFLAGS) $(PTHREAD_CFLAGS) \
	$(CHECK_CFLAGS) -I$(builddir)/ssldata -DSRCDIR='"$(srcdir)"'
AM_CFLAGS = -std=gnu99

alltests_LDFLAGS = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto $(CHECK_LIBS) -lrt -ldl
alltests_SOURCES = \
	tests/addrlist_utest.c \
	tests/droplet_utest.c \
	tests/getdate_utest.c \
	tests/taskpool_utest.c \
	tests/ntinydb_utest.c \
	tests/profile_utest.c \
	tests/dbuf_utest.c \
	tests/sbuf_utest.c \
	tests/dict_utest.c \
	tests/utest_utest.c \
	tests/util_utest.c \
	tests/vec_utest.c \
	tests/sproxyd_utest.c \
	tests/s3/auth_common_utest.c \
	tests/s3/auth_v2_utest.c \
	tests/s3/auth_v4_utest.c \
	utest_main.c \
	testutils.c testutils.h \
	toyctl.c toyctl.h

SSL_INCLUDE_FILES = \
	$(builddir)/ssldata/client-cert.c \
	$(builddir)/ssldata/client.c \
	$(builddir)/ssldata/demoCA/cacert.c

$(srcdir)/tests/profile_utest.c: $(SSL_INCLUDE_FILES)
.pem.c: 
	@[ ! -d $(@D) ] && mkdir -p $(@D) || true
	@sed -e 's|^|"|' -e 's|$$|\\n"|' < $< > $@
CLEANFILES = $(SSL_INCLUDE_FILES)

if COVERAGE
clean: clean-am
	nodefiles=`find $(SUBDIRS) -type f -name \*.gcno -print` ; test -z "$$nodefiles" || $(RM) $$nodefiles
	datafiles=`find $(SUBDIRS) -type f -name \*.gcno -print` ; test -z "$$datafiles" || $(RM) $$datafiles
endif

EXTRA_DIST = \
    $(srcdir)/tools/testenv.sh \
    $(srcdir)/tools/valgrind.supp \
    $(srcdir)/tools/ssldata

toyserver_LDADD = -lcrypto
toyserver_SOURCES = toyserver.c toyserver.h
