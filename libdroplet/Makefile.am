lib_LTLIBRARIES = libdroplet.la

AM_LDFLAGS = $(LIBXML_LIBS) $(OPENSSL_LIBS) $(JSON_LIBS) $(PTHREAD_CFLAGS) $(PTHREAD_LIBS) -version-info $(LIBDROPLET_SO_VERSION) -no-undefined
AM_CPPFLAGS = -I$(top_srcdir)/libdroplet/include $(LIBXML_CFLAGS) $(JSON_CFLAGS) $(OPENSSL_CFLAGS) $(PTHREAD_CFLAGS)
AM_CFLAGS = -Wall \
  # -Werror \
  # -Wno-error=deprecated-declarations \
  # -Wno-error=unused-variable \
  # -Wno-error=unused-function \
  # -g -ggdb3 -O0

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
	src/uks.c \
	src/gc.c \
	src/backend/s3/backend.c \
	src/backend/s3/replyparser.c \
	src/backend/s3/reqbuilder.c \
	src/backend/s3/auth/v2.c \
	src/backend/s3/auth/v4.c \
	src/backend/s3/backend/copy.c \
	src/backend/s3/backend/delete.c \
	src/backend/s3/backend/delete_all.c \
	src/backend/s3/backend/delete_bucket.c \
	src/backend/s3/backend/genurl.c \
	src/backend/s3/backend/get.c \
	src/backend/s3/backend/get_capabilities.c \
	src/backend/s3/backend/head.c \
	src/backend/s3/backend/head_raw.c \
	src/backend/s3/backend/list_all_my_buckets.c \
	src/backend/s3/backend/list_bucket.c \
	src/backend/s3/backend/list_bucket_attrs.c \
	src/backend/s3/backend/make_bucket.c \
	src/backend/s3/backend/put.c \
	src/backend/s3/backend/multipart.c \
	src/backend/s3/backend/stream_resume.c \
	src/backend/s3/backend/stream_get.c \
	src/backend/s3/backend/stream_put.c \
	src/backend/s3/backend/stream_flush.c \
	src/backend/cdmi/backend.c \
	src/backend/cdmi/replyparser.c \
	src/backend/cdmi/reqbuilder.c \
	src/backend/cdmi/crcmodel.c \
	src/backend/cdmi/object_id.c \
	src/backend/swift/backend.c \
	src/backend/swift/replyparser.c \
	src/backend/swift/reqbuilder.c \
	src/backend/srws/backend.c \
	src/backend/srws/replyparser.c \
	src/backend/srws/reqbuilder.c \
	src/backend/srws/key.c \
	src/backend/sproxyd/backend.c \
	src/backend/sproxyd/replyparser.c \
	src/backend/sproxyd/reqbuilder.c \
	src/backend/sproxyd/backend/copy_id.c \
	src/backend/sproxyd/backend/delete_all_id.c \
	src/backend/sproxyd/backend/delete_id.c \
	src/backend/sproxyd/backend/get_id.c \
	src/backend/sproxyd/backend/head_id.c \
	src/backend/sproxyd/backend/put_id.c \
	src/backend/sproxyd/key.c \
	src/backend/posix/backend.c \
	src/backend/posix/reqbuilder.c \
	src/backend/posix/replyparser.c

libdropletincludedir = $(includedir)
libdropletinclude_HEADERS = \
	include/droplet.h \
	include/dropletp.h

libdropletdropletincludedir = $(includedir)/droplet
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
	include/droplet/queue.h \
	include/droplet/json_adapter.h

libdropletdroplets3includedir = $(includedir)/droplet/s3
libdropletdroplets3include_HEADERS = \
	include/droplet/s3/backend.h \
	include/droplet/s3/replyparser.h \
	include/droplet/s3/reqbuilder.h \
	include/droplet/s3/s3.h

libdropletdropletcdmiincludedir = $(includedir)/droplet/cdmi
libdropletdropletcdmiinclude_HEADERS = \
	include/droplet/cdmi/backend.h \
	include/droplet/cdmi/replyparser.h \
	include/droplet/cdmi/reqbuilder.h \
	include/droplet/cdmi/object_id.h \
	include/droplet/cdmi/crcmodel.h \
	include/droplet/cdmi/cdmi.h

libdropletdropletswiftincludedir = $(includedir)/droplet/swift
libdropletdropletswiftinclude_HEADERS = \
	include/droplet/swift/backend.h \
	include/droplet/swift/replyparser.h \
	include/droplet/swift/reqbuilder.h \
	include/droplet/swift/swift.h

libdropletdropletsrwsincludedir = $(includedir)/droplet/srws
libdropletdropletsrwsinclude_HEADERS = \
	include/droplet/srws/backend.h \
	include/droplet/srws/replyparser.h \
	include/droplet/srws/reqbuilder.h \
	include/droplet/srws/srws.h \
	include/droplet/srws/key.h

libdropletdropletsproxydincludedir = $(includedir)/droplet/sproxyd
libdropletdropletsproxydinclude_HEADERS = \
	include/droplet/sproxyd/backend.h \
	include/droplet/sproxyd/replyparser.h \
	include/droplet/sproxyd/reqbuilder.h \
	include/droplet/sproxyd/sproxyd.h \
	include/droplet/sproxyd/key.h

libdropletdropletposixincludedir = $(includedir)/droplet/posix
libdropletdropletposixinclude_HEADERS = \
	include/droplet/posix/backend.h \
	include/droplet/posix/reqbuilder.h \
	include/droplet/posix/replyparser.h \
	include/droplet/posix/posix.h

libdropletdropletuksincludedir = $(includedir)/droplet/uks
libdropletdropletuksinclude_HEADERS = \
	include/droplet/uks/uks.h

libdropletdropletscalincludedir = $(includedir)/droplet/scal
libdropletdropletscalinclude_HEADERS = \
	include/droplet/scal/gc.h

if COVERAGE
clean: clean-am
	nodefiles=`find $(SUBDIRS) -type f -name \*.gcno -print` ; test -z "$$nodefiles" || $(RM) $$nodefiles
	datafiles=`find $(SUBDIRS) -type f -name \*.gcno -print` ; test -z "$$datafiles" || $(RM) $$datafiles
endif
