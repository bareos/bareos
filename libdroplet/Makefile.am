lib_LTLIBRARIES = libdroplet.la

libdroplet_la_LDFLAGS = $(LIBXML_LIBS) $(OPENSSL_LIBS) $(PTHREAD_CFLAGS) $(PTHREAD_LIBS) -version-info $(LIBDROPLET_SO_VERSION) -no-undefined $(JSON_LIBS)
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
	src/dbuf.c \
	src/ntinydb.c \
	src/utils.c \
	src/rest.c \
	src/sysmd.c \
	src/id_scheme.c \
	src/task.c \
	src/async.c \
	src/addrlist.c \
	src/getdate.y \
	src/vfs.c \
	s3/backend.c \
	s3/replyparser.c \
	s3/reqbuilder.c \
	cdmi/backend.c \
	cdmi/replyparser.c \
	cdmi/reqbuilder.c \
	cdmi/crcmodel.c \
	cdmi/object_id.c \
	swift/backend.c \
	swift/replyparser.c \
	swift/reqbuilder.c \
	srws/backend.c \
	srws/replyparser.c \
	srws/reqbuilder.c \
	srws/key.c \
	sproxyd/backend.c \
	sproxyd/replyparser.c \
	sproxyd/reqbuilder.c \
	sproxyd/key.c \
	posix/backend.c \
	posix/reqbuilder.c \
	posix/replyparser.c \
	uks/uks.c \
	scal/gc.c

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
	include/droplet/dbuf.h \
	include/droplet/ntinydb.h \
	include/droplet/utils.h \
	include/droplet/backend.h \
	include/droplet/rest.h \
	include/droplet/sysmd.h \
	include/droplet/id_scheme.h \
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

libdropletdropletswiftincludedir = $(includedir)/droplet-3.0/droplet/swift
libdropletdropletswiftinclude_HEADERS = \
	include/droplet/swift/backend.h \
	include/droplet/swift/replyparser.h \
	include/droplet/swift/reqbuilder.h \
	include/droplet/swift/swift.h

libdropletdropletsrwsincludedir = $(includedir)/droplet-3.0/droplet/srws
libdropletdropletsrwsinclude_HEADERS = \
	include/droplet/srws/backend.h \
	include/droplet/srws/replyparser.h \
	include/droplet/srws/reqbuilder.h \
	include/droplet/srws/srws.h \
	include/droplet/srws/key.h

libdropletdropletsproxydincludedir = $(includedir)/droplet-3.0/droplet/sproxyd
libdropletdropletsproxydinclude_HEADERS = \
	include/droplet/sproxyd/backend.h \
	include/droplet/sproxyd/replyparser.h \
	include/droplet/sproxyd/reqbuilder.h \
	include/droplet/sproxyd/sproxyd.h \
	include/droplet/sproxyd/key.h

libdropletdropletposixincludedir = $(includedir)/droplet-3.0/droplet/posix
libdropletdropletposixinclude_HEADERS = \
	include/droplet/posix/backend.h \
	include/droplet/posix/reqbuilder.h \
	include/droplet/posix/replyparser.h \
	include/droplet/posix/posix.h

libdropletdropletuksincludedir = $(includedir)/droplet-3.0/droplet/uks
libdropletdropletuksinclude_HEADERS = \
	include/droplet/uks/uks.h

libdropletdropletscalincludedir = $(includedir)/droplet-3.0/droplet/scal
libdropletdropletscalinclude_HEADERS = \
	include/droplet/scal/gc.h
