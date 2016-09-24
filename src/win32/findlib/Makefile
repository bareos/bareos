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
VPATH = .:../../findlib

include ../Makefile.inc

INCLUDES = -I../.. \
	   -I../../include \
	   -I../include \
	   -I../../findlib \
	   -I../compat/include
LDLIBS = ../lib/libbareos.dll \
	 $(MINGW_LIB)/libwinpthread.a \
	 $(WINSOCKLIB)

LIBBAREOSFIND_SRCS = acl.c attribs.c bfile.c create_file.c \
                     drivetype.c enable_priv.c find_one.c \
                     find.c fstype.c hardlink.c match.c mkpath.c \
                     shadowing.c win32.c xattr.c
LIBBAREOSFIND_OBJS = $(LIBBAREOSFIND_SRCS:.c=.o)

DYNAMIC_OBJS = $(LIBBAREOSFIND_OBJS)

all: libbareosfind.dll

bareos$(WIN_VERSION).def: $(DYNAMIC_OBJS) make_def
	./make_def $(WIN_VERSION) $(DYNAMIC_OBJS) > $@

libbareosfind.dll: DLL_USAGE = -DBUILDING_DLL
libbareosfind.dll: $(DYNAMIC_OBJS) \
		bareos$(WIN_VERSION).def
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a $(DYNAMIC_OBJS) bareos$(WIN_VERSION).def $(LDLIBS) -o $@

clean:
	rm -f *.o bareos$(WIN_VERSION).def

distclean: clean
	rm -f libbareosfind.dll libbareosfind.dll.a

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<
