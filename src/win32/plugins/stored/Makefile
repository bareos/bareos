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
PYTHON_INCLUDES = -I../python/include
LDLIBS = ../../lib/libbareos.dll \
         $(MINGW_LIB)/libwinpthread.a \
	 $(MINGW_LIB)/libz.dll.a \
	 $(MINGW_LIB)/libfastlz.dll.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid

ifeq ($(WIN_DEBUG),yes)
OPT_CXXFLAGS = $(CXXFLAGS) -O2
else
OPT_CXXFLAGS = $(CXXFLAGS)
endif

all: autoxflate-sd.dll python-sd.dll

autoxflate-sd.dll: DLL_USAGE = -DBUILDING_DLL
autoxflate-sd.dll: autoxflate-sd.o
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a autoxflate-sd.o $(LDLIBS) -o $@

ifeq ($(WIN_VERSION),64)
python-sd.o: CXXFLAGS += -DMS_WIN64
endif
python-sd.o: python-sd.c
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(OPT_CXXFLAGS) $(PYTHON_INCLUDES) -c -o $@ $<

python-sd.dll: DLL_USAGE = -DBUILDING_DLL
python-sd.dll: python-sd.o
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a python-sd.o $(LDLIBS) ../python/lib/$(WIN_VERSION)/python27.dll -o $@

clean:
	rm -f *.o

distclean:: clean
	rm -f autoxflate-sd.dll python-sd.dll autoxflate-sd.dll.a python-sd.dll.a

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<
