check_PROGRAMS = alltests
TESTS = alltests

alltests_CPPFLAGS = -I$(top_srcdir)/libdroplet/include $(CHECK_CFLAGS)
alltests_CFLAGS = -std=gnu99
alltests_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto $(CHECK_LIBS) -lrt
alltests_SOURCES = taskpool_utest.c ntinydb_utest.c dbuf_utest.c sbuf_utest.c dict_utest.c vec_utest.c utest_main.c

