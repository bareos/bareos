/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald MMI
 * Extracted from findlib/find.h Nov 2010
 * Switched to enum Nov 2014, Marco van Wieringen
 */
/**
 * @file
 * File types
 */

#ifndef __BFILEOPTS_H
#define __BFILEOPTS_H

/**
 * Options saved in "options" of the include/exclude lists.
 * They are directly jammed into "flag" of ff packet
 */
enum {
   FO_PORTABLE_DATA = 0, /**< Data is portable */
   FO_MD5 = 1,           /**< Do MD5 checksum */
   FO_COMPRESS = 2,      /**< Do compression */
   FO_NO_RECURSION = 3,  /**< No recursion in directories */
   FO_MULTIFS = 4,       /**< Multiple file systems */
   FO_SPARSE = 5,        /**< Do sparse file checking */
   FO_IF_NEWER = 6,      /**< Replace if newer */
   FO_NOREPLACE = 7,     /**< Never replace */
   FO_READFIFO = 8,      /**< Read data from fifo */
   FO_SHA1 = 9,          /**< Do SHA1 checksum */
   FO_PORTABLE = 10,     /**< Use portable data format -- no BackupWrite */
   FO_MTIMEONLY = 11,    /**< Use mtime rather than mtime & ctime */
   FO_KEEPATIME = 12,    /**< Reset access time */
   FO_EXCLUDE = 13,      /**< Exclude file */
   FO_ACL = 14,          /**< Backup ACLs */
   FO_NO_HARDLINK = 15,  /**< Don't handle hard links */
   FO_IGNORECASE = 16,   /**< Ignore file name case */
   FO_HFSPLUS = 17,      /**< Resource forks and Finder Info */
   FO_WIN32DECOMP = 18,  /**< Use BackupRead decomposition */
   FO_SHA256 = 19,       /**< Do SHA256 checksum */
   FO_SHA512 = 20,       /**< Do SHA512 checksum */
   FO_ENCRYPT = 21,      /**< Encrypt data stream */
   FO_NOATIME = 22,      /**< Use O_NOATIME to prevent atime change */
   FO_ENHANCEDWILD = 23, /**< Enhanced wild card processing */
   FO_CHKCHANGES = 24,   /**< Check if file have been modified during backup */
   FO_STRIPPATH = 25,    /**< Check for stripping path */
   FO_HONOR_NODUMP = 26, /**< Honor NODUMP flag */
   FO_XATTR = 27,        /**< Backup Extended Attributes */
   FO_DELTA = 28,        /**< Delta data -- i.e. all copies returned on restore */
   FO_PLUGIN = 29,       /**< Plugin data stream -- return to plugin on restore */
   FO_OFFSETS = 30,      /**< Keep I/O file offsets */
   FO_NO_AUTOEXCL = 31,  /**< Don't use autoexclude methods */
   FO_FORCE_ENCRYPT = 32 /**< Force encryption */
};

/**
 * Keep this set to the last entry in the enum.
 */
#define FO_MAX FO_FORCE_ENCRYPT

/**
 * Make sure you have enough bits to store all above bit fields.
 */
#define FOPTS_BYTES nbytes_for_bits(FO_MAX + 1)

#endif /* __BFILEOPTSS_H */
