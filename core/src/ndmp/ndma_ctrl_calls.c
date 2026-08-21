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

static void ndmca_clear_addr_env(ndmp9_pval** envp, unsigned* env_lenp)
{
  if (*envp) {
    ndmp_4to9_pval_vec_free(*envp, *env_lenp);
    *envp = NULL;
    *env_lenp = 0;
  }
}

static int ndmca_store_v4_addr_env(ndmp4_addr* addr,
                                   ndmp9_pval** envp,
                                   unsigned* env_lenp)
{
  ndmp4_pval* addr_env = NULL;
  unsigned addr_env_len = 0;

  ndmca_clear_addr_env(envp, env_lenp);

  switch (addr->addr_type) {
    case NDMP4_ADDR_TCP:
      if (addr->ndmp4_addr_u.tcp_addr.tcp_addr_len < 1) { return 0; }
      addr_env = addr->ndmp4_addr_u.tcp_addr.tcp_addr_val[0].addr_env.addr_env_val;
      addr_env_len
          = addr->ndmp4_addr_u.tcp_addr.tcp_addr_val[0].addr_env.addr_env_len;
      break;

    case NDMP4_ADDR_TCP_IPV6:
      if (addr->ndmp4_addr_u.tcp_ipv6_addr.tcp_ipv6_addr_len < 1) { return 0; }
      addr_env = addr->ndmp4_addr_u.tcp_ipv6_addr.tcp_ipv6_addr_val[0]
                     .addr_env.addr_env_val;
      addr_env_len = addr->ndmp4_addr_u.tcp_ipv6_addr.tcp_ipv6_addr_val[0]
                         .addr_env.addr_env_len;
      break;

    default:
      return 0;
  }

  if (addr_env_len == 0) { return 0; }

  if (ndmp_4to9_pval_vec_dup(addr_env, envp, addr_env_len) != 0) {
    return -1;
  }

  *env_lenp = addr_env_len;
  return 0;
}

static ndmp9_addr_type ndmca_preferred_tcp_addr_type(struct ndmconn* conn)
{
  struct sockaddr_storage ss;
  socklen_t ss_len = sizeof(ss);

  if (conn && conn->conn_type == NDMCONN_TYPE_REMOTE
      && conn->ipv6_extensions_enabled
      && getsockname(ndmconn_fileno(conn), (struct sockaddr*)&ss, &ss_len) == 0
      && ss.ss_family == AF_INET6) {
    return NDMP9_ADDR_TCP_IPV6;
  }

  return NDMP9_ADDR_TCP;
}

static ndmp4_addr_type ndmca_preferred_v4_addr_type(struct ndmconn* conn)
{
  return ndmca_preferred_tcp_addr_type(conn) == NDMP9_ADDR_TCP_IPV6
             ? NDMP4_ADDR_TCP_IPV6
             : NDMP4_ADDR_TCP;
}

static int ndmca_parse_tape_tcp_endpoint(const char* endpoint,
                                         char* host_buf,
                                         size_t host_buf_size,
                                         char** host,
                                         const char** port)
{
  const char* separator;
  size_t host_len;

  if (!endpoint || !host_buf || host_buf_size == 0 || !host || !port) {
    return -1;
  }

  if (endpoint[0] == '[') {
    separator = strchr(endpoint, ']');
    if (!separator || separator[1] != ':') { return -1; }

    host_len = (size_t)(separator - (endpoint + 1));
    if (host_len >= host_buf_size) { return -1; }
    memcpy(host_buf, endpoint + 1, host_len);
    host_buf[host_len] = '\0';
    *host = host_buf;
    *port = separator + 2;
    return 0;
  }

  separator = strrchr(endpoint, ':');
  if (!separator) { return -1; }

  host_len = (size_t)(separator - endpoint);
  if (host_len >= host_buf_size) { return -1; }
  memcpy(host_buf, endpoint, host_len);
  host_buf[host_len] = '\0';
  *host = host_buf;
  *port = separator + 1;
  return 0;
}


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

  if (conn->protocol_version == NDMP4VER && conn->cab_extensions_enabled) {
    NDMC_WITH(ndmp4_data_listen, NDMP4VER)
    if (sess->plumb.tape == sess->plumb.data) {
      request->addr_type = NDMP4_ADDR_LOCAL;
    } else {
      request->addr_type = ndmca_preferred_v4_addr_type(conn);
    }
    rc = NDMC_CALL(conn);
    if (rc) return rc;

    if (request->addr_type != reply->connect_addr.addr_type) {
      ndmalogf(sess, 0, 0, "DATA_LISTEN addr_type mismatch");
      return -1;
    }

    if (ndmp_4to9_addr(&reply->connect_addr, &ca->data_addr) != 0) {
      ndmalogf(sess, 0, 0, "DATA_LISTEN address conversion failed");
      return -1;
    }

    if (ndmca_store_v4_addr_env(&reply->connect_addr, &ca->data_addr_env,
                                &ca->data_addr_env_len) != 0) {
      ndmalogf(sess, 0, 0, "DATA_LISTEN addr_env copy failed");
      return -1;
    }
    NDMC_ENDWITH

    return 0;
  }

  ndmca_clear_addr_env(&ca->data_addr_env, &ca->data_addr_env_len);

  NDMC_WITH(ndmp9_data_listen, NDMP9VER)
  if (sess->plumb.tape == sess->plumb.data) {
    request->addr_type = NDMP9_ADDR_LOCAL;
  } else {
    request->addr_type = ndmca_preferred_tcp_addr_type(conn);
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
    char host_buf[1024];
    char* host = NULL;
    const char* port = NULL;
    struct sockaddr_storage ss;

    if (ndmca_parse_tape_tcp_endpoint(ca->job.tape_tcp, host_buf,
                                      sizeof(host_buf), &host, &port)
        != 0) {
      return 1;
    }
    rc = ndmhost_lookup(host, &ss);
    if (rc != 0) { return rc; }
    if (ndm_sockaddr_set_port(&ss, atoi(port)) != 0) { return -1; }
    if (ndm_sockaddr_to_ndmp9_addr((struct sockaddr*)&ss, &addr) != 0) {
      return -1;
    }
  } else {
    addr = ca->mover_addr;
  }

  NDMC_WITH(ndmp9_data_connect, NDMP9VER)
  request->addr = addr;
  rc = NDMC_CALL(conn);
  NDMC_ENDWITH

  return rc;
}

