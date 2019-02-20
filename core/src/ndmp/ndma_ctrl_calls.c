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

#ifndef NDMOS_OPTION_NO_CONTROL_AGENT


/*
 * DATA Agent calls
 ****************************************************************
 */

int ndmca_data_get_state(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.data;
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndmp9_data_get_state_reply* state = &ca->data_state;
  int rc;

  NDMC_WITH_VOID_REQUEST(ndmp9_data_get_state, NDMP9VER)
  rc = NDMC_CALL(conn);
  if (rc) {
    NDMOS_MACRO_ZEROFILL(state);
    ca->data_state.state = -1;
  } else {
    *state = *reply;
  }
  NDMC_ENDWITH

  return rc;
}

int ndmca_data_listen(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.data;
  struct ndm_control_agent* ca = sess->control_acb;
  int rc;

  NDMC_WITH(ndmp9_data_listen, NDMP9VER)
  if (sess->plumb.tape == sess->plumb.data) {
    request->addr_type = NDMP9_ADDR_LOCAL;
  } else {
    request->addr_type = NDMP9_ADDR_TCP;
  }
  rc = NDMC_CALL(conn);
  if (rc) return rc;

  if (request->addr_type != reply->data_connection_addr.addr_type) {
    ndmalogf(sess, 0, 0, "DATA_LISTEN addr_type mismatch");
    return -1;
  }

  ca->data_addr = reply->data_connection_addr;
  NDMC_ENDWITH

  return 0;
}

