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
#include "include/bareos.h"
#include "dird.h"
#include "dird/autorecycle.h"
#include "dird/jcr_private.h"
#include "dird/next_vol.h"

namespace directordaemon {

/* Forward referenced functions */

bool FindRecycledVolume(JobControlRecord* jcr,
                        bool InChanger,
                        MediaDbRecord* mr,
                        StorageResource* store,
                        const char* unwanted_volumes)
{
  bstrncpy(mr->VolStatus, "Recycle", sizeof(mr->VolStatus));
  SetStorageidInMr(store, mr);
  if (jcr->db->FindNextVolume(jcr, 1, InChanger, mr, unwanted_volumes)) {
    jcr->impl->MediaId = mr->MediaId;
    Dmsg1(20, "Find_next_vol MediaId=%u\n", jcr->impl->MediaId);
    PmStrcpy(jcr->VolumeName, mr->VolumeName);
    SetStorageidInMr(store, mr);

    return true;
  }

  return false;
}

/**
 * Look for oldest Purged volume
 */
bool RecycleOldestPurgedVolume(JobControlRecord* jcr,
                               bool InChanger,
                               MediaDbRecord* mr,
                               StorageResource* store,
                               const char* unwanted_volumes)
{
  bstrncpy(mr->VolStatus, "Purged", sizeof(mr->VolStatus));
  SetStorageidInMr(store, mr);

  if (jcr->db->FindNextVolume(jcr, 1, InChanger, mr, unwanted_volumes)) {
    Dmsg1(20, "Find_next_vol MediaId=%u\n", mr->MediaId);
    SetStorageidInMr(store, mr);
    if (RecycleVolume(jcr, mr)) {
      Jmsg(jcr, M_INFO, 0, _("Recycled volume \"%s\"\n"), mr->VolumeName);
      Dmsg1(100, "return 1  RecycleOldestPurgedVolume Vol=%s\n",
            mr->VolumeName);

      return true;
    }
  }
  Dmsg0(100, "return 0  RecycleOldestPurgedVolume end\n");

  return false;
}

/**
 * Recycle the specified volume
 */
bool RecycleVolume(JobControlRecord* jcr, MediaDbRecord* mr)
{
  bstrncpy(mr->VolStatus, "Recycle", sizeof(mr->VolStatus));
  mr->VolJobs = mr->VolFiles = mr->VolBlocks = mr->VolErrors = 0;
  mr->VolBytes = 1;
  mr->FirstWritten = mr->LastWritten = 0;
  mr->RecycleCount++;
  mr->set_first_written = true;
  SetStorageidInMr(NULL, mr);

  return jcr->db->UpdateMediaRecord(jcr, mr);
}
} /* namespace directordaemon */
