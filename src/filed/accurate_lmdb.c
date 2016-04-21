/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
/**
 * This file contains the LMDB abstraction of the accurate payload storage.
 *
 * Marco van Wieringen, December 2013
 */

#include "bareos.h"
#include "filed.h"

#ifdef HAVE_LMDB

#include "accurate.h"

static int dbglvl = 100;

#define AVG_NR_BYTES_PER_ENTRY 256
#define B_PAGE_SIZE 4096

B_ACCURATE_LMDB::B_ACCURATE_LMDB()
{
   m_filenr = 0;
   m_pay_load = NULL;
   m_lmdb_name = NULL;
   m_seen_bitmap = NULL;
   m_db_env = NULL;
   m_db_ro_txn = NULL;
   m_db_rw_txn = NULL;
   m_db_dbi = 0;
}

B_ACCURATE_LMDB::~B_ACCURATE_LMDB()
{
}

bool B_ACCURATE_LMDB::init(JCR *jcr, uint32_t nbfile)
{
   int result;
   MDB_env *env;
   size_t mapsize = 10485760;

   if (!m_db_env) {
      result = mdb_env_create(&env);
      if (result) {
         Jmsg1(jcr, M_FATAL, 0, _("Unable to create MDB environment: %s\n"), mdb_strerror(result));
         return false;
      }

      if ((nbfile * AVG_NR_BYTES_PER_ENTRY) > mapsize) {
         size_t pagesize;

#ifdef HAVE_GETPAGESIZE
         pagesize = getpagesize();
#else
         pagesize = B_PAGE_SIZE;
#endif

         mapsize = (((nbfile * AVG_NR_BYTES_PER_ENTRY) / pagesize) + 1) * pagesize;
      }
      result = mdb_env_set_mapsize(env, mapsize);
      if (result) {
         Jmsg1(jcr, M_FATAL, 0, _("Unable to set MDB mapsize: %s\n"), mdb_strerror(result));
         goto bail_out;
      }

      /*
       * Explicitly set the number of readers to 1.
       */
      result = mdb_env_set_maxreaders(env, 1);
      if (result) {
         Jmsg1(jcr, M_FATAL, 0, _("Unable to set MDB maxreaders: %s\n"), mdb_strerror(result));
         goto bail_out;
      }

      Mmsg(m_lmdb_name, "%s/.accurate_lmdb.%d", me->working_directory, jcr->JobId);
      result = mdb_env_open(env, m_lmdb_name, MDB_NOSUBDIR | MDB_NOLOCK | MDB_NOSYNC, 0600);
      if (result) {
         Jmsg2(jcr, M_FATAL, 0, _("Unable create LDMD database %s: %s\n"), m_lmdb_name, mdb_strerror(result));
         goto bail_out;
      }

      result = mdb_txn_begin(env, NULL, 0, &m_db_rw_txn);
      if (result) {
         Jmsg1(jcr, M_FATAL, 0, _("Unable to start a write transaction: %s\n"), mdb_strerror(result));
         goto bail_out;
      }

      result = mdb_dbi_open(m_db_rw_txn, NULL, MDB_CREATE, &m_db_dbi);
      if (result) {
         Jmsg1(jcr, M_FATAL, 0, _("Unable to open LMDB internal database: %s\n"), mdb_strerror(result));
         mdb_txn_abort(m_db_rw_txn);
         m_db_rw_txn = NULL;
         goto bail_out;
      }

      m_db_env = env;
   }

   if (!m_pay_load) {
      m_pay_load = get_pool_memory(PM_MESSAGE);
   }

   if (!m_lmdb_name) {
      m_lmdb_name = get_pool_memory(PM_FNAME);
   }

   if (!m_seen_bitmap) {
      m_seen_bitmap = (char *)malloc(nbytes_for_bits(nbfile));
      clear_all_bits(nbfile, m_seen_bitmap);
   }

   return true;

bail_out:
   if (env) {
      mdb_env_close(env);
   }

   return false;
}

