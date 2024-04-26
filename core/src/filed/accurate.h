/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2014 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_FILED_ACCURATE_H_
#define BAREOS_FILED_ACCURATE_H_

/*
 * Generic accurate payload is the same for any storage class.
 *
 * We store the lstat field as a character field as it makes storing of the
 * data much more efficient than storing it in a normal stat structure.
 * Nowadays most of these structures are 64 bits and they will use 136
 * bytes (32 bits) and 128 bytes (64 bits) on Solaris. The ASCII encoding
 * of the lstat field most of the time is between 43 and 75 bytes with an
 * average of 60 bytes. (From live data of 152562393 files.)
 *
 * The seen flag is not in the actual accurate payload but stored in a
 * bitmap. This makes it much more efficient for a storage engine that has
 * to update the whole data when something changes. For things like a in
 * memory hash table it doesn't make much difference and it has the
 * disadvantage that we need to keep a filenr to index the bitmap which
 * also cost some bytes.
 */

#include <vector>
#include <algorithm>
#include "include/config.h"
#include "include/baconfig.h"
#include "lib/jcr.h"
#include "lib/htable.h"

namespace filedaemon {

struct accurate_payload {
  std::size_t filenr;
  int32_t delta_seq;
  char* lstat;
  char* chksum;
};


// Accurate payload storage abstraction classes.
class BareosAccurateFilelist {
 protected:
  std::vector<bool> seen_bitmap_;
  JobControlRecord* jcr_ = nullptr;

 public:
  BareosAccurateFilelist(JobControlRecord* jcr, std::size_t initial_capacity)
      : jcr_{jcr}
  {
    seen_bitmap_.reserve(initial_capacity);
  }

  virtual ~BareosAccurateFilelist() {}

  virtual bool init() = 0;
  virtual bool AddFile(char* fname,
                       int fname_length,
                       char* lstat,
                       int lstat_length,
                       char* chksum,
                       int checksum_length,
                       int32_t delta_seq)
      = 0;
  virtual bool EndLoad() = 0;
  virtual accurate_payload* lookup_payload(char* fname) = 0;
  virtual bool UpdatePayload(char* fname, accurate_payload* payload) = 0;
  virtual bool SendBaseFileList() = 0;
  virtual bool SendDeletedList() = 0;
  void MarkFileAsSeen(accurate_payload* payload)
  {
    /* Something went really wrong if we are supposed to mark a file as seen
     * that we have not even registered.  The best course of action is to
     * crash in that situation. */
    ASSERT(seen_bitmap_.size() > payload->filenr);
    seen_bitmap_[payload->filenr] = true;
  }

  void UnmarkFileAsSeen(accurate_payload* payload)
  {
    /* Something went really wrong if we are supposed to mark a file as seen
     * that we have not even registered.  The best course of action is to
     * crash in that situation. */
    ASSERT(seen_bitmap_.size() > payload->filenr);
    seen_bitmap_[payload->filenr] = false;
  }

  void MarkAllFilesAsSeen()
  {
    std::fill(std::begin(seen_bitmap_), std::end(seen_bitmap_), 1);
  }

  void UnmarkAllFilesAsSeen()
  {
    std::fill(std::begin(seen_bitmap_), std::end(seen_bitmap_), 0);
  }
};

/*
 * Hash table specific storage abstraction class using the internal htable
 * datastructure.
 */
struct CurFile {
  hlink link;
  char* fname;
  accurate_payload payload;
};

class BareosAccurateFilelistHtable : public BareosAccurateFilelist {
  using FileList = htable<char*, CurFile>;

 protected:
  FileList* file_list_;
  void destroy();

 public:
  /* methods */
  BareosAccurateFilelistHtable() = delete;
  BareosAccurateFilelistHtable(JobControlRecord* jcr, uint32_t number_of_files);
  ~BareosAccurateFilelistHtable() { destroy(); }

  bool init() override { return true; }

  bool AddFile(char* fname,
               int fname_length,
               char* lstat,
               int lstat_length,
               char* chksum,
               int checksum_length,
               int32_t delta_seq) override;
  bool EndLoad() override;
  accurate_payload* lookup_payload(char* fname) override;
  bool UpdatePayload(char* fname, accurate_payload* payload) override;
  bool SendBaseFileList() override;
  bool SendDeletedList() override;
};

#ifdef HAVE_LMDB

#  include "lmdb/lmdb.h"

/*
 * Lightning Memory DataBase (LMDB) specific storage abstraction class using the
 * Symas LMDB.
 */
class BareosAccurateFilelistLmdb : public BareosAccurateFilelist {
 protected:
  int pay_load_length_;
  POOLMEM* pay_load_;
  POOLMEM* lmdb_name_;
  MDB_env* db_env_;
  MDB_dbi db_dbi_;
  MDB_txn* db_rw_txn_;
  MDB_txn* db_ro_txn_;
  uint32_t max_capacity_;

  void destroy();

 public:
  /* methods */
  BareosAccurateFilelistLmdb() = delete;
  BareosAccurateFilelistLmdb(JobControlRecord* jcr, uint32_t number_of_files);
  ~BareosAccurateFilelistLmdb() { destroy(); }
  bool init() override;
  bool AddFile(char* fname,
               int fname_length,
               char* lstat,
               int lstat_length,
               char* chksum,
               int checksum_length,
               int32_t delta_seq) override;
  bool EndLoad() override;
  accurate_payload* lookup_payload(char* fname) override;
  bool UpdatePayload(char* fname, accurate_payload* payload) override;
  bool SendBaseFileList() override;
  bool SendDeletedList() override;
};
#endif /* HAVE_LMDB */

bool AccurateFinish(JobControlRecord* jcr);
bool AccurateCheckFile(JobControlRecord* jcr, FindFilesPacket* ff_pkt);
bool AccurateMarkFileAsSeen(JobControlRecord* jcr, char* fname);
bool accurate_unMarkFileAsSeen(JobControlRecord* jcr, char* fname);
bool AccurateMarkAllFilesAsSeen(JobControlRecord* jcr);
bool accurate_unMarkAllFilesAsSeen(JobControlRecord* jcr);
void AccurateFree(JobControlRecord* jcr);


} /* namespace filedaemon */
#endif  // BAREOS_FILED_ACCURATE_H_
