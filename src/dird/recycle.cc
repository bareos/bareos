/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Planets Communications B.V.
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
 * Kern Sibbald, May MMII
 */
/**
 * @file
 * BAREOS Director -- Automatic Recycling of Volumes
 *                    Recycles Volumes that have been purged
 */
#include "bareos.h"
#include "dird.h"

/* Forward referenced functions */

bool find_recycled_volume(JCR *jcr, bool InChanger, MEDIA_DBR *mr,
                          STORERES *store, const char *unwanted_volumes)
{
   bstrncpy(mr->VolStatus, "Recycle", sizeof(mr->VolStatus));
   set_storageid_in_mr(store, mr);
   if (jcr->db->find_next_volume(jcr, 1, InChanger, mr, unwanted_volumes)) {
      jcr->MediaId = mr->MediaId;
      Dmsg1(20, "Find_next_vol MediaId=%u\n", jcr->MediaId);
      pm_strcpy(jcr->VolumeName, mr->VolumeName);
      set_storageid_in_mr(store, mr);

      return true;
   }

   return false;
}

/**
 * Look for oldest Purged volume
 */
bool recycle_oldest_purged_volume(JCR *jcr, bool InChanger, MEDIA_DBR *mr,
                                  STORERES *store, const char *unwanted_volumes)
{
   bstrncpy(mr->VolStatus, "Purged", sizeof(mr->VolStatus));
   set_storageid_in_mr(store, mr);

   if (jcr->db->find_next_volume(jcr, 1, InChanger, mr, unwanted_volumes)) {
      Dmsg1(20, "Find_next_vol MediaId=%u\n", mr->MediaId);
      set_storageid_in_mr(store, mr);
      if (recycle_volume(jcr, mr)) {
         Jmsg(jcr, M_INFO, 0, _("Recycled volume \"%s\"\n"), mr->VolumeName);
         Dmsg1(100, "return 1  recycle_oldest_purged_volume Vol=%s\n", mr->VolumeName);

         return true;
      }
   }
   Dmsg0(100, "return 0  recycle_oldest_purged_volume end\n");

   return false;
}

/**
 * Recycle the specified volume
 */
bool recycle_volume(JCR *jcr, MEDIA_DBR *mr)
{
   bstrncpy(mr->VolStatus, "Recycle", sizeof(mr->VolStatus));
   mr->VolJobs = mr->VolFiles = mr->VolBlocks = mr->VolErrors = 0;
   mr->VolBytes = 1;
   mr->FirstWritten = mr->LastWritten = 0;
   mr->RecycleCount++;
   mr->set_first_written = true;
   set_storageid_in_mr(NULL, mr);

   return jcr->db->update_media_record(jcr, mr);
}
