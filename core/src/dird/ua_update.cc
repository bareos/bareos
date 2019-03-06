/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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
 * Split from ua_cmds.c March 2005
 * Kern Sibbald, September MM
 */
/**
 * @file
 * Update command processing
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/next_vol.h"
#include "dird/sd_cmds.h"
#include "dird/storage.h"
#include "dird/ua_db.h"
#include "dird/ua_input.h"
#include "dird/ua_select.h"
#include "lib/edit.h"
#include "lib/util.h"

namespace directordaemon {

/* Forward referenced functions */
static bool UpdateVolume(UaContext* ua);
static bool UpdatePool(UaContext* ua);
static bool UpdateJob(UaContext* ua);
static bool UpdateStats(UaContext* ua);
static void UpdateSlots(UaContext* ua);

/**
 * Update a Pool Record in the database.
 *  It is always updated from the Resource record.
 *
 *    update volume pool=<pool-name> volume=<volume-name>
 *         changes pool info for volume
 *    update pool=<pool-name>
 *         updates pool from Pool resource
 *    update slots[=..] [scan]
 *         updates autochanger slots
 *    update jobid[=..] ...
 *         update job information
 *    update stats [days=...]
 *         updates long term statistics
 */
bool UpdateCmd(UaContext* ua, const char* cmd)
{
  static const char* kw[] = {NT_("media"),  /* 0 */
                             NT_("volume"), /* 1 */
                             NT_("pool"),   /* 2 */
                             NT_("slots"),  /* 3 */
                             NT_("slot"),   /* 4 */
                             NT_("jobid"),  /* 5 */
                             NT_("stats"),  /* 6 */
                             NULL};

  if (!OpenClientDb(ua)) { return true; }

  switch (FindArgKeyword(ua, kw)) {
    case 0:
    case 1:
      UpdateVolume(ua);
      return true;
    case 2:
      UpdatePool(ua);
      return true;
    case 3:
    case 4:
      UpdateSlots(ua);
      return true;
    case 5:
      UpdateJob(ua);
      return true;
    case 6:
      UpdateStats(ua);
      return true;
    default:
      break;
  }

  StartPrompt(ua, _("Update choice:\n"));
  AddPrompt(ua, _("Volume parameters"));
  AddPrompt(ua, _("Pool from resource"));
  AddPrompt(ua, _("Slots from autochanger"));
  AddPrompt(ua, _("Long term statistics"));
  switch (
      DoPrompt(ua, _("item"), _("Choose catalog item to update"), NULL, 0)) {
    case 0:
      UpdateVolume(ua);
      break;
    case 1:
      UpdatePool(ua);
      break;
    case 2:
      UpdateSlots(ua);
      break;
    case 3:
      UpdateStats(ua);
      break;
    default:
      break;
  }
  return true;
}

static void UpdateVolstatus(UaContext* ua, const char* val, MediaDbRecord* mr)
{
  PoolMem query(PM_MESSAGE);
  const char* kw[] = {NT_("Append"),   NT_("Archive"),
                      NT_("Disabled"), NT_("Full"),
                      NT_("Used"),     NT_("Cleaning"),
                      NT_("Recycle"),  NT_("Read-Only"),
                      NT_("Error"),    NULL};
  bool found = false;
  int i;

  for (i = 0; kw[i]; i++) {
    if (Bstrcasecmp(val, kw[i])) {
      found = true;
      break;
    }
  }
  if (!found) {
    ua->ErrorMsg(_("Invalid VolStatus specified: %s\n"), val);
  } else {
    char ed1[50];
    bstrncpy(mr->VolStatus, kw[i], sizeof(mr->VolStatus));
    Mmsg(query, "UPDATE Media SET VolStatus='%s' WHERE MediaId=%s",
         mr->VolStatus, edit_int64(mr->MediaId, ed1));
    if (!ua->db->SqlQuery(query.c_str())) {
      ua->ErrorMsg("%s", ua->db->strerror());
    } else {
      ua->InfoMsg(_("New Volume status is: %s\n"), mr->VolStatus);
    }
  }
}

static void UpdateVolretention(UaContext* ua, char* val, MediaDbRecord* mr)
{
  char ed1[150], ed2[50];
  PoolMem query(PM_MESSAGE);

  if (!DurationToUtime(val, &mr->VolRetention)) {
    ua->ErrorMsg(_("Invalid retention period specified: %s\n"), val);
    return;
  }
  Mmsg(query, "UPDATE Media SET VolRetention=%s WHERE MediaId=%s",
       edit_uint64(mr->VolRetention, ed1), edit_int64(mr->MediaId, ed2));
  if (!ua->db->SqlQuery(query.c_str())) {
    ua->ErrorMsg("%s", ua->db->strerror());
  } else {
    ua->InfoMsg(_("New retention period is: %s\n"),
                edit_utime(mr->VolRetention, ed1, sizeof(ed1)));
  }
}

static void UpdateVoluseduration(UaContext* ua, char* val, MediaDbRecord* mr)
{
  char ed1[150], ed2[50];
  PoolMem query(PM_MESSAGE);

  if (!DurationToUtime(val, &mr->VolUseDuration)) {
    ua->ErrorMsg(_("Invalid use duration specified: %s\n"), val);
    return;
  }
  Mmsg(query, "UPDATE Media SET VolUseDuration=%s WHERE MediaId=%s",
       edit_uint64(mr->VolUseDuration, ed1), edit_int64(mr->MediaId, ed2));
  if (!ua->db->SqlQuery(query.c_str())) {
    ua->ErrorMsg("%s", ua->db->strerror());
  } else {
    ua->InfoMsg(_("New use duration is: %s\n"),
                edit_utime(mr->VolUseDuration, ed1, sizeof(ed1)));
  }
}

static void UpdateVolmaxjobs(UaContext* ua, char* val, MediaDbRecord* mr)
{
  PoolMem query(PM_MESSAGE);
  char ed1[50];

  Mmsg(query, "UPDATE Media SET MaxVolJobs=%s WHERE MediaId=%s", val,
       edit_int64(mr->MediaId, ed1));
  if (!ua->db->SqlQuery(query.c_str())) {
    ua->ErrorMsg("%s", ua->db->strerror());
  } else {
    ua->InfoMsg(_("New max jobs is: %s\n"), val);
  }
}

