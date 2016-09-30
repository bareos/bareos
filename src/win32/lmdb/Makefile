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
VPATH = .:../../lmdb

include ../Makefile.inc

INCLUDES = -I../../lmdb
LDLIBS = $(MINGW_LIB)/libwinpthread.a \
         -lntdll

LIBBAREOSLMDB_SRCS = mdb.c midl.c
LIBBAREOSLMDB_OBJS = $(LIBBAREOSLMDB_SRCS:.c=.o)

DYNAMIC_OBJS = $(LIBBAREOSLMDB_OBJS)

all: libbareoslmdb.dll

bareos$(WIN_VERSION).def: $(DYNAMIC_OBJS) make_def
	./make_def $(WIN_VERSION) $(DYNAMIC_OBJS) > $@

libbareoslmdb.dll: DLL_USAGE = -DBUILDING_DLL
libbareoslmdb.dll: $(DYNAMIC_OBJS) \
		bareos$(WIN_VERSION).def
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a $(DYNAMIC_OBJS) bareos$(WIN_VERSION).def $(LDLIBS) -o $@

clean:
	rm -f *.o bareos$(WIN_VERSION).def

distclean: clean
	rm -f libbareoslmdb.dll libbareoslmdb.dll.a

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CC) $(DLL_USAGE) $(CFLAGS) -c -o $@ $<