bool B_ACCURATE_LMDB::add_file(JCR *jcr,
                               char *fname,
                               int fname_length,
                               char *lstat,
                               int lstat_length,
                               char *chksum,
                               int chksum_length,
                               int32_t delta_seq)
{
   accurate_payload *payload;
   int result;
   int total_length;
   MDB_val key, data;
   bool retval = false;

   total_length = sizeof(accurate_payload) + lstat_length + chksum_length + 2;

   /*
    * Make sure m_pay_load is large enough.
    */
   m_pay_load = check_pool_memory_size(m_pay_load, total_length);

   /*
    * We store the total pay load as:
    *
    * accurate_payload structure\0lstat\0chksum\0
    */
   payload = (accurate_payload *)m_pay_load;

   payload->lstat = (char *)payload + sizeof(accurate_payload);
   memcpy(payload->lstat, lstat, lstat_length);
   payload->lstat[lstat_length] = '\0';

   payload->chksum = (char *)payload->lstat + lstat_length + 1;
   if (chksum_length) {
      memcpy(payload->chksum, chksum, chksum_length);
   }
   payload->chksum[chksum_length] = '\0';

   payload->delta_seq = delta_seq;
   payload->filenr = m_filenr++;

   key.mv_data = fname;
   key.mv_size = strlen(fname) + 1;

   data.mv_data = payload;
   data.mv_size = total_length;

retry:
   result = mdb_put(m_db_rw_txn, m_db_dbi, &key, &data, MDB_NOOVERWRITE);
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
      result = mdb_txn_commit(m_db_rw_txn);
      if (result == 0) {
         result = mdb_txn_begin(m_db_env, NULL, 0, &m_db_rw_txn);
         if (result == 0) {
            goto retry;
         } else {
            Jmsg1(jcr, M_FATAL, 0, _("Unable create new transaction: %s\n"), mdb_strerror(result));
         }
      } else {
         Jmsg1(jcr, M_FATAL, 0, _("Unable to commit full transaction: %s\n"), mdb_strerror(result));
      }
      break;
   default:
      Jmsg1(jcr, M_FATAL, 0, _("Unable insert new data: %s\n"), mdb_strerror(result));
      break;
   }

   return retval;
}

bool B_ACCURATE_LMDB::end_load(JCR *jcr)
{
   int result;

   /*
    * Commit any pending write transactions.
    */
   if (m_db_rw_txn) {
      result = mdb_txn_commit(m_db_rw_txn);
      if (result != 0) {
         Jmsg1(jcr, M_FATAL, 0, _("Unable close write transaction: %s\n"), mdb_strerror(result));
         return false;
      }
      result = mdb_txn_begin(m_db_env, NULL, 0, &m_db_rw_txn);
      if (result != 0) {
         Jmsg1(jcr, M_FATAL, 0, _("Unable to create write transaction: %s\n"), mdb_strerror(result));
         return false;
      }
   }

   /*
    * From now on we also will be doing read transactions so create a read transaction context.
    */
   if (!m_db_ro_txn) {
      result = mdb_txn_begin(m_db_env, NULL, MDB_RDONLY, &m_db_ro_txn);
      if (result != 0) {
         Jmsg1(jcr, M_FATAL, 0, _("Unable to create read transaction: %s\n"), mdb_strerror(result));
         return false;
      }
   }

   return true;
}

