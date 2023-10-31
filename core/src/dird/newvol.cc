/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

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
// Kern Sibbald, May MMI
/**
 * @file
 * creates new Volumes in
 *                                catalog Media table from the
 *                                LabelFormat specification.
 *
 * This routine runs as a thread and must be thread reentrant.
 *
 * Basic tasks done here:
 *    If possible create a new Media entry
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/expand.h"
#include "dird/director_jcr_impl.h"
#include "dird/next_vol.h"
#include "cats/sql.h"
#include "dird/ua_db.h"
#include "dird/ua_label.h"
#include "lib/edit.h"

namespace directordaemon {

// Forward referenced functions
static bool CreateSimpleName(JobControlRecord* jcr,
                             MediaDbRecord* mr,
                             PoolDbRecord* pr);
static bool PerformFullNameSubstitution(JobControlRecord* jcr,
                                        MediaDbRecord* mr,
                                        PoolDbRecord* pr);

/**
 * Automatic Volume name creation using the LabelFormat
 *
 * The media record must have the PoolId filled in when
 * calling this routine.
 */
bool newVolume(JobControlRecord* jcr, MediaDbRecord* mr, StorageResource* store)
{
  PoolDbRecord pr;

  // See if we can create a new Volume
  DbLocker _{jcr->db};
  pr.PoolId = mr->PoolId;
  if (!jcr->db->GetPoolRecord(jcr, &pr)) { return false; }
  if (pr.MaxVols == 0 || pr.NumVols < pr.MaxVols) {
    *mr = MediaDbRecord{};
    SetPoolDbrDefaultsInMediaDbr(mr, &pr);
    jcr->VolumeName[0] = 0;
    bstrncpy(mr->MediaType, jcr->dir_impl->res.write_storage->media_type,
             sizeof(mr->MediaType));
    GeneratePluginEvent(jcr, bDirEventNewVolume); /* return void... */
    if (jcr->VolumeName[0] && IsVolumeNameLegal(NULL, jcr->VolumeName)) {
      bstrncpy(mr->VolumeName, jcr->VolumeName, sizeof(mr->VolumeName));
    } else if (pr.LabelFormat[0] && pr.LabelFormat[0] != '*') {
      // Check for special characters
      if (IsVolumeNameLegal(NULL, pr.LabelFormat)) {
        // No special characters, so apply simple algorithm
        if (!CreateSimpleName(jcr, mr, &pr)) { return false; }
      } else {
        // Found special characters, so try full substitution
        if (!PerformFullNameSubstitution(jcr, mr, &pr)) { return false; }
        if (!IsVolumeNameLegal(NULL, mr->VolumeName)) {
          Jmsg(jcr, M_ERROR, 0, T_("Illegal character in Volume name \"%s\"\n"),
               mr->VolumeName);
          return false;
        }
      }
    } else {
      return false;
    }
    pr.NumVols++;
    mr->Enabled = VOL_ENABLED;
    SetStorageidInMr(store, mr);
    if (jcr->db->CreateMediaRecord(jcr, mr)
        && jcr->db->UpdatePoolRecord(jcr, &pr)) {
      Jmsg(jcr, M_INFO, 0, T_("Created new Volume \"%s\" in catalog.\n"),
           mr->VolumeName);
      Dmsg1(90, "Created new Volume=%s\n", mr->VolumeName);
      return true;
    } else {
      Jmsg(jcr, M_ERROR, 0, "%s", jcr->db->strerror());
    }
  }

  return false;
}

static bool CreateSimpleName(JobControlRecord* jcr,
                             MediaDbRecord* mr,
                             PoolDbRecord* pr)
{
  char num[20];
  db_int64_ctx ctx;
  PoolMem query(PM_MESSAGE), name(PM_NAME);
  char ed1[50];

  /* See if volume already exists */
  mr->VolumeName[0] = 0;
  PmStrcpy(name, pr->LabelFormat);
  ctx.value = 0;
  Mmsg(query, "SELECT MAX(MediaId) FROM Media,Pool WHERE Pool.PoolId=%s",
       edit_int64(pr->PoolId, ed1));
  if (!jcr->db->SqlQuery(query.c_str(), db_int64_handler, (void*)&ctx)) {
    Jmsg(jcr, M_WARNING, 0, T_("SQL failed, but ignored. ERR=%s\n"),
         jcr->db->strerror());
    ctx.value = pr->NumVols + 1;
  }
  for (int i = (int)ctx.value + 1; i < (int)ctx.value + 100; i++) {
    MediaDbRecord tmr;

    sprintf(num, "%04d", i);
    bstrncpy(tmr.VolumeName, name.c_str(), sizeof(tmr.VolumeName));
    bstrncat(tmr.VolumeName, num, sizeof(tmr.VolumeName));
    if (jcr->db->GetMediaRecord(jcr, &tmr)) {
      Jmsg(jcr, M_WARNING, 0,
           T_("Wanted to create Volume \"%s\", but it already exists. Trying "
             "again.\n"),
           tmr.VolumeName);
      continue;
    }
    bstrncpy(mr->VolumeName, name.c_str(), sizeof(mr->VolumeName));
    bstrncat(mr->VolumeName, num, sizeof(mr->VolumeName));
    break; /* Got good name */
  }
  if (mr->VolumeName[0] == 0) {
    Jmsg(jcr, M_ERROR, 0,
         T_("Too many failures. Giving up creating Volume name.\n"));
    return false;
  }
  return true;
}

// Perform full substitution on Label
static bool PerformFullNameSubstitution(JobControlRecord* jcr,
                                        MediaDbRecord* mr,
                                        PoolDbRecord* pr)
{
  bool ok = false;
  POOLMEM* label = GetPoolMemory(PM_FNAME);

  jcr->dir_impl->NumVols = pr->NumVols;
  if (VariableExpansion(jcr, pr->LabelFormat, label)) {
    bstrncpy(mr->VolumeName, label, sizeof(mr->VolumeName));
    ok = true;
  }
  FreePoolMemory(label);

  return ok;
}
} /* namespace directordaemon */
