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
VPATH = .:../../tools

include ../Makefile.inc

INCLUDES = -I../.. \
	   -I../../include \
	   -I../include \
	   -I../compat/include \
	   -I../vss/include
LDLIBS = ../lib/libbareos.a \
	 ../lib/libbareos.dll \
         $(MINGW_LIB)/libwinpthread.a \
         $(MINGW_LIB)/libz.dll.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid

BSMTPSRCS = bsmtp.c
BSMTPOBJS = $(BSMTPSRCS:.c=.o)

BSMTPRESSRCS = bsmtpres.rc
BSMTPRESOBJS = $(BSMTPRESSRCS:.rc=.o)

BREGEXSRCS = bregex.c
BREGEXOBJS = $(BREGEXSRCS:.c=.o)

BREGEXRESSRCS = bregexres.rc
BREGEXRESOBJS = $(BREGEXRESSRCS:.rc=.o)

BWILDSRCS = bwild.c
BWILDOBJS = $(BWILDSRCS:.c=.o)

BWILDRESSRCS = bwildres.rc
BWILDRESOBJS = $(BWILDRESSRCS:.rc=.o)

all: bsmtp.exe bregex.exe bwild.exe

bsmtp.exe: DLL_USAGE = -DUSING_DLL
bsmtp.exe: $(BSMTPOBJS) $(BSMTPRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(BSMTPOBJS) $(LDLIBS) $(BSMTPRESOBJS) -o $@

bregex.exe: DLL_USAGE = -DUSING_DLL
bregex.exe: $(BREGEXOBJS) $(BREGEXRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(BREGEXOBJS) $(LDLIBS) $(BREGEXRESOBJS) -o $@

bwild.exe: DLL_USAGE = -DUSING_DLL
bwild.exe: $(BWILDOBJS) $(BWILDRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(BWILDOBJS) $(LDLIBS) $(BWILDRESOBJS) -o $@

clean:
	rm -f *.o

distclean:: clean
	rm -f bsmtp.exe bregex.exe bwild.exe

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<

%.o : %.rc
	@echo "Ressource Compiling $^"
	$(WINDRES) $^ -o $@
