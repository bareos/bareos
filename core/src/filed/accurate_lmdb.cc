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
/*
 * Marco van Wieringen, December 2013
 */
/**
 * This file contains the LMDB abstraction of the accurate payload storage.
 */

#include "bareos.h"
#include "filed.h"

#ifdef HAVE_LMDB

#include "accurate.h"

static int dbglvl = 100;

#define AVG_NR_BYTES_PER_ENTRY 256
#define B_PAGE_SIZE 4096

BareosAccurateFilelistLmdb::BareosAccurateFilelistLmdb(JCR *jcr, uint32_t number_of_files)
{
   filenr_ = 0;
   pay_load_ = NULL;
   lmdb_name_ = NULL;
   seen_bitmap_ = NULL;
   db_env_ = NULL;
   db_ro_txn_ = NULL;
   db_rw_txn_ = NULL;
   db_dbi_ = 0;
   number_of_previous_files_ = number_of_files;
   init();
}

BareosAccurateFilelistLmdb::~BareosAccurateFilelistLmdb()
{
}

bool BareosAccurateFilelistLmdb::init()
{
   int result;
   MDB_env *env;
   size_t mapsize = 10485760;

   if (!db_env_) {
      result = mdb_env_create(&env);
      if (result) {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable to create MDB environment: %s\n"), mdb_strerror(result));
         return false;
      }

      if ((number_of_previous_files_ * AVG_NR_BYTES_PER_ENTRY) > mapsize) {
         size_t pagesize;

#ifdef HAVE_GETPAGESIZE
         pagesize = getpagesize();
#else
         pagesize = B_PAGE_SIZE;
#endif

         mapsize = (((number_of_previous_files_ * AVG_NR_BYTES_PER_ENTRY) / pagesize) + 1) * pagesize;
      }
      result = mdb_env_set_mapsize(env, mapsize);
      if (result) {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable to set MDB mapsize: %s\n"), mdb_strerror(result));
         goto bail_out;
      }

      /*
       * Explicitly set the number of readers to 1.
       */
      result = mdb_env_set_maxreaders(env, 1);
      if (result) {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable to set MDB maxreaders: %s\n"), mdb_strerror(result));
         goto bail_out;
      }

      Mmsg(lmdb_name_, "%s/.accurate_lmdb.%d", me->working_directory, jcr_->JobId);
      result = mdb_env_open(env, lmdb_name_, MDB_NOSUBDIR | MDB_NOLOCK | MDB_NOSYNC, 0600);
      if (result) {
         Jmsg2(jcr_, M_FATAL, 0, _("Unable create LDMD database %s: %s\n"), lmdb_name_, mdb_strerror(result));
         goto bail_out;
      }

      result = mdb_txn_begin(env, NULL, 0, &db_rw_txn_);
      if (result) {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable to start a write transaction: %s\n"), mdb_strerror(result));
         goto bail_out;
      }

      result = mdb_dbi_open(db_rw_txn_, NULL, MDB_CREATE, &db_dbi_);
      if (result) {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable to open LMDB internal database: %s\n"), mdb_strerror(result));
         mdb_txn_abort(db_rw_txn_);
         db_rw_txn_ = NULL;
         goto bail_out;
      }

      db_env_ = env;
   }

   if (!pay_load_) {
      pay_load_ = get_pool_memory(PM_MESSAGE);
   }

   if (!lmdb_name_) {
      lmdb_name_ = get_pool_memory(PM_FNAME);
   }

   if (!seen_bitmap_) {
      seen_bitmap_ = (char *)malloc(nbytes_for_bits(number_of_previous_files_));
      clear_all_bits(number_of_previous_files_, seen_bitmap_);
   }

   return true;

bail_out:
   if (env) {
      mdb_env_close(env);
   }

   return false;
}

