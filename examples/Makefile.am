noinst_PROGRAMS = recurse idtest idrangetest restrangetest srwskey resttest resttest_async timo idgetnoalloc idtestbuffered3 ukstest

recurse_CFLAGS = -I$(top_srcdir)/libdroplet/include
recurse_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto

recurse_SOURCES = recurse.c

idtest_CFLAGS = -I$(top_srcdir)/libdroplet/include
idtest_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto
idtest_SOURCES = idtest.c

idrangetest_CFLAGS = -I$(top_srcdir)/libdroplet/include
idrangetest_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto
idrangetest_SOURCES = idrangetest.c

restrangetest_CFLAGS = -I$(top_srcdir)/libdroplet/include
restrangetest_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto
restrangetest_SOURCES = restrangetest.c

srwskey_CFLAGS = -I$(top_srcdir)/libdroplet/include
srwskey_LDADD = $(top_builddir)/libdroplet/libdroplet.la -lcrypto $(JSON_LIBS)
srwskey_SOURCES = srwskey.c

resttest_CFLAGS = -I$(top_srcdir)/libdroplet/include
resttest_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto
resttest_SOURCES = resttest.c

resttest_async_CFLAGS = -I$(top_srcdir)/libdroplet/include
resttest_async_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto
resttest_async_SOURCES = resttest_async.c

timo_CFLAGS = -I$(top_srcdir)/libdroplet/include
timo_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto
timo_SOURCES = timo.c

idgetnoalloc_CFLAGS = -I$(top_srcdir)/libdroplet/include
idgetnoalloc_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto
idgetnoalloc_SOURCES = idgetnoalloc.c

idtestbuffered3_CFLAGS = -I$(top_srcdir)/libdroplet/include
idtestbuffered3_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto
idtestbuffered3_SOURCES = idtestbuffered3.c

ukstest_CFLAGS = -I$(top_srcdir)/libdroplet/include
ukstest_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(JSON_LIBS) -lcrypto
ukstest_SOURCES = ukstest.c