int ndmca_data_connect(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.data;
  struct ndm_control_agent* ca = sess->control_acb;
  int rc;
  ndmp9_addr addr;

  if (ca->job.tape_tcp) {
    char* host;
    char* port;
    struct sockaddr_in sin;

    host = ca->job.tape_tcp;
    port = strchr(ca->job.tape_tcp, ':');
    if (!port) { return 1; }
    *port++ = '\0';
    rc = ndmhost_lookup(host, &sin);
    addr.addr_type = NDMP9_ADDR_TCP;
    addr.ndmp9_addr_u.tcp_addr.ip_addr = ntohl(sin.sin_addr.s_addr);
    addr.ndmp9_addr_u.tcp_addr.port = atoi(port);
  } else {
    addr = ca->mover_addr;
  }

  NDMC_WITH(ndmp9_data_connect, NDMP9VER)
  request->addr = addr;
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_data_start_backup(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.data;
  struct ndm_control_agent* ca = sess->control_acb;
  unsigned n_env;
  ndmp9_pval* env;
  ndmp9_addr addr;
  int rc;

  if (conn->protocol_version > 2) {
    if (ca->swap_connect) {
      if ((rc = ndmca_mover_connect(sess)) != 0) { return rc; }
    } else {
      if ((rc = ndmca_data_connect(sess)) != 0) { return rc; }
    }
    addr.addr_type = NDMP9_ADDR_AS_CONNECTED;
  } else {
    addr = ca->mover_addr;
  }

  env = ndma_enumerate_env_list(&ca->job.env_tab);
  if (!env) {
    ndmalogf(sess, 0, 0, "Failed allocating enumerate buffer");
    return -1;
  }
  n_env = ca->job.env_tab.n_env;
  NDMC_WITH(ndmp9_data_start_backup, NDMP9VER)
  request->addr = addr;
  request->bu_type = ca->job.bu_type;
  request->env.env_len = n_env;
  request->env.env_val = env;

  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_data_start_recover(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.data;
  struct ndm_control_agent* ca = sess->control_acb;
  unsigned n_env;
  ndmp9_pval* env;
  unsigned n_nlist;
  ndmp9_name* nlist;
  ndmp9_addr addr;
  int rc;

  if (conn->protocol_version > 2) {
    if (ca->swap_connect) {
      if ((rc = ndmca_mover_connect(sess)) != 0) { return rc; }
    } else {
      if ((rc = ndmca_data_connect(sess)) != 0) { return rc; }
    }
    addr.addr_type = NDMP9_ADDR_AS_CONNECTED;
  } else {
    addr = ca->mover_addr;
  }

  env = ndma_enumerate_env_list(&ca->job.env_tab);
  if (!env) {
    ndmalogf(sess, 0, 0, "Failed allocating enumerate buffer");
    return -1;
  }
  n_env = ca->job.env_tab.n_env;
  nlist = ndma_enumerate_nlist(&ca->job.nlist_tab);
  n_nlist = ca->job.nlist_tab.n_nlist;
  NDMC_WITH(ndmp9_data_start_recover, NDMP9VER)
  request->addr = addr;
  request->bu_type = ca->job.bu_type;
  request->env.env_len = n_env;
  request->env.env_val = env;
  request->nlist.nlist_len = n_nlist;
  request->nlist.nlist_val = nlist;

  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_data_start_recover_filehist(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.data;
  struct ndm_control_agent* ca = sess->control_acb;
  unsigned n_env;
  ndmp9_pval* env;
  unsigned n_nlist;
  ndmp9_name* nlist;
  ndmp9_addr addr;
  int rc;

  if (conn->protocol_version > 2) {
    if (ca->swap_connect) {
      if ((rc = ndmca_mover_connect(sess)) != 0) { return rc; }
    } else {
      if ((rc = ndmca_data_connect(sess)) != 0) { return rc; }
    }
    addr.addr_type = NDMP9_ADDR_AS_CONNECTED;
  } else {
    addr = ca->mover_addr;
  }

  env = ndma_enumerate_env_list(&ca->job.env_tab);
  if (!env) {
    ndmalogf(sess, 0, 0, "Failed allocating enumerate buffer");
    return -1;
  }
  n_env = ca->job.env_tab.n_env;
  nlist = ndma_enumerate_nlist(&ca->job.nlist_tab);
  n_nlist = ca->job.nlist_tab.n_nlist;
  NDMC_WITH(ndmp9_data_start_recover_filehist, NDMP9VER)
  request->addr = addr;
  request->bu_type = ca->job.bu_type;
  request->env.env_len = n_env;
  request->env.env_val = env;
  request->nlist.nlist_len = n_nlist;
  request->nlist.nlist_val = nlist;

  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_data_abort(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.data;
  int rc;

  NDMC_WITH_VOID_REQUEST(ndmp9_data_abort, NDMP9VER)
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_data_get_env(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.data;
  struct ndm_control_agent* ca = sess->control_acb;
  int rc;
  unsigned int i;

  NDMC_WITH_VOID_REQUEST(ndmp9_data_get_env, NDMP9VER)
  rc = NDMC_CALL(conn);
  if (rc) return rc;

  for (i = 0; i < reply->env.env_len; i++) {
    ndma_store_env_list(&ca->job.result_env_tab, &reply->env.env_val[i]);
  }

  NDMC_FREE_REPLY();
  NDMC_ENDWITH

  return rc;
}

int ndmca_data_stop(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.data;
  int rc;

  NDMC_WITH_VOID_REQUEST(ndmp9_data_stop, NDMP9VER)
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}


/*
 * TAPE Agent calls -- TAPE
 ****************************************************************
 */

int ndmca_tape_open(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.tape;
  struct ndm_control_agent* ca = sess->control_acb;
  int rc;

  NDMC_WITH(ndmp9_tape_open, NDMP9VER)
  request->device = ca->job.tape_device;
  request->mode = ca->tape_mode;
  rc = NDMC_CALL(conn);
  ca->tape_state.error = reply->error;
  NDMC_ENDWITH

  return rc;
}

int ndmca_tape_close(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.tape;
  int rc;

  NDMC_WITH_VOID_REQUEST(ndmp9_tape_close, NDMP9VER)
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_tape_get_state(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.tape;
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndmp9_tape_get_state_reply* state = &ca->tape_state;
  int rc;

  NDMC_WITH_VOID_REQUEST(ndmp9_tape_get_state, NDMP9VER)
  rc = NDMC_CALL(conn);
  if (rc) {
    NDMOS_MACRO_ZEROFILL(state);
    /* tape_state.state = -1; */
    state->error = reply->error;
  } else {
    *state = *reply;
  }
  NDMC_ENDWITH

  return rc;
}

int ndmca_tape_get_state_no_tattle(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.tape;
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndmp9_tape_get_state_reply* state = &ca->tape_state;
  int rc;

  NDMC_WITH_VOID_REQUEST(ndmp9_tape_get_state, NDMP9VER)
  rc = ndma_call_no_tattle(conn, xa);
  if (rc) {
    NDMOS_MACRO_ZEROFILL(state);
    /* tape_state.state = -1; */
  } else {
    *state = *reply;
  }
  if (rc < 0 ||
      (reply->error != NDMP9_DEV_NOT_OPEN_ERR && reply->error != NDMP9_NO_ERR))
    ndma_tattle(sess->plumb.tape, xa, rc);
  NDMC_ENDWITH

  return rc;
}

int ndmca_tape_mtio(struct ndm_session* sess,
                    ndmp9_tape_mtio_op op,
                    uint32_t count,
                    uint32_t* resid)
{
  struct ndmconn* conn = sess->plumb.tape;
  int rc;

  NDMC_WITH(ndmp9_tape_mtio, NDMP9VER)
  request->tape_op = op;
  request->count = count;

  rc = NDMC_CALL(conn);
  if (!rc) {
    if (resid) {
      *resid = reply->resid_count;
    } else if (reply->resid_count != 0) {
      return -1;
    }
  }
  NDMC_ENDWITH

  return rc;
}

int ndmca_tape_write(struct ndm_session* sess, char* buf, unsigned count)
{
  struct ndmconn* conn = sess->plumb.tape;
  int rc;

  NDMC_WITH(ndmp9_tape_write, NDMP9VER)
  request->data_out.data_out_len = count;
  request->data_out.data_out_val = buf;
  rc = NDMC_CALL(conn);
  if (rc == 0) {
    if (reply->count != count) rc = -1;
  }
  NDMC_ENDWITH

  return rc;
}

int ndmca_tape_read(struct ndm_session* sess, char* buf, unsigned count)
{
  struct ndmconn* conn = sess->plumb.tape;
  int rc;

  NDMC_WITH(ndmp9_tape_read, NDMP9VER)
  request->count = count;
  rc = NDMC_CALL(conn);
  if (rc == 0) {
    if (reply->data_in.data_in_len == count) {
      bcopy(reply->data_in.data_in_val, buf, count);
    } else {
      rc = -1;
    }
  }
  NDMC_FREE_REPLY();
  NDMC_ENDWITH

  return rc;
}


int ndmca_tape_read_partial(struct ndm_session* sess,
                            char* buf,
                            unsigned count,
                            int* read_count)
{
  struct ndmconn* conn = sess->plumb.tape;
  int rc;

  NDMC_WITH(ndmp9_tape_read, NDMP9VER)
  request->count = count;
  rc = NDMC_CALL(conn);
  if (rc == 0) {
    *read_count = reply->data_in.data_in_len;
    bcopy(reply->data_in.data_in_val, buf, *read_count);
  } else {
    rc = reply->error;
  }
  NDMC_FREE_REPLY();
  NDMC_ENDWITH

  return rc;
}


/*
 * TAPE Agent calls -- MOVER
 ****************************************************************
 */

int ndmca_mover_get_state(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.tape;
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndmp9_mover_get_state_reply* state = &ca->mover_state;
  int rc;

  NDMC_WITH_VOID_REQUEST(ndmp9_mover_get_state, NDMP9VER)
  rc = NDMC_CALL(conn);
  if (rc) {
    NDMOS_MACRO_ZEROFILL(state);
    ca->mover_state.state = -1;
  } else {
    *state = *reply;
  }
  NDMC_ENDWITH

  return rc;
}

int ndmca_mover_listen(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.tape;
  struct ndm_control_agent* ca = sess->control_acb;
  int rc;

  NDMC_WITH(ndmp9_mover_listen, NDMP9VER)
  request->mode = ca->mover_mode;

  if (sess->plumb.tape == sess->plumb.data) {
    request->addr_type = NDMP9_ADDR_LOCAL;
  } else {
    request->addr_type = NDMP9_ADDR_TCP;
  }
  rc = NDMC_CALL(conn);
  if (rc) return rc;

  if (request->addr_type != reply->data_connection_addr.addr_type) {
    ndmalogf(sess, 0, 0, "MOVER_LISTEN addr_type mismatch");
    return -1;
  }

  ca->mover_addr = reply->data_connection_addr;
  NDMC_ENDWITH

  return 0;
}

int ndmca_mover_connect(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.tape;
  struct ndm_control_agent* ca = sess->control_acb;
  int rc;

  NDMC_WITH(ndmp9_mover_connect, NDMP9VER)
  request->mode = ca->mover_mode;
  request->addr = ca->data_addr;
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_mover_continue(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.tape;
  int rc;

  NDMC_WITH_VOID_REQUEST(ndmp9_mover_continue, NDMP9VER)
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_mover_abort(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.tape;
  int rc;

  NDMC_WITH_VOID_REQUEST(ndmp9_mover_abort, NDMP9VER)
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_mover_stop(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.tape;
  int rc;

  NDMC_WITH_VOID_REQUEST(ndmp9_mover_stop, NDMP9VER)
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_connect_close(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.tape;
  int rc;

  NDMC_WITH_NO_REPLY(ndmp9_connect_close, NDMP9VER)
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_mover_set_window(struct ndm_session* sess,
                           uint64_t offset,
                           uint64_t length)
{
  struct ndmconn* conn = sess->plumb.tape;
  int rc;

  NDMC_WITH(ndmp9_mover_set_window, NDMP9VER)
  request->offset = offset;
  request->length = length;
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_mover_read(struct ndm_session* sess, uint64_t offset, uint64_t length)
{
  struct ndmconn* conn = sess->plumb.tape;
  int rc;

  NDMC_WITH(ndmp9_mover_read, NDMP9VER)
  request->offset = offset;
  request->length = length;
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_mover_close(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.tape;
  int rc;

  NDMC_WITH_VOID_REQUEST(ndmp9_mover_close, NDMP9VER)
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_mover_set_record_size(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.tape;
  struct ndm_control_agent* ca = sess->control_acb;
  int rc;

  NDMC_WITH(ndmp9_mover_set_record_size, NDMP9VER)
  request->record_size = ca->job.record_size;
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
