/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2023 Bareos GmbH & Co. KG

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
//  Kern Sibbald, MM
/**
 * @file
 * Stream definitions.  Split from baconfig.h Nov 2010
 */

#ifndef BAREOS_INCLUDE_FILETYPES_H_
#define BAREOS_INCLUDE_FILETYPES_H_

#include <cstdint>

/**
 *  File type (Bareos defined).
 *  NOTE!!! These are saved in the Attributes record on the tape, so
 *          do not change them. If need be, add to them.
 *
 *  This is stored as 32 bits on the Volume, but only FT_MASK (16) bits are
 *    used for the file type. The upper bits are used to indicate
 *    additional optional fields in the attribute record.
 */
constexpr int32_t FT_MASK = 0xFFFF; /**< Bits used by FT (type) */
enum FILETYPES : int32_t
{
  FT_LNKSAVED = 1, /**< hard link to file already saved */
  FT_REGE,         /**< Regular file but empty */
  FT_REG,          /**< Regular file */
  FT_LNK,          /**< Soft Link */
  FT_DIREND,       /**< Directory at end (saved) */
  FT_SPEC,         /**< Special file -- chr, blk, fifo, sock */
  FT_NOACCESS,     /**< Not able to access */
  FT_NOFOLLOW,     /**< Could not follow link */
  FT_NOSTAT,       /**< Could not stat file */
  FT_NOCHG,        /**< Incremental option, file not changed */
  FT_DIRNOCHG,     /**< Incremental option, directory not changed */
  FT_ISARCH,       /**< Trying to save archive file */
  FT_NORECURSE,    /**< No recursion into directory */
  FT_NOFSCHG,      /**< Different file system, prohibited */
  FT_NOOPEN,       /**< Could not open directory */
  FT_RAW,          /**< Raw block device */
  FT_FIFO,         /**< Raw fifo device */
  /*
   * The DIRBEGIN packet is sent to the FD file processing routine so
   * that it can filter packets, but otherwise, it is not used
   * or saved */
  FT_DIRBEGIN,             /**< Directory at beginning (not saved) */
  FT_INVALIDFS,            /**< File system not allowed for */
  FT_INVALIDDT,            /**< Drive type not allowed for */
  FT_REPARSE,              /**< Win NTFS reparse point */
  FT_PLUGIN,               /**< Plugin generated filename */
  FT_DELETED,              /**< Deleted file entry */
  FT_BASE,                 /**< Duplicate base file entry */
  FT_RESTORE_FIRST,        /**< Restore this "object" first */
  FT_JUNCTION,             /**< Win32 Junction point */
  FT_PLUGIN_CONFIG,        /**< Object for Plugin configuration */
  FT_PLUGIN_CONFIG_FILLED, /**< Object for Plugin configuration filled by
                              Director */
};
static_assert(FT_PLUGIN_CONFIG_FILLED == 28);

constexpr int32_t FT_UNSET
    = FT_MASK; /**< Initial value when not determined yet  */
/* Definitions for upper part of type word (see above). */
constexpr int32_t AR_DATA_STREAM = (1 << 16); /**< Data stream id present */

/* Quick way to know if a Filetype is about a plugin "Object" */
inline bool IS_FT_OBJECT(int32_t x)
{
  return x == FT_RESTORE_FIRST || x == FT_PLUGIN_CONFIG_FILLED
         || x == FT_PLUGIN_CONFIG;
}

#endif  // BAREOS_INCLUDE_FILETYPES_H_
