/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2014 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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

#ifndef ACCURATE_H
#define ACCURATE_H

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

#include "hostconfig.h"

#ifdef HAVE_HPUX_OS
#pragma pack(push,4)
#endif

struct accurate_payload {
   int64_t filenr;
   int32_t delta_seq;
   char *lstat;
   char *chksum;
};


/*
 * Accurate payload storage abstraction classes.
 */
class BareosAccurateFilelist: public SmartAlloc {
protected:
   int64_t filenr_;
   char *seen_bitmap_;
   JobControlRecord *jcr_;
   int32_t number_of_previous_files_;

public:
   /* methods */
   BareosAccurateFilelist() {
      filenr_ = 0;
      seen_bitmap_ = NULL;
      number_of_previous_files_ = 0;
      jcr_ = NULL;
   }

   virtual ~BareosAccurateFilelist() {
   }

   virtual bool init() = 0;
   virtual bool add_file(char *fname,
                         int fname_length,
                         char *lstat,
                         int lstat_length,
                         char *chksum,
                         int checksum_length,
                         int32_t delta_seq) = 0;
   virtual bool end_load() = 0;
   virtual accurate_payload *lookup_payload(char *fname) = 0;
   virtual bool update_payload(char *fname, accurate_payload *payload) = 0;
   virtual bool send_base_file_list() = 0;
   virtual bool send_deleted_list() = 0;
   void mark_file_as_seen(accurate_payload *payload) {
      set_bit(payload->filenr, seen_bitmap_);
   }

   void unmark_file_as_seen(accurate_payload *payload) {
      clear_bit(payload->filenr, seen_bitmap_);
   }

   void mark_all_files_as_seen() {
      set_bits(0, filenr_ - 1, seen_bitmap_);
   }

   void unmark_all_files_as_seen() {
      clear_bits(0, filenr_ - 1, seen_bitmap_);
   }
};

/*
 * Hash table specific storage abstraction class using the internal htable datastructure.
 */
struct CurFile {
   hlink link;
   char *fname;
   accurate_payload payload;
};

#ifdef HAVE_HPUX_OS
#pragma pack(pop)
#endif

class BareosAccurateFilelistHtable: public BareosAccurateFilelist {
protected:
   htable *file_list_;
   void destroy();

public:
   /* methods */
   BareosAccurateFilelistHtable() = delete;
   BareosAccurateFilelistHtable(JobControlRecord *jcr, uint32_t number_of_files);
   ~BareosAccurateFilelistHtable() {
      destroy();
   }

   bool init() {
      return true;
   }

   bool add_file(char *fname,
                 int fname_length,
                 char *lstat,
                 int lstat_length,
                 char *chksum,
                 int checksum_length,
                 int32_t delta_seq);
   bool end_load();
   accurate_payload *lookup_payload(char *fname);
   bool update_payload(char *fname, accurate_payload *payload);
   bool send_base_file_list();
   bool send_deleted_list();
};

#ifdef HAVE_LMDB

#include "lmdb.h"

/*
 * Lightning Memory DataBase (LMDB) specific storage abstraction class using the Symas LMDB.
 */
class BareosAccurateFilelistLmdb: public BareosAccurateFilelist {
protected:
   int pay_load_length_;
   POOLMEM *pay_load_;
   POOLMEM *lmdb_name_;
   MDB_env *db_env_;
   MDB_dbi db_dbi_;
   MDB_txn *db_rw_txn_;
   MDB_txn *db_ro_txn_;

   void destroy();

public:
   /* methods */
   BareosAccurateFilelistLmdb() = delete;
   BareosAccurateFilelistLmdb(JobControlRecord *jcr, uint32_t number_of_files);
   ~BareosAccurateFilelistLmdb() {
      destroy();
   }
   bool init();
   bool add_file(char *fname,
                 int fname_length,
                 char *lstat,
                 int lstat_length,
                 char *chksum,
                 int checksum_length,
                 int32_t delta_seq);
   bool end_load();
   accurate_payload *lookup_payload(char *fname);
   bool update_payload(char *fname, accurate_payload *payload);
   bool send_base_file_list();
   bool send_deleted_list();
};
#endif /* HAVE_LMDB */

#endif /* ACCURATE_H */
