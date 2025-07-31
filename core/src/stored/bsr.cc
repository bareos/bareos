/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2025 Bareos GmbH & Co. KG

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
// Kern Sibbald, June MMII

/**
 * @file
 * Match Bootstrap Records (used for restores) against
 * Volume Records
 */

/*
 * ***FIXME***
 * Also for efficiency, once a bsr is done, it really should be
 *   delinked from the bsr chain.  This will avoid the above
 *   problem and make traversal of the bsr chain more efficient.
 *
 *   To be done ...
 */

#include "include/bareos.h"
#include "include/streams.h"
#include "stored/bsr.h"
#include <algorithm>
#include "stored/device_control_record.h"
#include "stored/stored_jcr_impl.h"
#include "stored/stored.h"
#include "include/jcr.h"

namespace storagedaemon {

const int dbglevel = 500;

/* Forward references */
namespace bsr {
static bool match_volume(volume& volume, Volume_Label* label);
static bool match_sesstime(volume& volume, DeviceRecord* rec);
static bool match_sessid(volume& volume, DeviceRecord* rec);
static bool match_client(volume& volume, Session_Label* sessrec);
static bool match_job(volume& volume, Session_Label* sessrec);
static bool match_job_type(volume& volume, Session_Label* sessrec);
static bool match_job_level(volume& volume, Session_Label* sessrec);
static bool match_job_id(volume& volume, Session_Label* sessrec);
static bool match_file_index(volume& volume, DeviceRecord* rec);
static bool match_vol_file(volume& volume, DeviceRecord* rec);
static bool match_vol_addr(volume& volume, DeviceRecord* rec);
// static bool match_vol_block(volume& volume, DeviceRecord* rec);
static bool match_stream(volume& volume, DeviceRecord* rec);

static int match_all(volume& volume,
                     DeviceRecord* rec,
                     Volume_Label* volrec,
                     Session_Label* sessrec,
                     JobControlRecord* jcr);

static bool match_sesstime(volume& volume, DeviceBlock* block);
static bool match_sessid(volume& volume, DeviceBlock* block);

};  // namespace bsr

#if 0

static BootStrapRecord* find_smallest_volfile(BootStrapRecord* fbsr,
                                              BootStrapRecord* bsr);

#endif

/**
 *
 *      Match Bootstrap records
 *        returns  1 on match
 *        returns  0 no match and Reposition is set if we should
 *                      Reposition the tape
 *       returns -1 no additional matches possible
 */
int match_bsr(BootStrapRecord* bsr,
              DeviceRecord* rec,
              Volume_Label* volrec,
              Session_Label* sessrec,
              JobControlRecord* jcr)
{
  if (!bsr) { return 1; }

  auto res = bsr::match_all(*bsr->current(), rec, volrec, sessrec, jcr);

  // the following check only works if sizes are given,
  // but bareos always does that!
  if (res == -1) {
    if (!bsr->current()->done) { bsr->current()->done = true; }

    if (bsr->current_volume < bsr->volumes.size() - 1) {
      auto& next = bsr->volumes[bsr->current_volume + 1];

      if (bsr::match_all(next, rec, volrec, sessrec, jcr) == 1) {
        // this code should not be here, but it has to because of the
        // way split records work.
        // if we have a situation like so:

        // volume entry 1 | volume entry 2
        //                |
        //  ... [  b1   ] | [   b2     ] ....
        //           [   r1    ]

        // with a bsr with two volume entries, one including only b1,
        //  and another one including only b2,
        // then the "ReadRecordFromBlock" function will read both
        // b1 & b2 and then ask the first bsr if r1 matches, but with
        // the address set to being inside b2.  This will return "no match"
        // as the address does not match.
        // In that case bareos will throw away r1, and position to the start
        // of the second bsr entry, b2.  This will cause us to only read
        // the second half of r1, which will get thrown away.

        // to prevent this we have to peek ahead here, and continue with
        // the next bsr entry if that returns a match!

        bsr->current_volume += 1;
        res = 1;
      } else {
        res = 0;
      }
    }
  }

  return res;
}

/**
 * Check if the current volume is done, and if so, if we can
 * continue reading from the current tape.
 */
bool find_next_bsr(BootStrapRecord* bsr, Device* dev)
{
  if (!bsr) {
    Dmsg0(dbglevel, "NULL root bsr pointer passed to find_next_bsr.\n");
    return false;
  }

  if (bsr->current_volume >= bsr->volumes.size() - 1) { return false; }

  auto& current = bsr->volumes[bsr->current_volume];

  if (!current.done) {
    Dmsg0(dbglevel, "current volume not done yet!\n");
    return false;
  }

  auto& next = bsr->volumes[bsr->current_volume + 1];

  if (!bsr->use_positioning || !bsr->Reposition
      || !dev->HasCap(CAP_POSITIONBLOCKS)) {
    Dmsg2(dbglevel, "No nxt_bsr use_pos=%d repos=%d\n", bsr->use_positioning,
          bsr->Reposition);
    return false;
  }
  Dmsg2(dbglevel, "use_pos=%d repos=%d\n", bsr->use_positioning,
        bsr->Reposition);

  if (match_volume(next, &dev->VolHdr)) {
    bsr->mount_next_volume = false;
    bsr->current_volume += 1;
    return true;
  } else {
    bsr->mount_next_volume = true;
    return false;
  }
}

#if 0

/**
 * Get the smallest address from this voladdr part
 * Don't use "done" elements
 */
static bool GetSmallestVoladdr(BsrVolumeAddress* va, uint64_t* ret)
{
  bool ok = false;
  uint64_t min_val = 0;

  for (; va; va = va->next) {
    if (!va->done) {
      if (ok) {
        min_val = MIN(min_val, va->saddr);
      } else {
        min_val = va->saddr;
        ok = true;
      }
    }
  }
  *ret = min_val;
  return ok;
}

/* FIXME
 * This routine needs to be fixed to only look at items that
 *   are not marked as done.  Otherwise, it can find a bsr
 *   that has already been consumed, and this will cause the
 *   bsr to be used, thus we may seek back and re-read the
 *   same records, causing an error.  This deficiency must
 *   be fixed.  For the moment, it has been kludged in
 *   read_record.c to avoid seeking back if find_next_bsr
 *   returns a bsr pointing to a smaller address (file/block).
 *
 */
static BootStrapRecord* find_smallest_volfile(BootStrapRecord* found_bsr,
                                              BootStrapRecord* bsr)
{
  BootStrapRecord* return_bsr = found_bsr;
  BsrVolumeFile* vf;
  BsrVolumeBlock* vb;
  uint32_t found_bsr_sfile, bsr_sfile;
  uint32_t found_bsr_sblock, bsr_sblock;
  uint64_t found_bsr_saddr, bsr_saddr;

  /* if we have VolAddr, use it, else try with File and Block */
  if (GetSmallestVoladdr(found_bsr->voladdr, &found_bsr_saddr)) {
    if (GetSmallestVoladdr(bsr->voladdr, &bsr_saddr)) {
      if (found_bsr_saddr > bsr_saddr) {
        return bsr;
      } else {
        return found_bsr;
      }
    }
  }

  /* Find the smallest file in the found_bsr */
  vf = found_bsr->volfile;
  found_bsr_sfile = vf->sfile;
  while ((vf = vf->next)) {
    if (vf->sfile < found_bsr_sfile) { found_bsr_sfile = vf->sfile; }
  }

  /* Find the smallest file in the bsr */
  vf = bsr->volfile;
  bsr_sfile = vf->sfile;
  while ((vf = vf->next)) {
    if (vf->sfile < bsr_sfile) { bsr_sfile = vf->sfile; }
  }

  /* if the bsr file is less than the found_bsr file, return bsr */
  if (found_bsr_sfile > bsr_sfile) {
    return_bsr = bsr;
  } else if (found_bsr_sfile == bsr_sfile) {
    /* Files are equal */
    /* find smallest block in found_bsr */
    vb = found_bsr->volblock;
    found_bsr_sblock = vb->sblock;
    while ((vb = vb->next)) {
      if (vb->sblock < found_bsr_sblock) { found_bsr_sblock = vb->sblock; }
    }
    /* Find smallest block in bsr */
    vb = bsr->volblock;
    bsr_sblock = vb->sblock;
    while ((vb = vb->next)) {
      if (vb->sblock < bsr_sblock) { bsr_sblock = vb->sblock; }
    }
    /* Compare and return the smallest */
    if (found_bsr_sblock > bsr_sblock) { return_bsr = bsr; }
  }
  return return_bsr;
}

#endif

/**
 * Called after the signature record so that
 *   we can see if the current bsr has been
 *   fully processed (i.e. is done).
 *  The bsr argument is not used, but is included
 *    for consistency with the other match calls.
 *
 * Returns: true if we should reposition
 *        : false otherwise.
 */
bool IsThisBsrDone(BootStrapRecord*, DeviceRecord* rec)
{
  bsr::volume* rec_volume = rec->bsr_volume;
  Dmsg1(dbglevel, "match_set %p\n", rec_volume);
  if (!rec_volume) { return false; }
  rec->bsr_volume = NULL;
  rec_volume->found++;
  if (rec_volume->count && rec_volume->found >= rec_volume->count) {
    rec_volume->done = true;
    rec_volume->root->Reposition = true;
    Dmsg2(dbglevel, "is_end_this_bsr set Reposition=1 count=%zu found=%zu\n",
          rec_volume->count, rec_volume->found);
    return true;
  }
  Dmsg2(dbglevel, "is_end_this_bsr not done count=%zu found=%zu\n",
        rec_volume->count, rec_volume->found);
  return false;
}

namespace bsr {

struct match_result {
  bool found;
  bool too_big;
};

static match_result match_interval(const std::vector<interval>& intervals,
                                   std::uint64_t value)
{
  if (intervals.empty()) { return {true, false}; }

  bool too_big = true;
  for (auto& iv : intervals) {
    if (iv.contains(value)) { return {true, false}; }
    if (value < iv.end) { too_big = false; }
  }

  return {false, too_big};
}

/**
 *
 *  Do fast block rejection based on bootstrap records.
 *    use_fast_rejection will be set if we have VolSessionId and VolSessTime
 *    in each record. When BlockVer is >= 2, we have those in the block header
 *    so can do fast rejection.
 *
 *   returns:  1 if block may contain valid records
 *             0 if block may be skipped (i.e. it contains no records of
 *                  that can match the bsr).
 *
 */
bool match_bsr_block(BootStrapRecord* bsr, volume& volume, DeviceBlock* block)
{
  if (!bsr || !bsr->use_fast_rejection || (block->BlockVer < 2)) {
    return 1; /* cannot fast reject */
  }

  if (!match_sessid(volume, block)) { return 0; }
  if (!match_sesstime(volume, block)) { return 0; }
  return 1;
}

static bool match_sesstime(volume& volume, DeviceBlock* block)
{
  if (volume.session_times.empty()) { return true; }

  return std::find(std::begin(volume.session_times),
                   std::end(volume.session_times), block->VolSessionTime)
         != std::end(volume.session_times);
}

[[maybe_unused]] static bool match_sessid(volume& volume, DeviceBlock* block)
{
  return match_interval(volume.session_ids, block->VolSessionId).found;
}

static bool match_fileregex(BootStrapRecord* bsr,
                            bsr::volume& volume,
                            DeviceRecord* rec,
                            JobControlRecord* jcr)
{
  if (!volume.fileregex_re) { return true; }

  if (bsr->attr == NULL) { bsr->attr = new_attr(jcr); }

  /* The code breaks if the first record associated with a file is
   * not of this type */
  if (rec->maskedStream == STREAM_UNIX_ATTRIBUTES
      || rec->maskedStream == STREAM_UNIX_ATTRIBUTES_EX) {
    bsr->skip_file = false;
    if (UnpackAttributesRecord(jcr, rec->Stream, rec->data, rec->data_len,
                               bsr->attr)) {
      if (regexec(volume.fileregex_re.get(), bsr->attr->fname, 0, NULL, 0)
          == 0) {
        Dmsg2(dbglevel, "Matched pattern, fname=%s FI=%d\n", bsr->attr->fname,
              rec->FileIndex);
      } else {
        Dmsg2(dbglevel, "Didn't match, skipping fname=%s FI=%d\n",
              bsr->attr->fname, rec->FileIndex);
        bsr->skip_file = true;
        /*** FIXUP: shouldnt there be a false here ? ***/
        return false;
      }
    }
  }
  return true;
}


/**
 * Match all the components of current record
 *   returns  1 on match
 *   returns  0 no match
 *   returns -1 no additional matches possible
 */
static int match_all(volume& volume,
                     DeviceRecord* rec,
                     Volume_Label* volrec,
                     Session_Label* sessrec,
                     JobControlRecord* jcr)
{
  Dmsg0(dbglevel, "Enter MatchAll\n");
  if (volume.done) {
    //    Dmsg0(dbglevel, "bsr->done set\n");
    goto no_match;
  }
  if (!match_volume(volume, volrec)) {
    Dmsg2(dbglevel, "bsr fail bsr_vol=%s != rec read_vol=%s\n",
          volume.volume_name.c_str(), volrec->VolumeName);
    goto no_match;
  }
  Dmsg2(dbglevel, "OK bsr match bsr_vol=%s read_vol=%s\n",
        volume.volume_name.c_str(), volrec->VolumeName);

  if (!match_vol_file(volume, rec)) {
    Dmsg3(dbglevel, "Fail on file=%u\n", rec->File);
    goto no_match;
  }

  if (!match_vol_addr(volume, rec)) {
    Dmsg3(dbglevel, "Fail on Addr=%" PRIu64 "\n", GetRecordAddress(rec));
    goto no_match;
  }

  if (!match_sesstime(volume, rec)) {
    Dmsg2(dbglevel, "Fail on sesstime. rec=%u\n", rec->VolSessionTime);
    goto no_match;
  }

  /* NOTE!! This test MUST come after the sesstime test */
  if (!match_sessid(volume, rec)) {
    Dmsg2(dbglevel, "Fail on sessid. rec=%u\n", rec->VolSessionId);
    goto no_match;
  }

  /* NOTE!! This test MUST come after sesstime and sessid tests */
  if (!volume.file_indices.empty()) {
    if (!match_file_index(volume, rec)) {
      Dmsg3(dbglevel, "Fail on findex=%d\n", rec->FileIndex);
      goto no_match;
    } else {
      Dmsg3(dbglevel, "match on findex=%d\n", rec->FileIndex);
    }
  } else {
    Dmsg0(dbglevel, "No bsr->FileIndex!\n");
  }

  if (!match_fileregex(volume.root, volume, rec, jcr)) {
    Dmsg1(dbglevel, "Fail on fileregex='%s'\n", volume.fileregex.c_str());
    goto no_match;
  }

  /* This flag is set by MatchFileregex (and perhaps other tests) */
  if (volume.root->skip_file) {
    Dmsg1(dbglevel, "Skipping findex=%d (skip file)\n", rec->FileIndex);
    goto no_match;
  }

  /* If a count was specified and we have a FileIndex, assume
   *   it is a Bareos created bsr (or the equivalent). We
   *   then save the bsr where the match occurred so that
   *   after processing the record or records, we can update
   *   the found count. I.e. rec->bsr points to the bsr that
   *   satisfied the match. */
  if (volume.count && !volume.file_indices.empty()) {
    rec->bsr_volume = &volume;
    Dmsg0(dbglevel, "Leave MatchAll 1\n");
    return 1; /* this is a complete match */
  }

  /* The selections below are not used by Bareos's
   *   restore command, and don't work because of
   *   the rec->bsr = bsr optimization above. */
  if (!match_job_id(volume, sessrec)) {
    Dmsg0(dbglevel, "fail on JobId\n");
    goto no_match;
  }
  if (!match_job(volume, sessrec)) {
    Dmsg0(dbglevel, "fail on Job\n");
    goto no_match;
  }
  if (!match_client(volume, sessrec)) {
    Dmsg0(dbglevel, "fail on Client\n");
    goto no_match;
  }
  if (!match_job_type(volume, sessrec)) {
    Dmsg0(dbglevel, "fail on Job type\n");
    goto no_match;
  }
  if (!match_job_level(volume, sessrec)) {
    Dmsg0(dbglevel, "fail on Job level\n");
    goto no_match;
  }
  if (!match_stream(volume, rec)) {
    Dmsg0(dbglevel, "fail on stream\n");
    goto no_match;
  }
  return 1;

no_match:
  if (volume.done) {
    Dmsg0(dbglevel, "Leave match all -1\n");
    return -1;
  }
  Dmsg0(dbglevel, "Leave match all 0\n");
  return 0;
}

static bool match_volume(volume& volume, Volume_Label* volrec)
{
  if (bstrcmp(volume.volume_name.c_str(), volrec->VolumeName)) {
    Dmsg1(dbglevel, "MatchVolume=%s\n", volrec->VolumeName);
    return true;
  }
  return false;
}

static bool match_client(volume& volume, Session_Label* sessrec)
{
  if (volume.clients.empty()) { return true; }

  for (auto& client : volume.clients) {
    if (bstrcmp(client.c_str(), sessrec->ClientName)) { return true; }
  }

  return false;
}

static bool match_job(volume& volume, Session_Label* sessrec)
{
  if (volume.jobs.empty()) { return true; }

  for (auto& job : volume.jobs) {
    if (bstrcmp(job.c_str(), sessrec->Job)) { return true; }
  }

  return false;
}

static bool match_job_type(volume& volume, Session_Label* sessrec)
{
  if (volume.job_types.empty()) { return true; }

  for (auto& job_type : volume.job_types) {
    if (job_type == sessrec->JobType) { return true; }
  }

  return false;
}

static bool match_job_level(volume& volume, Session_Label* sessrec)
{
  if (volume.job_levels.empty()) { return true; }

  for (auto& job_level : volume.job_levels) {
    if (job_level == sessrec->JobLevel) { return true; }
  }

  return false;
}

static bool match_job_id(volume& volume, Session_Label* sessrec)
{
  return match_interval(volume.job_ids, sessrec->JobId).found;
}

static bool match_vol_file(volume& volume, DeviceRecord* rec)
{
  auto result = match_interval(volume.files, rec->File);
  if (result.too_big) {
    volume.done = true;
    volume.root->Reposition = true;
    Dmsg2(dbglevel, "volume done from volfile rec=%u\n", rec->File);
  }
  return result.found;
}

static bool match_vol_addr(volume& volume, DeviceRecord* rec)
{
  uint64_t addr = GetRecordAddress(rec);

  auto result = match_interval(volume.addresses, addr);

  if (result.too_big) {
    volume.done = true;
    volume.root->Reposition = true;
    Dmsg2(dbglevel, "bsr done from voladdr rec=%" PRIu64 "\n", addr);
  }
  return result.found;
}


static bool match_stream(volume& volume, DeviceRecord* rec)
{
  if (volume.streams.empty()) { return true; }

  return std::find(std::begin(volume.streams), std::end(volume.streams),
                   rec->Stream)
         != std::end(volume.streams);
}

static bool match_sesstime(volume& volume, DeviceRecord* rec)
{
  if (volume.session_times.empty()) { return true; }

  bool done = true;
  for (auto time : volume.session_times) {
    if (time == rec->VolSessionTime) { return true; }
    if (time < rec->VolSessionTime) { done = false; }
  }

  volume.done = done;

  volume.root->Reposition = true;
  Dmsg0(dbglevel, "volume done from sesstime\n");

  return false;
}

/**
 * Note, we cannot mark bsr done based on session id because we may
 *  have interleaved records, and there may be more of what we want
 *  later.
 */
static bool match_sessid(volume& volume, DeviceRecord* rec)
{
  return match_interval(volume.session_ids, rec->VolSessionId).found;
}

/**
 * When reading the Volume, the Volume Findex (rec->FileIndex) always
 *   are found in sequential order. Thus we can make optimizations.
 *
 *  ***FIXME*** optimizations
 * We could optimize by removing the recursion.
 */
static bool match_file_index(volume& volume, DeviceRecord* rec)
{
  auto result = match_interval(volume.file_indices, rec->FileIndex);
  if (result.too_big) {
    volume.done = true;
    volume.root->Reposition = true;
    Dmsg1(dbglevel, "volume done from findex %d\n", rec->FileIndex);
  }
  return result.found;
}
}  // namespace bsr

uint64_t GetBsrStartAddr(BootStrapRecord* bsr, uint32_t* file, uint32_t* block)
{
  uint64_t bsr_addr = 0;
  uint32_t sfile = 0, sblock = 0;

  if (bsr) {
    auto* volume = bsr->current();
    if (!volume->addresses.empty()) {
      bsr_addr = volume->addresses[0].start;
      sfile = static_cast<std::uint32_t>(bsr_addr >> 32);
      sblock = static_cast<std::uint32_t>(bsr_addr);
    } else {
      if (!volume->files.empty()) { sfile = volume->files[0].start; }
      if (!volume->blocks.empty()) { sblock = volume->blocks[0].start; }

      bsr_addr = static_cast<std::uint64_t>(sfile) << 32
                 | static_cast<std::uint64_t>(sblock);
    }
  }

  if (file) *file = sfile;
  if (block) *block = sblock;

  return bsr_addr;
}

/**
 * Create a list of Volumes (and Slots and Start positions) to be
 *  used in the current restore job.
 */
void CreateRestoreVolumeList(JobControlRecord* jcr)
{
  // Build a list of volumes to be processed
  if (!jcr->sd_impl->read_session.bsr) {
    BootStrapRecord* bsr = new BootStrapRecord;
    /* This is the old way -- deprecated */
    for (char* p = jcr->sd_impl->dcr->VolumeName; p && *p;) {
      char* n = strchr(p, '|'); /* volume name separator */
      if (n) { *n++ = 0; /* Terminate name */ }
      auto& volume = bsr->volumes.emplace_back();

      volume.volume_name = p;
      volume.media_type = jcr->sd_impl->dcr->media_type;
      volume.root = bsr;

      p = n;
    }

    jcr->sd_impl->read_session.bsr = bsr;
  }
  for (auto& volume : jcr->sd_impl->read_session.bsr->volumes) {
    AddReadVolume(jcr, volume.volume_name.c_str());
  }
}

void FreeRestoreVolumeList(JobControlRecord* jcr)
{
  if (!jcr->sd_impl->read_session.bsr) { return; }
  for (auto& volume : jcr->sd_impl->read_session.bsr->volumes) {
    RemoveReadVolume(jcr, volume.volume_name.c_str());
  }
}

} /* namespace storagedaemon */
