lib_LTLIBRARIES = libdroplet.la

libdroplet_la_LDFLAGS = $(LIBXML_LIBS) $(JSON_LIBS) $(OPENSSL_LIBS) $(PTHREAD_CFLAGS) $(PTHREAD_LIBS) -version-info $(LIBDROPLET_SO_VERSION) -no-undefined
libdroplet_la_CFLAGS = -I$(top_srcdir)/libdroplet/include $(LIBXML_CFLAGS) $(JSON_CFLAGS) $(OPENSSL_CFLAGS) $(PTHREAD_CFLAGS) #-Werror -Wall

libdroplet_la_SOURCES = \
	src/conn.c \
	src/converters.c \
	src/dict.c \
	src/droplet.c \
	src/httprequest.c \
	src/httpreply.c \
	src/pricing.c \
	src/profile.c \
	src/req.c \
	src/vec.c \
	src/utils.c \
	src/backend.c \
	src/vdir.c \
	src/vfile.c \
	s3/convapi.c \
	s3/replyparser.c \
	s3/reqbuilder.c \
	s3/s3.c \
	cdmi/convapi.c \
	cdmi/replyparser.c \
	cdmi/reqbuilder.c \
	cdmi/cdmi.c

libdropletincludedir = $(includedir)/droplet-1.0/
libdropletinclude_HEADERS = \
	include/droplet.h \
	include/dropletp.h

libdropletdropletincludedir = $(includedir)/droplet-1.0/droplet
libdropletdropletinclude_HEADERS = \
	include/droplet/conn.h \
	include/droplet/converters.h \
	include/droplet/dict.h \
	include/droplet/httprequest.h \
	include/droplet/httpreply.h \
	include/droplet/pricing.h \
	include/droplet/profile.h \
	include/droplet/req.h \
	include/droplet/vec.h \
	include/droplet/utils.h \
	include/droplet/backend.h \
	include/droplet/vdir.h \
	include/droplet/vfile.h

libdropletdroplets3includedir = $(includedir)/droplet-1.0/droplet/s3
libdropletdroplets3include_HEADERS = \
	include/droplet/s3/convapi.h \
	include/droplet/s3/replyparser.h \
	include/droplet/s3/reqbuilder.h \
	include/droplet/s3/s3.h

libdropletdropletcdmiincludedir = $(includedir)/droplet-1.0/droplet/cdmi
libdropletdropletcdmiinclude_HEADERS = \
	include/droplet/cdmi/convapi.h \
	include/droplet/cdmi/replyparser.h \
	include/droplet/cdmi/reqbuilder.h \
	include/droplet/cdmi/cdmi.h

