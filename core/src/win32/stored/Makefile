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
VPATH = .:../../stored:../../stored/backends:../generic:backends

include ../Makefile.inc

INCLUDES = -I. \
	   -I../.. \
	   -I../../include \
	   -I../../stored \
	   -I../../stored/backends \
	   -I../include \
	   -I../compat/include
LDLIBS = ../lib/libbareos.a \
	 ../lib/libbareos.dll \
         $(MINGW_LIB)/libwinpthread.a \
         $(MINGW_LIB)/libjansson.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid -lcomctl32

LDLIBS_FIND = ../lib/libbareos.a \
	      ../lib/libbareos.dll \
	      ../findlib/libbareosfind.dll \
	      $(MINGW_LIB)/libwinpthread.a \
	      $(MINGW_LIB)/libjansson.a \
	      $(WINSOCKLIB) -lole32 -loleaut32 -luuid -lcomctl32

DEVICE_API_SRCS = generic_tape_device.cc win32_tape_device.cc \
		  win32_fifo_device.cc win32_file_device.cc

# objects used in all daemons collected in (shared) library.
LIBBAREOSSD_SRCS = acquire.cc ansi_label.cc askdir.cc autochanger.cc block.cc \
		   bsr.cc butil.cc crc32.cc dev.cc device.cc ebcdic.cc label.cc \
		   lock.cc mount.cc read_record.cc record.cc reserve.cc scan.cc \
		   sd_backends.cc sd_plugins.cc sd_stats.cc spool.cc \
		   stored_conf.cc vol_mgr.cc wait.cc $(DEVICE_API_SRCS)
LIBBAREOSSD_OBJS = $(LIBBAREOSSD_SRCS:.cc=.o)

DYNAMIC_OBJS = $(LIBBAREOSSD_OBJS)

SVRSRCS = append.cc askdir.cc authenticate.cc dir_cmd.cc fd_cmds.cc job.cc mac.cc \
	  ndmp_tape.cc read.cc sd_cmds.cc sd_stats.cc socket_server.cc status.cc stored.cc \
	  service.cc main.cc
SVROBJS = $(SVRSRCS:.cc=.o)

TAPESRCS = btape.cc
TAPEOBJS = $(TAPESRCS:.cc=.o)

BLSSRCS = bls.cc
BLSOBJS = $(BLSSRCS:.cc=.o)

BEXTSRCS = bextract.cc
BEXTOBJS = $(BEXTSRCS:.cc=.o)

BSCANSRCS = bscan.cc
BSCANOBJS = $(BSCANSRCS:.cc=.o)

WINDRESSRCS = storedres.rc
WINDRESOBJS = $(WINDRESSRCS:.rc=.o)

BSCANRESSRCS = bscanres.rc
BSCANRESOBJS = $(BSCANRESSRCS:.rc=.o)

BLSRESSRCS = blsres.rc
BLSRESOBJS = $(BLSRESSRCS:.rc=.o)

BTAPERESSRCS = btaperes.rc
BTAPERESOBJS = $(BTAPERESSRCS:.rc=.o)

BEXTRACTRESSRCS = bextractres.rc
BEXTRACTRESOBJS = $(BEXTRACTRESSRCS:.rc=.o)

all: libbareossd.dll bareos-sd.exe btape.exe bls.exe bextract.exe bscan.exe

bareos$(WIN_VERSION).def: $(DYNAMIC_OBJS) make_def
	./make_def $(WIN_VERSION) $(DYNAMIC_OBJS) > $@

libbareossd.dll: DLL_USAGE = -DBUILDING_DLL
libbareossd.dll: $(DYNAMIC_OBJS) \
		bareos$(WIN_VERSION).def
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a $(DYNAMIC_OBJS) bareos$(WIN_VERSION).def $(LDLIBS) -o $@

bareos-sd.exe: DLL_USAGE = -DUSING_DLL
bareos-sd.exe: libbareossd.dll $(SVROBJS) $(WINDRESOBJS)
	$(CXX) $(LDFLAGS_WINAPP) $(SVROBJS) $(WINDRESOBJS) $(LDLIBS) ./libbareossd.dll -o $@

btape.exe: DLL_USAGE = -DUSING_DLL
btape.exe: libbareossd.dll $(TAPEOBJS) $(BTAPERESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(TAPEOBJS) $(BTAPERESOBJS) $(LDLIBS) ./libbareossd.dll -o $@

bls.exe: DLL_USAGE = -DUSING_DLL
bls.exe: libbareossd.dll $(BLSOBJS) $(BLSRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(BLSOBJS) $(BLSRESOBJS) $(LDLIBS_FIND) ./libbareossd.dll -o $@

bextract.exe: DLL_USAGE = -DUSING_DLL
bextract.exe: libbareossd.dll $(BEXTOBJS) $(BEXTRACTRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(BEXTOBJS) $(BEXTRACTRESOBJS) $(LDLIBS_FIND) ./libbareossd.dll -o $@

bscan.exe: DLL_USAGE = -DUSING_DLL
bscan.exe: libbareossd.dll $(BSCANOBJS) $(BSCANRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(BSCANOBJS) $(BSCANRESOBJS) $(LDLIBS_FIND) ../cats/libbareoscats.dll  ./libbareossd.dll -o $@

clean:
	rm -f *.o bareos$(WIN_VERSION).def

distclean:: clean
	rm -f libbareossd.dll libbareossd.dll.a bareos-sd.exe btape.exe bls.exe bextract.exe

# inference rules
.cc.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<

%.o : %.rc
	@echo "Ressource Compiling $^"
	$(WINDRES) $^ -o $@
