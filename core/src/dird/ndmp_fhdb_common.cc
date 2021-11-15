/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2018 Bareos GmbH & Co. KG

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
// Philipp Storz, Nov 2016
/**
 * @file
 * Functions common to mem and lmdb backends of fhdb handling
 */
#include "include/bareos.h"
#include "dird.h"

#if defined(HAVE_NDMP)

#  include "ndmp/ndmagents.h"
#  include "ndmp_dma_priv.h"

namespace directordaemon {

extern "C" int BndmpFhdbAddFile(struct ndmlog* ixlog,
                                int tagc,
                                char* raw_name,
                                ndmp9_file_stat* fstat)
{
  NIS* nis = (NIS*)ixlog->ctx;

  nis->jcr->lock();
  nis->jcr->JobFiles++;
  nis->jcr->unlock();

  if (nis->save_filehist) {
    int8_t FileType = 0;
    PoolMem attribs(PM_FNAME), pathname(PM_FNAME);

    /*
     * Every file entry is relative from the filesystem currently being backed
     * up.
     */
    Dmsg2(100, "BndmpFhdbAddFile: New filename ==> %s%s\n", nis->filesystem,
          raw_name);

    // if (nis->jcr->ar) {
    // See if this is the top level entry of the tree e.g. len == 0
    if (strlen(raw_name) == 0) {
      NdmpConvertFstat(fstat, nis->FileIndex, &FileType, attribs);

      PmStrcpy(pathname, nis->filesystem);
      PmStrcat(pathname, "/");
      return 0;
    } else {
      NdmpConvertFstat(fstat, nis->FileIndex, &FileType, attribs);

      bool filesystem_ends_with_slash
          = (nis->filesystem[strlen(nis->filesystem) - 1] == '/');
      bool raw_name_starts_with_slash = (*raw_name == '/');
      bool raw_name_ends_with_slash = (raw_name[strlen(raw_name) - 1] == '/');

      PmStrcpy(pathname, nis->filesystem);
      // make sure we have a trailing slash
      if (!filesystem_ends_with_slash) { PmStrcat(pathname, "/"); }

      // skip leading slash to avoid double slashes
      if (raw_name_starts_with_slash) {
        PmStrcat(pathname, raw_name + 1);
      } else {
        PmStrcat(pathname, raw_name);
      }

      if (FileType == FT_DIREND) {
        /*
         * A directory needs to end with a '/'
         * so append it if it is missing
         */
        if (!raw_name_ends_with_slash) { PmStrcat(pathname, "/"); }
      }
    }

    NdmpStoreAttributeRecord(
        nis->jcr, pathname.c_str(), nis->virtual_filename, attribs.c_str(),
        FileType,
        (fstat->node.valid == NDMP9_VALIDITY_VALID) ? fstat->node.value : 0,
        (fstat->fh_info.valid == NDMP9_VALIDITY_VALID) ? fstat->fh_info.value
                                                       : 0);
  }

  return 0;
}

} /* namespace directordaemon */
#endif /* #if defined(HAVE_NDMP) */