int ndmca_data_conn_prepare(struct ndm_session* sess)
{
  struct ndmconn* conn = sess->plumb.data;
  struct ndm_control_agent* ca = sess->control_acb;
  unsigned n_env;
  ndmp9_pval* env;
  ndmp4_pval* env4 = NULL;
  int rc;

  if (!ca->job.use_cab_extensions) { return 0; }

  if (!conn->cab_extensions_enabled) {
    ndmalogf(sess, 0, 0, "CAB extension not negotiated on data connection");
    return -1;
  }

  env = ndma_enumerate_env_list(&ca->job.env_tab);
  if (!env) {
    ndmalogf(sess, 0, 0, "Failed allocating enumerate buffer");
    return -1;
  }
  n_env = ca->job.env_tab.n_env;

  if (ndmp_9to4_pval_vec_dup(env, &env4, n_env) != 0) {
    ndmalogf(sess, 0, 0, "Failed converting backup environment for CAB");
    return -1;
  }

  {
    struct ndmp_xa_buf* xa = &conn->call_xa_buf;
    ndmp4_data_start_backup_request* request;

    request = &xa->request.body.ndmp4_cab_data_conn_prepare_request_body;
    NDMOS_MACRO_ZEROFILL(xa);
    xa->request.protocol_version = NDMP4VER;
    xa->request.header.message = NDMP4_CAB_DATA_CONN_PREPARE;

    request->butype_name = ca->job.bu_type;
    request->env.env_len = n_env;
    request->env.env_val = env4;

    rc = NDMC_CALL(conn);
    if (rc == NDMCONN_CALL_STATUS_OK) { NDMC_FREE_REPLY(); }
  }

  ndmp_9to4_pval_vec_free(env4, n_env);

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

  if (conn->protocol_version == NDMP4VER && conn->cab_extensions_enabled) {
    NDMC_WITH(ndmp4_mover_listen, NDMP4VER)
    request->mode = ca->mover_mode;

    if (sess->plumb.tape == sess->plumb.data) {
      request->addr_type = NDMP4_ADDR_LOCAL;
    } else {
      request->addr_type = ndmca_preferred_v4_addr_type(conn);
    }
    rc = NDMC_CALL(conn);
    if (rc) return rc;

    if (request->addr_type != reply->connect_addr.addr_type) {
      ndmalogf(sess, 0, 0, "MOVER_LISTEN addr_type mismatch");
      return -1;
    }

    if (ndmp_4to9_addr(&reply->connect_addr, &ca->mover_addr) != 0) {
      ndmalogf(sess, 0, 0, "MOVER_LISTEN address conversion failed");
      return -1;
    }

    if (ndmca_store_v4_addr_env(&reply->connect_addr, &ca->mover_addr_env,
                                &ca->mover_addr_env_len) != 0) {
      ndmalogf(sess, 0, 0, "MOVER_LISTEN addr_env copy failed");
      return -1;
    }
    NDMC_ENDWITH

    return 0;
  }

  ndmca_clear_addr_env(&ca->mover_addr_env, &ca->mover_addr_env_len);

  NDMC_WITH(ndmp9_mover_listen, NDMP9VER)
  request->mode = ca->mover_mode;

  if (sess->plumb.tape == sess->plumb.data) {
    request->addr_type = NDMP9_ADDR_LOCAL;
  } else {
    request->addr_type = ndmca_preferred_tcp_addr_type(conn);
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
