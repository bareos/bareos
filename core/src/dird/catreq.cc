/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
// Kern Sibbald, March MMI
/**
 * @file
 * handles the message channel
 *             catalog request from the Storage daemon.
 *
 * This routine runs as a thread and must be thread reentrant.
 *
 * Basic tasks done here:
 *    Handle Catalog services.
 */

#include "include/fcntl_def.h"
#include "include/bareos.h"
#include "include/filetypes.h"
#include "include/streams.h"
#include "dird.h"
#include "dird/next_vol.h"
#include "dird/director_jcr_impl.h"
#include "dird/sd_cmds.h"
#include "findlib/find.h"
#include "lib/berrno.h"
#include "lib/edit.h"
#include "lib/util.h"
#include "lib/serial.h"

namespace directordaemon {

inline constexpr const char Find_media[]
    = "CatReq Job=%127s FindMedia=%d pool_name=%127s media_type=%127s "
      "unwanted_volumes=%s\n";
inline constexpr const char Get_Vol_Info[]
    = "CatReq Job=%127s GetVolInfo VolName=%127s write=%d\n";
inline constexpr const char Update_media[]
    = "CatReq Job=%127s UpdateMedia VolName=%s"
      " VolJobs=%u VolFiles=%u VolBlocks=%u VolBytes=%lld VolMounts=%u"
      " VolErrors=%u VolWrites=%u MaxVolBytes=%lld EndTime=%lld VolStatus=%10s"
      " Slot=%d relabel=%d InChanger=%d VolReadTime=%lld VolWriteTime=%lld"
      " VolFirstWritten=%lld\n";
inline constexpr const char Create_job_media[]
    = "CatReq Job=%127s CreateJobMedia "
      " FirstIndex=%u LastIndex=%u StartFile=%u EndFile=%u "
      " StartBlock=%u EndBlock=%u Copy=%d Strip=%d MediaId=%lld\n";

inline constexpr const char Update_filelist[]
    = "Catreq Job=%127s UpdateFileList\n";

inline constexpr const char Update_jobrecord[]
    = "Catreq Job=%127s UpdateJobRecord JobFiles=%" PRIu32 " JobBytes=%" PRIu64
      "\n";
inline constexpr const char Delete_nulljobmediarecord[]
    = "CatReq Job=%127s DeleteNullJobmediaRecords jobid=%u\n";

// Responses sent to Storage daemon
inline constexpr const char OK_media[]
    = "1000 OK VolName=%s VolJobs=%u VolFiles=%u"
      " VolBlocks=%u VolBytes=%s VolMounts=%u VolErrors=%u VolWrites=%u"
      " MaxVolBytes=%s VolCapacityBytes=%s VolStatus=%s Slot=%d"
      " MaxVolJobs=%u MaxVolFiles=%u InChanger=%d VolReadTime=%s"
      " VolWriteTime=%s EndFile=%u EndBlock=%u LabelType=%d"
      " MediaId=%s EncryptionKey=%s MinBlocksize=%d MaxBlocksize=%d\n";
inline constexpr const char OK_create[] = "1000 OK CreateJobMedia\n";
inline constexpr const char OK_delete[] = "1000 OK DeleteNullJobmediaRecords\n";

/*
 * Handle catalog request
 *  For now, we simply return next Volume to be used
 */

// Requests from the Storage daemon
static int SendVolumeInfoToStorageDaemon(JobControlRecord* jcr,
                                         BareosSocket* sd,
                                         MediaDbRecord* mr)
{
  int status;
  char ed1[50], ed2[50], ed3[50], ed4[50], ed5[50], ed6[50];

  jcr->dir_impl->MediaId = mr->MediaId;
  PmStrcpy(jcr->VolumeName, mr->VolumeName);
  BashSpaces(mr->VolumeName);
  status = sd->fsend(
      OK_media, mr->VolumeName, mr->VolJobs, mr->VolFiles, mr->VolBlocks,
      edit_uint64(mr->VolBytes, ed1), mr->VolMounts, mr->VolErrors,
      mr->VolWrites, edit_uint64(mr->MaxVolBytes, ed2),
      edit_uint64(mr->VolCapacityBytes, ed3), mr->VolStatus, mr->Slot,
      mr->MaxVolJobs, mr->MaxVolFiles, mr->InChanger,
      edit_int64(mr->VolReadTime, ed4), edit_int64(mr->VolWriteTime, ed5),
      mr->EndFile, mr->EndBlock, mr->LabelType, edit_uint64(mr->MediaId, ed6),
      mr->EncrKey, mr->MinBlocksize, mr->MaxBlocksize);
  UnbashSpaces(mr->VolumeName);
  Dmsg2(100, "Vol Info for %s: %s", jcr->Job, sd->msg);
  return status;
}

void CatalogRequest(JobControlRecord* jcr, BareosSocket* bs)
{
  MediaDbRecord mr, sdmr;
  JobMediaDbRecord jm;
  char Job[MAX_NAME_LENGTH];
  char pool_name[MAX_NAME_LENGTH];
  PoolMem unwanted_volumes(PM_MESSAGE);
  int index, ok, label, writing;
  POOLMEM* omsg;
  uint32_t Stripe, Copy;
  uint64_t MediaId;
  utime_t VolFirstWritten;
  utime_t VolLastWritten;
  std::uint32_t jobid;

  // Request to find next appendable Volume for this Job
  Dmsg1(100, "catreq %s", bs->msg);
  if (!jcr->db) {
    omsg = GetMemory(bs->message_length + 1);
    PmStrcpy(omsg, bs->msg);
    bs->fsend(T_("1990 Invalid Catalog Request: %s"), omsg);
    Jmsg1(jcr, M_FATAL, 0, T_("Invalid Catalog request; DB not open: %s"),
          omsg);
    FreeMemory(omsg);

    return;
  }

  uint32_t update_jobfiles = 0;
  uint64_t update_jobbytes = 0;

  // Find next appendable medium for SD
  unwanted_volumes.check_size(bs->message_length);
  if (sscanf(bs->msg, Find_media, &Job, &index, &pool_name, &mr.MediaType,
             unwanted_volumes.c_str())
      == 5) {
    PoolDbRecord pr;
    bstrncpy(pr.Name, pool_name, sizeof(pr.Name));
    UnbashSpaces(pr.Name);
    ok = jcr->db->GetPoolRecord(jcr, &pr);
    if (ok) {
      mr.PoolId = pr.PoolId;
      SetStorageidInMr(jcr->dir_impl->res.write_storage, &mr);
      mr.ScratchPoolId = pr.ScratchPoolId;
      ok = FindNextVolumeForAppend(jcr, &mr, index, unwanted_volumes.c_str(),
                                   fnv_create_vol, fnv_prune);
      Dmsg3(050, "find_media ok=%d idx=%d vol=%s\n", ok, index, mr.VolumeName);
    }

    if (ok) {
      SendVolumeInfoToStorageDaemon(jcr, bs, &mr);
    } else {
      bs->fsend(T_("1901 No Media.\n"));
      Dmsg0(500, "1901 No Media.\n");
    }
  } else if (sscanf(bs->msg, Get_Vol_Info, &Job, &mr.VolumeName, &writing)
             == 3) {
    // Request to find specific Volume information
    Dmsg1(100, "CatReq GetVolInfo Vol=%s\n", mr.VolumeName);

    // Find the Volume
    UnbashSpaces(mr.VolumeName);
    if (jcr->db->GetMediaRecord(jcr, &mr)) {
      const char* reason = NULL; /* detailed reason for rejection */
      /* If we are reading, accept any volume (reason == NULL)
       * If we are writing, check if the Volume is valid
       *   for this job, and do a recycle if necessary */
      if (writing) {
        /* SD wants to write this Volume, so make
         *   sure it is suitable for this job, i.e.
         *   Pool matches, and it is either Append or Recycle
         *   and Media Type matches and Pool allows any volume. */
        if (mr.PoolId != jcr->dir_impl->jr.PoolId) {
          reason = T_("not in Pool");
        } else if (!bstrcmp(mr.MediaType,
                            jcr->dir_impl->res.write_storage->media_type)) {
          reason = T_("not correct MediaType");
        } else {
          /* Now try recycling if necessary
           *   reason set non-NULL if we cannot use it */
          CheckIfVolumeValidOrRecyclable(jcr, &mr, &reason);
        }
      }

      if (!reason && mr.Enabled != VOL_ENABLED) {
        reason = T_("is not Enabled");
      }

      if (reason == NULL) {
        SendVolumeInfoToStorageDaemon(jcr, bs, &mr);
      } else {
        /* Not suitable volume */
        bs->fsend(T_("1998 Volume \"%s\" catalog status is %s, %s.\n"),
                  mr.VolumeName, mr.VolStatus, reason);
      }
    } else {
      bs->fsend(T_("1997 Volume \"%s\" not in catalog.\n"), mr.VolumeName);
      Dmsg1(100, "1997 Volume \"%s\" not in catalog.\n", mr.VolumeName);
    }
  } else if (sscanf(bs->msg, Update_media, &Job, &sdmr.VolumeName,
                    &sdmr.VolJobs, &sdmr.VolFiles, &sdmr.VolBlocks,
                    &sdmr.VolBytes, &sdmr.VolMounts, &sdmr.VolErrors,
                    &sdmr.VolWrites, &sdmr.MaxVolBytes, &VolLastWritten,
                    &sdmr.VolStatus, &sdmr.Slot, &label, &sdmr.InChanger,
                    &sdmr.VolReadTime, &sdmr.VolWriteTime, &VolFirstWritten)
             == 18) {
    /* Request to update Media record. Comes typically at the end
     * of a Storage daemon Job Session, when labeling/relabeling a
     * Volume, or when an EOF mark is written. */
    DbLocker _{jcr->db};
    Dmsg3(400, "Update media %s oldStat=%s newStat=%s\n", sdmr.VolumeName,
          mr.VolStatus, sdmr.VolStatus);
    bstrncpy(mr.VolumeName, sdmr.VolumeName,
             sizeof(mr.VolumeName)); /* copy Volume name */
    UnbashSpaces(mr.VolumeName);
    if (!jcr->db->GetMediaRecord(jcr, &mr)) {
      Jmsg(jcr, M_ERROR, 0,
           T_("Unable to get Media record for Volume %s: ERR=%s\n"),
           mr.VolumeName, jcr->db->strerror());
      bs->fsend(T_("1991 Catalog Request for vol=%s failed: %s"), mr.VolumeName,
                jcr->db->strerror());
      goto bail_out;
    }

    // Set first written time if this is first job
    if (mr.FirstWritten == 0) {
      if (VolFirstWritten == 0) {
        mr.FirstWritten
            = jcr->start_time; /* use Job start time as first write */
      } else {
        mr.FirstWritten = VolFirstWritten;
      }
      mr.set_first_written = true;
    }

    // If we just labeled the tape set time
    if (label || mr.LabelDate == 0) {
      mr.LabelDate = jcr->start_time;
      mr.set_label_date = true;
      if (mr.InitialWrite == 0) { mr.InitialWrite = jcr->start_time; }
      Dmsg2(400, "label=%d labeldate=%lld\n", label,
            static_cast<long long int>(mr.LabelDate));
    } else {
      // Sanity check for VolFiles to be increasing
      if (sdmr.VolFiles < mr.VolFiles) {
        Jmsg(jcr, M_INFO, 0,
             T_("Ignoring Volume Files at %u being set to %u"
                " for Volume \"%s\".\n"),
             mr.VolFiles, sdmr.VolFiles, mr.VolumeName);
        sdmr.VolFiles = mr.VolFiles;

        // if the file is wrong, then so is the block, so we need to clean
        // that up as well
        sdmr.VolBlocks = mr.VolBlocks;
      } else if (sdmr.VolFiles == mr.VolFiles) {
        // Sanity check for VolBlocks to be increasing on a filemark
        if (sdmr.VolBlocks < mr.VolBlocks) {
          Jmsg(jcr, M_INFO, 0,
               T_("Ignoring Volume Blocks at %u being set to %u"
                  " for Volume \"%s\".\n"),
               mr.VolBlocks, sdmr.VolBlocks, mr.VolumeName);
          sdmr.VolBlocks = mr.VolBlocks;
        }
      }

      // Sanity check for VolBytes to be increasing
      if (sdmr.VolBytes < mr.VolBytes) {
        Jmsg(jcr, M_INFO, 0,
             T_("Ignoring Volume Bytes at %" PRIu64 " being set to %" PRIu64
                " for Volume \"%s\".\n"),
             mr.VolBytes, sdmr.VolBytes, mr.VolumeName);
        sdmr.VolBytes = mr.VolBytes;
      }
    }
    Dmsg2(400, "Update media: BefVolJobs=%u After=%u\n", mr.VolJobs,
          sdmr.VolJobs);

    /* Check if the volume has been written by the job,
     * and update the LastWritten field if needed. */
    if (mr.VolBlocks != sdmr.VolBlocks && VolLastWritten != 0) {
      mr.LastWritten = VolLastWritten;
    }

    /* Update to point to the last device used to write the Volume.
     * However, do so only if we are writing the tape, i.e.
     * the number of VolWrites has increased. */
    if (jcr->dir_impl->res.write_storage && sdmr.VolWrites > mr.VolWrites) {
      Dmsg2(050, "Update StorageId old=%" PRIdbid " new=%" PRId64 "\n",
            mr.StorageId, jcr->dir_impl->res.write_storage->StorageId);
      // Update StorageId after write
      SetStorageidInMr(jcr->dir_impl->res.write_storage, &mr);
    } else {
      // Nothing written, reset same StorageId
      SetStorageidInMr(NULL, &mr);
    }

    // Copy updated values to original media record
    mr.VolJobs = sdmr.VolJobs;
    mr.VolFiles = sdmr.VolFiles;
    mr.VolBlocks = sdmr.VolBlocks;
    mr.VolBytes = sdmr.VolBytes;
    mr.VolMounts = sdmr.VolMounts;
    mr.VolErrors = sdmr.VolErrors;
    mr.VolWrites = sdmr.VolWrites;
    mr.Slot = sdmr.Slot;
    mr.InChanger = sdmr.InChanger;
    bstrncpy(mr.VolStatus, sdmr.VolStatus, sizeof(mr.VolStatus));
    mr.VolReadTime = sdmr.VolReadTime;
    mr.VolWriteTime = sdmr.VolWriteTime;

    Dmsg2(400, "UpdateMediaRecord. Stat=%s Vol=%s\n", mr.VolStatus,
          mr.VolumeName);

    /* Update the database, then before sending the response to the SD,
     * check if the Volume has expired. */
    if (!jcr->db->UpdateMediaRecord(jcr, &mr)) {
      Jmsg(jcr, M_FATAL, 0, T_("Catalog error updating Media record. %s"),
           jcr->db->strerror());
      bs->fsend(T_("1993 Update Media error\n"));
      Dmsg0(400, "send error\n");
    } else {
      (void)HasVolumeExpired(jcr, &mr);
      SendVolumeInfoToStorageDaemon(jcr, bs, &mr);
    }

  bail_out:

    Dmsg1(400, ">CatReq response: %s", bs->msg);
    Dmsg1(400, "Leave catreq jcr %p\n", jcr);
    return;
  } else if (sscanf(bs->msg, Create_job_media, &Job, &jm.FirstIndex,
                    &jm.LastIndex, &jm.StartFile, &jm.EndFile, &jm.StartBlock,
                    &jm.EndBlock, &Copy, &Stripe, &MediaId)
             == 10) {
    // Request to create a JobMedia record
    if (jcr->dir_impl->mig_jcr) {
      jm.JobId = jcr->dir_impl->mig_jcr->JobId;
    } else {
      jm.JobId = jcr->JobId;
    }
    jm.MediaId = MediaId;
    Dmsg6(400, "create_jobmedia JobId=%d MediaId=%d SF=%d EF=%d FI=%d LI=%d\n",
          jm.JobId, jm.MediaId, jm.StartFile, jm.EndFile, jm.FirstIndex,
          jm.LastIndex);
    DbLocker _{jcr->db};
    if (!jcr->db->CreateJobmediaRecord(jcr, &jm)) {
      Jmsg(jcr, M_FATAL, 0, T_("Catalog error creating JobMedia record. %s\n"),
           jcr->db->strerror());
      bs->fsend(T_("1992 Create JobMedia error\n"));
    } else {
      Dmsg0(400, "JobMedia record created\n");
      bs->fsend(OK_create);
    }
  } else if (sscanf(bs->msg, Update_filelist, &Job) == 1) {
    Dmsg0(0, "Updating filelist\n");

    if (jcr->db_batch) {
      if (DbLocker _{jcr->db_batch};
          !jcr->db_batch->WriteBatchFileRecords(jcr)) {
        Jmsg(jcr, M_FATAL, 0, T_("Catalog error updating File table. %s\n"),
             jcr->db_batch->strerror());
        bs->fsend(T_("1992 Update File table error\n"));
      }
    } else {
      Dmsg0(400,
            "No batch database connection exists, no files have been added to "
            "the batch (yet).\n");
    }
  } else if (sscanf(bs->msg, Update_jobrecord, &Job, &update_jobfiles,
                    &update_jobbytes)
             == 3) {
    Dmsg0(0, "Updating job record\n");

    jcr->JobFiles = update_jobfiles;
    jcr->JobBytes = update_jobbytes;

    if (DbLocker _{jcr->db}; !jcr->db->UpdateRunningJobRecord(jcr)) {
      Jmsg(jcr, M_FATAL, 0, T_("Catalog error updating Job record. %s\n"),
           jcr->db->strerror());
      bs->fsend(T_("1992 Update job record error\n"));
    }
  } else if (sscanf(bs->msg, Delete_nulljobmediarecord, &Job, &jobid) == 2) {
    int numrows = jcr->db->DeleteNullJobmediaRecords(jcr, jobid);
    Dmsg1(400, "Deleted %d rows.\n", numrows);
    if (numrows == -1) {
      bs->fsend(T_("1992 DeleteNullJobMediaRecords error\n"));
    } else {
      bs->fsend(OK_delete);
    }
  } else {
    omsg = GetMemory(bs->message_length + 1);
    PmStrcpy(omsg, bs->msg);
    bs->fsend(T_("1990 Invalid Catalog Request: %s"), omsg);
    Jmsg1(jcr, M_FATAL, 0, T_("Invalid Catalog request: %s"), omsg);
    FreeMemory(omsg);
  }

  Dmsg1(400, ">CatReq response: %s", bs->msg);
  Dmsg1(400, "Leave catreq jcr %p\n", jcr);

  return;
}

/**
 * Note, we receive the whole attribute record, but we select out only the
 * stat packet, VolSessionId, VolSessionTime, FileIndex, file type, and file
 * name to store in the catalog.
 */
static void UpdateAttribute(JobControlRecord* jcr,
                            char* msg,
                            int32_t message_length)
{
  unser_declare;
  uint32_t VolSessionId, VolSessionTime;
  int32_t Stream;
  uint32_t FileIndex;
  char* p;
  int len;
  char *fname, *attr;
  AttributesDbRecord* ar = NULL;
  uint32_t reclen;

  // Start transaction allocates jcr->attr and jcr->ar if needed
  jcr->db->StartTransaction(jcr); /* start transaction if not already open */
  ar = jcr->ar;

  /* Start by scanning directly in the message buffer to get Stream
   * there may be a cached attr so we cannot yet write into
   * jcr->attr or jcr->ar */
  p = msg;
  SkipNonspaces(&p); /* UpdCat */
  SkipSpaces(&p);
  SkipNonspaces(&p); /* Job=nnn */
  SkipSpaces(&p);
  SkipNonspaces(&p); /* "FileAttributes" */
  p += 1;

  UnserBegin(p, 0);
  unser_uint32(VolSessionId);   /* VolSessionId */
  unser_uint32(VolSessionTime); /* VolSessionTime */
  unser_int32(FileIndex);       /* FileIndex */
  unser_int32(Stream);          /* Stream */
  unser_uint32(reclen);         /* Record length */
  p += UnserLength(p);          /* Raw record follows */

  /* At this point p points to the raw record, which varies according
   *  to what kind of a record (Stream) was sent.  Note, the integer
   *  fields at the beginning of these "raw" records are in ASCII with
   *  spaces between them so one can use scanf or manual scanning to
   *  extract the fields.
   *
   * File Attributes
   *   File_index
   *   File type
   *   Filename (full path)
   *   Encoded attributes
   *   Link name (if type==FT_LNK or FT_LNKSAVED)
   *   Encoded extended-attributes (for Win32)
   *   Delta sequence number (32 bit int)
   *
   * Restore Object
   *   File_index
   *   File_type
   *   Object_index
   *   Object_len (possibly compressed)
   *   Object_full_len (not compressed)
   *   Object_compression
   *   Plugin_name
   *   Object_name
   *   Binary Object data */

  Dmsg1(400, "UpdCat msg=%s\n", msg);
  Dmsg5(400, "UpdCat VolSessId=%d VolSessT=%d FI=%d Strm=%d reclen=%d\n",
        VolSessionId, VolSessionTime, FileIndex, Stream, reclen);

  jcr->dir_impl->SDJobBytes
      += reclen; /* update number of bytes transferred for quotas */

  switch (Stream) {
    case STREAM_UNIX_ATTRIBUTES:
    case STREAM_UNIX_ATTRIBUTES_EX:
      if (jcr->cached_attribute) {
        Dmsg2(400, "Cached attr. Stream=%d fname=%s\n", ar->Stream, ar->fname);
        if (DbLocker _{jcr->db}; !jcr->db->CreateAttributesRecord(jcr, ar)) {
          Jmsg1(jcr, M_FATAL, 0, T_("Attribute create error: ERR=%s"),
                jcr->db->strerror());
        }
        jcr->cached_attribute = false;
      }

      // Any cached attr is flushed so we can reuse jcr->attr and jcr->ar
      jcr->attr = CheckPoolMemorySize(jcr->attr, message_length);
      memcpy(jcr->attr, msg, message_length);
      p = jcr->attr - msg + p; /* point p into jcr->attr */
      SkipNonspaces(&p);       /* skip FileIndex */
      SkipSpaces(&p);
      ar->FileType = str_to_int32(p);
      SkipNonspaces(&p); /* skip FileType */
      SkipSpaces(&p);
      fname = p;
      len = strlen(fname); /* length before attributes */
      attr = &fname[len + 1];
      ar->DeltaSeq = 0;
      if (ar->FileType == FT_REG) {
        p = attr + strlen(attr) + 1; /* point to link */
        p = p + strlen(p) + 1;       /* point to extended attributes */
        p = p + strlen(p) + 1;       /* point to delta sequence */
        // Older FDs don't have a delta sequence, so check if it is there
        if (p - jcr->attr < message_length) {
          ar->DeltaSeq = str_to_int32(p); /* delta_seq */
        }
      }

      Dmsg2(400, "dird<stored: stream=%d %s\n", Stream, fname);
      Dmsg1(400, "dird<stored: attr=%s\n", attr);

      ar->attr = attr;
      ar->fname = fname;
      if (ar->FileType == FT_DELETED) {
        ar->FileIndex = 0; /* special value */
      } else {
        ar->FileIndex = FileIndex;
      }
      ar->Stream = Stream;
      ar->link = NULL;
      if (jcr->dir_impl->mig_jcr) {
        ar->JobId = jcr->dir_impl->mig_jcr->JobId;
      } else {
        ar->JobId = jcr->JobId;
      }
      ar->Digest = NULL;
      ar->DigestType = CRYPTO_DIGEST_NONE;
      jcr->cached_attribute = true;


      /* Fhinfo and Fhnode are not sent from the SD,
       * they exist only in NDMP 2-Way backups so we
       * set them to 0 here */
      ar->Fhinfo = 0;
      ar->Fhnode = 0;


      Dmsg2(400, "dird<filed: stream=%d %s\n", Stream, fname);
      Dmsg1(400, "dird<filed: attr=%s\n", attr);
      break;
    case STREAM_RESTORE_OBJECT: {
      RestoreObjectDbRecord ro;

      ro.Stream = Stream;
      ro.FileIndex = FileIndex;
      if (jcr->dir_impl->mig_jcr) {
        ro.JobId = jcr->dir_impl->mig_jcr->JobId;
      } else {
        ro.JobId = jcr->JobId;
      }

      Dmsg1(100, "Robj=%s\n", p);

      SkipNonspaces(&p); /* skip FileIndex */
      SkipSpaces(&p);
      ro.FileType = str_to_int32(p); /* FileType */
      SkipNonspaces(&p);
      SkipSpaces(&p);
      ro.object_index = str_to_int32(p); /* Object Index */
      SkipNonspaces(&p);
      SkipSpaces(&p);
      ro.object_len = str_to_int32(p); /* object length possibly compressed */
      SkipNonspaces(&p);
      SkipSpaces(&p);
      ro.object_full_len = str_to_int32(p); /* uncompressed object length */
      SkipNonspaces(&p);
      SkipSpaces(&p);
      ro.object_compression = str_to_int32(p); /* compression */
      SkipNonspaces(&p);
      SkipSpaces(&p);

      ro.plugin_name = p; /* point to plugin name */
      len = strlen(ro.plugin_name);
      ro.object_name = &ro.plugin_name[len + 1]; /* point to object name */
      len = strlen(ro.object_name);
      ro.object = &ro.object_name[len + 1]; /* point to object */
      ro.object[ro.object_len]
          = 0; /* add zero for those who attempt printing */

      Dmsg7(100,
            "oname=%s stream=%d FT=%d FI=%d JobId=%d, obj_len=%d\nobj=\"%s\"\n",
            ro.object_name, ro.Stream, ro.FileType, ro.FileIndex, ro.JobId,
            ro.object_len, ro.object);

      // Store it.
      if (DbLocker _{jcr->db}; !jcr->db->CreateRestoreObjectRecord(jcr, &ro)) {
        Jmsg1(jcr, M_FATAL, 0, T_("Restore object create error. %s"),
              jcr->db->strerror());
      }
      break;
    }
    default:
      if (CryptoDigestStreamType(Stream) != CRYPTO_DIGEST_NONE) {
        fname = p;
        if (ar->FileIndex != FileIndex) {
          Jmsg3(jcr, M_WARNING, 0, T_("%s not same File=%d as attributes=%d\n"),
                stream_to_ascii(Stream), FileIndex, ar->FileIndex);
        } else {
          // Update digest in catalog
          char digestbuf[BASE64_SIZE(CRYPTO_DIGEST_MAX_SIZE)];
          int digest_len = 0;
          int type = CRYPTO_DIGEST_NONE;

          switch (Stream) {
            case STREAM_MD5_DIGEST:
              digest_len = CRYPTO_DIGEST_MD5_SIZE;
              type = CRYPTO_DIGEST_MD5;
              break;
            case STREAM_SHA1_DIGEST:
              digest_len = CRYPTO_DIGEST_SHA1_SIZE;
              type = CRYPTO_DIGEST_SHA1;
              break;
            case STREAM_SHA256_DIGEST:
              digest_len = CRYPTO_DIGEST_SHA256_SIZE;
              type = CRYPTO_DIGEST_SHA256;
              break;
            case STREAM_SHA512_DIGEST:
              digest_len = CRYPTO_DIGEST_SHA512_SIZE;
              type = CRYPTO_DIGEST_SHA512;
              break;
            case STREAM_XXH128_DIGEST:
              digest_len = CRYPTO_DIGEST_XXH128_SIZE;
              type = CRYPTO_DIGEST_XXH128;
              break;
            default:
              Jmsg(jcr, M_ERROR, 0,
                   T_("Catalog error updating file digest. Unsupported digest "
                      "stream type: %d\n"),
                   Stream);
          }

          BinToBase64(digestbuf, sizeof(digestbuf), fname, digest_len, true);

          Dmsg3(400, "DigestLen=%" PRIuz " Digest=%s type=%d\n",
                strlen(digestbuf), digestbuf, Stream);

          if (jcr->cached_attribute) {
            ar->Digest = digestbuf;
            ar->DigestType = type;

            Dmsg2(400, "Cached attr with digest. Stream=%d fname=%s\n",
                  ar->Stream, ar->fname);

            // Update BaseFile table
            if (DbLocker _{jcr->db};
                !jcr->db->CreateAttributesRecord(jcr, ar)) {
              Jmsg1(jcr, M_FATAL, 0, T_("attribute create error. %s\n"),
                    jcr->db->strerror());
            }
            jcr->cached_attribute = false;
          } else {
            if (DbLocker _{jcr->db}; !jcr->db->AddDigestToFileRecord(
                    jcr, ar->FileId, digestbuf, type)) {
              Jmsg(jcr, M_ERROR, 0,
                   T_("Catalog error updating file digest. %s\n"),
                   jcr->db->strerror());
            }
          }
        }
      }
      break;
  }
}

// Update File Attributes in the catalog with data sent by the Storage daemon.
void CatalogUpdate(JobControlRecord* jcr, BareosSocket* bs)
{
  if (!jcr->dir_impl->res.pool->catalog_files) {
    return; /* user disabled cataloging */
  }

  if (jcr->IsJobCanceled()) { goto bail_out; }

  if (!jcr->db) {
    POOLMEM* omsg = GetMemory(bs->message_length + 1);
    PmStrcpy(omsg, bs->msg);
    bs->fsend(T_("1994 Invalid Catalog Update: %s"), omsg);
    Jmsg1(jcr, M_FATAL, 0, T_("Invalid Catalog Update; DB not open: %s"), omsg);
    FreeMemory(omsg);
    goto bail_out;
  }

  UpdateAttribute(jcr, bs->msg, bs->message_length);

bail_out:
  if (jcr->IsJobCanceled()) { CancelStorageDaemonJob(jcr); }
}

/**
 * Update File Attributes in the catalog with data read from
 * the storage daemon spool file. We receive the filename and
 * we try to read it.
 */
bool DespoolAttributesFromFile(JobControlRecord* jcr, const char* file)
{
  bool retval = false;
  int32_t pktsiz;
  size_t nbytes;
  int32_t message_length; /* message length */
  int spool_fd = -1;
  POOLMEM* msg = GetPoolMemory(PM_MESSAGE);

  Dmsg0(100, "Begin DespoolAttributesFromFile\n");

  if (jcr->IsJobCanceled() || !jcr->dir_impl->res.pool->catalog_files
      || !jcr->db) {
    goto bail_out; /* user disabled cataloging */
  }

  spool_fd = open(file, O_RDONLY | O_BINARY);
  if (spool_fd == -1) {
    Dmsg0(100, "cancel DespoolAttributesFromFile\n");
    /* send an error message */
    goto bail_out;
  }

#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
  posix_fadvise(spool_fd, 0, 0, POSIX_FADV_WILLNEED);
#endif

  while ((nbytes = read(spool_fd, (char*)&pktsiz, sizeof(int32_t)))
         == sizeof(int32_t)) {
    message_length = ntohl(pktsiz);

    if (message_length > 0) {
      // check for message_length + \0
      if ((message_length + 1) > (int32_t)SizeofPoolMemory(msg)) {
        msg = ReallocPoolMemory(msg, message_length + 1);
      }
      nbytes = read(spool_fd, msg, message_length);
      if (nbytes != (size_t)message_length) {
        BErrNo be;
        Dmsg2(400, "nbytes=%" PRIuz " message_length=%" PRId32 "\n", nbytes,
              message_length);
        Qmsg1(jcr, M_FATAL, 0, T_("read attr spool error. ERR=%s\n"),
              be.bstrerror());
        goto bail_out;
      }
      msg[nbytes] = '\0';
    }

    if (!jcr->IsJobCanceled()) {
      UpdateAttribute(jcr, msg, message_length);
      if (jcr->IsJobCanceled()) { goto bail_out; }
    }
  }

  retval = true;

bail_out:
  if (spool_fd != -1) { close(spool_fd); }

  if (jcr->IsJobCanceled()) { CancelStorageDaemonJob(jcr); }

  FreePoolMemory(msg);
  Dmsg1(100, "End DespoolAttributesFromFile retval=%i\n", retval);
  return retval;
}
} /* namespace directordaemon */
