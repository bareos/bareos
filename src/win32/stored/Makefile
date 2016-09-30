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

DEVICE_API_SRCS = generic_tape_device.c win32_tape_device.c \
		  win32_fifo_device.c win32_file_device.c

# objects used in all daemons collected in (shared) library.
LIBBAREOSSD_SRCS = acquire.c ansi_label.c askdir.c autochanger.c block.c \
		   bsr.c butil.c crc32.c dev.c device.c ebcdic.c label.c \
		   lock.c mount.c read_record.c record.c reserve.c scan.c \
		   sd_backends.c sd_plugins.c sd_stats.c spool.c \
		   stored_conf.c vol_mgr.c wait.c $(DEVICE_API_SRCS)
LIBBAREOSSD_OBJS = $(LIBBAREOSSD_SRCS:.c=.o)

DYNAMIC_OBJS = $(LIBBAREOSSD_OBJS)

SVRSRCS = append.c askdir.c authenticate.c dir_cmd.c fd_cmds.c job.c mac.c \
	  ndmp_tape.c read.c sd_cmds.c sd_stats.c socket_server.c status.c stored.c \
	  service.c main.c
SVROBJS = $(SVRSRCS:.c=.o)

TAPESRCS = btape.c
TAPEOBJS = $(TAPESRCS:.c=.o)

BLSSRCS = bls.c
BLSOBJS = $(BLSSRCS:.c=.o)

BEXTSRCS = bextract.c
BEXTOBJS = $(BEXTSRCS:.c=.o)

BSCANSRCS = bscan.c
BSCANOBJS = $(BSCANSRCS:.c=.o)

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
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<

%.o : %.rc
	@echo "Ressource Compiling $^"
	$(WINDRES) $^ -o $@
