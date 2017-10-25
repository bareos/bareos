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

LIBBAREOS_SRCS = address_conf.cc alist.cc attr.cc attribs.cc base64.cc \
		 berrno.cc bget_msg.cc binflate.cc bnet_server_tcp.cc bnet.cc \
		 bpipe.cc breg.cc bregex.cc bsnprintf.cc bsock.cc bsock_sctp.cc \
		 bsock_tcp.cc bsock_udt.cc bsys.cc btime.cc btimers.cc \
		 compression.cc connection_pool.cc cram-md5.cc cbuf.cc crypto.cc \
		 crypto_cache.cc crypto_gnutls.cc crypto_none.cc crypto_nss.cc \
		 crypto_openssl.cc crypto_wrap.cc daemon.cc devlock.cc dlist.cc \
		 edit.cc fnmatch.cc guid_to_name.cc hmac.cc htable.cc jcr.cc json.cc \
		 lockmgr.cc md5.cc mem_pool.cc message.cc mntent_cache.cc \
		 output_formatter.cc passphrase.cc path_list.cc plugins.cc poll.cc \
		 priv.cc queue.cc rblist.cc runscript.cc rwlock.cc scan.cc \
		 scsi_crypto.cc scsi_lli.cc sellist.cc serial.cc sha1.cc signal.cc \
		 smartall.cc tls_gnutls.cc tls_none.cc tls_nss.cc tls_openssl.cc \
		 tree.cc util.cc var.cc watchdog.cc workq.cc
LIBBAREOS_OBJS = $(LIBBAREOS_SRCS:.cc=.o)

LIBBAREOSCFG_SRCS = ini.cc lex.cc parse_bsr.cc
LIBBAREOSCFG_OBJS = $(LIBBAREOSCFG_SRCS:.cc=.o)

COMPAT_SRCS = compat.cc glob.cc print.cc winapi.cc
COMPAT_OBJS = $(COMPAT_SRCS:.cc=.o)

DYNAMIC_OBJS = $(LIBBAREOS_OBJS) $(LIBBAREOSCFG_OBJS) $(COMPAT_OBJS)

STATIC_SRCS = parse_conf.cc res.cc
STATIC_OBJS = $(STATIC_SRCS:.cc=.o)

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
.cc.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<

btime.o: DEFINES += -D_POSIX_C_SOURCE=200112
btime.o: btime.cc
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<