static void UpdateVolmaxfiles(UaContext* ua, char* val, MediaDbRecord* mr)
{
  PoolMem query(PM_MESSAGE);
  char ed1[50];

  Mmsg(query, "UPDATE Media SET MaxVolFiles=%s WHERE MediaId=%s", val,
       edit_int64(mr->MediaId, ed1));
  if (!ua->db->SqlQuery(query.c_str())) {
    ua->ErrorMsg("%s", ua->db->strerror());
  } else {
    ua->InfoMsg(_("New max files is: %s\n"), val);
  }
}

static void UpdateVolmaxbytes(UaContext* ua, char* val, MediaDbRecord* mr)
{
  uint64_t maxbytes;
  char ed1[50], ed2[50];
  PoolMem query(PM_MESSAGE);

  if (!size_to_uint64(val, &maxbytes)) {
    ua->ErrorMsg(_("Invalid max. bytes specification: %s\n"), val);
    return;
  }
  Mmsg(query, "UPDATE Media SET MaxVolBytes=%s WHERE MediaId=%s",
       edit_uint64(maxbytes, ed1), edit_int64(mr->MediaId, ed2));
  if (!ua->db->SqlQuery(query.c_str())) {
    ua->ErrorMsg("%s", ua->db->strerror());
  } else {
    ua->InfoMsg(_("New Max bytes is: %s\n"), edit_uint64(maxbytes, ed1));
  }
}

static void UpdateVolrecycle(UaContext* ua, char* val, MediaDbRecord* mr)
{
  bool recycle;
  char ed1[50];
  PoolMem query(PM_MESSAGE);

  if (!IsYesno(val, &recycle)) {
    ua->ErrorMsg(_("Invalid value. It must be yes or no.\n"));
    return;
  }

  Mmsg(query, "UPDATE Media SET Recycle=%d WHERE MediaId=%s", recycle ? 1 : 0,
       edit_int64(mr->MediaId, ed1));

  if (!ua->db->SqlQuery(query.c_str())) {
    ua->ErrorMsg("%s", ua->db->strerror());
  } else {
    ua->InfoMsg(_("New Recycle flag is: %s\n"), recycle ? _("yes") : _("no"));
  }
}

static void UpdateVolinchanger(UaContext* ua, char* val, MediaDbRecord* mr)
{
  char ed1[50];
  bool InChanger;
  PoolMem query(PM_MESSAGE);

  if (!IsYesno(val, &InChanger)) {
    ua->ErrorMsg(_("Invalid value. It must be yes or no.\n"));
    return;
  }

  Mmsg(query, "UPDATE Media SET InChanger=%d WHERE MediaId=%s",
       InChanger ? 1 : 0, edit_int64(mr->MediaId, ed1));

  if (!ua->db->SqlQuery(query.c_str())) {
    ua->ErrorMsg("%s", ua->db->strerror());
  } else {
    ua->InfoMsg(_("New InChanger flag is: %s\n"),
                InChanger ? _("yes") : _("no"));
  }
}

static void UpdateVolslot(UaContext* ua, char* val, MediaDbRecord* mr)
{
  PoolDbRecord pr;

  memset(&pr, 0, sizeof(pr));
  pr.PoolId = mr->PoolId;
  if (!ua->db->GetPoolRecord(ua->jcr, &pr)) {
    ua->ErrorMsg("%s", ua->db->strerror());
    return;
  }
  mr->Slot = atoi(val);
  if (pr.MaxVols > 0 && mr->Slot > (int)pr.MaxVols) {
    ua->ErrorMsg(_("Invalid slot, it must be between 0 and MaxVols=%d\n"),
                 pr.MaxVols);
    return;
  }
  /*
   * Make sure to use db_update... rather than doing this directly,
   * so that any Slot is handled correctly.
   */
  SetStorageidInMr(NULL, mr);
  if (!ua->db->UpdateMediaRecord(ua->jcr, mr)) {
    ua->ErrorMsg(_("Error updating media record Slot: ERR=%s"),
                 ua->db->strerror());
  } else {
    ua->InfoMsg(_("New Slot is: %d\n"), mr->Slot);
  }
}

/**
 * Modify the Pool in which this Volume is located
 */
void UpdateVolPool(UaContext* ua,
                   char* val,
                   MediaDbRecord* mr,
                   PoolDbRecord* opr)
{
  PoolDbRecord pr;
  PoolMem query(PM_MESSAGE);
  char ed1[50], ed2[50];

  memset(&pr, 0, sizeof(pr));
  bstrncpy(pr.Name, val, sizeof(pr.Name));
  if (!GetPoolDbr(ua, &pr)) { return; }
  mr->PoolId = pr.PoolId; /* set new PoolId */

  DbLock(ua->db);
  Mmsg(query, "UPDATE Media SET PoolId=%s WHERE MediaId=%s",
       edit_int64(mr->PoolId, ed1), edit_int64(mr->MediaId, ed2));
  if (!ua->db->SqlQuery(query.c_str())) {
    ua->ErrorMsg("%s", ua->db->strerror());
  } else {
    ua->InfoMsg(_("New Pool is: %s\n"), pr.Name);
    opr->NumVols--;
    if (!ua->db->UpdatePoolRecord(ua->jcr, opr)) {
      ua->ErrorMsg("%s", ua->db->strerror());
    }
    pr.NumVols++;
    if (!ua->db->UpdatePoolRecord(ua->jcr, &pr)) {
      ua->ErrorMsg("%s", ua->db->strerror());
    }
  }
  DbUnlock(ua->db);
}

/**
 * Modify the RecyclePool of a Volume
 */
void UpdateVolRecyclepool(UaContext* ua, char* val, MediaDbRecord* mr)
{
  PoolDbRecord pr;
  PoolMem query(PM_MESSAGE);
  char ed1[50], ed2[50];
  const char* poolname;

  if (val && *val) { /* update volume recyclepool="Scratch" */
    /*
     * If a pool name is given, look up the PoolId
     */
    memset(&pr, 0, sizeof(pr));
    bstrncpy(pr.Name, val, sizeof(pr.Name));
    if (!GetPoolDbr(ua, &pr, NT_("recyclepool"))) { return; }
    mr->RecyclePoolId = pr.PoolId; /* get the PoolId */
    poolname = pr.Name;
  } else { /* update volume recyclepool="" */
    /*
     * If no pool name is given, set the PoolId to 0 (the default)
     */
    mr->RecyclePoolId = 0;
    poolname = _("*None*");
  }

  DbLock(ua->db);
  Mmsg(query, "UPDATE Media SET RecyclePoolId=%s WHERE MediaId=%s",
       edit_int64(mr->RecyclePoolId, ed1), edit_int64(mr->MediaId, ed2));
  if (!ua->db->SqlQuery(query.c_str())) {
    ua->ErrorMsg("%s", ua->db->strerror());
  } else {
    ua->InfoMsg(_("New RecyclePool is: %s\n"), poolname);
  }
  DbUnlock(ua->db);
}

