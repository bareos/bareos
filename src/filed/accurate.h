/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2014 Planets Communications B.V.
   Copyright (C) 2013-2014 Bareos GmbH & Co. KG

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
 * data much more efficient then storing it in a normal stat structure.
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
#pragma pack(4)
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
class B_ACCURATE: public SMARTALLOC {
protected:
   int64_t m_filenr;
   char *m_seen_bitmap;

public:
   /* methods */
   B_ACCURATE() { m_filenr = 0; m_seen_bitmap = NULL; };
   virtual ~B_ACCURATE() {};
   virtual bool init(JCR *jcr, uint32_t nbfile) = 0;
   virtual bool add_file(JCR *jcr,
                         char *fname,
                         int fname_length,
                         char *lstat,
                         int lstat_length,
                         char *chksum,
                         int chksum_length,
                         int32_t delta_seq) = 0;
   virtual bool end_load(JCR *jcr) = 0;
   virtual accurate_payload *lookup_payload(JCR *jcr, char *fname) = 0;
   virtual bool update_payload(JCR *jcr, char *fname, accurate_payload *payload) = 0;
   virtual bool send_base_file_list(JCR *jcr) = 0;
   virtual bool send_deleted_list(JCR *jcr) = 0;
   virtual void destroy(JCR *jcr) = 0;
   void mark_file_as_seen(JCR *jcr, accurate_payload *payload) { set_bit(payload->filenr, m_seen_bitmap); };
   void unmark_file_as_seen(JCR *jcr, accurate_payload *payload) { clear_bit(payload->filenr, m_seen_bitmap); };
   void mark_all_files_as_seen(JCR *jcr) { set_bits(0, m_filenr - 1, m_seen_bitmap); };
   void unmark_all_files_as_seen(JCR *jcr) { clear_bits(0, m_filenr - 1, m_seen_bitmap); };
};

/*
 * Hash table specific storage abstraction class using the internal htable datastructure.
 */
struct CurFile {
   hlink link;
   char *fname;
   accurate_payload payload;
};

class B_ACCURATE_HTABLE: public B_ACCURATE {
protected:
   htable *m_file_list;

public:
   /* methods */
   B_ACCURATE_HTABLE();
   ~B_ACCURATE_HTABLE();
   bool init(JCR *jcr, uint32_t nbfile);
   bool add_file(JCR *jcr,
                 char *fname,
                 int fname_length,
                 char *lstat,
                 int lstat_length,
                 char *chksum,
                 int chksum_length,
                 int32_t delta_seq);
   bool end_load(JCR *jcr);
   accurate_payload *lookup_payload(JCR *jcr, char *fname);
   bool update_payload(JCR *jcr, char *fname, accurate_payload *payload);
   bool send_base_file_list(JCR *jcr);
   bool send_deleted_list(JCR *jcr);
   void destroy(JCR *jcr);
};

#ifdef HAVE_LMDB

#include "lmdb.h"

/*
 * Lighning Memory DataBase (LMDB) specific storage abstraction class using the Symas LMDB.
 */
class B_ACCURATE_LMDB: public B_ACCURATE {
protected:
   int m_pay_load_length;
   POOLMEM *m_pay_load;
   POOLMEM *m_lmdb_name;
   MDB_env *m_db_env;
   MDB_dbi m_db_dbi;
   MDB_txn *m_db_rw_txn;
   MDB_txn *m_db_ro_txn;

public:
   /* methods */
   B_ACCURATE_LMDB();
   ~B_ACCURATE_LMDB();
   bool init(JCR *jcr, uint32_t nbfile);
   bool add_file(JCR *jcr,
                 char *fname,
                 int fname_length,
                 char *lstat,
                 int lstat_length,
                 char *chksum,
                 int chksum_length,
                 int32_t delta_seq);
   bool end_load(JCR *jcr);
   accurate_payload *lookup_payload(JCR *jcr, char *fname);
   bool update_payload(JCR *jcr, char *fname, accurate_payload *payload);
   bool send_base_file_list(JCR *jcr);
   bool send_deleted_list(JCR *jcr);
   void destroy(JCR *jcr);
};
#endif /* HAVE_LMDB */

#endif /* ACCURATE_H */
