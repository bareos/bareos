/*
 * Copyright (c) 1998,1999,2000
 *      Traakan, Inc., Los Altos, CA
 *      All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Project:  NDMJOB
 * Ident:    $Id: $
 *
 * Description:
 *
 */


#include "ndmagents.h"

#ifndef NDMOS_OPTION_NO_DATA_AGENT


/*
 * Initialization and Cleanup
 ****************************************************************
 */

/* Initialize -- Set data structure to know value, ignore current value */
int ndmda_fh_initialize(struct ndm_session* sess)
{
  struct ndm_data_agent* da = sess->data_acb;
  struct ndmfhheap* fhh = &da->fhh;

  ndmfhh_initialize(fhh);

  return 0;
}

/* Commission -- Get agent ready. Entire session has been initialize()d */
int ndmda_fh_commission(struct ndm_session* sess)
{
  struct ndm_data_agent* da = sess->data_acb;
  struct ndmfhheap* fhh = &da->fhh;

  da->fhh_buf = NDMOS_API_MALLOC(NDMDA_N_FHH_BUF);
  if (!da->fhh_buf) return -1;
  // ndmfhh_commission (fhh, da->fhh_buf, sizeof *da->fhh_buf);
  ndmfhh_commission(fhh, da->fhh_buf, NDMDA_N_FHH_BUF);

  return 0;
}

/* Decommission -- Discard agent */
int ndmda_fh_decommission([[maybe_unused]] struct ndm_session* sess)
{
  return 0;
}

/* Destroy -- Destroy agent */
int ndmda_fh_destroy(struct ndm_session* sess)
{
  if (sess->data_acb->fhh_buf) {
    NDMOS_API_FREE(sess->data_acb->fhh_buf);
    sess->data_acb->fhh_buf = NULL;
  }

  return 0;
}

/* Belay -- Cancel partially issued activation/start */
int ndmda_fh_belay([[maybe_unused]] struct ndm_session* sess) { return 0; }


/*
 * Semantic actions -- called from ndmda_XXX() butype formatters
 ****************************************************************
 */

void ndmda_fh_add_file(struct ndm_session* sess,
                       ndmp9_file_stat* filestat,
                       char* name)
{
  struct ndm_data_agent* da = sess->data_acb;
  int nlen = strlen(name) + 1;
  ndmp9_file* file9;
  int rc;

  rc = ndmda_fh_prepare(sess, NDMP9VER, NDMP9_FH_ADD_FILE, sizeof(ndmp9_file),
                        1, nlen);

  if (rc != NDMFHH_RET_OK) return;

  file9 = ndmfhh_add_entry(&da->fhh);
  file9->fstat = *filestat;
  file9->unix_path = ndmfhh_save_item(&da->fhh, name, nlen);
}

void ndmda_fh_add_dir(struct ndm_session* sess,
                      uint64_t dir_fileno,
                      char* name,
                      uint64_t fileno)
{
  struct ndm_data_agent* da = sess->data_acb;
  int nlen = strlen(name) + 1;
  ndmp9_dir* dir9;
  int rc;

  rc = ndmda_fh_prepare(sess, NDMP9VER, NDMP9_FH_ADD_DIR, sizeof(ndmp9_dir), 1,
                        nlen);

  if (rc != NDMFHH_RET_OK) return;

  dir9 = ndmfhh_add_entry(&da->fhh);
  dir9->unix_name = ndmfhh_save_item(&da->fhh, name, nlen);
  dir9->parent = dir_fileno;
  dir9->node = fileno;
}

void ndmda_fh_add_node(struct ndm_session* sess, ndmp9_file_stat* filestat)
{
  struct ndm_data_agent* da = sess->data_acb;
  ndmp9_node* node9;
  int rc;

  rc = ndmda_fh_prepare(sess, NDMP9VER, NDMP9_FH_ADD_NODE, sizeof(ndmp9_node),
                        1, 0);

  if (rc != NDMFHH_RET_OK) return;

  node9 = ndmfhh_add_entry(&da->fhh);
  node9->fstat = *filestat;
}


/*
 * Helpers -- prepare/flush
 ****************************************************************
 */

int ndmda_fh_prepare(struct ndm_session* sess,
                     int vers,
                     int msg,
                     int entry_size,
                     unsigned n_item,
                     unsigned total_size_of_items)
{
  struct ndm_data_agent* da = sess->data_acb;
  struct ndmfhheap* fhh = &da->fhh;
  int fhtype = (vers << 16) + msg;
  int rc;

  rc = ndmfhh_prepare(fhh, fhtype, entry_size, n_item, total_size_of_items);

  if (rc == NDMFHH_RET_OK) return NDMFHH_RET_OK;

  ndmda_fh_flush(sess);

  rc = ndmfhh_prepare(fhh, fhtype, entry_size, n_item, total_size_of_items);

  return rc;
}

void ndmda_fh_flush(struct ndm_session* sess)
{
  struct ndm_data_agent* da = sess->data_acb;
  struct ndmfhheap* fhh = &da->fhh;
  int rc;
  int fhtype;
  void* table;
  unsigned n_entry;

  rc = ndmfhh_get_table(fhh, &fhtype, &table, &n_entry);
  if (rc == NDMFHH_RET_OK && n_entry > 0) {
    struct ndmp_xa_buf xa;
    struct ndmfhh_generic_table* request;

    request = (void*)&xa.request.body;
    NDMOS_MACRO_ZEROFILL(&xa);

    xa.request.protocol_version = fhtype >> 16;
    xa.request.header.message = fhtype & 0xFFFF;

    request->table_len = n_entry;
    request->table_val = table;

    ndma_send_to_control(sess, &xa, sess->plumb.data);
  }

  ndmfhh_reset(fhh);
}

#endif /* !NDMOS_OPTION_NO_DATA_AGENT */