/**
 * Modify the Storage in which this Volume is located
 */
void UpdateVolStorage(UaContext* ua, char* val, MediaDbRecord* mr)
{
  StorageDbRecord sr;
  PoolMem query(PM_MESSAGE);
  char ed1[50], ed2[50];

  memset(&sr, 0, sizeof(sr));
  bstrncpy(sr.Name, val, sizeof(sr.Name));
  if (!GetStorageDbr(ua, &sr)) { return; }
  mr->StorageId = sr.StorageId; /* set new StorageId */

  DbLock(ua->db);
  Mmsg(query, "UPDATE Media SET StorageId=%s WHERE MediaId=%s",
       edit_int64(mr->StorageId, ed1), edit_int64(mr->MediaId, ed2));
  if (!ua->db->SqlQuery(query.c_str())) {
    ua->ErrorMsg("%s", ua->db->strerror());
  }

  DbUnlock(ua->db);
}

/**
 * Refresh the Volume information from the Pool record
 */
static void UpdateVolFromPool(UaContext* ua, MediaDbRecord* mr)
{
  PoolDbRecord pr;

  memset(&pr, 0, sizeof(pr));
  pr.PoolId = mr->PoolId;
  if (!ua->db->GetPoolRecord(ua->jcr, &pr) ||
      !ua->AclAccessOk(Pool_ACL, pr.Name, true)) {
    return;
  }
  SetPoolDbrDefaultsInMediaDbr(mr, &pr);
  if (!ua->db->UpdateMediaDefaults(ua->jcr, mr)) {
    ua->ErrorMsg(_("Error updating Volume record: ERR=%s"), ua->db->strerror());
  } else {
    ua->InfoMsg(_("Volume defaults updated from \"%s\" Pool record.\n"),
                pr.Name);
  }
}

/**
 * Refresh the Volume information from the Pool record for all Volumes
 */
static void UpdateAllVolsFromPool(UaContext* ua, const char* pool_name)
{
  PoolDbRecord pr;
  MediaDbRecord mr;

  memset(&pr, 0, sizeof(pr));
  memset(&mr, 0, sizeof(mr));

  bstrncpy(pr.Name, pool_name, sizeof(pr.Name));
  if (!GetPoolDbr(ua, &pr)) { return; }
  SetPoolDbrDefaultsInMediaDbr(&mr, &pr);
  mr.PoolId = pr.PoolId;
  if (!ua->db->UpdateMediaDefaults(ua->jcr, &mr)) {
    ua->ErrorMsg(_("Error updating Volume records: ERR=%s"),
                 ua->db->strerror());
  } else {
    ua->InfoMsg(_("All Volume defaults updated from \"%s\" Pool record.\n"),
                pr.Name);
  }
}

static void UpdateAllVols(UaContext* ua)
{
  int i, num_pools;
  uint32_t* ids;
  PoolDbRecord pr;
  MediaDbRecord mr;

  memset(&pr, 0, sizeof(pr));
  memset(&mr, 0, sizeof(mr));

  if (!ua->db->GetPoolIds(ua->jcr, &num_pools, &ids)) {
    ua->ErrorMsg(_("Error obtaining pool ids. ERR=%s\n"), ua->db->strerror());
    return;
  }

  for (i = 0; i < num_pools; i++) {
    pr.PoolId = ids[i];
    if (!ua->db->GetPoolRecord(ua->jcr, &pr)) {
      ua->WarningMsg(_("Updating all pools, but skipped PoolId=%d. ERR=%s\n"),
                     ua->db->strerror());
      continue;
    }

    /*
     * Check access to pool.
     */
    if (!ua->AclAccessOk(Pool_ACL, pr.Name, false)) { continue; }

    SetPoolDbrDefaultsInMediaDbr(&mr, &pr);
    mr.PoolId = pr.PoolId;

    if (!ua->db->UpdateMediaDefaults(ua->jcr, &mr)) {
      ua->ErrorMsg(_("Error updating Volume records: ERR=%s"),
                   ua->db->strerror());
    } else {
      ua->InfoMsg(_("All Volume defaults updated from \"%s\" Pool record.\n"),
                  pr.Name);
    }
  }

  free(ids);
}

static void UpdateVolenabled(UaContext* ua, char* val, MediaDbRecord* mr)
{
  mr->Enabled = GetEnabled(ua, val);
  if (mr->Enabled < 0) { return; }
  SetStorageidInMr(NULL, mr);
  if (!ua->db->UpdateMediaRecord(ua->jcr, mr)) {
    ua->ErrorMsg(_("Error updating media record Enabled: ERR=%s"),
                 ua->db->strerror());
  } else {
    ua->InfoMsg(_("New Enabled is: %d\n"), mr->Enabled);
  }
}

static void UpdateVolActiononpurge(UaContext* ua, char* val, MediaDbRecord* mr)
{
  PoolMem ret;
  if (Bstrcasecmp(val, "truncate")) {
    mr->ActionOnPurge = ON_PURGE_TRUNCATE;
  } else {
    mr->ActionOnPurge = 0;
  }

  SetStorageidInMr(NULL, mr);
  if (!ua->db->UpdateMediaRecord(ua->jcr, mr)) {
    ua->ErrorMsg(_("Error updating media record ActionOnPurge: ERR=%s"),
                 ua->db->strerror());
  } else {
    ua->InfoMsg(_("New ActionOnPurge is: %s\n"),
                action_on_purge_to_string(mr->ActionOnPurge, ret));
  }
}

/**
 * Update a media record -- allows you to change the
 *  Volume status. E.g. if you want BAREOS to stop
 *  writing on the volume, set it to anything other
 *  than Append.
 */
