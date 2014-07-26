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
         $(MINGW_LIB)/libpthreadGCE2.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid -lcomctl32

LDLIBS_FIND = ../lib/libbareos.a \
	      ../lib/libbareos.dll \
	      ../findlib/libbareosfind.dll \
	      $(MINGW_LIB)/libpthreadGCE2.a \
	      $(WINSOCKLIB) -lole32 -loleaut32 -luuid -lcomctl32

DEVICE_API_SRCS = generic_tape_device.c win32_tape_device.c win32_file_device.c

SVRSRCS = acquire.c ansi_label.c append.c askdir.c authenticate.c \
	  autochanger.c block.c bsr.c butil.c crc32.c dev.c device.c \
	  dir_cmd.c ebcdic.c fd_cmds.c job.c label.c lock.c mac.c mount.c \
	  ndmp_tape.c read_record.c read.c record.c reserve.c scan.c sd_cmds.c \
	  sd_plugins.c sd_stats.c spool.c status.c stored_conf.c stored.c \
          vol_mgr.c wait.c service.c main.c $(DEVICE_API_SRCS)
SVROBJS = $(SVRSRCS:.c=.o)

TAPESRCS = acquire.c ansi_label.c autochanger.c block.c btape.c bsr.c \
	   butil.c crc32.c dev.c device.c dummy.c ebcdic.c label.c lock.c \
	   mount.c read_record.c record.c reserve.c scan.c sd_plugins.c \
	   spool.c stored_conf.c vol_mgr.c wait.c $(DEVICE_API_SRCS)
TAPEOBJS = $(TAPESRCS:.c=.o)

BLSSRCS = acquire.c ansi_label.c autochanger.c block.c bls.c bsr.c \
	  butil.c crc32.c dev.c device.c dummy.c ebcdic.c label.c lock.c \
	  mount.c read_record.c record.c reserve.c scan.c sd_plugins.c \
	  spool.c stored_conf.c vol_mgr.c wait.c $(DEVICE_API_SRCS)
BLSOBJS = $(BLSSRCS:.c=.o)

BEXTSRCS = acquire.c ansi_label.c autochanger.c bextract.c block.c bsr.c \
	   butil.c crc32.c dev.c device.c dummy.c ebcdic.c label.c lock.c \
	   mount.c read_record.c record.c reserve.c scan.c sd_plugins.c \
	   spool.c stored_conf.c vol_mgr.c wait.c $(DEVICE_API_SRCS)
BEXTOBJS = $(BEXTSRCS:.c=.o)

WINDRESSRCS = storedres.rc
WINDRESOBJS = $(WINDRESSRCS:.rc=.o)

all: bareos-sd.exe btape.exe bls.exe bextract.exe

bareos-sd.exe: DLL_USAGE = -DUSING_DLL
bareos-sd.exe: $(SVROBJS) $(WINDRESOBJS)
	$(CXX) $(LDFLAGS_WINAPP) $(SVROBJS) $(WINDRESOBJS) $(LDLIBS) -o $@

btape.exe: DLL_USAGE = -DUSING_DLL
btape.exe: $(TAPEOBJS) $(WINDRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(TAPEOBJS) $(WINDRESOBJS) $(LDLIBS) -o $@

bls.exe: DLL_USAGE = -DUSING_DLL
bls.exe: $(BLSOBJS) $(WINDRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(BLSOBJS) $(WINDRESOBJS) $(LDLIBS_FIND) -o $@

bextract.exe: DLL_USAGE = -DUSING_DLL
bextract.exe: $(BEXTOBJS) $(WINDRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(BEXTOBJS) $(WINDRESOBJS) $(LDLIBS_FIND) -o $@

clean:
	rm -f *.o

distclean:: clean
	rm -f bareos-sd.exe btape.exe bls.exe bextract.exe

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<

%.o : %.rc
	@echo "Ressource Compiling $^"
	$(WINDRES) $^ -o $@
