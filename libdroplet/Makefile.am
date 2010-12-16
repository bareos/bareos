lib_LTLIBRARIES = libdroplet.la

libdroplet_la_LDFLAGS = $(LIBXML_LIBS) $(OPENSSL_LIBS) $(PTHREAD_LIBS) -version-info $(LIBDROPLET_SO_VERSION) -no-undefined
libdroplet_la_CFLAGS = -I$(top_srcdir)/libdroplet/include $(LIBXML_CFLAGS) $(OPENSSL_CFLAGS) $(PTHREAD_CFLAGS)

libdroplet_la_SOURCES = \
	src/conn.c \
	src/convapi.c \
	src/converters.c \
	src/dict.c \
	src/droplet.c \
	src/httpreply.c \
	src/pricing.c \
	src/profile.c \
	src/replyparser.c \
	src/reqbuilder.c \
	src/utils.c \
	src/vdir.c \
	src/vec.c \
	src/vfile.c

libdropletincludedir = $(includedir)/droplet-1.0/
libdropletinclude_HEADERS = \
	include/droplet.h \
	include/dropletp.h

libdropletdropletincludedir = $(includedir)/droplet-1.0/droplet
libdropletdropletinclude_HEADERS = \
	include/droplet/conn.h \
	include/droplet/convapi.h \
	include/droplet/converters.h \
	include/droplet/dict.h \
	include/droplet/httpreply.h \
	include/droplet/pricing.h \
	include/droplet/profile.h \
	include/droplet/replyparser.h \
	include/droplet/reqbuilder.h \
	include/droplet/utils.h \
	include/droplet/vdir.h \
	include/droplet/vec.h \
	include/droplet/vfile.h