static bool UpdateVolume(UaContext* ua)
{
  PoolResource* pool;
  POOLMEM* query;
  PoolMem ret;
  char buf[1000];
  char ed1[130];
  bool done = false;
  int i;
  const char* kw[] = {NT_("VolStatus"),     /* 0 */
                      NT_("VolRetention"),  /* 1 */
                      NT_("VolUse"),        /* 2 */
                      NT_("MaxVolJobs"),    /* 3 */
                      NT_("MaxVolFiles"),   /* 4 */
                      NT_("MaxVolBytes"),   /* 5 */
                      NT_("Recycle"),       /* 6 */
                      NT_("InChanger"),     /* 7 */
                      NT_("Slot"),          /* 8 */
                      NT_("Pool"),          /* 9 */
                      NT_("FromPool"),      /* 10 */
                      NT_("AllFromPool"),   /* 11 !!! see below !!! */
                      NT_("Enabled"),       /* 12 */
                      NT_("RecyclePool"),   /* 13 */
                      NT_("ActionOnPurge"), /* 14 */
                      NT_("Storage"),       /* 15 */
                      NULL};

#define AllFromPool 11 /* keep this updated with above */

  for (i = 0; kw[i]; i++) {
    int j;
    PoolDbRecord pr;
    MediaDbRecord mr;

    memset(&pr, 0, sizeof(pr));
    memset(&mr, 0, sizeof(mr));

    if ((j = FindArgWithValue(ua, kw[i])) > 0) {
      /* If all from pool don't select a media record */
      if (i != AllFromPool && !SelectMediaDbr(ua, &mr)) { return false; }
      switch (i) {
        case 0:
          UpdateVolstatus(ua, ua->argv[j], &mr);
          break;
        case 1:
          UpdateVolretention(ua, ua->argv[j], &mr);
          break;
        case 2:
          UpdateVoluseduration(ua, ua->argv[j], &mr);
          break;
        case 3:
          UpdateVolmaxjobs(ua, ua->argv[j], &mr);
          break;
        case 4:
          UpdateVolmaxfiles(ua, ua->argv[j], &mr);
          break;
        case 5:
          UpdateVolmaxbytes(ua, ua->argv[j], &mr);
          break;
        case 6:
          UpdateVolrecycle(ua, ua->argv[j], &mr);
          break;
        case 7:
          UpdateVolinchanger(ua, ua->argv[j], &mr);
          break;
        case 8:
          UpdateVolslot(ua, ua->argv[j], &mr);
          break;
        case 9:
          pr.PoolId = mr.PoolId;
          if (!ua->db->GetPoolRecord(ua->jcr, &pr)) {
            ua->ErrorMsg("%s", ua->db->strerror());
            break;
          }
          UpdateVolPool(ua, ua->argv[j], &mr, &pr);
          break;
        case 10:
          UpdateVolFromPool(ua, &mr);
          return true;
        case 11:
          UpdateAllVolsFromPool(ua, ua->argv[j]);
          return true;
        case 12:
          UpdateVolenabled(ua, ua->argv[j], &mr);
          break;
        case 13:
          UpdateVolRecyclepool(ua, ua->argv[j], &mr);
          break;
        case 14:
          UpdateVolActiononpurge(ua, ua->argv[j], &mr);
          break;
        case 15:
          UpdateVolStorage(ua, ua->argv[j], &mr);
          break;
      }
      done = true;
    }
  }

  /* Allow user to simply update all volumes */
  if (FindArg(ua, NT_("fromallpools")) > 0) {
    UpdateAllVols(ua);
    return true;
  }

  for (; !done;) {
    PoolDbRecord pr;
    MediaDbRecord mr;
    StorageDbRecord sr;

    memset(&pr, 0, sizeof(pr));
    memset(&mr, 0, sizeof(mr));
    memset(&sr, 0, sizeof(sr));

    StartPrompt(ua, _("Parameters to modify:\n"));
    AddPrompt(ua, _("Volume Status"));              /* 0 */
    AddPrompt(ua, _("Volume Retention Period"));    /* 1 */
    AddPrompt(ua, _("Volume Use Duration"));        /* 2 */
    AddPrompt(ua, _("Maximum Volume Jobs"));        /* 3 */
    AddPrompt(ua, _("Maximum Volume Files"));       /* 4 */
    AddPrompt(ua, _("Maximum Volume Bytes"));       /* 5 */
    AddPrompt(ua, _("Recycle Flag"));               /* 6 */
    AddPrompt(ua, _("Slot"));                       /* 7 */
    AddPrompt(ua, _("InChanger Flag"));             /* 8 */
    AddPrompt(ua, _("Volume Files"));               /* 9 */
    AddPrompt(ua, _("Pool"));                       /* 10 */
    AddPrompt(ua, _("Volume from Pool"));           /* 11 */
    AddPrompt(ua, _("All Volumes from Pool"));      /* 12 */
    AddPrompt(ua, _("All Volumes from all Pools")); /* 13 */
    AddPrompt(ua, _("Enabled")),                    /* 14 */
        AddPrompt(ua, _("RecyclePool")),            /* 15 */
        AddPrompt(ua, _("Action On Purge")),        /* 16 */
        AddPrompt(ua, _("Storage")),                /* 17 */
        AddPrompt(ua, _("Done"));                   /* 18 */
    i = DoPrompt(ua, "", _("Select parameter to modify"), NULL, 0);

    /* For All Volumes, All Volumes from Pool, and Done, we don't need
     * a Volume record */
    if (i != 12 && i != 13 && i != 18) {
      if (!SelectMediaDbr(ua, &mr)) { /* Get Volume record */
        return false;
      }
      ua->InfoMsg(_("Updating Volume \"%s\"\n"), mr.VolumeName);
    }
    switch (i) {
      case 0: /* Volume Status */
        /* Modify Volume Status */
        ua->InfoMsg(_("Current Volume status is: %s\n"), mr.VolStatus);
        StartPrompt(ua, _("Possible Values are:\n"));
        AddPrompt(ua, NT_("Append"));
        AddPrompt(ua, NT_("Archive"));
        AddPrompt(ua, NT_("Disabled"));
        AddPrompt(ua, NT_("Full"));
        AddPrompt(ua, NT_("Used"));
        AddPrompt(ua, NT_("Cleaning"));
        if (bstrcmp(mr.VolStatus, NT_("Purged"))) {
          AddPrompt(ua, NT_("Recycle"));
        }
        AddPrompt(ua, NT_("Read-Only"));
        if (DoPrompt(ua, "", _("Choose new Volume Status"), ua->cmd,
                     sizeof(mr.VolStatus)) < 0) {
          return true;
        }
        UpdateVolstatus(ua, ua->cmd, &mr);
        break;
      case 1: /* Retention */
        ua->InfoMsg(_("Current retention period is: %s\n"),
                    edit_utime(mr.VolRetention, ed1, sizeof(ed1)));
        if (!GetCmd(ua, _("Enter Volume Retention period: "))) { return false; }
        UpdateVolretention(ua, ua->cmd, &mr);
        break;

      case 2: /* Use Duration */
        ua->InfoMsg(_("Current use duration is: %s\n"),
                    edit_utime(mr.VolUseDuration, ed1, sizeof(ed1)));
        if (!GetCmd(ua, _("Enter Volume Use Duration: "))) { return false; }
        UpdateVoluseduration(ua, ua->cmd, &mr);
        break;

      case 3: /* Max Jobs */
        ua->InfoMsg(_("Current max jobs is: %u\n"), mr.MaxVolJobs);
        if (!GetPint(ua, _("Enter new Maximum Jobs: "))) { return false; }
        UpdateVolmaxjobs(ua, ua->cmd, &mr);
        break;

      case 4: /* Max Files */
        ua->InfoMsg(_("Current max files is: %u\n"), mr.MaxVolFiles);
        if (!GetPint(ua, _("Enter new Maximum Files: "))) { return false; }
        UpdateVolmaxfiles(ua, ua->cmd, &mr);
        break;

      case 5: /* Max Bytes */
        ua->InfoMsg(_("Current value is: %s\n"),
                    edit_uint64(mr.MaxVolBytes, ed1));
        if (!GetCmd(ua, _("Enter new Maximum Bytes: "))) { return false; }
        UpdateVolmaxbytes(ua, ua->cmd, &mr);
        break;


      case 6: /* Recycle */
        ua->InfoMsg(_("Current recycle flag is: %s\n"),
                    (mr.Recycle == 1) ? _("yes") : _("no"));
        if (!GetYesno(ua, _("Enter new Recycle status: "))) { return false; }
        UpdateVolrecycle(ua, ua->cmd, &mr);
        break;

      case 7: /* Slot */
        ua->InfoMsg(_("Current Slot is: %d\n"), mr.Slot);
        if (!GetPint(ua, _("Enter new Slot: "))) { return false; }
        UpdateVolslot(ua, ua->cmd, &mr);
        break;

      case 8: /* InChanger */
        ua->InfoMsg(_("Current InChanger flag is: %d\n"), mr.InChanger);
        Bsnprintf(buf, sizeof(buf),
                  _("Set InChanger flag for Volume \"%s\": yes/no: "),
                  mr.VolumeName);
        if (!GetYesno(ua, buf)) { return false; }
        mr.InChanger = ua->pint32_val;
        /*
         * Make sure to use db_update... rather than doing this directly,
         *   so that any Slot is handled correctly.
         */
        SetStorageidInMr(NULL, &mr);
        if (!ua->db->UpdateMediaRecord(ua->jcr, &mr)) {
          ua->ErrorMsg(_("Error updating media record Slot: ERR=%s"),
                       ua->db->strerror());
        } else {
          ua->InfoMsg(_("New InChanger flag is: %d\n"), mr.InChanger);
        }
        break;


      case 9: /* Volume Files */
        int32_t VolFiles;
        ua->WarningMsg(
            _("Warning changing Volume Files can result\n"
              "in loss of data on your Volume\n\n"));
        ua->InfoMsg(_("Current Volume Files is: %u\n"), mr.VolFiles);
        if (!GetPint(ua, _("Enter new number of Files for Volume: "))) {
          return false;
        }
        VolFiles = ua->pint32_val;
        if (VolFiles != (int)(mr.VolFiles + 1)) {
          ua->WarningMsg(
              _("Normally, you should only increase Volume Files by one!\n"));
          if (!GetYesno(ua, _("Increase Volume Files? (yes/no): ")) ||
              !ua->pint32_val) {
            break;
          }
        }
        query = GetPoolMemory(PM_MESSAGE);
        Mmsg(query, "UPDATE Media SET VolFiles=%u WHERE MediaId=%s", VolFiles,
             edit_int64(mr.MediaId, ed1));
        if (!ua->db->SqlQuery(query)) {
          ua->ErrorMsg("%s", ua->db->strerror());
        } else {
          ua->InfoMsg(_("New Volume Files is: %u\n"), VolFiles);
        }
        FreePoolMemory(query);
        break;

      case 10: /* Volume's Pool */
        pr.PoolId = mr.PoolId;
        if (!ua->db->GetPoolRecord(ua->jcr, &pr)) {
          ua->ErrorMsg("%s", ua->db->strerror());
          return false;
        }
        ua->InfoMsg(_("Current Pool is: %s\n"), pr.Name);
        if (!GetCmd(ua, _("Enter new Pool name: "))) { return false; }
        UpdateVolPool(ua, ua->cmd, &mr, &pr);
        return true;

      case 11:
        UpdateVolFromPool(ua, &mr);
        return true;
      case 12:
        pool = select_pool_resource(ua);
        if (pool) { UpdateAllVolsFromPool(ua, pool->name()); }
        return true;

      case 13:
        UpdateAllVols(ua);
        return true;

      case 14:
        ua->InfoMsg(_("Current Enabled is: %d\n"), mr.Enabled);
        if (!GetCmd(ua, _("Enter new Enabled: "))) { return false; }

        UpdateVolenabled(ua, ua->cmd, &mr);
        break;

      case 15:
        pr.PoolId = mr.RecyclePoolId;
        if (ua->db->GetPoolRecord(ua->jcr, &pr)) {
          ua->InfoMsg(_("Current RecyclePool is: %s\n"), pr.Name);
        } else {
          ua->InfoMsg(_("No current RecyclePool\n"));
        }
        if (!SelectPoolDbr(ua, &pr, NT_("recyclepool"))) { return false; }
        UpdateVolRecyclepool(ua, pr.Name, &mr);
        return true;

      case 16:
        PmStrcpy(ret, "");
        ua->InfoMsg(_("Current ActionOnPurge is: %s\n"),
                    action_on_purge_to_string(mr.ActionOnPurge, ret));
        if (!GetCmd(ua,
                    _("Enter new ActionOnPurge (one of: Truncate, None): "))) {
          return false;
        }

        UpdateVolActiononpurge(ua, ua->cmd, &mr);
        break;

      case 17:
        sr.StorageId = mr.StorageId;
        if (ua->db->GetStorageRecord(ua->jcr, &sr)) {
          ua->InfoMsg(_("Current Storage is: %s\n"), sr.Name);
        } else {
          ua->InfoMsg(_("Warning, could not find current Storage\n"));
        }
        if (!SelectStorageDbr(ua, &sr, NT_("storage"))) { return false; }
        UpdateVolStorage(ua, sr.Name, &mr);
        ua->InfoMsg(_("New Storage is: %s\n"), sr.Name);
        return true;

      default: /* Done or error */
        ua->InfoMsg(_("Selection terminated.\n"));
        return true;
    }
  }
  return true;
}

