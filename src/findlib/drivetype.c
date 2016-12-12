/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2007 Free Software Foundation Europe e.V.
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
/*
 * Written by Robert Nelson, June 2006
 */
/**
 * @file
 * Implement routines to determine drive type (Windows specific).
 */

#include "bareos.h"
#include "find.h"

/**
 * These functions should be implemented for each OS
 *
 * bool drivetype(const char *fname, char *dt, int dtlen);
 */
#if defined (HAVE_WIN32)
/* Windows */

bool drivetype(const char *fname, char *dt, int dtlen)
{
   CHAR rootpath[4];
   UINT type;

   /*
    * Copy Drive Letter, colon, and backslash to rootpath
    */
   bstrncpy(rootpath, fname, 3);
   rootpath[3] = '\0';

   type = GetDriveType(rootpath);

   switch (type) {
   case DRIVE_REMOVABLE:
      bstrncpy(dt, "removable", dtlen);
      return true;
   case DRIVE_FIXED:
      bstrncpy(dt, "fixed", dtlen);
      return true;
   case DRIVE_REMOTE:
      bstrncpy(dt, "remote", dtlen);
      return true;
   case DRIVE_CDROM:
      bstrncpy(dt, "cdrom", dtlen);
      return true;
   case DRIVE_RAMDISK:
      bstrncpy(dt, "ramdisk", dtlen);
      return true;
   case DRIVE_UNKNOWN:
   case DRIVE_NO_ROOT_DIR:
   default:
      return false;
   }
}
/* Windows */

#else    /* No recognised OS */

bool drivetype(const char *fname, char *dt, int dtlen)
{
   Dmsg0(10, "!!! drivetype() not implemented for this OS. !!!\n");
   return false;
}
#endif

