/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015      Bareos GmbH & Co.KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "ndmp/ndmp9.h"
#include "ndmp/ndmlib.h"

extern "C" {
   /*
    * common for both back ends
    */
   int bndmp_fhdb_add_file(struct ndmlog *ixlog, int tagc, char *raw_name, ndmp9_file_stat *fstat);

   int bndmp_fhdb_mem_add_dir(struct ndmlog *ixlog, int tagc, char *raw_name, ndmp9_u_quad dir_node, ndmp9_u_quad node);
   int bndmp_fhdb_mem_add_dirnode_root(struct ndmlog *ixlog, int tagc, ndmp9_u_quad root_node);
   int bndmp_fhdb_mem_add_node(struct ndmlog *ixlog, int tagc, ndmp9_u_quad node, ndmp9_file_stat *fstat);

   int bndmp_fhdb_lmdb_add_dir(struct ndmlog *ixlog, int tagc, char *raw_name, ndmp9_u_quad dir_node, ndmp9_u_quad node);
   int bndmp_fhdb_lmdb_add_dirnode_root(struct ndmlog *ixlog, int tagc, ndmp9_u_quad root_node);
   int bndmp_fhdb_lmdb_add_node(struct ndmlog *ixlog, int tagc, ndmp9_u_quad node, ndmp9_file_stat *fstat);
}

void ndmp_fhdb_mem_process_db(struct ndmlog *ixlog);

void test_ndmp_fhdb_mem(void **state);
void test_ndmp_fhdb_lmdb(void **state);