bool BareosAccurateFilelistLmdb::add_file( char *fname,
                                           int fname_length,
                                           char *lstat,
                                           int lstat_length,
                                           char *chksum,
                                           int chksulength_,
                                           int32_t delta_seq)
{
   accurate_payload *payload;
   int result;
   int total_length;
   MDB_val key, data;
   bool retval = false;

   total_length = sizeof(accurate_payload) + lstat_length + chksulength_ + 2;

   /*
    * Make sure pay_load_ is large enough.
    */
   pay_load_ = check_pool_memory_size(pay_load_, total_length);

   /*
    * We store the total pay load as:
    *
    * accurate_payload structure\0lstat\0chksum\0
    */
   payload = (accurate_payload *)pay_load_;

   payload->lstat = (char *)payload + sizeof(accurate_payload);
   memcpy(payload->lstat, lstat, lstat_length);
   payload->lstat[lstat_length] = '\0';

   payload->chksum = (char *)payload->lstat + lstat_length + 1;
   if (chksulength_) {
      memcpy(payload->chksum, chksum, chksulength_);
   }
   payload->chksum[chksulength_] = '\0';

   payload->delta_seq = delta_seq;
   payload->filenr = filenr_++;

   key.mv_data = fname;
   key.mv_size = strlen(fname) + 1;

   data.mv_data = payload;
   data.mv_size = total_length;

retry:
   result = mdb_put(db_rw_txn_, db_dbi_, &key, &data, MDB_NOOVERWRITE);
   switch (result) {
   case 0:
      if (chksum) {
         Dmsg4(dbglvl, "add fname=<%s> lstat=%s delta_seq=%i chksum=%s\n", fname, lstat, delta_seq, chksum);
      } else {
         Dmsg2(dbglvl, "add fname=<%s> lstat=%s\n", fname, lstat);
      }
      retval = true;
      break;
   case MDB_TXN_FULL:
      /*
       * Seems we filled the transaction.
       * Flush the current transaction start a new one and retry the put.
       */
      result = mdb_txn_commit(db_rw_txn_);
      if (result == 0) {
         result = mdb_txn_begin(db_env_, NULL, 0, &db_rw_txn_);
         if (result == 0) {
            goto retry;
         } else {
            Jmsg1(jcr_, M_FATAL, 0, _("Unable create new transaction: %s\n"), mdb_strerror(result));
         }
      } else {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable to commit full transaction: %s\n"), mdb_strerror(result));
      }
      break;
   default:
      Jmsg1(jcr_, M_FATAL, 0, _("Unable insert new data: %s\n"), mdb_strerror(result));
      break;
   }

   return retval;
}

bool BareosAccurateFilelistLmdb::end_load()
{
   int result;

   /*
    * Commit any pending write transactions.
    */
   if (db_rw_txn_) {
      result = mdb_txn_commit(db_rw_txn_);
      if (result != 0) {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable close write transaction: %s\n"), mdb_strerror(result));
         return false;
      }
      result = mdb_txn_begin(db_env_, NULL, 0, &db_rw_txn_);
      if (result != 0) {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable to create write transaction: %s\n"), mdb_strerror(result));
         return false;
      }
   }

   /*
    * From now on we also will be doing read transactions so create a read transaction context.
    */
   if (!db_ro_txn_) {
      result = mdb_txn_begin(db_env_, NULL, MDB_RDONLY, &db_ro_txn_);
      if (result != 0) {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable to create read transaction: %s\n"), mdb_strerror(result));
         return false;
      }
   }

   return true;
}

accurate_payload *BareosAccurateFilelistLmdb::lookup_payload(char *fname)
{
   int result;
   int lstat_length;
   MDB_val key, data;
   accurate_payload *payload = NULL;

   key.mv_data = fname;
   key.mv_size = strlen(fname) + 1;

   result = mdb_get(db_ro_txn_, db_dbi_, &key, &data);
   switch (result) {
   case 0:
      /*
       * Success.
       *
       * We need to make a private copy of the LDMB data as we are not
       * allowed to change its content and we need to update the lstat
       * and chksum pointer to point to the actual lstat and chksum that
       * is stored behind the accurate_payload structure in the LMDB.
       */
      pay_load_ = check_pool_memory_size(pay_load_, data.mv_size);

      payload = (accurate_payload *)pay_load_;
      memcpy(payload, data.mv_data, data.mv_size);
      payload->lstat = (char *)payload + sizeof(accurate_payload);
      lstat_length = strlen(payload->lstat);
      payload->chksum = (char *)payload->lstat + lstat_length + 1;

      /*
       * We keep the transaction as short a possible so after a lookup
       * and copying the actual data out we reset the read transaction
       * and do a renew of the read transaction for a new run.
       */
      mdb_txn_reset(db_ro_txn_);
      result = mdb_txn_renew(db_ro_txn_);
      if (result != 0) {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable to renew read transaction: %s\n"), mdb_strerror(result));
         return NULL;
      }
      break;
   case MDB_NOTFOUND:
      /*
       * Failed to find the given key.
       */
      break;
   default:
      break;
   }

   return payload;
}

