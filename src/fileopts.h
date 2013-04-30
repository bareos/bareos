/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2010 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * File types 
 *
 *     Kern Sibbald MMI
 *
 *  Extracted from findlib/find.h Nov 2010
 *
 */

#ifndef __BFILEOPTS_H
#define __BFILEOPTS_H

/*  
 * Options saved int "options" of the include/exclude lists.
 * They are directly jammed ito  "flag" of ff packet
 */
#define FO_PORTABLE_DATA (1<<0)       /* Data is portable */
#define FO_MD5           (1<<1)       /* Do MD5 checksum */
#define FO_COMPRESS      (1<<2)       /* Do compression */
#define FO_NO_RECURSION  (1<<3)       /* no recursion in directories */
#define FO_MULTIFS       (1<<4)       /* multiple file systems */
#define FO_SPARSE        (1<<5)       /* do sparse file checking */
#define FO_IF_NEWER      (1<<6)       /* replace if newer */
#define FO_NOREPLACE     (1<<7)       /* never replace */
#define FO_READFIFO      (1<<8)       /* read data from fifo */
#define FO_SHA1          (1<<9)       /* Do SHA1 checksum */
#define FO_PORTABLE      (1<<10)      /* Use portable data format -- no BackupWrite */
#define FO_MTIMEONLY     (1<<11)      /* Use mtime rather than mtime & ctime */
#define FO_KEEPATIME     (1<<12)      /* Reset access time */
#define FO_EXCLUDE       (1<<13)      /* Exclude file */
#define FO_ACL           (1<<14)      /* Backup ACLs */
#define FO_NO_HARDLINK   (1<<15)      /* don't handle hard links */
#define FO_IGNORECASE    (1<<16)      /* Ignore file name case */
#define FO_HFSPLUS       (1<<17)      /* Resource forks and Finder Info */
#define FO_WIN32DECOMP   (1<<18)      /* Use BackupRead decomposition */
#define FO_SHA256        (1<<19)      /* Do SHA256 checksum */
#define FO_SHA512        (1<<20)      /* Do SHA512 checksum */
#define FO_ENCRYPT       (1<<21)      /* Encrypt data stream */
#define FO_NOATIME       (1<<22)      /* Use O_NOATIME to prevent atime change */
#define FO_ENHANCEDWILD  (1<<23)      /* Enhanced wild card processing */
#define FO_CHKCHANGES    (1<<24)      /* Check if file have been modified during backup */
#define FO_STRIPPATH     (1<<25)      /* Check for stripping path */
#define FO_HONOR_NODUMP  (1<<26)      /* honor NODUMP flag */
#define FO_XATTR         (1<<27)      /* Backup Extended Attributes */
#define FO_DELTA         (1<<28)      /* Delta data -- i.e. all copies returned on restore */
#define FO_PLUGIN        (1<<29)      /* Plugin data stream -- return to plugin on restore */
#define FO_OFFSETS       (1<<30)      /* Keep I/O file offsets */

#endif /* __BFILEOPTSS_H */
