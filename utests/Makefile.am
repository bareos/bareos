bin_PROGRAMS = alltests

alltests_CFLAGS = -std=gnu99 -I$(top_srcdir)/libdroplet/include
alltests_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto -lcheck
alltests_SOURCES = taskpool_utest.c ntinydb_utest.c dbuf_utest.c sbuf_utest.c dict_utest.c vec_utest.c utest_main.c

