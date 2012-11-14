lib_LTLIBRARIES = libdroplet.la

libdroplet_la_LDFLAGS = $(LIBXML_LIBS) $(JSON_LIBS) $(OPENSSL_LIBS) $(PTHREAD_CFLAGS) $(PTHREAD_LIBS) -version-info $(LIBDROPLET_SO_VERSION) -no-undefined
libdroplet_la_CFLAGS = -I$(top_srcdir)/libdroplet/include $(LIBXML_CFLAGS) $(JSON_CFLAGS) $(OPENSSL_CFLAGS) $(PTHREAD_CFLAGS) #-Werror -Wall

libdroplet_la_SOURCES = \
	src/conn.c \
	src/converters.c \
	src/value.c \
	src/dict.c \
	src/droplet.c \
	src/httprequest.c \
	src/httpreply.c \
	src/pricing.c \
	src/profile.c \
	src/req.c \
	src/vec.c \
	src/sbuf.c \
	src/ntinydb.c \
	src/utils.c \
	src/rest.c \
	src/sysmd.c \
	src/task.c \
	src/async.c \
	src/addrlist.c \
	src/getdate.y \
	src/vfs.c \
	s3/backend.c \
	s3/replyparser.c \
	s3/reqbuilder.c \
	s3/s3.c \
	cdmi/backend.c \
	cdmi/replyparser.c \
	cdmi/reqbuilder.c \
	cdmi/crcmodel.c \
	cdmi/object_id.c \
	cdmi/cdmi.c \
	srws/backend.c \
	srws/replyparser.c \
	srws/reqbuilder.c \
	srws/srws.c \
	posix/posix.c \
	posix/backend.c

libdropletincludedir = $(includedir)/droplet-3.0/
libdropletinclude_HEADERS = \
	include/droplet.h \
	include/dropletp.h

libdropletdropletincludedir = $(includedir)/droplet-3.0/droplet
libdropletdropletinclude_HEADERS = \
	include/droplet/conn.h \
	include/droplet/converters.h \
	include/droplet/value.h \
	include/droplet/dict.h \
	include/droplet/httprequest.h \
	include/droplet/httpreply.h \
	include/droplet/pricing.h \
	include/droplet/profile.h \
	include/droplet/req.h \
	include/droplet/vec.h \
	include/droplet/sbuf.h \
	include/droplet/ntinydb.h \
	include/droplet/utils.h \
	include/droplet/backend.h \
	include/droplet/rest.h \
	include/droplet/sysmd.h \
	include/droplet/vfs.h \
	include/droplet/task.h \
	include/droplet/addrlist.h \
	include/droplet/queue.h

libdropletdroplets3includedir = $(includedir)/droplet-3.0/droplet/s3
libdropletdroplets3include_HEADERS = \
	include/droplet/s3/backend.h \
	include/droplet/s3/replyparser.h \
	include/droplet/s3/reqbuilder.h \
	include/droplet/s3/s3.h

libdropletdropletcdmiincludedir = $(includedir)/droplet-3.0/droplet/cdmi
libdropletdropletcdmiinclude_HEADERS = \
	include/droplet/cdmi/backend.h \
	include/droplet/cdmi/replyparser.h \
	include/droplet/cdmi/reqbuilder.h \
	include/droplet/cdmi/object_id.h \
	include/droplet/cdmi/crcmodel.h \
	include/droplet/cdmi/cdmi.h

libdropletdropletsrwsincludedir = $(includedir)/droplet-3.0/droplet/srws
libdropletdropletsrwsinclude_HEADERS = \
	include/droplet/srws/backend.h \
	include/droplet/srws/replyparser.h \
	include/droplet/srws/reqbuilder.h \
	include/droplet/srws/srws.h

libdropletdropletposixincludedir = $(includedir)/droplet-3.0/droplet/posix
libdropletdropletposixinclude_HEADERS = \
	include/droplet/posix/backend.h \
	include/droplet/posix/posix.h
