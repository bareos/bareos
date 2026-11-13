/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2016 Planets Communications B.V.
   Copyright (C) 2015-2026 Bareos GmbH & Co. KG

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
// Marco van Wieringen, May 2015
/**
 * @file
 * FHDB helper routines for NDMP Data Management Application (DMA)
 */

#include "include/bareos.h"
#include "include/streams.h"
#include "dird.h"
#include "include/protocol_types.h"
#include "include/filetypes.h"
#include "lib/attribs.h"

#if HAVE_NDMP

#  include "ndmp/ndmagents.h"

namespace directordaemon {

/**
 * Store all entries from the FHDB in the Database.
 *
 * * NMDP_BAREOS backup (remote NDMP, we use our emulated tape drive in the SD),
 *   files are stored as hardlinked items to the NDMP * archive in the backup
 * catalog. This way, any file that was selected for restore will trigger the
 * NDMP Backup Stream File itself to be restored
 *
 * * NDMP_NATIVE (real NDMP backup),
 *   files are stored as they come in including the Fhinfo and Fhnode date that
 * is needed for direct access recovery (DAR) and Directory DAR (DDAR)
 */
void NdmpStoreAttributeRecord(JobControlRecord* jcr,
                              int32_t FileIndex,
                              char* fname,
                              char* linked_fname,
                              char* attributes,
                              int8_t FileType,
                              uint64_t Node,
                              uint64_t Fhinfo)
{
  /* Use an own record instead of jcr->ar. With NDMP_BAREOS jcr->ar belongs
   * to the storage daemon message thread, which stores the attributes of the
   * virtual NDMP archive file asynchronously. Sharing it lets the file
   * history inherit an uninitialized or foreign JobId and FileIndex. */
  AttributesDbRecord ar;

  ar.JobId = jcr->JobId;
  ar.FileIndex = FileIndex;
  ar.fname = fname;
  ar.attr = attributes;
  ar.Stream = STREAM_UNIX_ATTRIBUTES;
  ar.FileType = FileType;
  ar.DeltaSeq = 0;

  if (jcr->getJobProtocol() == PT_NDMP_BAREOS) {
    /* the files are hardlinked to the virtual NDMP archive file, which the
     * storage daemon saved with the same FileIndex */
    ar.link = linked_fname;
  } else {
    // NDMP_NATIVE needs fhinfo and node for direct access recovery (DAR)
    ar.Fhinfo = Fhinfo;
    ar.Fhnode = Node;
  }

  if (DbLocker _{jcr->db}; !jcr->db->CreateAttributesRecord(jcr, &ar)) {
    Jmsg1(jcr, M_FATAL, 0, T_("Attribute create error: ERR=%s"),
          jcr->db->strerror());
  }
}

void NdmpConvertFstat(ndmp9_file_stat* fstat,
                      int32_t FileIndex,
                      int8_t* FileType,
                      PoolMem& attribs)
{
  struct stat statp;

  // Convert the NDMP file_stat structure into a UNIX one.
  memset(&statp, 0, sizeof(statp));

  Dmsg11(100,
         "ftype:%d mtime:%lu atime:%lu ctime:%lu uid:%lu gid:%lu mode:%lu "
         "size:%llu links:%lu node:%llu fh_info:%llu \n",
         fstat->ftype, fstat->mtime.value, fstat->atime.value,
         fstat->ctime.value, fstat->uid.value, fstat->gid.value,
         fstat->mode.value, fstat->size.value, fstat->links.value,
         fstat->node.value, fstat->fh_info.value);

  // If we got a valid mode of the file fill the UNIX stat struct.
  if (fstat->mode.valid == NDMP9_VALIDITY_VALID) {
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
  }

  // Encode a stat structure into an ASCII string.
  EncodeStat(attribs.c_str(), &statp, sizeof(statp), FileIndex,
             STREAM_UNIX_ATTRIBUTES);
}

} /* namespace directordaemon */
#endif /* #if HAVE_NDMP */