bool BareosAccurateFilelistLmdb::update_payload(char *fname, accurate_payload *payload)
{
   int result,
       total_length,
       lstat_length,
       chksulength_;
   MDB_val key, data;
   bool retval = false;
   accurate_payload *new_payload;

   lstat_length = strlen(payload->lstat);
   chksulength_ = strlen(payload->chksum);
   total_length = sizeof(accurate_payload) + lstat_length + chksulength_ + 2;

   /*
    * Make sure pay_load_ is large enough.
    */
   pay_load_ = check_pool_memory_size(pay_load_, total_length);

   /*
    * We store the total pay load as:
    *
    * accurate_payload structure\0lstat\0chksum\0
    */
   new_payload = (accurate_payload *)pay_load_;

   new_payload->lstat = (char *)payload + sizeof(accurate_payload);
   memcpy(new_payload->lstat, payload->lstat, lstat_length);
   new_payload->lstat[lstat_length] = '\0';

   new_payload->chksum = (char *)new_payload->lstat + lstat_length + 1;
   if (chksulength_) {
      memcpy(new_payload->chksum, payload->chksum, chksulength_);
   }
   new_payload->chksum[chksulength_] = '\0';

   new_payload->delta_seq = payload->delta_seq;
   new_payload->filenr = payload->filenr;

   key.mv_data = fname;
   key.mv_size = strlen(fname) + 1;

   data.mv_data = new_payload;
   data.mv_size = total_length;

retry:
   result = mdb_put(db_rw_txn_, db_dbi_, &key, &data, 0);
   switch (result) {
   case 0:
      result = mdb_txn_commit(db_rw_txn_);
      if (result == 0) {
         result = mdb_txn_begin(db_env_, NULL, 0, &db_rw_txn_);
         if (result != 0) {
            Jmsg1(jcr_, M_FATAL, 0, _("Unable to create write transaction: %s\n"), mdb_strerror(result));
         } else {
            retval = true;
         }
      } else {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable close write transaction: %s\n"), mdb_strerror(result));
      }
      break;
   case MDB_TXN_FULL:
      /*
       * Seems we filled the transaction.
       * Flush the current transaction start a new one and retry the put.
       */
      result = mdb_txn_commit(db_rw_txn_);
      if (result == 0) {
         result = mdb_txn_begin(db_env_, NULL, 0, &db_rw_txn_);
         if (result == 0) {
            goto retry;
         } else {
            Jmsg1(jcr_, M_FATAL, 0, _("Unable create new transaction: %s\n"), mdb_strerror(result));
         }
      } else {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable to commit full transaction: %s\n"), mdb_strerror(result));
      }
      break;
   default:
      Jmsg1(jcr_, M_FATAL, 0, _("Unable insert new data: %s\n"), mdb_strerror(result));
      break;
   }

   return retval;
}

