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
VPATH = .:../../../plugins/stored

include ../../Makefile.inc

ifeq ($(WIN_VERSION),32)
INCLUDES = -I../../.. \
	   -I../../../include \
	   -I../../../stored \
	   -I../../../plugins/stored \
	   -I../../include \
	   -I../../compat/include
else
INCLUDES = -I../../.. \
	   -I../../../include \
	   -I../../../stored \
	   -I../../../plugins/stored \
	   -I../../include \
	   -I../../compat/include
endif
LDLIBS = ../../lib/libbareos.dll \
         $(MINGW_LIB)/libpthreadGCE2.a \
	 $(MINGW_LIB)/libz.dll.a \
	 $(MINGW_LIB)/libfastlz.dll.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid

all: autoxflate-sd.dll

autoxflate-sd.dll: DLL_USAGE = -DBUILDING_DLL
autoxflate-sd.dll: autoxflate-sd.o
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a autoxflate-sd.o $(LDLIBS) -o $@

clean:
	rm -f *.o

distclean:: clean
	rm -f autoxflate-sd.dll

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<
