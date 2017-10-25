/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016      Bareos GmbH & Co. KG

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
/*
 * Philipp Storz, February 2016
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

extern "C" {
#include <cmocka.h>
}
#include "bareos.h"
#include "protos.h"
#include "jcr.h"
#include "stdio.h"
#include "time.h"

typedef void *UAContext, *CLIENTRES, *STORERES;
#include "../ndmp_dma_priv.h"
#include "../ndmp_fhdb_mem.h"
#include "ndmp_testdata.h"

ndmlog *ixlog;
ndmp_internal_state *nis;
JCR *jcr = (JCR*)(malloc(sizeof(JCR)));

int tagc=0;
ndmp9_file_stat filestat;
bool check_lmdb = false;

extern "C" void __wrap__Z18ndmp_convert_fstatP15ndmp9_file_statiPaR8POOL_MEM(ndmp9_file_stat *fstat, int32_t FileIndex,
                                                                             int8_t *FileType, POOL_MEM &attribs)
{
   struct stat statp;

   /*
    * Convert the NDMP file_stat structure into a UNIX one.
    */
   memset(&statp, 0, sizeof(statp));

   /*
    * If we got a valid mode of the file fill the UNIX stat struct.
    */
   //if (fstat->mode.valid == NDMP9_VALIDITY_VALID) {
      switch (fstat->ftype) {
      case NDMP9_FILE_DIR:
         statp.st_mode = fstat->mode.value | S_IFDIR;
         *FileType = FT_DIREND;
         break;
      case NDMP9_FILE_FIFO:
         statp.st_mode = fstat->mode.value | S_IFIFO;
         *FileType = FT_FIFO;
         break;
      case NDMP9_FILE_CSPEC:
         statp.st_mode = fstat->mode.value | S_IFCHR;
         *FileType = FT_SPEC;
         break;
      case NDMP9_FILE_BSPEC:
         statp.st_mode = fstat->mode.value | S_IFBLK;
         *FileType = FT_SPEC;
         break;
      case NDMP9_FILE_REG:
         statp.st_mode = fstat->mode.value | S_IFREG;
         *FileType = FT_REG;
         break;
      case NDMP9_FILE_SLINK:
         statp.st_mode = fstat->mode.value | S_IFLNK;
         *FileType = FT_LNK;
         break;
      case NDMP9_FILE_SOCK:
         statp.st_mode = fstat->mode.value | S_IFSOCK;
         *FileType = FT_SPEC;
         break;
      case NDMP9_FILE_REGISTRY:
         statp.st_mode = fstat->mode.value | S_IFREG;
         *FileType = FT_REG;
         break;
      case NDMP9_FILE_OTHER:
         statp.st_mode = fstat->mode.value | S_IFREG;
         *FileType = FT_REG;
         break;
      default:
         break;
      }

      if (fstat->mtime.valid == NDMP9_VALIDITY_VALID) {
         statp.st_mtime = fstat->mtime.value;
      }

      if (fstat->atime.valid == NDMP9_VALIDITY_VALID) {
         statp.st_atime = fstat->atime.value;
      }

      if (fstat->ctime.valid == NDMP9_VALIDITY_VALID) {
         statp.st_ctime = fstat->ctime.value;
      }

      if (fstat->uid.valid == NDMP9_VALIDITY_VALID) {
         statp.st_uid = fstat->uid.value;
      }

      if (fstat->gid.valid == NDMP9_VALIDITY_VALID) {
         statp.st_gid = fstat->gid.value;
      }

      if (fstat->links.valid == NDMP9_VALIDITY_VALID) {
         statp.st_nlink = fstat->links.value;
      }

   /*
    * Encode a stat structure into an ASCII string.
    */
   encode_stat(attribs.c_str(), &statp, sizeof(statp), FileIndex, STREAM_UNIX_ATTRIBUTES);
}

extern "C" void __wrap__Z27ndmp_store_attribute_recordP3JCRPcS1_S1_amm(JCR *jcr, char *fname, char *linked_fname,
                                                                       char *attributes, int8_t FileType, uint64_t Node, uint64_t Offset)
{
   if (debug_level > 0) {
      printf ("ndmp_store_attribute_record called with filename %s, attributes [%s] \n", fname, attributes);
   }

}

void replay_backup(fileinfo *testdata, ndmlog *ixlog, int tagc, ndm_fhdb_callbacks fhdb_callbacks)
{

   fileinfo *finfo;
   ndmp9_file_stat ndmfstat;

   for (finfo = testdata; (finfo->filename || finfo->dir_node || finfo->node); finfo ++) {

      if (!finfo->filename && !finfo->dir_node && finfo->node) {
         //printf("root_node: %lu\n", finfo->node);
         fhdb_callbacks.add_dirnode_root(ixlog, tagc,  finfo->node);

      } else if (finfo->filename && finfo->dir_node && finfo->node) {
         //printf("add dir: %s, %lu, %lu\n", finfo->filename, finfo->node, finfo->dir_node);
         fhdb_callbacks.add_dir(ixlog, tagc, finfo->filename, finfo->dir_node, finfo->node);

      } else if (!finfo->filename && finfo->dir_node && !finfo->node) {
         //printf("add node: %lu\n", finfo->dir_node);
         fhdb_callbacks.add_node(ixlog, tagc, finfo->dir_node, &ndmfstat);

      } else {
         //printf("unexpected finfo: %s, %lu, %lu\n", finfo->filename, finfo->node, finfo->dir_node);
      }
   }
}