bool BareosAccurateFilelistLmdb::send_base_file_list()
{
   int result;
   int32_t LinkFIc;
   FF_PKT *ff_pkt;
   MDB_cursor *cursor;
   MDB_val key, data;
   bool retval = false;
   accurate_payload *payload;
   int stream = STREAM_UNIX_ATTRIBUTES;

   if (!jcr_->accurate || jcr_->getJobLevel() != L_FULL) {
      return true;
   }

   /*
    * Commit any pending write transactions.
    */
   if (db_rw_txn_) {
      result = mdb_txn_commit(db_rw_txn_);
      if (result != 0) {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable close write transaction: %s\n"), mdb_strerror(result));
         return false;
      }
      db_rw_txn_ = NULL;
   }

   ff_pkt = init_find_files();
   ff_pkt->type = FT_BASE;

   result = mdb_cursor_open(db_ro_txn_, db_dbi_, &cursor);
   if (result == 0) {
      while ((result = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
         payload = (accurate_payload *)data.mv_data;
         if (bit_is_set(payload->filenr, seen_bitmap_)) {
            Dmsg1(dbglvl, "base file fname=%s\n", key.mv_data);
            decode_stat(payload->lstat, &ff_pkt->statp, sizeof(struct stat), &LinkFIc); /* decode catalog stat */
            ff_pkt->fname = (char *)key.mv_data;
            encode_and_send_attributes(jcr_, ff_pkt, stream);
         }
      }
      mdb_cursor_close(cursor);
   } else {
      Jmsg1(jcr_, M_FATAL, 0, _("Unable create cursor: %s\n"), mdb_strerror(result));
   }

   mdb_txn_reset(db_ro_txn_);
   result = mdb_txn_renew(db_ro_txn_);
   if (result != 0) {
      Jmsg1(jcr_, M_FATAL, 0, _("Unable to renew read transaction: %s\n"), mdb_strerror(result));
      goto bail_out;
   }

   retval = true;

bail_out:
   term_find_files(ff_pkt);
   return retval;
}

bool BareosAccurateFilelistLmdb::send_deleted_list()
{
   int result;
   int32_t LinkFIc;
   struct stat statp;
   FF_PKT *ff_pkt;
   MDB_cursor *cursor;
   MDB_val key, data;
   bool retval = false;
   accurate_payload *payload;
   int stream = STREAM_UNIX_ATTRIBUTES;

   if (!jcr_->accurate) {
      return true;
   }

   /*
    * Commit any pending write transactions.
    */
   if (db_rw_txn_) {
      result = mdb_txn_commit(db_rw_txn_);
      if (result != 0) {
         Jmsg1(jcr_, M_FATAL, 0, _("Unable close write transaction: %s\n"), mdb_strerror(result));
         return false;
      }
      db_rw_txn_ = NULL;
   }

   ff_pkt = init_find_files();
   ff_pkt->type = FT_DELETED;

   result = mdb_cursor_open(db_ro_txn_, db_dbi_, &cursor);
   if (result == 0) {
      while ((result = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
         payload = (accurate_payload *)data.mv_data;

         if (bit_is_set(payload->filenr, seen_bitmap_) ||
             plugin_check_file(jcr_, (char *)key.mv_data)) {
            continue;
         }

         Dmsg1(dbglvl, "deleted fname=%s\n", key.mv_data);
         decode_stat(payload->lstat, &statp, sizeof(struct stat), &LinkFIc); /* decode catalog stat */
         ff_pkt->fname = (char *)key.mv_data;
         ff_pkt->statp.st_mtime = statp.st_mtime;
         ff_pkt->statp.st_ctime = statp.st_ctime;
         encode_and_send_attributes(jcr_, ff_pkt, stream);
      }
      mdb_cursor_close(cursor);
   } else {
      Jmsg1(jcr_, M_FATAL, 0, _("Unable create cursor: %s\n"), mdb_strerror(result));
   }

   mdb_txn_reset(db_ro_txn_);
   result = mdb_txn_renew(db_ro_txn_);
   if (result != 0) {
      Jmsg1(jcr_, M_FATAL, 0, _("Unable to renew read transaction: %s\n"), mdb_strerror(result));
      goto bail_out;
   }

   retval = true;

bail_out:
   term_find_files(ff_pkt);
   return retval;
}

void BareosAccurateFilelistLmdb::destroy()
{
   /*
    * Abort any pending read transaction.
    */
   if (db_ro_txn_) {
      mdb_txn_abort(db_ro_txn_);
      db_ro_txn_ = NULL;
   }

   /*
    * Abort any pending write transaction.
    */
   if (db_rw_txn_) {
      mdb_txn_abort(db_rw_txn_);
      db_rw_txn_ = NULL;
   }

   if (db_env_) {
      /*
       * Drop the contents of the LMDB.
       */
      if (db_dbi_) {
         int result;
         MDB_txn *txn;

         result = mdb_txn_begin(db_env_, NULL, 0, &txn);
         if (result == 0) {
            result = mdb_drop(txn, db_dbi_, 1);
            if (result == 0) {
               mdb_txn_commit(txn);
            } else {
               mdb_txn_abort(txn);
            }
         }
         db_dbi_ = 0;
      }

      /*
       * Close the environment.
       */
      mdb_env_close(db_env_);
      db_env_ = NULL;
   }

   if (pay_load_) {
      free_pool_memory(pay_load_);
      pay_load_ = NULL;
   }

   if (lmdb_name_) {
      secure_erase(jcr_, lmdb_name_);
      free_pool_memory(lmdb_name_);
      lmdb_name_ = NULL;
   }

   if (seen_bitmap_) {
      free(seen_bitmap_);
      seen_bitmap_ = NULL;
   }

   filenr_ = 0;
}
#endif /* HAVE_LMDB */
