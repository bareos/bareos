#
# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2014 Bareos GmbH & Co. KG
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
VPATH = .:../../filed:../generic

include ../Makefile.inc

DEFINES += -DWIN32_VSS
ifeq ($(WIN_VERSION),64)
DEFINES += -DHAVE_VSS64
endif

INCLUDES = -I. \
	   -I../.. \
	   -I../../include \
	   -I../../lmdb \
	   -I../../filed \
	   -I../include \
	   -I../compat/include \
	   -I../vss/include

LDLIBS = ../lib/libbareos.a \
	 ../lib/libbareos.dll \
	 ../findlib/libbareosfind.dll \
	 ../lmdb/libbareoslmdb.dll \
         $(MINGW_LIB)/libz.dll.a \
         $(MINGW_LIB)/liblzo2.dll.a \
         $(MINGW_LIB)/libfastlz.dll.a \
         $(MINGW_LIB)/libwinpthread.a \
         $(MINGW_LIB)/libjansson.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid -lcomctl32

SVRSRCS = accurate.c accurate_htable.c accurate_lmdb.c authenticate.c \
	  backup.c compression.c crypto.c dir_cmd.c estimate.c \
	  fd_plugins.c filed_conf.c filed.c fileset.c heartbeat.c \
	  restore.c sd_cmds.c socket_server.c status.c verify.c verify_vol.c \
	  vss.c vss_XP.c vss_W2K3.c vss_Vista.c service.c main.c

SVROBJS = $(SVRSRCS:.c=.o)

WINDRESSRCS = filedres.rc
WINDRESOBJS = $(WINDRESSRCS:.rc=.o)

all: bareos-fd.exe

bareos-fd.exe: DLL_USAGE = -DUSING_DLL
bareos-fd.exe: $(SVROBJS) $(WINDRESOBJS)
	$(CXX) $(LDFLAGS_WINAPP) $(SVROBJS) $(WINDRESOBJS) $(LDLIBS) -o $@

clean:
	rm -f *.o

distclean:: clean
	rm -f bareos-fd.exe

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<

%.o : %.rc
	@echo "Ressource Compiling $^"
	$(WINDRES) $^ -o $@

main.o: ../generic/main.c
	@echo "Compiling $<"
	$(CXX) -DFILE_DAEMON $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<