accurate_payload *B_ACCURATE_LMDB::lookup_payload(JCR *jcr, char *fname)
{
   int result;
   int lstat_length;
   MDB_val key, data;
   accurate_payload *payload = NULL;

   key.mv_data = fname;
   key.mv_size = strlen(fname) + 1;

   result = mdb_get(m_db_ro_txn, m_db_dbi, &key, &data);
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
      m_pay_load = check_pool_memory_size(m_pay_load, data.mv_size);

      payload = (accurate_payload *)m_pay_load;
      memcpy(payload, data.mv_data, data.mv_size);
      payload->lstat = (char *)payload + sizeof(accurate_payload);
      lstat_length = strlen(payload->lstat);
      payload->chksum = (char *)payload->lstat + lstat_length + 1;

      /*
       * We keep the transaction as short a possible so after a lookup
       * and copying the actual data out we reset the read transaction
       * and do a renew of the read transaction for a new run.
       */
      mdb_txn_reset(m_db_ro_txn);
      result = mdb_txn_renew(m_db_ro_txn);
      if (result != 0) {
         Jmsg1(jcr, M_FATAL, 0, _("Unable to renew read transaction: %s\n"), mdb_strerror(result));
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

bool B_ACCURATE_LMDB::update_payload(JCR *jcr, char *fname, accurate_payload *payload)
{
   int result,
       total_length,
       lstat_length,
       chksum_length;
   MDB_val key, data;
   bool retval = false;
   accurate_payload *new_payload;

   lstat_length = strlen(payload->lstat);
   chksum_length = strlen(payload->chksum);
   total_length = sizeof(accurate_payload) + lstat_length + chksum_length + 2;

   /*
    * Make sure m_pay_load is large enough.
    */
   m_pay_load = check_pool_memory_size(m_pay_load, total_length);

   /*
    * We store the total pay load as:
    *
    * accurate_payload structure\0lstat\0chksum\0
    */
   new_payload = (accurate_payload *)m_pay_load;

   new_payload->lstat = (char *)payload + sizeof(accurate_payload);
   memcpy(new_payload->lstat, payload->lstat, lstat_length);
   new_payload->lstat[lstat_length] = '\0';

   new_payload->chksum = (char *)new_payload->lstat + lstat_length + 1;
   if (chksum_length) {
      memcpy(new_payload->chksum, payload->chksum, chksum_length);
   }
   new_payload->chksum[chksum_length] = '\0';

   new_payload->delta_seq = payload->delta_seq;
   new_payload->filenr = payload->filenr;

   key.mv_data = fname;
   key.mv_size = strlen(fname) + 1;

   data.mv_data = new_payload;
   data.mv_size = total_length;

retry:
   result = mdb_put(m_db_rw_txn, m_db_dbi, &key, &data, 0);
   switch (result) {
   case 0:
      result = mdb_txn_commit(m_db_rw_txn);
      if (result == 0) {
         result = mdb_txn_begin(m_db_env, NULL, 0, &m_db_rw_txn);
         if (result != 0) {
            Jmsg1(jcr, M_FATAL, 0, _("Unable to create write transaction: %s\n"), mdb_strerror(result));
         } else {
            retval = true;
         }
      } else {
         Jmsg1(jcr, M_FATAL, 0, _("Unable close write transaction: %s\n"), mdb_strerror(result));
      }
      break;
   case MDB_TXN_FULL:
      /*
       * Seems we filled the transaction.
       * Flush the current transaction start a new one and retry the put.
       */
      result = mdb_txn_commit(m_db_rw_txn);
      if (result == 0) {
         result = mdb_txn_begin(m_db_env, NULL, 0, &m_db_rw_txn);
         if (result == 0) {
            goto retry;
         } else {
            Jmsg1(jcr, M_FATAL, 0, _("Unable create new transaction: %s\n"), mdb_strerror(result));
         }
      } else {
         Jmsg1(jcr, M_FATAL, 0, _("Unable to commit full transaction: %s\n"), mdb_strerror(result));
      }
      break;
   default:
      Jmsg1(jcr, M_FATAL, 0, _("Unable insert new data: %s\n"), mdb_strerror(result));
      break;
   }

   return retval;
}

bool B_ACCURATE_LMDB::send_base_file_list(JCR *jcr)
{
   int result;
   int32_t LinkFIc;
   FF_PKT *ff_pkt;
   MDB_cursor *cursor;
   MDB_val key, data;
   bool retval = false;
   accurate_payload *payload;
   int stream = STREAM_UNIX_ATTRIBUTES;

   if (!jcr->accurate || jcr->getJobLevel() != L_FULL) {
      return true;
   }

   /*
    * Commit any pending write transactions.
    */
   if (m_db_rw_txn) {
      result = mdb_txn_commit(m_db_rw_txn);
      if (result != 0) {
         Jmsg1(jcr, M_FATAL, 0, _("Unable close write transaction: %s\n"), mdb_strerror(result));
         return false;
      }
      m_db_rw_txn = NULL;
   }

   ff_pkt = init_find_files();
   ff_pkt->type = FT_BASE;

   result = mdb_cursor_open(m_db_ro_txn, m_db_dbi, &cursor);
   if (result == 0) {
      while ((result = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
         payload = (accurate_payload *)data.mv_data;
         if (bit_is_set(payload->filenr, m_seen_bitmap)) {
            Dmsg1(dbglvl, "base file fname=%s\n", key.mv_data);
            decode_stat(payload->lstat, &ff_pkt->statp, sizeof(struct stat), &LinkFIc); /* decode catalog stat */
            ff_pkt->fname = (char *)key.mv_data;
            encode_and_send_attributes(jcr, ff_pkt, stream);
         }
      }
      mdb_cursor_close(cursor);
   } else {
      Jmsg1(jcr, M_FATAL, 0, _("Unable create cursor: %s\n"), mdb_strerror(result));
   }

   mdb_txn_reset(m_db_ro_txn);
   result = mdb_txn_renew(m_db_ro_txn);
   if (result != 0) {
      Jmsg1(jcr, M_FATAL, 0, _("Unable to renew read transaction: %s\n"), mdb_strerror(result));
      goto bail_out;
   }

   retval = true;

bail_out:
   term_find_files(ff_pkt);
   return retval;
}

bool B_ACCURATE_LMDB::send_deleted_list(JCR *jcr)
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

   if (!jcr->accurate) {
      return true;
   }

   /*
    * Commit any pending write transactions.
    */
   if (m_db_rw_txn) {
      result = mdb_txn_commit(m_db_rw_txn);
      if (result != 0) {
         Jmsg1(jcr, M_FATAL, 0, _("Unable close write transaction: %s\n"), mdb_strerror(result));
         return false;
      }
      m_db_rw_txn = NULL;
   }

   ff_pkt = init_find_files();
   ff_pkt->type = FT_DELETED;

   result = mdb_cursor_open(m_db_ro_txn, m_db_dbi, &cursor);
   if (result == 0) {
      while ((result = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
         payload = (accurate_payload *)data.mv_data;

         if (bit_is_set(payload->filenr, m_seen_bitmap) ||
             plugin_check_file(jcr, (char *)key.mv_data)) {
            continue;
         }

         Dmsg1(dbglvl, "deleted fname=%s\n", key.mv_data);
         decode_stat(payload->lstat, &statp, sizeof(struct stat), &LinkFIc); /* decode catalog stat */
         ff_pkt->fname = (char *)key.mv_data;
         ff_pkt->statp.st_mtime = statp.st_mtime;
         ff_pkt->statp.st_ctime = statp.st_ctime;
         encode_and_send_attributes(jcr, ff_pkt, stream);
      }
      mdb_cursor_close(cursor);
   } else {
      Jmsg1(jcr, M_FATAL, 0, _("Unable create cursor: %s\n"), mdb_strerror(result));
   }

   mdb_txn_reset(m_db_ro_txn);
   result = mdb_txn_renew(m_db_ro_txn);
   if (result != 0) {
      Jmsg1(jcr, M_FATAL, 0, _("Unable to renew read transaction: %s\n"), mdb_strerror(result));
      goto bail_out;
   }

   retval = true;

bail_out:
   term_find_files(ff_pkt);
   return retval;
}

void B_ACCURATE_LMDB::destroy(JCR *jcr)
{
   /*
    * Abort any pending read transaction.
    */
   if (m_db_ro_txn) {
      mdb_txn_abort(m_db_ro_txn);
      m_db_ro_txn = NULL;
   }

   /*
    * Abort any pending write transaction.
    */
   if (m_db_rw_txn) {
      mdb_txn_abort(m_db_rw_txn);
      m_db_rw_txn = NULL;
   }

   if (m_db_env) {
      /*
       * Drop the contents of the LMDB.
       */
      if (m_db_dbi) {
         int result;
         MDB_txn *txn;

         result = mdb_txn_begin(m_db_env, NULL, 0, &txn);
         if (result == 0) {
            result = mdb_drop(txn, m_db_dbi, 1);
            if (result == 0) {
               mdb_txn_commit(txn);
            } else {
               mdb_txn_abort(txn);
            }
         }
         m_db_dbi = 0;
      }

      /*
       * Close the environment.
       */
      mdb_env_close(m_db_env);
      m_db_env = NULL;
   }

   if (m_pay_load) {
      free_pool_memory(m_pay_load);
      m_pay_load = NULL;
   }

   if (m_lmdb_name) {
      unlink(m_lmdb_name);
      free_pool_memory(m_lmdb_name);
      m_lmdb_name = NULL;
   }

   if (m_seen_bitmap) {
      free(m_seen_bitmap);
      m_seen_bitmap = NULL;
   }

   m_filenr = 0;
}
#endif /* HAVE_LMDB */
