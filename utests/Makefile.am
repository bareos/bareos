check_PROGRAMS = alltests
TESTS = alltests

TESTENV_ARGS= --top-srcdir $(top_srcdir)
if COVERAGE
TESTENV_ARGS+= --coverage
endif
TESTS_ENVIRONMENT= sh $(srcdir)/testenv.sh $(TESTENV_ARGS) --

alltests_CPPFLAGS = -I$(top_srcdir)/libdroplet/include $(CHECK_CFLAGS)
alltests_CFLAGS = -std=gnu99
alltests_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto $(CHECK_LIBS) -lrt
alltests_SOURCES = \
	addrlist_utest.c \
	droplet_utest.c \
	getdate_utest.c \
	taskpool_utest.c \
	ntinydb_utest.c \
	profile_utest.c \
	dbuf_utest.c \
	sbuf_utest.c \
	dict_utest.c \
	utest_utest.c \
	util_utest.c \
	vec_utest.c \
	utest_main.c


SSL_INCLUDE_FILES =  ssldata/client-cert.c ssldata/client.c ssldata/demoCA/cacert.c
profile_utest.c: $(SSL_INCLUDE_FILES)
.pem.c:
	sed -e 's|^|"|' -e 's|$$|\\n"|' <$< >$@
CLEANFILES = $(SSL_INCLUDE_FILES)

if COVERAGE
clean: clean-am
	nodefiles=`find $(SUBDIRS) -type f -name \*.gcno -print` ; test -z "$$nodefiles" || $(RM) $$nodefiles
	datafiles=`find $(SUBDIRS) -type f -name \*.gcno -print` ; test -z "$$datafiles" || $(RM) $$datafiles
endif

EXTRA_DIST= \
    testenv.sh \
    valgrind.supp
