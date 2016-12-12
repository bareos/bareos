/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
/**
 *  Kern Sibbald, MM
 */
/**
 * @file
 * Stream definitions.  Split from baconfig.h Nov 2010
 */

#ifndef __BFILETYPES_H
#define __BFILETYPES_H 1


/**
 *  File type (Bareos defined).
 *  NOTE!!! These are saved in the Attributes record on the tape, so
 *          do not change them. If need be, add to them.
 *
 *  This is stored as 32 bits on the Volume, but only FT_MASK (16) bits are
 *    used for the file type. The upper bits are used to indicate
 *    additional optional fields in the attribute record.
 */
#define FT_MASK       0xFFFF          /**< Bits used by FT (type) */
#define FT_LNKSAVED   1               /**< hard link to file already saved */
#define FT_REGE       2               /**< Regular file but empty */
#define FT_REG        3               /**< Regular file */
#define FT_LNK        4               /**< Soft Link */
#define FT_DIREND     5               /**< Directory at end (saved) */
#define FT_SPEC       6               /**< Special file -- chr, blk, fifo, sock */
#define FT_NOACCESS   7               /**< Not able to access */
#define FT_NOFOLLOW   8               /**< Could not follow link */
#define FT_NOSTAT     9               /**< Could not stat file */
#define FT_NOCHG     10               /**< Incremental option, file not changed */
#define FT_DIRNOCHG  11               /**< Incremental option, directory not changed */
#define FT_ISARCH    12               /**< Trying to save archive file */
#define FT_NORECURSE 13               /**< No recursion into directory */
#define FT_NOFSCHG   14               /**< Different file system, prohibited */
#define FT_NOOPEN    15               /**< Could not open directory */
#define FT_RAW       16               /**< Raw block device */
#define FT_FIFO      17               /**< Raw fifo device */
/*
 * The DIRBEGIN packet is sent to the FD file processing routine so
 * that it can filter packets, but otherwise, it is not used
 * or saved */
#define FT_DIRBEGIN  18               /**< Directory at beginning (not saved) */
#define FT_INVALIDFS 19               /**< File system not allowed for */
#define FT_INVALIDDT 20               /**< Drive type not allowed for */
#define FT_REPARSE   21               /**< Win NTFS reparse point */
#define FT_PLUGIN    22               /**< Plugin generated filename */
#define FT_DELETED   23               /**< Deleted file entry */
#define FT_BASE      24               /**< Duplicate base file entry */
#define FT_RESTORE_FIRST 25           /**< Restore this "object" first */
#define FT_JUNCTION  26               /**< Win32 Junction point */
#define FT_PLUGIN_CONFIG 27           /**< Object for Plugin configuration */
#define FT_PLUGIN_CONFIG_FILLED 28    /**< Object for Plugin configuration filled by Director */

#define FT_UNSET     FT_MASK          /**< Initial value when not determined yet  */
/* Definitions for upper part of type word (see above). */
#define AR_DATA_STREAM (1<<16)        /**< Data stream id present */

/* Quick way to know if a Filetype is about a plugin "Object" */
#define IS_FT_OBJECT(x) (((x) == FT_RESTORE_FIRST) || ((x) == FT_PLUGIN_CONFIG_FILLED) || ((x) == FT_PLUGIN_CONFIG))

#endif /* __BFILETYPES_H */
