/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2011 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, MMVI
 */
/**
 * scan.c scan a directory (on a removable file) for a valid
 * Volume name. If found, open the file for append.
 */

#include "bareos.h"
#include "stored.h"

/* Forward referenced functions */
static bool is_volume_name_legal(char *name);


bool DEVICE::scan_dir_for_volume(DCR *dcr)
{
   DIR* dp;
   struct dirent *result;
#ifdef USE_READDIR_R
   struct dirent *entry;
#endif
   int name_max;
   char *mount_point;
   VOLUME_CAT_INFO dcrVolCatInfo, devVolCatInfo;
   char VolumeName[MAX_NAME_LENGTH];
   struct stat statp;
   bool found = false;
   POOL_MEM fname(PM_FNAME);
   bool need_slash = false;
   int len;

   dcrVolCatInfo = dcr->VolCatInfo;     /* structure assignment */
   devVolCatInfo = VolCatInfo;          /* structure assignment */
   bstrncpy(VolumeName, dcr->VolumeName, sizeof(VolumeName));

   name_max = pathconf(".", _PC_NAME_MAX);
   if (name_max < 1024) {
      name_max = 1024;
   }

   if (device->mount_point) {
      mount_point = device->mount_point;
   } else {
      mount_point = device->device_name;
   }

   if (!(dp = opendir(mount_point))) {
      berrno be;
      dev_errno = errno;
      Dmsg3(29, "scan_dir_for_vol: failed to open dir %s (dev=%s), ERR=%s\n",
            mount_point, print_name(), be.bstrerror());
      goto get_out;
   }

   len = strlen(mount_point);
   if (len > 0) {
      need_slash = !IsPathSeparator(mount_point[len - 1]);
   }
         
#ifdef USE_READDIR_R
   entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
   while (1) {
      if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
#else
   while (1) {
      result = readdir(dp);
      if (result == NULL) {
#endif
         dev_errno = EIO;
         Dmsg2(129, "scan_dir_for_vol: failed to find suitable file in dir %s (dev=%s)\n",
               mount_point, print_name());
         break;
      }
      if (bstrcmp(result->d_name, ".") ||
          bstrcmp(result->d_name, "..")) {
         continue;
      }

      if (!is_volume_name_legal(result->d_name)) {
         continue;
      }
      pm_strcpy(fname, mount_point);
      if (need_slash) {
         pm_strcat(fname, "/");
      }
      pm_strcat(fname, result->d_name);
      if (lstat(fname.c_str(), &statp) != 0 ||
          !S_ISREG(statp.st_mode)) {
         continue;                 /* ignore directories & special files */
      }

      /*
       * OK, we got a different volume mounted. First save the
       *  requested Volume info (dcr) structure, then query if
       *  this volume is really OK. If not, put back the desired
       *  volume name, mark it not in changer and continue.
       */
      /* Check if this is a valid Volume in the pool */
      bstrncpy(dcr->VolumeName, result->d_name, sizeof(dcr->VolumeName));
      if (!dcr->dir_get_volume_info(GET_VOL_INFO_FOR_WRITE)) {
         continue;
      }
      /* This was not the volume we expected, but it is OK with
       * the Director, so use it.
       */
      VolCatInfo = dcr->VolCatInfo;       /* structure assignment */
      found = true;
      break;                /* got a Volume */
   }
#ifdef USE_READDIR_R
   free(entry);
#endif
   closedir(dp);

get_out:
   if (!found) {
      /* Restore VolumeName we really wanted */
      bstrncpy(dcr->VolumeName, VolumeName, sizeof(dcr->VolumeName));
      dcr->VolCatInfo = dcrVolCatInfo;     /* structure assignment */
      VolCatInfo = devVolCatInfo;          /* structure assignment */
   }
   Dsm_check(100);
   return found;
}

/**
 * Check if the Volume name has legal characters
 * If ua is non-NULL send the message
 */
static bool is_volume_name_legal(char *name)
{
   int len;
   const char *p;
   const char *accept = ":.-_/";

   if (name[0] == '/') {
      return false;
   }
   /* Restrict the characters permitted in the Volume name */
   for (p=name; *p; p++) {
      if (B_ISALPHA(*p) || B_ISDIGIT(*p) || strchr(accept, (int)(*p))) {
         continue;
      }
      return false;
   }
   len = strlen(name);
   if (len >= MAX_NAME_LENGTH) {
      return false;
   }
   if (len == 0) {
      return false;
   }
   return true;
}