/**
 * Update long term statistics
 */
static bool UpdateStats(UaContext* ua)
{
  int i = FindArgWithValue(ua, NT_("days"));
  utime_t since = 0;

  if (i >= 0) { since = ((int64_t)atoi(ua->argv[i]) * 24 * 60 * 60); }

  int nb = ua->db->UpdateStats(ua->jcr, since);
  ua->InfoMsg(_("Updating %i job(s).\n"), nb);

  return true;
}

/**
 * Update pool record -- pull info from current POOL resource
 */
static bool UpdatePool(UaContext* ua)
{
  int id;
  PoolDbRecord pr;
  char ed1[50];
  PoolResource* pool;
  PoolMem query(PM_MESSAGE);

  pool = get_pool_resource(ua);
  if (!pool) { return false; }

  memset(&pr, 0, sizeof(pr));
  bstrncpy(pr.Name, pool->name(), sizeof(pr.Name));
  if (!GetPoolDbr(ua, &pr)) { return false; }

  SetPooldbrFromPoolres(&pr, pool, POOL_OP_UPDATE); /* update */
  SetPooldbrReferences(ua->jcr, ua->db, &pr, pool);

  id = ua->db->UpdatePoolRecord(ua->jcr, &pr);
  if (id <= 0) {
    ua->ErrorMsg(_("UpdatePoolRecord returned %d. ERR=%s\n"), id,
                 ua->db->strerror());
  }
  ua->db->FillQuery(query, BareosDb::SQL_QUERY_list_pool,
                    edit_int64(pr.PoolId, ed1));
  ua->db->ListSqlQuery(ua->jcr, query.c_str(), ua->send, HORZ_LIST, true);
  ua->InfoMsg(_("Pool DB record updated from resource.\n"));

  return true;
}

