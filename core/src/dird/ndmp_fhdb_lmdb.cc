/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2015 Planets Communications B.V.
   Copyright (C) 2015-2023 Bareos GmbH & Co. KG

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
// Marco van Wieringen & Philipp Storz, May 2015
/*
 * @file
 * FHDB using LMDB for NDMP Data Management Application (DMA)
 */

#include "include/bareos.h"
#include "include/filetypes.h"
#include "dird.h"

#if defined(HAVE_NDMP) && defined(HAVE_LMDB)

#  include "ndmp/ndmagents.h"
#  include "ndmp_dma_priv.h"
#  include "lmdb/lmdb.h"

namespace directordaemon {

// What is actually stored in the LMDB
struct fhdb_payload {
  ndmp9_u_quad node;
  ndmp9_u_quad dir_node;
  ndmp9_file_stat ndmp_fstat;
  char namebuffer[1];
};

struct fhdb_state {
  uint64_t root_node;
  POOLMEM* pay_load;
  POOLMEM* lmdb_name;
  POOLMEM* path;
  MDB_env* db_env;
  MDB_dbi db_dbi;
  MDB_txn* db_rw_txn;
  MDB_txn* db_ro_txn;
};

static int debuglevel = 100;

// Payload = 8 + 8 + 96 = 112 bytes + namelength.
#  define AVG_NR_BYTES_PER_ENTRY 256
#  define B_PAGE_SIZE 4096

extern "C" int bndmp_fhdb_lmdb_add_dir(struct ndmlog* ixlog,
                                       int,
                                       char* raw_name,
                                       ndmp9_u_quad dir_node,
                                       ndmp9_u_quad node)
{
  NIS* nis = (NIS*)ixlog->ctx;

  // Ignore . and .. directory entries.
  if (bstrcmp(raw_name, ".") || bstrcmp(raw_name, "..")) { return 0; }

  if (nis->save_filehist) {
    int result, length;
    int total_length;
    MDB_val key, data;
    struct fhdb_payload* payload;
    struct fhdb_state* fhdb_state = (struct fhdb_state*)nis->fhdb_state;

    Dmsg3(100, "{ \"%s\", %llu , %llu},\n", raw_name, dir_node, node);
    Dmsg3(100,
          "bndmp_fhdb_lmdb_add_dir New dir \"%s\" - dirnode:[%llu] - "
          "node:[%llu]\n",
          raw_name, dir_node, node);

    // Make sure fhdb_state->pay_load is large enough.
    length = strlen(raw_name);
    total_length = sizeof(struct fhdb_payload) + length + 2;
    fhdb_state->pay_load
        = CheckPoolMemorySize(fhdb_state->pay_load, total_length);
    payload = (struct fhdb_payload*)fhdb_state->pay_load;
    payload->node = node;
    payload->dir_node = dir_node;
    memset(&payload->ndmp_fstat, 0, sizeof(ndmp9_file_stat));
    memcpy(payload->namebuffer, raw_name, length + 1);

    key.mv_data = &payload->node;
    key.mv_size = sizeof(payload->node);

    data.mv_data = payload;
    data.mv_size = total_length;

  retry:
    result = mdb_put(fhdb_state->db_rw_txn, fhdb_state->db_dbi, &key, &data, 0);
    switch (result) {
      case 0:
        Dmsg3(debuglevel, "added file \"%s\", node=%llu, dir_node=%llu\n",
              raw_name, payload->node, payload->dir_node);
        break;
      case MDB_TXN_FULL:
        /* Seems we filled the transaction.
         * Flush the current transaction start a new one and retry the put. */
        result = mdb_txn_commit(fhdb_state->db_rw_txn);
        if (result == 0) {
          result = mdb_txn_begin(fhdb_state->db_env, NULL, 0,
                                 &fhdb_state->db_rw_txn);
          if (result == 0) {
            goto retry;
          } else {
            Dmsg1(debuglevel, T_("Unable to create new transaction: %s\n"),
                  mdb_strerror(result));
            Jmsg1(nis->jcr, M_FATAL, 0,
                  T_("Unable create new transaction: %s\n"),
                  mdb_strerror(result));
            goto bail_out;
          }
        } else {
          Dmsg1(debuglevel, T_("Unable to commit full transaction: %s\n"),
                mdb_strerror(result));
          Jmsg1(nis->jcr, M_FATAL, 0,
                T_("Unable to commit full transaction: %s\n"),
                mdb_strerror(result));
          goto bail_out;
        }
        break;
      default:
        Dmsg2(debuglevel, T_("Unable to insert new data at %llu: %s\n"),
              payload->node, mdb_strerror(result));
        Jmsg2(nis->jcr, M_FATAL, 0, T_("Unable insert new data at %llu: %s\n"),
              payload->node, mdb_strerror(result));
        goto bail_out;
    }
  }

  return 0;

bail_out:
  return 1;
}

extern "C" int bndmp_fhdb_lmdb_add_node(struct ndmlog* ixlog,
                                        int,
                                        ndmp9_u_quad node,
                                        ndmp9_file_stat* ndmp_fstat)
{
  NIS* nis;
  nis = (NIS*)ixlog->ctx;

  nis->jcr->lock();
  nis->jcr->JobFiles++;
  nis->jcr->unlock();

  if (nis->save_filehist) {
    int result;
    MDB_val key, data;
    struct fhdb_payload* payload;
    struct fhdb_state* fhdb_state = (struct fhdb_state*)nis->fhdb_state;

    Dmsg1(100, "{ NULL, %llu , 0},\n", node);
    Dmsg1(debuglevel, "bndmp_fhdb_lmdb_add_node node:[%llu]\n", node);

    /* Need to update which means we first get the existing data
     * and update the fields and write back. */
    key.mv_data = &node;
    key.mv_size = sizeof(node);

  retry_get:
    result = mdb_get(fhdb_state->db_rw_txn, fhdb_state->db_dbi, &key, &data);
    switch (result) {
      case 0:
        // Make a copy of the current pay_load.
        fhdb_state->pay_load
            = CheckPoolMemorySize(fhdb_state->pay_load, data.mv_size);
        memcpy(fhdb_state->pay_load, data.mv_data, data.mv_size);
        payload = (struct fhdb_payload*)fhdb_state->pay_load;

        // Copy the new file statistics,
        memcpy(&payload->ndmp_fstat, ndmp_fstat, sizeof(ndmp9_file_stat));

        // Keys and length don't change only content.
        data.mv_data = payload;

      retry_del:
        result = mdb_del(fhdb_state->db_rw_txn, fhdb_state->db_dbi, &key, NULL);
        switch (result) {
          case 0:
            break;
          case MDB_TXN_FULL:
            /* Seems we filled the transaction.
             * Flush the current transaction start a new one and retry the
             * delete. */
            result = mdb_txn_commit(fhdb_state->db_rw_txn);
            if (result == 0) {
              result = mdb_txn_begin(fhdb_state->db_env, NULL, 0,
                                     &fhdb_state->db_rw_txn);
              if (result == 0) {
                goto retry_del;
              } else {
                Dmsg1(debuglevel, T_("Unable to create new transaction: %s\n"),
                      mdb_strerror(result));
                Jmsg1(nis->jcr, M_FATAL, 0,
                      T_("Unable create new transaction: %s\n"),
                      mdb_strerror(result));
                goto bail_out;
              }
            } else {
              Dmsg1(debuglevel, T_("Unable to commit full transaction: %s\n"),
                    mdb_strerror(result));
              Jmsg1(nis->jcr, M_FATAL, 0,
                    T_("Unable to commit full transaction: %s\n"),
                    mdb_strerror(result));
              goto bail_out;
            }
            break;
          default:
            Dmsg1(debuglevel, T_("Unable delete old data: %s\n"),
                  mdb_strerror(result));
            Jmsg1(nis->jcr, M_FATAL, 0, T_("Unable delete old data: %s\n"),
                  mdb_strerror(result));
            goto bail_out;
        }

      retry_put:
        // Overwrite existing key
        result = mdb_put(fhdb_state->db_rw_txn, fhdb_state->db_dbi, &key, &data,
                         MDB_NODUPDATA);
        switch (result) {
          case 0:
            break;
          case MDB_TXN_FULL:
            /* Seems we filled the transaction.
             * Flush the current transaction start a new one and retry the put.
             */
            result = mdb_txn_commit(fhdb_state->db_rw_txn);
            if (result == 0) {
              result = mdb_txn_begin(fhdb_state->db_env, NULL, 0,
                                     &fhdb_state->db_rw_txn);
              if (result == 0) {
                goto retry_put;
              } else {
                Dmsg1(debuglevel, T_("Unable to create new transaction: %s\n"),
                      mdb_strerror(result));
                Jmsg1(nis->jcr, M_FATAL, 0,
                      T_("Unable create new transaction: %s\n"),
                      mdb_strerror(result));
                goto bail_out;
              }
            } else {
              Dmsg1(debuglevel, T_("Unable to commit full transaction: %s\n"),
                    mdb_strerror(result));
              Jmsg1(nis->jcr, M_FATAL, 0,
                    T_("Unable to commit full transaction: %s\n"),
                    mdb_strerror(result));
              goto bail_out;
            }
            break;
          default:
            Dmsg1(debuglevel, T_("Unable put new data: %s\n"),
                  mdb_strerror(result));
            Jmsg1(nis->jcr, M_FATAL, 0, T_("Unable put new data: %s\n"),
                  mdb_strerror(result));
            goto bail_out;
        }
        break;
      case MDB_TXN_FULL:
        /* Seems we filled the transaction.
         * Flush the current transaction start a new one and retry the put. */
        result = mdb_txn_commit(fhdb_state->db_rw_txn);
        if (result == 0) {
          result = mdb_txn_begin(fhdb_state->db_env, NULL, 0,
                                 &fhdb_state->db_rw_txn);
          if (result == 0) {
            goto retry_get;
          } else {
            Dmsg1(debuglevel, T_("Unable to create new transaction: %s\n"),
                  mdb_strerror(result));
            Jmsg1(nis->jcr, M_FATAL, 0,
                  T_("Unable to create new transaction: %s\n"),
                  mdb_strerror(result));
            goto bail_out;
          }
        } else {
          Dmsg1(debuglevel, T_("Unable to commit full transaction: %s\n"),
                mdb_strerror(result));
          Jmsg1(nis->jcr, M_FATAL, 0,
                T_("Unable to commit full transaction: %s\n"),
                mdb_strerror(result));
          goto bail_out;
        }
        break;
      default:
        Dmsg1(debuglevel, T_("Unable get old data: %s\n"),
              mdb_strerror(result));
        Jmsg1(nis->jcr, M_FATAL, 0, T_("Unable get old data: %s\n"),
              mdb_strerror(result));
        goto bail_out;
    }
  }

  return 0;

bail_out:
  return 1;
}

extern "C" int bndmp_fhdb_lmdb_add_dirnode_root(struct ndmlog* ixlog,
                                                int,
                                                ndmp9_u_quad root_node)
{
  NIS* nis = (NIS*)ixlog->ctx;

  if (nis->save_filehist) {
    int result;
    int total_length;
    MDB_val key, data;
    struct fhdb_payload* payload;
    struct fhdb_state* fhdb_state = (struct fhdb_state*)nis->fhdb_state;

    Dmsg1(100, "{ NULL, 0, %llu },\n", root_node);
    Dmsg1(100, "bndmp_fhdb_lmdb_add_dirnode_root: New root node [%llu]\n",
          root_node);

    // Make sure fhdb_state->pay_load is large enough.
    total_length = sizeof(struct fhdb_payload);
    fhdb_state->pay_load
        = CheckPoolMemorySize(fhdb_state->pay_load, total_length);
    payload = (struct fhdb_payload*)fhdb_state->pay_load;

    fhdb_state->root_node = root_node;
    payload->node = root_node;
    payload->dir_node = root_node;
    payload->namebuffer[0] = '\0';
    memset(&payload->ndmp_fstat, 0, sizeof(payload->ndmp_fstat));

    key.mv_data = &payload->node;
    key.mv_size = sizeof(payload->node);

    data.mv_data = payload;
    data.mv_size = total_length;

  retry:
    result = mdb_put(fhdb_state->db_rw_txn, fhdb_state->db_dbi, &key, &data,
                     MDB_NOOVERWRITE);
    switch (result) {
      case 0:
        Dmsg1(100, "new rootnode=%llu\n", root_node);
        break;
      case MDB_TXN_FULL:
        /* Seems we filled the transaction.
         * Flush the current transaction start a new one and retry the put. */
        result = mdb_txn_commit(fhdb_state->db_rw_txn);
        if (result == 0) {
          result = mdb_txn_begin(fhdb_state->db_env, NULL, 0,
                                 &fhdb_state->db_rw_txn);
          if (result == 0) {
            goto retry;
          } else {
            Dmsg1(debuglevel, T_("Unable to create new transaction: %s\n"),
                  mdb_strerror(result));
            Jmsg1(nis->jcr, M_FATAL, 0,
                  T_("Unable to create new transaction: %s\n"),
                  mdb_strerror(result));
            goto bail_out;
          }
        } else {
          Dmsg1(debuglevel, T_("Unable to commit full transaction: %s\n"),
                mdb_strerror(result));
          Jmsg1(nis->jcr, M_FATAL, 0,
                T_("Unable to commit full transaction: %s\n"),
                mdb_strerror(result));
          goto bail_out;
        }
        break;
      default:
        Dmsg1(debuglevel, T_("Unable insert new data: %s\n"),
              mdb_strerror(result));
        Jmsg1(nis->jcr, M_FATAL, 0, T_("Unable insert new data: %s\n"),
              mdb_strerror(result));
        goto bail_out;
    }
  }

  return 0;

bail_out:
  return 1;
}

// This glues the NDMP File Handle DB with internal code.
void NdmpFhdbLmdbRegister(struct ndmlog* ixlog)
{
  NIS* nis = (NIS*)ixlog->ctx;
  struct ndm_fhdb_callbacks fhdb_callbacks;

  // Register the FileHandleDB callbacks.
  fhdb_callbacks.add_file = BndmpFhdbAddFile;
  fhdb_callbacks.add_dir = bndmp_fhdb_lmdb_add_dir;
  fhdb_callbacks.add_node = bndmp_fhdb_lmdb_add_node;
  fhdb_callbacks.add_dirnode_root = bndmp_fhdb_lmdb_add_dirnode_root;
  ndmfhdb_register_callbacks(ixlog, &fhdb_callbacks);

  Dmsg0(100, "NdmpFhdbLmdbRegister\n");

  if (nis->save_filehist) {
    int result;
    ssize_t mapsize = 10485760;
    NIS* nis = (NIS*)ixlog->ctx;
    struct fhdb_state* fhdb_state;

    // Initiate LMDB environment
    fhdb_state = (struct fhdb_state*)malloc(sizeof(struct fhdb_state));
    memset(fhdb_state, 0, sizeof(struct fhdb_state));

    fhdb_state->lmdb_name = GetPoolMemory(PM_FNAME);
    fhdb_state->pay_load = GetPoolMemory(PM_MESSAGE);
    fhdb_state->path = GetPoolMemory(PM_FNAME);

    result = mdb_env_create(&fhdb_state->db_env);
    if (result) {
      Jmsg1(nis->jcr, M_FATAL, 0, T_("Unable to create MDB environment: %s\n"),
            mdb_strerror(result));
      Dmsg1(debuglevel, T_("Unable to create MDB environment: %s\n"),
            mdb_strerror(result));
      goto bail_out;
    }

    if ((nis->filehist_size * AVG_NR_BYTES_PER_ENTRY) > mapsize) {
      size_t pagesize;

#  ifdef HAVE_GETPAGESIZE
      pagesize = getpagesize();
#  else
      pagesize = B_PAGE_SIZE;
#  endif

      mapsize = (((nis->filehist_size * AVG_NR_BYTES_PER_ENTRY) / pagesize) + 1)
                * pagesize;
    }

    result = mdb_env_set_mapsize(fhdb_state->db_env, mapsize);
    if (result) {
      Dmsg1(debuglevel, T_("Unable to set MDB mapsize: %s\n"),
            mdb_strerror(result));
      Jmsg1(nis->jcr, M_FATAL, 0, T_("Unable to set MDB mapsize: %s\n"),
            mdb_strerror(result));
      goto bail_out;
    }

    // Explicitly set the number of readers to 1.
    result = mdb_env_set_maxreaders(fhdb_state->db_env, 1);
    if (result) {
      Dmsg1(debuglevel, T_("Unable to set MDB maxreaders: %s\n"),
            mdb_strerror(result));
      Jmsg1(nis->jcr, M_FATAL, 0, T_("Unable to set MDB maxreaders: %s\n"),
            mdb_strerror(result));
      goto bail_out;
    }

    Mmsg(fhdb_state->lmdb_name, "%s/.fhdb_lmdb.%d", working_directory,
         nis->jcr->JobId);
    result = mdb_env_open(fhdb_state->db_env, fhdb_state->lmdb_name,
                          MDB_NOSUBDIR | MDB_NOLOCK | MDB_NOSYNC, 0600);
    if (result != MDB_SUCCESS) {
      Dmsg2(
          debuglevel,
          T_("Unable to create LMDB database %s: %s. Check OS ulimit settings "
             "or adapt FileHistorySize\n"),
          fhdb_state->lmdb_name, mdb_strerror(result));
      Jmsg2(
          nis->jcr, M_FATAL, 0,
          T_("Unable to create LMDB database %s: %s. Check OS ulimit settings "
             "or adapt FileHistorySize\n"),
          fhdb_state->lmdb_name, mdb_strerror(result));
      goto bail_out;
    }

    result = mdb_txn_begin(fhdb_state->db_env, NULL, 0, &fhdb_state->db_rw_txn);
    if (result != MDB_SUCCESS) {
      Dmsg1(debuglevel, T_("Unable to start a write transaction: %s\n"),
            mdb_strerror(result));
      Jmsg1(nis->jcr, M_FATAL, 0,
            T_("Unable to start a write transaction: %s\n"),
            mdb_strerror(result));
      goto bail_out;
    }

    result = mdb_dbi_open(fhdb_state->db_rw_txn, NULL,
                          MDB_CREATE | MDB_INTEGERKEY | MDB_DUPSORT,
                          &fhdb_state->db_dbi);
    if (result != MDB_SUCCESS) {
      Dmsg1(debuglevel, T_("Unable to open LMDB internal database: %s\n"),
            mdb_strerror(result));
      Jmsg1(nis->jcr, M_FATAL, 0,
            T_("Unable to open LMDB internal database: %s\n"),
            mdb_strerror(result));
    }

    nis->fhdb_state = (void*)fhdb_state;

    return;

  bail_out:
    if (fhdb_state->db_env) { mdb_env_close(fhdb_state->db_env); }

    FreePoolMemory(fhdb_state->lmdb_name);
    FreePoolMemory(fhdb_state->pay_load);
    FreePoolMemory(fhdb_state->path);

    free(fhdb_state);

    return;
  }
}

void NdmpFhdbLmdbUnregister(struct ndmlog* ixlog)
{
  NIS* nis = (NIS*)ixlog->ctx;
  struct fhdb_state* fhdb_state = (struct fhdb_state*)nis->fhdb_state;

  ndmfhdb_unregister_callbacks(ixlog);

  if (fhdb_state) {
    // Abort any pending write transaction.
    if (fhdb_state->db_rw_txn) { mdb_txn_abort(fhdb_state->db_rw_txn); }

    if (fhdb_state->db_env) {
      // Drop the contents of the LMDB.
      if (fhdb_state->db_dbi) {
        int result;
        MDB_txn* txn;

        result = mdb_txn_begin(fhdb_state->db_env, NULL, 0, &txn);
        if (result == 0) {
          result = mdb_drop(txn, fhdb_state->db_dbi, 1);
          if (result == 0) {
            mdb_txn_commit(txn);
          } else {
            mdb_txn_abort(txn);
          }
        }
      }

      // Close the environment.
      mdb_env_close(fhdb_state->db_env);
    }

    FreePoolMemory(fhdb_state->pay_load);
    FreePoolMemory(fhdb_state->path);
    SecureErase(nis->jcr, fhdb_state->lmdb_name);
    FreePoolMemory(fhdb_state->lmdb_name);

    free(fhdb_state);
  }
}

static inline void CalculatePath(uint64_t node, fhdb_state* fhdb_state)
{
  PoolMem temp;
  int result = 0;
  MDB_val rkey, rdata;
  struct fhdb_payload* payload;
  bool root_node_reached = false;

  Dmsg1(100, "CalculatePath for node %llu\n", node);

  PmStrcpy(fhdb_state->path, "");
  while (!result && !root_node_reached) {
    rkey.mv_data = &node;
    rkey.mv_size = sizeof(node);

    result = mdb_get(fhdb_state->db_ro_txn, fhdb_state->db_dbi, &rkey, &rdata);
    switch (result) {
      case 0:
        payload = (struct fhdb_payload*)rdata.mv_data;
        if (node != fhdb_state->root_node) {
          PmStrcpy(temp, "/");
          PmStrcat(temp, payload->namebuffer);
          PmStrcat(temp, fhdb_state->path);
          PmStrcpy(fhdb_state->path, temp.c_str());
          node = payload->dir_node;
        } else {
          // Root reached
          root_node_reached = true;
        }
        break;
      case MDB_NOTFOUND:
        break;
      case EINVAL:
        Dmsg1(debuglevel, "%s\n", mdb_strerror(result));
        break;
      default:
        Dmsg1(debuglevel, "%s\n", mdb_strerror(result));
        break;
    }
  }
}

static inline void ProcessLmdb(NIS* nis, struct fhdb_state* fhdb_state)
{
  int result;
  uint64_t node;
  MDB_cursor* cursor;
  int8_t FileType = 0;
  MDB_val rkey, rdata;
  PoolMem attribs(PM_FNAME);
  ndmp9_file_stat ndmp_fstat;
  PoolMem full_path(PM_FNAME);
  struct fhdb_payload* payload;

  result = mdb_cursor_open(fhdb_state->db_ro_txn, fhdb_state->db_dbi, &cursor);
  if (result) { Dmsg1(debuglevel, "%s\n", mdb_strerror(result)); }

  rkey.mv_data = &node;
  rkey.mv_size = sizeof(node);
  result = mdb_cursor_get(cursor, &rkey, &rdata, MDB_FIRST);
  if (result) { Dmsg1(debuglevel, "%s\n", mdb_strerror(result)); }

  while (!result) {
    switch (result) {
      case 0:
        payload = (struct fhdb_payload*)rdata.mv_data;
        ndmp_fstat = payload->ndmp_fstat;
        node = *(uint64_t*)rkey.mv_data;

        if (ndmp_fstat.node.valid == NDMP9_VALIDITY_VALID) {
          CalculatePath(payload->dir_node, fhdb_state);
          NdmpConvertFstat(&ndmp_fstat, nis->FileIndex, &FileType, attribs);

          PmStrcpy(full_path, nis->filesystem);
          PmStrcat(full_path, fhdb_state->path);
          PmStrcat(full_path, "/");
          PmStrcat(full_path, payload->namebuffer);

          if (FileType == FT_DIREND) {
            /* SplitPathAndFilename() expects directories to end with a '/'
             * so append '/' if full_path does not already end with '/' */
            if (full_path.c_str()[strlen(full_path.c_str()) - 1] != '/') {
              Dmsg1(100, ("appending / to %s \n"), full_path.c_str());
              PmStrcat(full_path, "/");
            }
          }
          NdmpStoreAttributeRecord(
              nis->jcr, full_path.c_str(), nis->virtual_filename,
              attribs.c_str(), FileType, 0,
              (ndmp_fstat.fh_info.valid == NDMP9_VALIDITY_VALID)
                  ? ndmp_fstat.fh_info.value
                  : 0);
        } else {
          Dmsg1(100, "skipping node %lu because it has no valid node data\n",
                node);
        }
        result = mdb_cursor_get(cursor, &rkey, &rdata, MDB_NEXT);
        break;
      default:
        Dmsg1(debuglevel, "%s\n", mdb_strerror(result));
        break;
    }
  }
}

void NdmpFhdbLmdbProcessDb(struct ndmlog* ixlog)
{
  NIS* nis = (NIS*)ixlog->ctx;
  struct fhdb_state* fhdb_state = (struct fhdb_state*)nis->fhdb_state;

  if (!fhdb_state) { return; }

  Dmsg0(100, "NdmpFhdbLmdbProcessDb called\n");

  // Commit any pending write transactions.
  if (fhdb_state->db_rw_txn) {
    int result = mdb_txn_commit(fhdb_state->db_rw_txn);
    if (result != MDB_SUCCESS) {
      Jmsg1(nis->jcr, M_FATAL, 0, T_("Unable close write transaction: %s\n"),
            mdb_strerror(result));
      return;
    }
    result = mdb_txn_begin(fhdb_state->db_env, NULL, 0, &fhdb_state->db_rw_txn);
    if (result != MDB_SUCCESS) {
      Jmsg1(nis->jcr, M_FATAL, 0,
            T_("Unable to create write transaction: %s\n"),
            mdb_strerror(result));
      return;
    }
  }

  /* From now on we also will be doing read transactions so create a read
   * transaction context. */
  int result = mdb_txn_begin(fhdb_state->db_env, NULL, MDB_RDONLY,
                             &fhdb_state->db_ro_txn);
  if (result != MDB_SUCCESS) {
    Jmsg1(nis->jcr, M_FATAL, 0, T_("Unable to create read transaction: %s\n"),
          mdb_strerror(result));
    return;
  }

  Jmsg(nis->jcr, M_INFO, 0, "Now processing lmdb database\n");

  ProcessLmdb(nis, fhdb_state);

  Jmsg(nis->jcr, M_INFO, 0, "Processing lmdb database done\n");
}

} /* namespace directordaemon */
#endif /* HAVE_LMDB */
