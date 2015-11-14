/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2015 Planets Communications B.V.
   Copyright (C) 2015-2015 Bareos GmbH & Co. KG

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
 * FHDB using LMDB for NDMP Data Management Application (DMA)
 *
 * Marco van Wieringen, May 2015
 */

#include "bareos.h"
#include "dird.h"

#if defined(HAVE_NDMP) && defined(HAVE_LMDB)

#include "ndmp/ndmagents.h"
#include "ndmp_dma_priv.h"

extern "C" int bndmp_fhdb_lmdb_add_file(struct ndmlog *ixlog, int tagc, char *raw_name,
                                        ndmp9_file_stat *fstat)
{
   NIS *nis;

   nis = (NIS *)ixlog->ctx;
   nis->jcr->lock();
   nis->jcr->JobFiles++;
   nis->jcr->unlock();

   if (nis->save_filehist) {
      int8_t FileType = 0;
      char namebuf[NDMOS_CONST_PATH_MAX];
      POOL_MEM attribs(PM_FNAME),
               pathname(PM_FNAME);

      ndmcstr_from_str(raw_name, namebuf, sizeof(namebuf));

      /*
       * Every file entry is releative from the filesystem currently being backuped.
       */
      Dmsg2(100, "bndmp_add_file: New filename ==> %s/%s\n", nis->filesystem, namebuf);

      if (nis->jcr->ar) {
         /*
          * See if this is the top level entry of the tree e.g. len == 0
          */
         if (strlen(namebuf) == 0) {
            ndmp_convert_fstat(fstat, nis->FileIndex, &FileType, attribs);

            pm_strcpy(pathname, nis->filesystem);
            pm_strcat(pathname, "/");
            return 0;
         } else {
            ndmp_convert_fstat(fstat, nis->FileIndex, &FileType, attribs);

            pm_strcpy(pathname, nis->filesystem);
            pm_strcat(pathname, "/");
            pm_strcat(pathname, namebuf);

            if (FileType == FT_DIREND) {
               /*
                * A directory needs to end with a slash.
                */
               pm_strcat(pathname, "/");
            }
         }

         ndmp_store_attribute_record(nis->jcr, pathname.c_str(), nis->virtual_filename, attribs.c_str(), FileType,
                                     0, (fstat->fh_info.valid == NDMP9_VALIDITY_VALID) ? fstat->fh_info.value : 0);
      }
   }

   return 0;
}

extern "C" int bndmp_fhdb_lmdb_add_dir(struct ndmlog *ixlog, int tagc, char *raw_name,
                                       ndmp9_u_quad dir_node, ndmp9_u_quad node)
{
   NIS *nis;
   char namebuf[NDMOS_CONST_PATH_MAX];

   ndmcstr_from_str(raw_name, namebuf, sizeof(namebuf));

   /*
    * Ignore . and .. directory entries.
    */
   if (bstrcmp(namebuf, ".") || bstrcmp(namebuf, "..")) {
      return 0;
   }

   nis = (NIS *)ixlog->ctx;
   nis->jcr->lock();
   nis->jcr->JobFiles++;
   nis->jcr->unlock();

   if (nis->save_filehist) {
      Dmsg3(100, "bndmp_add_dir: New filename ==> %s [%llu] - [%llu]\n", namebuf, dir_node, node);
   }

   return 0;
}

extern "C" int bndmp_fhdb_lmdb_add_node(struct ndmlog *ixlog, int tagc,
                                        ndmp9_u_quad node, ndmp9_file_stat *fstat)
{
   NIS *nis;

   nis = (NIS *)ixlog->ctx;

   if (nis->save_filehist) {
      Dmsg1(100, "bndmp_add_node: New node [%llu]\n", node);
   }

   return 0;
}

extern "C" int bndmp_fhdb_lmdb_add_dirnode_root(struct ndmlog *ixlog, int tagc,
                                                ndmp9_u_quad root_node)
{
   NIS *nis;

   nis = (NIS *)ixlog->ctx;

   if (nis->save_filehist) {
      Dmsg1(100, "bndmp_add_dirnode_root: New root node [%llu]\n", root_node);
   }

   return 0;
}

/*
 * This glues the NDMP File Handle DB with internal code.
 */
void ndmp_fhdb_lmdb_register_callback_hooks(struct ndmlog *ixlog)
{
   NIS *nis = (NIS *)ixlog->ctx;
   struct ndm_fhdb_callbacks fhdb_callbacks;

   /*
    * Register the FileHandleDB callbacks.
    */
   fhdb_callbacks.add_file = bndmp_fhdb_lmdb_add_file;
   fhdb_callbacks.add_dir = bndmp_fhdb_lmdb_add_dir;
   fhdb_callbacks.add_node = bndmp_fhdb_lmdb_add_node;
   fhdb_callbacks.add_dirnode_root = bndmp_fhdb_lmdb_add_dirnode_root;
   ndmfhdb_register_callbacks(ixlog, &fhdb_callbacks);
}

void ndmp_fhdb_lmdb_unregister(struct ndmlog *ixlog)
{
   NIS *nis = (NIS *)ixlog->ctx;

   ndmfhdb_unregister_callbacks(ixlog);
}

void ndmp_fhdb_lmdb_process_db(struct ndmlog *ixlog)
{
   NIS *nis = (NIS *)ixlog->ctx;
}
#endif