/**
 * Update a Job record -- allows to change the fields in a Job record.
 */
static bool UpdateJob(UaContext* ua)
{
  int i;
  char ed1[50], ed2[50], ed3[50], ed4[50];
  PoolMem cmd(PM_MESSAGE);
  JobDbRecord jr;
  ClientDbRecord cr;
  utime_t StartTime;
  char* client_name = NULL;
  char* job_name = NULL;
  char* start_time = NULL;
  char job_type = '\0';
  DBId_t fileset_id = 0;
  const char* kw[] = {NT_("starttime"), /* 0 */
                      NT_("client"),    /* 1 */
                      NT_("filesetid"), /* 2 */
                      NT_("jobname"),   /* 3 */
                      NT_("jobtype"),   /* 4 */
                      NULL};

  Dmsg1(200, "cmd=%s\n", ua->cmd);
  i = FindArgWithValue(ua, NT_("jobid"));
  if (i < 0) {
    ua->ErrorMsg(_("Expect JobId keyword, not found.\n"));
    return false;
  }
  memset(&jr, 0, sizeof(jr));
  memset(&cr, 0, sizeof(cr));
  jr.JobId = str_to_int64(ua->argv[i]);
  if (!ua->db->GetJobRecord(ua->jcr, &jr)) {
    ua->ErrorMsg("%s", ua->db->strerror());
    return false;
  }

  for (i = 0; kw[i]; i++) {
    int j;
    if ((j = FindArgWithValue(ua, kw[i])) >= 0) {
      switch (i) {
        case 0: /* Start time */
          start_time = ua->argv[j];
          break;
        case 1: /* Client name */
          client_name = ua->argv[j];
          break;
        case 2: /* Fileset id */
          fileset_id = str_to_uint64(ua->argv[j]);
          break;
        case 3: /* Job name */
          job_name = ua->argv[j];
          break;
        case 4: /* Job Type */
          job_type = ua->argv[j][0];
          break;
      }
    }
  }
  if (!client_name && !start_time && !fileset_id && !job_name && !job_type) {
    ua->ErrorMsg(_(
        "Neither Client, StartTime, Filesetid, JobType nor Name specified.\n"));
    return false;
  }
  if (client_name) {
    if (!GetClientDbr(ua, &cr)) { return false; }
    jr.ClientId = cr.ClientId;
  }
  if (fileset_id) { jr.FileSetId = fileset_id; }
  if (job_name) { bstrncpy(jr.Name, job_name, MAX_NAME_LENGTH); }
  if (job_type) { jr.JobType = job_type; }
  if (start_time) {
    utime_t delta_start;

    StartTime = StrToUtime(start_time);
    if (StartTime == 0) {
      ua->ErrorMsg(_("Improper date format: %s\n"), ua->argv[i]);
      return false;
    }
    delta_start = StartTime - jr.StartTime;
    Dmsg3(200, "ST=%lld jr.ST=%lld delta=%lld\n", StartTime,
          (utime_t)jr.StartTime, delta_start);
    jr.StartTime = (time_t)StartTime;
    jr.SchedTime += (time_t)delta_start;
    jr.EndTime += (time_t)delta_start;
    jr.JobTDate += delta_start;
    /* Convert to DB times */
    bstrutime(jr.cStartTime, sizeof(jr.cStartTime), jr.StartTime);
    bstrutime(jr.cSchedTime, sizeof(jr.cSchedTime), jr.SchedTime);
    bstrutime(jr.cEndTime, sizeof(jr.cEndTime), jr.EndTime);
  }
  Mmsg(cmd,
       "UPDATE Job SET Name='%s', ClientId=%s,StartTime='%s',SchedTime='%s',"
       "EndTime='%s',JobTDate=%s, FileSetId='%s', Type='%c' WHERE JobId=%s",
       jr.Name, edit_int64(jr.ClientId, ed1), jr.cStartTime, jr.cSchedTime,
       jr.cEndTime, edit_uint64(jr.JobTDate, ed2),
       edit_uint64(jr.FileSetId, ed3), jr.JobType, edit_int64(jr.JobId, ed4));
  if (!ua->db->SqlQuery(cmd.c_str())) {
    ua->ErrorMsg("%s", ua->db->strerror());
    return false;
  }
  return true;
}

