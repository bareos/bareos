/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2014 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
// Marco van Wieringen, December 2013
/**
 * @file
 * This file contains the HTABLE abstraction of the accurate payload storage.
 */

#include "include/bareos.h"
#include "include/filetypes.h"
#include "include/streams.h"
#include "filed/filed.h"
#include "accurate.h"
#include "lib/attribs.h"

namespace filedaemon {

static int debuglevel = 100;

BareosAccurateFilelistHtable::BareosAccurateFilelistHtable(
    JobControlRecord* jcr,
    uint32_t number_of_files)
    : BareosAccurateFilelist(jcr, number_of_files)
{
  file_list_ = new FileList(number_of_files);
}

bool BareosAccurateFilelistHtable::AddFile(char* fname,
                                           int fname_length,
                                           char* lstat,
                                           int lstat_length,
                                           char* chksum,
                                           int chksum_length,
                                           int32_t delta_seq)
{
  CurFile* item;
  int total_length;
  bool retval = true;

  total_length
      = sizeof(CurFile) + fname_length + lstat_length + chksum_length + 3;
  item = (CurFile*)file_list_->hash_malloc(total_length);

  item->fname = (char*)item + sizeof(CurFile);
  memcpy(item->fname, fname, fname_length);
  item->fname[fname_length] = '\0';

  item->payload.lstat = item->fname + fname_length + 1;
  memcpy(item->payload.lstat, lstat, lstat_length);
  item->payload.lstat[lstat_length] = '\0';

  item->payload.chksum = item->payload.lstat + lstat_length + 1;
  if (chksum_length) { memcpy(item->payload.chksum, chksum, chksum_length); }
  item->payload.chksum[chksum_length] = '\0';

  item->payload.delta_seq = delta_seq;
  item->payload.filenr = seen_bitmap_.size();
  if (file_list_->insert(item->fname, item)) {
    if (chksum) {
      Dmsg4(debuglevel,
            "[file_nr = %" PRIuz
            "] add fname=<%s> lstat=%s delta_seq=%i chksum=%s\n",
            item->payload.filenr, fname, lstat, delta_seq, chksum);
    } else {
      Dmsg2(debuglevel, "[file_nr = %" PRIuz "] add fname=<%s> lstat=%s\n",
            item->payload.filenr, fname, lstat);
    }
    seen_bitmap_.push_back(false);
  } else {
    duplicate_files_ += 1;
    Dmsg1(debuglevel, "fname=<%s> is already registered.\n", fname);
  }

  return retval;
}

bool BareosAccurateFilelistHtable::EndLoad()
{
  if (duplicate_files_ > 0) {
    Jmsg1(jcr_, M_ERROR, 0,
          T_("%" PRIuz
             " duplicate files were sent by the director and removed. This "
             "may indicate problems with the database.\n"),
          duplicate_files_);
  }
  // seen_bitmap_.size() is the number of files sent by the director
  // without duplicates
  if (seen_bitmap_.size() > initial_capacity_) {
    Jmsg1(
        jcr_, M_ERROR, 0,
        T_("The director send too many files. %" PRIuz
           " were sent but only %" PRIuz " "
           "were anticipated. The accurate job may be in a corrupted state.\n"),
        seen_bitmap_.size(), initial_capacity_);
  }
  // Nothing to do.
  return true;
}

accurate_payload* BareosAccurateFilelistHtable::lookup_payload(char* fname)
{
  CurFile* temp;

  temp = (CurFile*)file_list_->lookup(fname);
  return (temp) ? &temp->payload : NULL;
}

bool BareosAccurateFilelistHtable::UpdatePayload(char*, accurate_payload*)
{
  return true;
}

bool BareosAccurateFilelistHtable::SendBaseFileList()
{
  CurFile* elt;
  FindFilesPacket* ff_pkt;
  int32_t LinkFIc;
  struct stat statp;
  int stream = STREAM_UNIX_ATTRIBUTES;

  if (!jcr_->accurate || jcr_->getJobLevel() != L_FULL) { return true; }

  if (file_list_ == NULL) { return true; }

  ff_pkt = init_find_files();
  ff_pkt->type = FT_BASE;

  foreach_htable (elt, file_list_) {
    if (seen_bitmap_.at(elt->payload.filenr)) {
      Dmsg1(debuglevel, "base file fname=%s\n", elt->fname);
      DecodeStat(elt->payload.lstat, &statp, sizeof(statp),
                 &LinkFIc); /* decode catalog stat */
      ff_pkt->fname = elt->fname;
      ff_pkt->statp = statp;
      EncodeAndSendAttributes(jcr_, ff_pkt, stream);
    }
  }

  TermFindFiles(ff_pkt);
  return true;
}

bool BareosAccurateFilelistHtable::SendDeletedList()
{
  CurFile* elt;
  FindFilesPacket* ff_pkt;
  int32_t LinkFIc;
  struct stat statp;
  int stream = STREAM_UNIX_ATTRIBUTES;

  if (!jcr_->accurate) { return true; }

  ff_pkt = init_find_files();
  ff_pkt->type = FT_DELETED;

  foreach_htable (elt, file_list_) {
    if (seen_bitmap_.at(elt->payload.filenr)
        || PluginCheckFile(jcr_, elt->fname)) {
      continue;
    }
    Dmsg1(debuglevel, "deleted fname=%s\n", elt->fname);
    ff_pkt->fname = elt->fname;
    DecodeStat(elt->payload.lstat, &statp, sizeof(statp),
               &LinkFIc); /* decode catalog stat */
    ff_pkt->statp.st_mtime = statp.st_mtime;
    ff_pkt->statp.st_ctime = statp.st_ctime;
    EncodeAndSendAttributes(jcr_, ff_pkt, stream);
  }

  TermFindFiles(ff_pkt);
  return true;
}

void BareosAccurateFilelistHtable::destroy()
{
  delete file_list_;
  file_list_ = nullptr;
}

} /* namespace filedaemon */
