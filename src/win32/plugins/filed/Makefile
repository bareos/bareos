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
VPATH = .:../../../plugins/filed

include ../../Makefile.inc

INCLUDES = -I../../.. \
	   -I../../../include \
	   -I../../../filed \
	   -I../../../plugins/filed \
	   -I../../include \
	   -I../../compat/include
VDI_INCLUDES = -I../../vdi/include
PYTHON_INCLUDES = -I../python/include
LDLIBS = ../../lib/libbareos.dll \
         $(MINGW_LIB)/libwinpthread.a \
         $(MINGW_LIB)/libz.dll.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid

ifeq ($(WIN_DEBUG),yes)
OPT_CXXFLAGS = $(CXXFLAGS) -O2
else
OPT_CXXFLAGS = $(CXXFLAGS)
endif

all: bpipe-fd.dll mssqlvdi-fd.dll python-fd.dll

bpipe-fd.dll: DLL_USAGE = -DBUILDING_DLL
bpipe-fd.dll: bpipe-fd.o
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a bpipe-fd.o $(LDLIBS) -o $@

mssqlvdi-fd.o: CXXFLAGS += -fpermissive
mssqlvdi-fd.o: mssqlvdi-fd.cc
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(OPT_CXXFLAGS) $(VDI_INCLUDES) -c -o $@ $<

mssqlvdi-fd.dll: DLL_USAGE = -DBUILDING_DLL
mssqlvdi-fd.dll: mssqlvdi-fd.o
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a mssqlvdi-fd.o $(LDLIBS) -o $@

ifeq ($(WIN_VERSION),64)
python-fd.o: CXXFLAGS += -DMS_WIN64
endif
python-fd.o: python-fd.cc
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(OPT_CXXFLAGS) $(PYTHON_INCLUDES) -c -o $@ $<

python-fd.dll: DLL_USAGE = -DBUILDING_DLL
python-fd.dll: python-fd.o
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a python-fd.o $(LDLIBS) ../python/lib/$(WIN_VERSION)/python27.dll -o $@

clean:
	rm -f *.o

distclean:: clean
	rm -f bpipe-fd.dll mssqlvdi-fd.dll python-fd.dll bpipe-fd.dll.a mssqlvdi-fd.dll.a python-fd.dll.a

# inference rules
.cc.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<