/**
 * Update Slots corresponding to Volumes in autochanger
 */
static void UpdateSlots(UaContext* ua)
{
  UnifiedStorageResource store;
  vol_list_t* vl;
  changer_vol_list_t* vol_list = NULL;
  MediaDbRecord mr;
  char* slot_list;
  bool scan;
  slot_number_t max_slots;
  drive_number_t drive = -1;
  int Enabled = VOL_ENABLED;
  bool have_enabled;
  int i;

  if (!OpenClientDb(ua)) { return; }
  store.store = get_storage_resource(ua, true, true);
  if (!store.store) { return; }
  PmStrcpy(store.store_source, _("command line"));
  SetWstorage(ua->jcr, &store);

  scan = FindArg(ua, NT_("scan")) >= 0;
  if (scan) { drive = GetStorageDrive(ua, store.store); }
  if ((i = FindArgWithValue(ua, NT_("Enabled"))) >= 0) {
    Enabled = GetEnabled(ua, ua->argv[i]);
    if (Enabled < 0) { return; }
    have_enabled = true;
  } else {
    have_enabled = false;
  }

  max_slots = GetNumSlots(ua, ua->jcr->res.write_storage);
  Dmsg1(100, "max_slots=%d\n", max_slots);
  if (max_slots <= 0) {
    ua->WarningMsg(_("No slots in changer to scan.\n"));
    return;
  }

  slot_list = (char*)malloc(NbytesForBits(max_slots));
  ClearAllBits(max_slots, slot_list);
  if (!GetUserSlotList(ua, slot_list, "slots", max_slots)) {
    free(slot_list);
    return;
  }

  vol_list = get_vol_list_from_storage(ua, store.store, false, scan, false);
  if (!vol_list) {
    ua->WarningMsg(_("No Volumes found to update, or no barcodes.\n"));
    goto bail_out;
  }

  /*
   * First zap out any InChanger with StorageId=0
   */
  ua->db->SqlQuery("UPDATE Media SET InChanger=0 WHERE StorageId=0");

  /*
   * Walk through the list updating the media records
   */
  memset(&mr, 0, sizeof(mr));
  foreach_dlist (vl, vol_list->contents) {
    if (vl->bareos_slot_number > max_slots) {
      ua->WarningMsg(_("Slot %d greater than max %d ignored.\n"),
                     vl->bareos_slot_number, max_slots);
      continue;
    }
    /*
     * Check if user wants us to look at this slot
     */
    if (!BitIsSet(vl->bareos_slot_number - 1, slot_list)) {
      Dmsg1(100, "Skipping slot=%d\n", vl->bareos_slot_number);
      continue;
    }
    /*
     * If scanning, we read the label rather than the barcode
     */
    if (scan) {
      if (vl->VolName) {
        free(vl->VolName);
        vl->VolName = NULL;
      }
      vl->VolName = get_volume_name_from_SD(ua, vl->bareos_slot_number, drive);
      Dmsg2(100, "Got Vol=%s from SD for Slot=%d\n", vl->VolName,
            vl->bareos_slot_number);
    }
    ClearBit(vl->bareos_slot_number - 1, slot_list); /* clear Slot */
    SetStorageidInMr(store.store, &mr);
    mr.Slot = vl->bareos_slot_number;
    mr.InChanger = 1;
    mr.MediaId = 0; /* Get by VolumeName */
    if (vl->VolName) {
      bstrncpy(mr.VolumeName, vl->VolName, sizeof(mr.VolumeName));
    } else {
      mr.VolumeName[0] = 0;
    }
    SetStorageidInMr(store.store, &mr);

    Dmsg4(100, "Before make unique: Vol=%s slot=%d inchanger=%d sid=%d\n",
          mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);
    DbLock(ua->db);
    /*
     * Set InChanger to zero for this Slot
     */
    ua->db->MakeInchangerUnique(ua->jcr, &mr);
    DbUnlock(ua->db);
    Dmsg4(100, "After make unique: Vol=%s slot=%d inchanger=%d sid=%d\n",
          mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);

    if (!vl->VolName) {
      Dmsg1(100, "No VolName for Slot=%d setting InChanger to zero.\n",
            vl->bareos_slot_number);
      ua->InfoMsg(_("No VolName for Slot=%d InChanger set to zero.\n"),
                  vl->bareos_slot_number);
      continue;
    }

    DbLock(ua->db);
    Dmsg4(100, "Before get MR: Vol=%s slot=%d inchanger=%d sid=%d\n",
          mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);
    if (ua->db->GetMediaRecord(ua->jcr, &mr)) {
      Dmsg4(100, "After get MR: Vol=%s slot=%d inchanger=%d sid=%d\n",
            mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);
      /*
       * If Slot, Inchanger, and StorageId have changed, update the Media record
       */
      if (mr.Slot != vl->bareos_slot_number || !mr.InChanger ||
          mr.StorageId != store.store->StorageId) {
        mr.Slot = vl->bareos_slot_number;
        mr.InChanger = 1;
        if (have_enabled) { mr.Enabled = Enabled; }
        SetStorageidInMr(store.store, &mr);
        if (!ua->db->UpdateMediaRecord(ua->jcr, &mr)) {
          ua->ErrorMsg("%s", ua->db->strerror());
        } else {
          ua->InfoMsg(_("Catalog record for Volume \"%s\" updated to reference "
                        "slot %d.\n"),
                      mr.VolumeName, mr.Slot);
        }
      } else {
        ua->InfoMsg(_("Catalog record for Volume \"%s\" is up to date.\n"),
                    mr.VolumeName);
      }
    } else {
      ua->WarningMsg(_("Volume \"%s\" not found in catalog. Slot=%d InChanger "
                       "set to zero.\n"),
                     mr.VolumeName, vl->bareos_slot_number);
    }
    DbUnlock(ua->db);
  }

  memset(&mr, 0, sizeof(mr));
  mr.InChanger = 1;
  SetStorageidInMr(store.store, &mr);

  /*
   * Any slot not visited gets it Inchanger flag reset.
   */
  DbLock(ua->db);
  for (i = 1; i <= max_slots; i++) {
    if (BitIsSet(i - 1, slot_list)) {
      /*
       * Set InChanger to zero for this Slot
       */
      mr.Slot = i;
      ua->db->MakeInchangerUnique(ua->jcr, &mr);
    }
  }
  DbUnlock(ua->db);

bail_out:
  if (vol_list) { StorageReleaseVolList(store.store, vol_list); }
  free(slot_list);
  CloseSdBsock(ua);

  return;
}

