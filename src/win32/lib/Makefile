#
# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2016 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation and included
# in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
VPATH = .:../../lib:../compat

include ../Makefile.inc

INCLUDES = -I../.. \
	   -I../../include \
	   -I../include \
	   -I../compat/include
LDLIBS = $(MINGW_LIB)/libssl.dll.a \
         $(MINGW_LIB)/libcrypto.dll.a \
         $(MINGW_LIB)/libwinpthread.a \
         $(MINGW_LIB)/libz.dll.a \
         $(MINGW_LIB)/liblzo2.dll.a \
         $(MINGW_LIB)/libfastlz.dll.a \
         $(MINGW_LIB)/libjansson.dll.a \
         $(WINSOCKLIB) -lole32 -loleaut32 -luuid

LIBBAREOS_SRCS = address_conf.c alist.c attr.c attribs.c base64.c \
		 berrno.c bget_msg.c binflate.c bnet_server_tcp.c bnet.c \
		 bpipe.c breg.c bregex.c bsnprintf.c bsock.c bsock_sctp.c \
		 bsock_tcp.c bsock_udt.c bsys.c btime.c btimers.c \
		 compression.c connection_pool.c cram-md5.c cbuf.c crypto.c \
		 crypto_cache.c crypto_gnutls.c crypto_none.c crypto_nss.c \
		 crypto_openssl.c crypto_wrap.c daemon.c devlock.c dlist.c \
		 edit.c fnmatch.c guid_to_name.c hmac.c htable.c jcr.c json.c \
		 lockmgr.c md5.c mem_pool.c message.c mntent_cache.c \
		 output_formatter.c passphrase.c path_list.c plugins.c poll.c \
		 priv.c queue.c rblist.c runscript.c rwlock.c scan.c \
		 scsi_crypto.c scsi_lli.c sellist.c serial.c sha1.c signal.c \
		 smartall.c tls_gnutls.c tls_none.c tls_nss.c tls_openssl.c \
		 tree.c util.c var.c watchdog.c workq.c
LIBBAREOS_OBJS = $(LIBBAREOS_SRCS:.c=.o)

LIBBAREOSCFG_SRCS = ini.c lex.c parse_bsr.c
LIBBAREOSCFG_OBJS = $(LIBBAREOSCFG_SRCS:.c=.o)

COMPAT_SRCS = compat.c glob.c print.c winapi.c
COMPAT_OBJS = $(COMPAT_SRCS:.c=.o)

DYNAMIC_OBJS = $(LIBBAREOS_OBJS) $(LIBBAREOSCFG_OBJS) $(COMPAT_OBJS)

STATIC_SRCS = parse_conf.c res.c
STATIC_OBJS = $(STATIC_SRCS:.c=.o)

all: libbareos.dll libbareos.a

bareos$(WIN_VERSION).def: $(DYNAMIC_OBJS) make_def
	./make_def $(WIN_VERSION) $(DYNAMIC_OBJS) > $@

libbareos.dll: DLL_USAGE = -DBUILDING_DLL
libbareos.dll: $(DYNAMIC_OBJS) \
	    bareos$(WIN_VERSION).def
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a $(DYNAMIC_OBJS) bareos$(WIN_VERSION).def $(LDLIBS) -o $@

libbareos.a: DLL_USAGE = -DUSING_DLL
libbareos.a: $(STATIC_OBJS)
	ar rsv $@ $(STATIC_OBJS)

clean:
	rm -f *.o bareos$(WIN_VERSION).def

distclean:: clean
	rm -f libbareos.dll libbareos.dll.a libbareos.a

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<

btime.o: DEFINES += -D_POSIX_C_SOURCE=200112
btime.o: btime.c
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<
