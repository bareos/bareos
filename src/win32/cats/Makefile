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
VPATH = .:../../cats

include ../Makefile.inc

INCLUDES = -I../.. \
	   -I../../include \
	   -I../include \
	   -I../../findlib \
	   -I../compat/include
LDLIBS = ../lib/libbareos.dll

LIBBAREOSCATS_SRCS = bvfs.c cats.c cats_backends.c sql.c sql_cmds.c \
                     sql_create.c sql_delete.c sql_find.c sql_get.c \
                     sql_list.c sql_pooling.c sql_query.c sql_update.c
LIBBAREOSCATS_OBJS = $(LIBBAREOSCATS_SRCS:.c=.o)

DEFINES += -D_BDB_PRIV_INTERFACE_

DYNAMIC_OBJS = $(LIBBAREOSCATS_OBJS)

all: libbareoscats.dll libbareoscats-postgresql.dll libbareoscats-sqlite3.dll

bareos$(WIN_VERSION).def: $(DYNAMIC_OBJS) make_def
	./make_def $(WIN_VERSION) $(DYNAMIC_OBJS) > $@

libbareoscats.dll: DLL_USAGE = -DBUILDING_DLL
libbareoscats.dll: $(DYNAMIC_OBJS) \
		bareos$(WIN_VERSION).def
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a $(DYNAMIC_OBJS) bareos$(WIN_VERSION).def $(LDLIBS) -o $@

postgresql.o: postgresql.c
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -Ipgsql/include -c -o $@ $<

libbareoscats-postgresql.dll: DLL_USAGE = -DBUILDING_DLL
libbareoscats-postgresql.dll: postgresql.o libbareoscats.dll
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a postgresql.o $(LDLIBS) libbareoscats.dll pgsql/lib/$(WIN_VERSION)/libpq.dll -o $@

libbareoscats-sqlite3.dll: DLL_USAGE = -DBUILDING_DLL
libbareoscats-sqlite3.dll: sqlite.o libbareoscats.dll
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a sqlite.o $(LDLIBS) libbareoscats.dll -lsqlite3 -o $@

clean:
	rm -f *.o bareos$(WIN_VERSION).def

distclean: clean
	rm -f libbareoscats.dll libbareoscats.dll.a \
              libbareoscats-postgresql.dll libbareoscats-postgresql.dll.a \
              libbareoscats-sqlite3.dll libbareoscats-sqlite3.dll.a

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<