/**
 * Update Slots corresponding to Volumes in autochanger.
 * We only update any new volume location of slots marked in
 * the given slot_list. If you want to do funky stuff
 * run an "update slots" with the options you want. This
 * is a simple function which syncs the info from the
 * vol_list to the database for each slot marked in
 * the slot_list.
 *
 * The vol_list passed here needs to be from an "autochanger listall" cmd.
 */
void UpdateSlotsFromVolList(UaContext* ua,
                            StorageResource* store,
                            changer_vol_list_t* vol_list,
                            char* slot_list)
{
  vol_list_t* vl;
  MediaDbRecord mr;

  if (!OpenClientDb(ua)) { return; }

  /*
   * Walk through the list updating the media records
   */
  foreach_dlist (vl, vol_list->contents) {
    /*
     * We are only interested in normal slots.
     */
    switch (vl->slot_type) {
      case slot_type_storage:
        break;
      default:
        continue;
    }

    /*
     * Only update entries of slots marked in the slot_list.
     */
    if (!BitIsSet(vl->bareos_slot_number - 1, slot_list)) { continue; }

    /*
     * Set InChanger to zero for this Slot
     */
    memset(&mr, 0, sizeof(mr));
    mr.Slot = vl->bareos_slot_number;
    mr.InChanger = 1;
    mr.MediaId = 0; /* Get by VolumeName */
    if (vl->VolName) {
      bstrncpy(mr.VolumeName, vl->VolName, sizeof(mr.VolumeName));
    } else {
      mr.VolumeName[0] = 0;
    }
    SetStorageidInMr(store, &mr);

    Dmsg4(100, "Before make unique: Vol=%s slot=%d inchanger=%d sid=%d\n",
          mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);
    DbLock(ua->db);

    /*
     * Set InChanger to zero for this Slot
     */
    ua->db->MakeInchangerUnique(ua->jcr, &mr);

    DbUnlock(ua->db);
    Dmsg4(100, "After make unique: Vol=%s slot=%d inchanger=%d sid=%d\n",
          mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);

    /*
     * See if there is anything in the slot.
     */
    switch (vl->slot_status) {
      case slot_status_full:
        if (!vl->VolName) {
          Dmsg1(100, "No VolName for Slot=%d setting InChanger to zero.\n",
                vl->bareos_slot_number);
          continue;
        }
        break;
      default:
        continue;
    }

    /*
     * There is something in the slot and it has a VolumeName so we can check
     * the database and perform an update if needed.
     */
    DbLock(ua->db);
    Dmsg4(100, "Before get MR: Vol=%s slot=%d inchanger=%d sid=%d\n",
          mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);
    if (ua->db->GetMediaRecord(ua->jcr, &mr)) {
      Dmsg4(100, "After get MR: Vol=%s slot=%d inchanger=%d sid=%d\n",
            mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);
      /* If Slot, Inchanger, and StorageId have changed, update the Media record
       */
      if (mr.Slot != vl->bareos_slot_number || !mr.InChanger ||
          mr.StorageId != store->StorageId) {
        mr.Slot = vl->bareos_slot_number;
        mr.InChanger = 1;
        SetStorageidInMr(store, &mr);
        if (!ua->db->UpdateMediaRecord(ua->jcr, &mr)) {
          ua->ErrorMsg("%s", ua->db->strerror());
        }
      }
    } else {
      ua->WarningMsg(_("Volume \"%s\" not found in catalog. Slot=%d InChanger "
                       "set to zero.\n"),
                     mr.VolumeName, vl->bareos_slot_number);
    }
    DbUnlock(ua->db);
  }
  return;
}

/**
 * Set the inchanger flag to zero for each slot marked in
 * the given slot_list.
 *
 * The vol_list passed here needs to be from an "autochanger listall" cmd.
 */
void UpdateInchangerForExport(UaContext* ua,
                              StorageResource* store,
                              changer_vol_list_t* vol_list,
                              char* slot_list)
{
  vol_list_t* vl;
  MediaDbRecord mr;

  if (!OpenClientDb(ua)) { return; }

  /*
   * Walk through the list updating the media records
   */
  foreach_dlist (vl, vol_list->contents) {
    /*
     * We are only interested in normal slots.
     */
    switch (vl->slot_type) {
      case slot_type_storage:
        break;
      default:
        continue;
    }

    /*
     * Only update entries of slots marked in the slot_list.
     */
    if (!BitIsSet(vl->bareos_slot_number - 1, slot_list)) { continue; }

    /*
     * Set InChanger to zero for this Slot
     */
    memset(&mr, 0, sizeof(mr));
    mr.Slot = vl->bareos_slot_number;
    mr.InChanger = 1;
    mr.MediaId = 0; /* Get by VolumeName */
    if (vl->VolName) {
      bstrncpy(mr.VolumeName, vl->VolName, sizeof(mr.VolumeName));
    } else {
      mr.VolumeName[0] = 0;
    }
    SetStorageidInMr(store, &mr);

    Dmsg4(100, "Before make unique: Vol=%s slot=%d inchanger=%d sid=%d\n",
          mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);
    DbLock(ua->db);

    /*
     * Set InChanger to zero for this Slot
     */
    ua->db->MakeInchangerUnique(ua->jcr, &mr);

    DbUnlock(ua->db);
    Dmsg4(100, "After make unique: Vol=%s slot=%d inchanger=%d sid=%d\n",
          mr.VolumeName, mr.Slot, mr.InChanger, mr.StorageId);
  }
  return;
}
} /* namespace directordaemon */
