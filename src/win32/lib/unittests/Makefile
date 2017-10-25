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
VPATH = .:../../../../src/lib/unittests/

include ../../Makefile.inc

INCLUDES = -I../../.. \
	        -I../../../include \
	        -I../../include \
           -I../../compat/include \
	        -I../../vss/include\
			  -I../include
LDLIBS = ../../lib/libbareos.a \
	      ../../lib/libbareos.dll \
         $(MINGW_LIB)/libwinpthread.a \
         $(MINGW_LIB)/libz.dll.a \
         $(MINGW_LIB)/libreadline.dll.a \
         $(MINGW_LIB)/libcmocka.dll.a
	      #$(WINSOCKLIB) -lole32 -loleaut32 -luuid


TEST_LIBSRCS = alist_test.cc  bsnprintf_test.cc edit_test.cc htable_test.cc passphrase_test.cc rblist_test.cc dlist_test.cc test_lib.cc sellist_test.cc scan_test.cc base64_test.cc devlock_test.cc rwlock_test.cc junction_test.cc
TEST_LIBOBJS = $(TEST_LIBSRCS:.cc=.o)

#WINDRESSRCS = consoleres.rc
#WINDRESOBJS = $(WINDRESSRCS:.rc=.o)

all: test_lib.exe

test_lib.exe: DLL_USAGE = -DUSING_DLL
test_lib.exe: $(TEST_LIBOBJS) $(WINDRESOBJS)
	$(CXX) $(TEST_LIBOBJS)  $(LDLIBS) -o $@

#test_lib.exe: $(TEST_LIBOBJS) $(WINDRESOBJS)
#	$(CXX) $(LDFLAGS_TEST_LIB) $(TEST_LIBOBJS) -mconsole -mwindows $(LDLIBS) $(WINDRESOBJS) -o $@

clean:
	rm -f *.o *.exe

distclean:: clean
	rm -f test_lib.exe

# inference rules
.cc.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<

%.o : %.rc
	@echo "Ressource Compiling $^"
	$(WINDRES) $^ -o $@