#define NFILES   10 * 1000 * 1000
//#define NFILES  10 * 1000 * 1000
#define SUBDIRS 10

#define ROOTNODENUMBER   10000000
#define SUBDIRNUMBER     10000001

void emulate_backup(ndmlog *ixlog, ndm_fhdb_callbacks fhdb_callbacks)
{
   ndmp9_file_stat ndmfstat;
   char buf[20];
   char *filename = buf;
   fhdb_callbacks.add_dirnode_root(ixlog, tagc, ROOTNODENUMBER);

   for (uint64_t i = 0; i < NFILES; i++){
      sprintf(buf, "node%lu", i);
      fhdb_callbacks.add_dir(ixlog, tagc, filename, SUBDIRNUMBER, i);
   }

   fhdb_callbacks.add_dir(ixlog, tagc, filename,  ROOTNODENUMBER, SUBDIRNUMBER);

   for (uint64_t i = 0; i < NFILES; i++){
      fhdb_callbacks.add_node(ixlog, tagc, i, &ndmfstat);
   }

/*
   for (uint64_t i = 0; i < NFILES; i++){
      sprintf(buf, "node%lu", i, i % SUBDIRS);
      fhdb_callbacks.add_dir(ixlog, tagc, filename, SUBDIRBASENUMBER + i % SUBDIRS, i);
   }

   for (uint64_t i = 0; i < NFILES; i++){
      fhdb_callbacks.add_node(ixlog, tagc, i, &ndmfstat);
   }

   for (uint64_t i = 0; i < SUBDIRS; i++){
      sprintf(buf, "subdir%lu", i % SUBDIRS);
      fhdb_callbacks.add_dir(ixlog, tagc, filename, ROOTNODENUMBER, SUBDIRBASENUMBER + i % SUBDIRS);
      fhdb_callbacks.add_node(ixlog, tagc, i % SUBDIRS, &ndmfstat);
   }
*/
}

void test_ndmp_fhdb(void **state);

void test_ndmp_fhdb_mem(void **state) {
   Dmsg0(0, "test_ndmp_fhdb_mem start #########################################################\n");
   check_lmdb = false;
   test_ndmp_fhdb(state);
   Dmsg0(0, "test_ndmp_fhdb_mem done  #########################################################\n");
}

void test_ndmp_fhdb_lmdb(void **state) {
   Dmsg0(0, "test_ndmp_fhdb_lmdb start ########################################################\n");
   check_lmdb = true;
   test_ndmp_fhdb(state);
   Dmsg0(0, "test_ndmp_fhdb_lmdb done #########################################################\n");
}

void test_ndmp_fhdb(void **state) {
   (void) state;
   time_t t, ot;

   struct ndm_fhdb_callbacks fhdb_callbacks;


   jcr = new_jcr(sizeof(JCR), NULL);

   ixlog = (ndmlog*) (malloc(sizeof(ndmlog)));
   nis = (ndmp_internal_state*) (malloc(sizeof(ndmp_internal_state)));
   nis->save_filehist = true;
   nis->filesystem = "/ifs";
   nis->jcr = jcr;
   ixlog->ctx = nis;
   ixlog->nfc = NULL;
   jcr->JobId = 99999;
#if HAVE_NDMP

   ndmp9_u_quad dir_node, node, root_node,air_node;
   dir_node = 0;
   node = 1;
   root_node = 0;
   int tagc = 0;
   #define AIR_NODE_BASE 11

   /*
    * Register the FileHandleDB callbacks.
    */
if (check_lmdb) {
   fhdb_callbacks.add_file = bndmp_fhdb_lmdb_add_file;
   fhdb_callbacks.add_dir = bndmp_fhdb_lmdb_add_dir;
   fhdb_callbacks.add_node = bndmp_fhdb_lmdb_add_node;
   fhdb_callbacks.add_dirnode_root = bndmp_fhdb_lmdb_add_dirnode_root;
   ndmp_fhdb_lmdb_register(ixlog);

} else {
   fhdb_callbacks.add_file = bndmp_fhdb_mem_add_file;
   fhdb_callbacks.add_dir = bndmp_fhdb_mem_add_dir;
   fhdb_callbacks.add_node = bndmp_fhdb_mem_add_node;
   fhdb_callbacks.add_dirnode_root = bndmp_fhdb_mem_add_dirnode_root;
   ndmp_fhdb_mem_register(ixlog);
}
   //replay_backup(testdata_netapp_big, ixlog, tagc, fhdb_callbacks);
   replay_backup(testdata_netapp, ixlog, tagc, fhdb_callbacks);
   //emulate_backup(ixlog, fhdb_callbacks);

if (check_lmdb) {
   ndmp_fhdb_lmdb_process_db(ixlog);
   ndmp_fhdb_lmdb_unregister(ixlog);
} else {
   ndmp_fhdb_mem_process_db(ixlog);
   ndmp_fhdb_mem_unregister(ixlog);
}

   Dsm_check(0);
   check_lmdb = true;
#else
   printf("HAVE_NDMP not set (NDMP not enabled?)\n");
#endif
}
