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
 *      Think of ndma_dispatch_request() as a parser (like yacc(1) input).
 *      This parses (audits) the sequence of requests. If the requests
 *      conform to the "grammar", semantic actions are taken.
 *
 *      This is, admittedly, a huge source file. The idea
 *      is to have all audits and associated errors here. This
 *      makes review, study, comparing to the specification, and
 *      discussion easier because we don't get balled up in
 *      the implementation of semantics. Further, with the hope
 *      of wide-scale deployment, revisions of this source file
 *      can readily be integrated into derivative works without
 *      disturbing other portions.
 */


#include "ndmagents.h"


extern struct ndm_dispatch_version_table ndma_dispatch_version_table[];

static int connect_open_common(struct ndm_session* sess,
                               struct ndmp_xa_buf* xa,
                               struct ndmconn* ref_conn,
                               int protocol_version);


int ndma_dispatch_request(struct ndm_session* sess,
                          struct ndmp_xa_buf* arg_xa,
                          struct ndmconn* ref_conn)
{
  struct ndm_dispatch_request_table* drt;
  struct ndmp_xa_buf* xa = arg_xa;
  struct ndmp_xa_buf xl_xa;
  struct reqrep_xlate* rrxl = 0;
  unsigned protocol_version = ref_conn->protocol_version;
  unsigned msg = xa->request.header.message;
  int rc;

  NDMOS_MACRO_ZEROFILL(&xa->reply);

  xa->reply.protocol_version = xa->request.protocol_version;
  xa->reply.flags |= NDMNMB_FLAG_NO_FREE;

  xa->reply.header.sequence = 0;   /* filled-in by xmit logic */
  xa->reply.header.time_stamp = 0; /* filled-in by xmit logic */
  xa->reply.header.message_type = NDMP0_MESSAGE_REPLY;
  xa->reply.header.message = xa->request.header.message;
  xa->reply.header.reply_sequence = xa->request.header.sequence;
  xa->reply.header.error = NDMP0_NO_ERR;

  /* assume no error */
  ndmnmb_set_reply_error_raw(&xa->reply, NDMP0_NO_ERR);

  switch ((int)msg & 0xFFFFFF00) {
    case 0x0500: /* notify */
    case 0x0600: /* log */
    case 0x0700: /* file history */
      xa->reply.flags |= NDMNMB_FLAG_NO_SEND;
      break;
  }

  /* sanity check */
  if (xa->request.protocol_version != protocol_version) {
    xa->reply.header.error = NDMP0_UNDEFINED_ERR;
    return 0;
  }

  /*
   * If the session is not open and the message
   * is anything other than CONNECT_OPEN, the client
   * has implicitly agreed to the protocol_version
   * offered by NOTIFY_CONNECTED (ref ndmconn_accept()).
   * Effectively perform CONNECT_OPEN for that
   * protocol_version.
   */
  if (!sess->conn_open && msg != NDMP0_CONNECT_OPEN) {
    connect_open_common(sess, xa, ref_conn, ref_conn->protocol_version);
  }

  /*
   * Give the OS/implementation specific module a chance
   * to intercept the request. Some requests are only implemented
   * by the module. Some requests are reimplemented by the module
   * when the standard implementation is inadequate to the app.
   */
  rc = ndmos_dispatch_request(sess, xa, ref_conn);
  if (rc >= 0) { return rc; /* request intercepted */ }

  /*
   * See if there is a standard, protocol_version specific
   * dispatch function for the request.
   */
  drt = ndma_drt_lookup(ndma_dispatch_version_table, protocol_version, msg);
  if (drt) { goto have_drt; }

  /*
   * Typical case....
   * Find the protocol_version specific translation
   * functions for this request/reply. The request
   * is translated from its native version, NDMPvX,
   * into NDMPv9. The NDMPv9 form is dispatched.
   * The resulting reply is translated back to
   * the native version
   */

  rrxl = reqrep_xlate_lookup_version(reqrep_xlate_version_table,
                                     protocol_version);

  /* find the protocol_version translation table */
  if (!rrxl) {
    /* can't do it */
    xa->reply.header.error = NDMP0_NOT_SUPPORTED_ERR;
    return 0;
  }

  /* find the interface's translation table entry */
  rrxl = ndmp_reqrep_by_vx(rrxl, msg);
  if (!rrxl) {
    /* can't do it */
    xa->reply.header.error = NDMP0_NOT_SUPPORTED_ERR;
    return 0;
  }

  /* find the NDMPv9 dispatch table entry */
  drt = ndma_drt_lookup(ndma_dispatch_version_table, NDMP9VER,
                        rrxl->v9_message);

  if (!drt) {
    /* can't do it */
    xa->reply.header.error = NDMP0_NOT_SUPPORTED_ERR;
    return 0;
  }

have_drt:
  // Permission checks, always.

  if (!sess->conn_open && !(drt->flags & NDM_DRT_FLAG_OK_NOT_CONNECTED)) {
    xa->reply.header.error = NDMP0_PERMISSION_ERR;
    return 0;
  }

  if (!sess->conn_authorized
      && !(drt->flags & NDM_DRT_FLAG_OK_NOT_AUTHORIZED)) {
    xa->reply.header.error = NDMP0_NOT_AUTHORIZED_ERR;
    return 0;
  }

  // If there is a translation afoot, translate the request now.

  if (rrxl) {
    NDMOS_MACRO_ZEROFILL(&xl_xa);
    xa = &xl_xa;

    xa->request.header = arg_xa->request.header;
    xa->request.header.message = rrxl->v9_message;
    xa->request.protocol_version = NDMP9VER;

    xa->reply.header = arg_xa->reply.header;
    xa->reply.flags = arg_xa->reply.flags;
    xa->reply.protocol_version = NDMP9VER;

    rc = (*rrxl->request_xto9)((void*)&arg_xa->request.body,
                               (void*)&xa->request.body);

    if (rc < 0) {
      /* unrecoverable translation error */
      xa = arg_xa;
      xa->reply.header.error = NDMP0_UNDEFINED_ERR;
      return 0;
    }
    /* NB: rc>0 means that there were tolerated xlate errors */

    /* allow reply to be freed */
    xa->reply.flags &= ~NDMNMB_FLAG_NO_FREE;
  }

  rc = (*drt->dispatch_request)(sess, xa, ref_conn);

  /* free up any memory allocated as part of the xto9 request */
  if (rrxl) (*rrxl->free_request_xto9)((void*)&xa->request.body);

  if (rc < 0) {
    /* unrecoverable dispatch error */
    if (rrxl) {
      ndmnmb_free(&xa->reply); /* clean up partials */
      xa = arg_xa;
    }
    xa->reply.header.error = NDMP0_NOT_SUPPORTED_ERR;
    return 0;
  }

  if (rrxl) {
    rc = (*rrxl->reply_9tox)((void*)&xa->reply.body,
                             (void*)&arg_xa->reply.body);

    /* free up any memory allocated as part of the 9tox reply */
    if (rrxl) (*rrxl->free_reply_9tox)((void*)&arg_xa->reply.body);

    ndmnmb_free(&xa->reply); /* clean up */

    xa = arg_xa;

    if (rc < 0) {
      /* unrecoverable translation error */
      xa->reply.header.error = NDMP0_UNDEFINED_ERR;
      return 0;
    }
    /* NB: rc>0 means that there were tolerated xlate errors */
  }
  return 0;
}

int ndma_dispatch_raise_error(struct ndm_session* sess,
                              struct ndmp_xa_buf* xa,
                              struct ndmconn* ref_conn,
                              ndmp9_error error,
                              char* errstr)
{
  int protocol_version = ref_conn->protocol_version;
  ndmp0_message msg = xa->request.header.message;

  if (errstr) {
    ndmalogf(sess, 0, 2, "op=%s err=%s why=%s",
             ndmp_message_to_str(protocol_version, msg),
             ndmp9_error_to_str(error), errstr);
  }
  sess->error_raised = 1;

  ndmnmb_set_reply_error(&xa->reply, error);

  return 1;
}


/*
 * Access paths to ndma_dispatch_request()
 ****************************************************************
 */

/* incomming requests on a ndmconn connection */
int ndma_dispatch_conn(struct ndm_session* sess, struct ndmconn* conn)
{
  struct ndmp_xa_buf xa;
  int rc;

  NDMOS_MACRO_ZEROFILL(&xa);

  rc = ndmconn_recv_nmb(conn, &xa.request);
  if (rc) {
    ndmnmb_free(&xa.request);
    return rc;
  }

  ndma_dispatch_request(sess, &xa, conn);
  ndmnmb_free(&xa.request);

  if (!(xa.reply.flags & NDMNMB_FLAG_NO_SEND)) {
    rc = ndmconn_send_nmb(conn, &xa.reply);
    if (rc) return rc;
  }

  ndmnmb_free(&xa.reply);

  return 0;
}

void ndma_dispatch_ctrl_unexpected(struct ndmconn* conn,
                                   struct ndmp_msg_buf* nmb)
{
  int protocol_version = conn->protocol_version;
  struct ndm_session* sess = conn->context;
  struct ndmp_xa_buf xa;

  if (nmb->header.message_type != NDMP0_MESSAGE_REQUEST) {
    ndmalogf(sess, conn->chan.name, 1,
             "Unexpected message -- probably reply "
             "w/ wrong reply_sequence");
    ndmnmb_free(nmb);
    return;
  }

  NDMOS_MACRO_ZEROFILL(&xa);
  xa.request = *nmb;

  ndmalogf(sess, conn->chan.name, 4, "Async request %s",
           ndmp_message_to_str(protocol_version, xa.request.header.message));

  ndma_dispatch_request(sess, &xa, conn);

  if (!(xa.reply.flags & NDMNMB_FLAG_NO_SEND)) {
    ndmconn_send_nmb(conn, &xa.reply);
  }

  ndmnmb_free(&xa.reply);
}


int ndma_call_no_tattle(struct ndmconn* conn, struct ndmp_xa_buf* arg_xa)
{
  struct ndmp_xa_buf* xa = arg_xa;
  struct ndmp_xa_buf xl_xa;
  struct reqrep_xlate* rrxl = 0;
  unsigned protocol_version = conn->protocol_version;
  unsigned msg = xa->request.header.message;
  int rc;

  if (xa->request.protocol_version == NDMP9VER) {
    /*
     * Typical case....
     * Find the protocol_version specific translation
     * functions for this request/reply. The request
     * is translated to its native version, NDMPvX,
     * from NDMPv9. The NDMPvX form is transmitted.
     * The resulting reply is translated back to NDMPv9.
     * NDMPvX is determined by the connection.
     */

    rrxl = reqrep_xlate_lookup_version(reqrep_xlate_version_table,
                                       protocol_version);

    /* find the protocol_version translation table */
    if (!rrxl) {
      /* can't do it */
      xa->reply.header.error = NDMP0_NOT_SUPPORTED_ERR;
      rc = NDMCONN_CALL_STATUS_HDR_ERROR;
      conn->last_header_error = xa->reply.header.error;
      return rc;
    }

    /* find the interface's translation table entry */
    rrxl = ndmp_reqrep_by_v9(rrxl, msg);
    if (!rrxl) {
      /* can't do it */
      xa->reply.header.error = NDMP0_NOT_SUPPORTED_ERR;
      rc = NDMCONN_CALL_STATUS_HDR_ERROR;
      conn->last_header_error = xa->reply.header.error;
      return rc;
    }

    NDMOS_MACRO_ZEROFILL(&xl_xa);
    xa = &xl_xa;

    xa->request.header = arg_xa->request.header;
    xa->request.header.message = rrxl->vx_message;
    xa->request.protocol_version = protocol_version;

    rc = (*rrxl->request_9tox)((void*)&arg_xa->request.body,
                               (void*)&xa->request.body);

    if (rc < 0) {
      /* unrecoverable translation error */
      ndmnmb_free(&xa->request); /* clean up partials */
      xa = arg_xa;
      xa->reply.header.error = NDMP0_NOT_SUPPORTED_ERR;
      rc = NDMCONN_CALL_STATUS_HDR_ERROR;
      conn->last_header_error = xa->reply.header.error;
      return rc;
    }
    /* NB: rc>0 means that there were tolerated xlate errors */
  }

  if (conn->conn_type == NDMCONN_TYPE_RESIDENT) {
    struct ndm_session* sess = conn->context;

    conn->last_message = xa->request.header.message;
    conn->last_call_status = NDMCONN_CALL_STATUS_BOTCH;
    conn->last_header_error = -1; /* invalid */
    conn->last_reply_error = -1;  /* invalid */

    xa->request.header.sequence = conn->next_sequence++;

    ndmconn_snoop_nmb(conn, &xa->request, "Send");

    rc = ndma_dispatch_request(sess, xa, conn);

    xa->reply.header.sequence = conn->next_sequence++;

    if (!(xa->reply.flags & NDMNMB_FLAG_NO_SEND))
      ndmconn_snoop_nmb(conn, &xa->reply, "Recv");
    if (rc) {
      /* keep it */
    } else if (xa->reply.header.error != NDMP0_NO_ERR) {
      rc = NDMCONN_CALL_STATUS_HDR_ERROR;
      conn->last_header_error = xa->reply.header.error;
    } else {
      conn->last_header_error = NDMP9_NO_ERR;
      conn->last_reply_error = ndmnmb_get_reply_error(&xa->reply);

      if (conn->last_reply_error == NDMP9_NO_ERR) {
        rc = NDMCONN_CALL_STATUS_OK;
      } else {
        rc = NDMCONN_CALL_STATUS_REPLY_ERROR;
      }
    }
  } else {
    rc = ndmconn_call(conn, xa);
    if (rc == 0) {
      if ((conn->time_limit > 0) && (conn->received_time > conn->sent_time)) {
        int delta;

        delta = conn->received_time - conn->sent_time;
        if (delta > conn->time_limit) rc = NDMCONN_CALL_STATUS_REPLY_LATE;
      }
    }
  }


  if (rrxl) {
    int xrc;

    xrc = (*rrxl->reply_xto9)((void*)&xa->reply.body,
                              (void*)&arg_xa->reply.body);

    ndmnmb_free(&xa->request); /* clean up */
    ndmnmb_free(&xa->reply);   /* clean up */

    arg_xa->reply.header = xa->reply.header;
    arg_xa->reply.flags = xa->reply.flags;
    arg_xa->reply.protocol_version = NDMP9VER;

    xa = arg_xa;

    if (xrc < 0) {
      /* unrecoverable translation error */
      xa->reply.header.error = NDMP0_UNDEFINED_ERR;
      rc = NDMCONN_CALL_STATUS_HDR_ERROR;
      conn->last_header_error = xa->reply.header.error;
      return rc;
    }
    /* NB: rc>0 means that there were tolerated xlate errors */
  }

  return rc;
}

int ndma_call(struct ndmconn* conn, struct ndmp_xa_buf* xa)
{
  int rc;

  rc = ndma_call_no_tattle(conn, xa);

  if (rc) { ndma_tattle(conn, xa, rc); }
  return rc;
}

int ndma_send_to_control(struct ndm_session* sess,
                         struct ndmp_xa_buf* xa,
                         struct ndmconn* from_conn)
{
  struct ndmconn* conn = sess->plumb.control;
  int rc;

  if (conn->conn_type == NDMCONN_TYPE_RESIDENT && from_conn) {
    /*
     * Control and sending agent are
     * resident. Substitute the sending
     * agents "connection" so that logs
     * look right and right protocol_version
     * is used.
     */
    conn = from_conn;
  }

  rc = ndma_call_no_tattle(conn, xa);

  if (rc) { ndma_tattle(conn, xa, rc); }
  return rc;
}

int ndma_tattle(struct ndmconn* conn, struct ndmp_xa_buf* xa, int rc)
{
  struct ndm_session* sess = conn->context;
  int protocol_version = conn->protocol_version;
  unsigned msg = xa->request.header.message;
  char* tag = conn->chan.name;
  char* msgname = ndmp_message_to_str(protocol_version, msg);
  unsigned err;

  switch (rc) {
    case 0:
      ndmalogf(sess, tag, 2, " ?OK %s", msgname);
      break;

    case 1: /* no error in header, error in reply */
      err = ndmnmb_get_reply_error_raw(&xa->reply);
      ndmalogf(sess, tag, 2, " ERR %s  %s", msgname,
               ndmp_error_to_str(protocol_version, err));
      break;

    case 2: /* no error in header or in reply, response late */
      ndmalogf(sess, tag, 2, " REPLY LATE %s, took %d seconds", msgname,
               (conn->received_time - conn->sent_time));
      break;

    case -2: /* error in header, no reply body */
      err = xa->reply.header.error;
      ndmalogf(sess, tag, 2, " ERR-AGENT %s  %s", msgname,
               ndmp_error_to_str(protocol_version, err));
      break;

    default:
      ndmalogf(sess, tag, 2, " ERR-CONN %s  %s", msgname,
               ndmconn_get_err_msg(conn));
      break;
  }

  return 0;
}


/*
 * NDMPx_CONNECT Interfaces
 ****************************************************************
 */


// NDMP[0234]_CONNECT_OPEN
int ndmp_sxa_connect_open(struct ndm_session* sess,
                          struct ndmp_xa_buf* xa,
                          struct ndmconn* ref_conn)
{
  NDMS_WITH(ndmp0_connect_open)
  if (sess->conn_open) {
    if (request->protocol_version != ref_conn->protocol_version) {
      NDMADR_RAISE_ILLEGAL_ARGS("too late to change version");
    }
  } else {
    switch (request->protocol_version) {
#ifndef NDMOS_OPTION_NO_NDMP2
      case NDMP2VER:
#endif /* !NDMOS_OPTION_NO_NDMP2 */
#ifndef NDMOS_OPTION_NO_NDMP3
      case NDMP3VER:
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
      case NDMP4VER:
#endif /* !NDMOS_OPTION_NO_NDMP4 */
        connect_open_common(sess, xa, ref_conn, request->protocol_version);
        break;

      default:
        NDMADR_RAISE_ILLEGAL_ARGS("unsupport protocol version");
        break;
    }
  }
  NDMS_ENDWITH

  return 0;
}

static int connect_open_common(struct ndm_session* sess,
                               [[maybe_unused]] struct ndmp_xa_buf* xa,
                               struct ndmconn* ref_conn,
                               int protocol_version)
{
#ifndef NDMOS_OPTION_NO_DATA_AGENT
  if (sess->data_acb) sess->data_acb->protocol_version = protocol_version;
#endif /* !NDMOS_OPTION_NO_DATA_AGENT */
#ifndef NDMOS_OPTION_NO_TAPE_AGENT
  if (sess->tape_acb) sess->tape_acb->protocol_version = protocol_version;
#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
#ifndef NDMOS_OPTION_NO_ROBOT_AGENT
  if (sess->robot_acb) sess->robot_acb->protocol_version = protocol_version;
#endif /* !NDMOS_OPTION_NO_ROBOT_AGENT */

  ref_conn->protocol_version = protocol_version;
  sess->conn_open = 1;

  return 0;
}


// NDMP[234]_CONNECT_CLIENT_AUTH
int ndmp_sxa_connect_client_auth(struct ndm_session* sess,
                                 struct ndmp_xa_buf* xa,
                                 struct ndmconn* ref_conn)
{
  ndmp9_auth_type auth_type;
  char* name = 0;
  char* proof = 0;

  NDMS_WITH(ndmp9_connect_client_auth)

  auth_type = request->auth_data.auth_type;
  switch (auth_type) {
    default:
      NDMADR_RAISE_ILLEGAL_ARGS("auth_type");

    case NDMP9_AUTH_TEXT: {
      ndmp9_auth_text* p;

      p = &request->auth_data.ndmp9_auth_data_u.auth_text;
      name = p->auth_id;
      proof = p->auth_password;
      if (!ndmos_ok_name_password(sess, name, proof)) {
        NDMADR_RAISE(NDMP9_NOT_AUTHORIZED_ERR, "password not OK");
      }
    } break;

    case NDMP9_AUTH_MD5: {
      ndmp9_auth_md5* p;

      p = &request->auth_data.ndmp9_auth_data_u.auth_md5;
      name = p->auth_id;
      proof = p->auth_digest;

      if (!sess->md5_challenge_valid) {
        NDMADR_RAISE(NDMP9_NOT_AUTHORIZED_ERR, "no challenge");
      }

      if (!ndmos_ok_name_md5_digest(sess, name, proof)) {
        NDMADR_RAISE(NDMP9_NOT_AUTHORIZED_ERR, "digest not OK");
      }
    } break;
  }
  sess->conn_authorized = 1;

  return 0;
  NDMS_ENDWITH
}


// NDMP[023]_CONNECT_CLOSE
int ndmp_sxa_connect_close(struct ndm_session* sess,
                           struct ndmp_xa_buf* xa,
                           struct ndmconn* ref_conn)
{
  xa->reply.flags |= NDMNMB_FLAG_NO_SEND; /* ??? */

  /* TODO: shutdown everything */
  sess->connect_status = 0;
  ndmchan_set_eof(&ref_conn->chan);

  return 0;
}


// NDMP[23]_CONNECT_SERVER_AUTH
int ndmp_sxa_connect_server_auth([[maybe_unused]] struct ndm_session* sess,
                                 [[maybe_unused]] struct ndmp_xa_buf* xa,
                                 [[maybe_unused]] struct ndmconn* ref_conn)
{
  return NDMADR_UNIMPLEMENTED_MESSAGE;
}


/*
 * NDMPx_CONFIG Interfaces
 ****************************************************************
 */


/*
 * NDMP[234]_CONFIG_GET_HOST_INFO
 * NDMP[34]_CONFIG_GET_SERVER_INFO
 * NDMP2_CONFIG_GET_MOVER_TYPE
 * NDMP[34]_CONFIG_GET_CONNECTION_TYPE
 * NDMP[34]_CONFIG_GET_BUTYPE_INFO
 * NDMP[34]_CONFIG_GET_FS_INFO
 * NDMP[34]_CONFIG_GET_TAPE_INFO
 * NDMP[34]_CONFIG_GET_SCSI_INFO
 */
int ndmp_sxa_config_get_info(struct ndm_session* sess,
                             struct ndmp_xa_buf* xa,
                             [[maybe_unused]] struct ndmconn* ref_conn)
{
  NDMS_WITH_VOID_REQUEST(ndmp9_config_get_info)
  ndmos_sync_config_info(sess);
  if (!sess->config_info) { return NDMP9_NO_MEM_ERR; }

  if (sess->config_info->conntypes == 0) {
    /* OS left it for us to do */
#ifndef NDMOS_OPTION_NO_DATA_AGENT
#  ifndef NDMOS_OPTION_NO_TAPE_AGENT
    sess->config_info->conntypes |= NDMP9_CONFIG_CONNTYPE_LOCAL;
    sess->config_info->conntypes |= NDMP9_CONFIG_CONNTYPE_TCP;
#  else  /* !NDMOS_OPTION_NO_TAPE_AGENT */
    sess->config_info->conntypes |= NDMP9_CONFIG_CONNTYPE_TCP;
#  endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
#else    /* !NDMOS_OPTION_NO_DATA_AGENT */
#  ifndef NDMOS_OPTION_NO_TAPE_AGENT
    sess->config_info->conntypes |= NDMP9_CONFIG_CONNTYPE_TCP;
#  else  /* !NDMOS_OPTION_NO_TAPE_AGENT */
#  endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
#endif   /* !NDMOS_OPTION_NO_DATA_AGENT */
  }

  if (sess->config_info->authtypes == 0) {
    /* OS left it for us to do */
    sess->config_info->authtypes |= NDMP9_CONFIG_AUTHTYPE_TEXT;
    sess->config_info->authtypes |= NDMP9_CONFIG_AUTHTYPE_MD5;
  }

  memcpy(&reply->config_info, sess->config_info, sizeof(ndmp9_config_info));

  return 0;
  NDMS_ENDWITH
}


#ifndef NDMOS_OPTION_NO_NDMP2
// NDMP2_CONFIG_GET_BUTYPE_ATTR
int ndmp2_sxa_config_get_butype_attr(struct ndm_session* sess,
                                     struct ndmp_xa_buf* xa,
                                     struct ndmconn* ref_conn)
{
  ndmp9_config_info* ci = sess->config_info;
  ndmp9_butype_info* bu = 0;
  unsigned int i;

  assert(xa->request.protocol_version == NDMP2VER);

  NDMS_WITH(ndmp2_config_get_butype_attr)
  ndmos_sync_config_info(sess);
  if (!sess->config_info) { return NDMP9_NO_MEM_ERR; }

  for (i = 0; i < ci->butype_info.butype_info_len; i++) {
    bu = &ci->butype_info.butype_info_val[i];

    if (strcmp(request->name, bu->butype_name) == 0) { break; }
  }

  if (i >= ci->butype_info.butype_info_len) {
    NDMADR_RAISE_ILLEGAL_ARGS("butype");
  }

  reply->attrs = bu->v2attr.value;

  return 0;
  NDMS_ENDWITH
}
#endif /* !NDMOS_OPTION_NO_NDMP2 */


/*
 * NDMP[234]_CONFIG_GET_AUTH_ATTR
 *
 * Credits to Rajiv of NetApp for helping with MD5 stuff.
 */
int ndmp_sxa_config_get_auth_attr(struct ndm_session* sess,
                                  struct ndmp_xa_buf* xa,
                                  struct ndmconn* ref_conn)
{
  NDMS_WITH(ndmp9_config_get_auth_attr)
  switch (request->auth_type) {
    default:
      NDMADR_RAISE_ILLEGAL_ARGS("auth_type");

    case NDMP9_AUTH_NONE:
      break;

    case NDMP9_AUTH_TEXT:
      break;

    case NDMP9_AUTH_MD5:
      ndmos_get_md5_challenge(sess);
      NDMOS_API_BCOPY(sess->md5_challenge,
                      reply->server_attr.ndmp9_auth_attr_u.challenge, 64);
      break;
  }
  reply->server_attr.auth_type = request->auth_type;

  return 0;
  NDMS_ENDWITH
}


#ifndef NDMOS_OPTION_NO_ROBOT_AGENT /* Surrounds SCSI intfs */
/*
 * NDMPx_SCSI Interfaces
 ****************************************************************
 *
 * If these are implemented, they should already have been
 * intercepted by ndmos_dispatch_request(). There is absolutely
 * no way to implement this generically, nor is there merit to
 * a generic "layer".
 *
 * Still, just in case, they are implemented here.
 */

static ndmp9_error scsi_open_ok(struct ndm_session* sess);
static ndmp9_error scsi_op_ok(struct ndm_session* sess);


// NDMP[234]_SCSI_OPEN
int ndmp_sxa_scsi_open(struct ndm_session* sess,
                       struct ndmp_xa_buf* xa,
                       struct ndmconn* ref_conn)
{
  ndmp9_error error;

  if (!sess->robot_acb) {
    NDMADR_RAISE(NDMP9_DEVICE_OPENED_ERR, "No Robot Agent");
  }

  NDMS_WITH(ndmp9_scsi_open)
  error = scsi_open_ok(sess);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "!scsi_open_ok"); }

  error = ndmos_scsi_open(sess, request->device);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "scsi_open"); }

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_SCSI_CLOSE
 */
int ndmp_sxa_scsi_close(struct ndm_session* sess,
                        struct ndmp_xa_buf* xa,
                        struct ndmconn* ref_conn)
{
  ndmp9_error error;

  NDMS_WITH_VOID_REQUEST(ndmp9_scsi_close)
  error = scsi_op_ok(sess);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "!scsi_op_ok"); }

  error = ndmos_scsi_close(sess);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "scsi_close"); }

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_SCSI_GET_STATE
 */
int ndmp_sxa_scsi_get_state(struct ndm_session* sess,
                            struct ndmp_xa_buf* xa,
                            [[maybe_unused]] struct ndmconn* ref_conn)
{
  struct ndm_robot_agent* ra = sess->robot_acb;

  NDMS_WITH_VOID_REQUEST(ndmp9_scsi_get_state)
  ndmos_scsi_sync_state(sess);

  *reply = ra->scsi_state;

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[23]_SCSI_SET_TARGET
 */
int ndmp_sxa_scsi_set_target(struct ndm_session* sess,
                             struct ndmp_xa_buf* xa,
                             struct ndmconn* ref_conn)
{
  ndmp9_error error;

  NDMS_WITH(ndmp9_scsi_set_target)
  error = scsi_op_ok(sess);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "!scsi_op_ok"); }

  error = ndmos_scsi_set_target(sess);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "scsi_set_target"); }

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_SCSI_RESET_DEVICE
 */
int ndmp_sxa_scsi_reset_device(struct ndm_session* sess,
                               struct ndmp_xa_buf* xa,
                               struct ndmconn* ref_conn)
{
  ndmp9_error error;

  NDMS_WITH_VOID_REQUEST(ndmp9_scsi_reset_device)
  error = scsi_op_ok(sess);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "!scsi_op_ok"); }

  error = ndmos_scsi_reset_device(sess);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "scsi_reset_device"); }

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[23]_SCSI_RESET_BUS
 */
int ndmp_sxa_scsi_reset_bus(struct ndm_session* sess,
                            struct ndmp_xa_buf* xa,
                            struct ndmconn* ref_conn)
{
  ndmp9_error error;

  NDMS_WITH_VOID_REQUEST(ndmp9_scsi_reset_bus)
  error = scsi_op_ok(sess);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "!scsi_op_ok"); }

  error = ndmos_scsi_reset_bus(sess);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "scsi_reset_bus"); }

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_SCSI_EXECUTE_CDB
 */
int ndmp_sxa_scsi_execute_cdb(struct ndm_session* sess,
                              struct ndmp_xa_buf* xa,
                              struct ndmconn* ref_conn)
{
  ndmp9_error error;

  NDMS_WITH(ndmp9_scsi_execute_cdb)
  error = scsi_op_ok(sess);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "!scsi_op_ok"); }

  error = ndmos_scsi_execute_cdb(sess, request, reply);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "scsi_execute_cdb"); }

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMPx_SCSI helper routines
 */

static ndmp9_error scsi_open_ok(struct ndm_session* sess)
{
  struct ndm_robot_agent* ra = sess->robot_acb;

  ndmos_scsi_sync_state(sess);
  if (ra->scsi_state.error != NDMP9_DEV_NOT_OPEN_ERR)
    return NDMP9_DEVICE_OPENED_ERR;

#  ifndef NDMOS_OPTION_ALLOW_SCSI_AND_TAPE_BOTH_OPEN
#    ifndef NDMOS_OPTION_NO_TAPE_AGENT
  ndmos_tape_sync_state(sess);
  if (sess->tape_acb->tape_state.error != NDMP9_DEV_NOT_OPEN_ERR)
    return NDMP9_DEVICE_OPENED_ERR;
#    endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
#  endif   /* NDMOS_OPTION_ALLOW_SCSI_AND_TAPE_BOTH_OPEN */

  return NDMP9_NO_ERR;
}

static ndmp9_error scsi_op_ok(struct ndm_session* sess)
{
  struct ndm_robot_agent* ra = sess->robot_acb;

  ndmos_scsi_sync_state(sess);
  if (ra->scsi_state.error != NDMP9_NO_ERR) return NDMP9_DEV_NOT_OPEN_ERR;

  return NDMP9_NO_ERR;
}
#endif /* !NDMOS_OPTION_NO_ROBOT_AGENT */ /* Surrounds SCSI intfs */


#ifndef NDMOS_OPTION_NO_TAPE_AGENT /* Surrounds TAPE intfs */
/*
 * NDMPx_TAPE Interfaces
 ****************************************************************
 */
static ndmp9_error tape_open_ok(struct ndm_session* sess, int will_write);
static ndmp9_error tape_op_ok(struct ndm_session* sess, int will_write);


/*
 * NDMP[234]_TAPE_OPEN
 */
int ndmp_sxa_tape_open(struct ndm_session* sess,
                       struct ndmp_xa_buf* xa,
                       struct ndmconn* ref_conn)
{
  ndmp9_error error;
  int will_write;

  if (!sess->tape_acb) {
    NDMADR_RAISE(NDMP9_DEVICE_OPENED_ERR, "No Tape Agent");
  }

  NDMS_WITH(ndmp9_tape_open)
  switch (request->mode) {
    default:
      NDMADR_RAISE_ILLEGAL_ARGS("tape_mode");

    case NDMP9_TAPE_READ_MODE:
      will_write = 0;
      break;

    case NDMP9_TAPE_RDWR_MODE:
    case NDMP9_TAPE_RAW_MODE:
      will_write = 1;
      break;
  }

  error = tape_open_ok(sess, will_write);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "!tape_open_ok"); }

  error = ndmos_tape_open(sess, request->device, will_write);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "tape_open"); }

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_TAPE_CLOSE
 */
int ndmp_sxa_tape_close(struct ndm_session* sess,
                        struct ndmp_xa_buf* xa,
                        struct ndmconn* ref_conn)
{
  ndmp9_error error;

  NDMS_WITH_VOID_REQUEST(ndmp9_tape_close)
  error = tape_op_ok(sess, 0);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "!tape_op_ok"); }

  error = ndmos_tape_close(sess);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "tape_close"); }

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_TAPE_GET_STATE
 */
int ndmp_sxa_tape_get_state(struct ndm_session* sess,
                            struct ndmp_xa_buf* xa,
                            [[maybe_unused]] struct ndmconn* ref_conn)
{
  struct ndm_tape_agent* ta = sess->tape_acb;

  NDMS_WITH_VOID_REQUEST(ndmp9_tape_get_state)

  ndmos_tape_sync_state(sess);

  *reply = ta->tape_state;

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_TAPE_MTIO
 */
int ndmp_sxa_tape_mtio(struct ndm_session* sess,
                       struct ndmp_xa_buf* xa,
                       struct ndmconn* ref_conn)
{
  ndmp9_error error;
  ndmp9_tape_mtio_op tape_op;
  int will_write = 0;
  uint32_t resid = 0;

  NDMS_WITH(ndmp9_tape_mtio)

  switch (request->tape_op) {
    default:
      NDMADR_RAISE_ILLEGAL_ARGS("tape_op");

    case NDMP9_MTIO_EOF:
      will_write = 1;
      tape_op = NDMP9_MTIO_EOF;
      break;

    case NDMP9_MTIO_FSF:
    case NDMP9_MTIO_BSF:
    case NDMP9_MTIO_FSR:
    case NDMP9_MTIO_BSR:
    case NDMP9_MTIO_REW:
    case NDMP9_MTIO_OFF:
      tape_op = request->tape_op;
      break;
  }

  error = tape_op_ok(sess, will_write);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "!tape_op_ok"); }

  error = ndmos_tape_mtio(sess, tape_op, request->count, &resid);

  reply->error = error;
  reply->resid_count = resid;

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_TAPE_WRITE
 */
int ndmp_sxa_tape_write(struct ndm_session* sess,
                        struct ndmp_xa_buf* xa,
                        struct ndmconn* ref_conn)
{
  ndmp9_error error;
  uint32_t done_count = 0;

  NDMS_WITH(ndmp9_tape_write)
  if (request->data_out.data_out_len == 0) {
    /*
     * NDMPv4 clarification -- a tape read or write with
     * a count==0 is a no-op. This is undoubtedly influenced
     * by the SCSI Sequential Access specification which
     * says much the same thing.
     *
     * NDMPv[23] MAY return NDMP_NO_ERR or
     * NDMP_ILLEGAL_ARGS_ERR.
     */
    reply->error = NDMP9_NO_ERR;
    reply->count = 0;

    return 0;
  }

  if (!NDMOS_MACRO_OK_TAPE_REC_LEN(request->data_out.data_out_len)) {
    NDMADR_RAISE_ILLEGAL_ARGS("!ok_tape_rec_len");
  }

  error = tape_op_ok(sess, 1);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "!tape_op_ok"); }

  error = ndmos_tape_write(sess, request->data_out.data_out_val,
                           request->data_out.data_out_len, &done_count);
  reply->error = error;
  reply->count = done_count;

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_TAPE_READ
 */
int ndmp_sxa_tape_read(struct ndm_session* sess,
                       struct ndmp_xa_buf* xa,
                       struct ndmconn* ref_conn)
{
  struct ndm_tape_agent* ta = sess->tape_acb;
  ndmp9_error error;
  uint32_t done_count = 0;

  /*
   * We are about to read data into a tape buffer so make sure
   * we have it available. We delay allocating buffers to the
   * moment we first need them.
   */
  if (!ta->tape_buffer) {
    ta->tape_buffer = NDMOS_API_MALLOC(NDMOS_CONST_TAPE_REC_MAX);
    if (!ta->tape_buffer) {
      NDMADR_RAISE(NDMP9_NO_MEM_ERR, "Allocating tape buffer");
    }
  }

  NDMS_WITH(ndmp9_tape_read)
  if (request->count == 0) {
    /*
     * NDMPv4 clarification -- a tape read or write with
     * a count==0 is a no-op. This is undoubtedly influenced
     * by the SCSI Sequential Access specification which
     * says much the same thing.
     *
     * NDMPv[23] MAY return NDMP_NO_ERR or
     * NDMP_ILLEGAL_ARGS_ERR.
     */
    reply->error = NDMP9_NO_ERR;
    reply->data_in.data_in_val = ta->tape_buffer;
    reply->data_in.data_in_len = 0;

    return 0;
  }

  if (!NDMOS_MACRO_OK_TAPE_REC_LEN(request->count)) {
    NDMADR_RAISE_ILLEGAL_ARGS("!ok_tape_rec_len");
  }

  error = tape_op_ok(sess, 0);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "!tape_op_ok"); }

  error = ndmos_tape_read(sess, ta->tape_buffer, request->count, &done_count);
  reply->error = error;
  reply->data_in.data_in_val = ta->tape_buffer;
  reply->data_in.data_in_len = done_count;

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_TAPE_EXECUTE_CDB
 *
 * If this is implemented, it should already have been
 * intercepted by ndmos_dispatch_request().
 * There is absolutely no way to implement this generically,
 * nor is there merit to a generic "layer".
 * Still, just in case, it is implemented here.
 */
int ndmp_sxa_tape_execute_cdb([[maybe_unused]] struct ndm_session* sess,
                              struct ndmp_xa_buf* xa,
                              [[maybe_unused]] struct ndmconn* ref_conn)
{
  NDMS_WITH(ndmp9_tape_execute_cdb)
  return NDMADR_UNIMPLEMENTED_MESSAGE;
  NDMS_ENDWITH
}


/*
 * NDMPx_TAPE helper routines
 */

static ndmp9_error tape_open_ok(struct ndm_session* sess,
                                [[maybe_unused]] int will_write)
{
  struct ndm_tape_agent* ta = sess->tape_acb;

  ndmos_tape_sync_state(sess);
  if (ta->tape_state.state != NDMP9_TAPE_STATE_IDLE)
    return NDMP9_DEVICE_OPENED_ERR;

#  ifndef NDMOS_OPTION_ALLOW_SCSI_AND_TAPE_BOTH_OPEN
#    ifndef NDMOS_OPTION_NO_ROBOT_AGENT
  ndmos_scsi_sync_state(sess);
  if (sess->robot_acb
      && sess->robot_acb->scsi_state.error != NDMP9_DEV_NOT_OPEN_ERR)
    return NDMP9_DEVICE_OPENED_ERR;
#    endif /* !NDMOS_OPTION_NO_ROBOT_AGENT */
#  endif   /* NDMOS_OPTION_ALLOW_SCSI_AND_TAPE_BOTH_OPEN */

  return NDMP9_NO_ERR;
}

/*
 * Tape operation is only OK if it is open and the MOVER
 * hasn't got a hold of it. We can't allow tape operations
 * to interfere with the MOVER.
 */

static ndmp9_error tape_op_ok(struct ndm_session* sess, int will_write)
{
  struct ndm_tape_agent* ta = sess->tape_acb;

  ndmos_tape_sync_state(sess);
  switch (ta->tape_state.state) {
    case NDMP9_TAPE_STATE_IDLE:
      return NDMP9_DEV_NOT_OPEN_ERR;

    case NDMP9_TAPE_STATE_OPEN:
      if (will_write && !NDMTA_TAPE_IS_WRITABLE(ta))
        return NDMP9_PERMISSION_ERR;
      break;

    case NDMP9_TAPE_STATE_MOVER:
      return NDMP9_ILLEGAL_STATE_ERR;
  }

  return NDMP9_NO_ERR;
}
#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */ /* Surrounds TAPE intfs */


#ifndef NDMOS_OPTION_NO_DATA_AGENT /* Surrounds DATA intfs */
/*
 * NDMPx_DATA Interfaces
 ****************************************************************
 */

static int data_ok_bu_type(struct ndm_session* sess,
                           struct ndmp_xa_buf* xa,
                           struct ndmconn* ref_conn,
                           char* bu_type);

static int data_can_connect_and_start(struct ndm_session* sess,
                                      struct ndmp_xa_buf* xa,
                                      struct ndmconn* ref_conn,
                                      ndmp9_addr* data_addr,
                                      ndmp9_mover_mode mover_mode);

static int data_can_connect(struct ndm_session* sess,
                            struct ndmp_xa_buf* xa,
                            struct ndmconn* ref_conn,
                            ndmp9_addr* data_addr);

static int data_can_start(struct ndm_session* sess,
                          struct ndmp_xa_buf* xa,
                          struct ndmconn* ref_conn,
                          ndmp9_mover_mode mover_mode);

static int data_connect(struct ndm_session* sess,
                        struct ndmp_xa_buf* xa,
                        struct ndmconn* ref_conn,
                        ndmp9_addr* data_addr);

static ndmp9_error data_copy_environment(struct ndm_session* sess,
                                         ndmp9_pval* env,
                                         unsigned n_env);

static ndmp9_error data_copy_nlist(struct ndm_session* sess,
                                   ndmp9_name* nlist,
                                   unsigned n_nlist);


/*
 * NDMP[234]_DATA_GET_STATE
 */
int ndmp_sxa_data_get_state(struct ndm_session* sess,
                            struct ndmp_xa_buf* xa,
                            struct ndmconn* ref_conn)
{
  struct ndm_data_agent* da = sess->data_acb;

  if (!da) { NDMADR_RAISE(NDMP9_CONNECT_ERR, "No Data Agent"); }

  NDMS_WITH_VOID_REQUEST(ndmp9_data_get_state)
  *reply = da->data_state;

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_DATA_START_BACKUP
 */
int ndmp_sxa_data_start_backup(struct ndm_session* sess,
                               struct ndmp_xa_buf* xa,
                               struct ndmconn* ref_conn)
{
  int rc;
  ndmp9_error error;

  if (!sess->data_acb) { NDMADR_RAISE(NDMP9_CONNECT_ERR, "No Data Agent"); }

  NDMS_WITH(ndmp9_data_start_backup)
  rc = data_ok_bu_type(sess, xa, ref_conn, request->bu_type);
  if (rc) { return rc; }

  if (request->addr.addr_type != NDMP9_ADDR_AS_CONNECTED) {
    rc = data_can_connect_and_start(sess, xa, ref_conn, &request->addr,
                                    NDMP9_MOVER_MODE_READ);
  } else {
    rc = data_can_start(sess, xa, ref_conn, NDMP9_MOVER_MODE_READ);
  }
  if (rc) { return rc; /* already tattled */ }

  strncpy(sess->data_acb->bu_type, request->bu_type,
          sizeof(sess->data_acb->bu_type) - 1);
  sess->data_acb->bu_type[sizeof(sess->data_acb->bu_type) - 1] = '\0';

  error
      = data_copy_environment(sess, request->env.env_val, request->env.env_len);
  if (error != NDMP9_NO_ERR) {
    ndmda_belay(sess);
    NDMADR_RAISE(error, "copy-env");
  }

  if (request->addr.addr_type != NDMP9_ADDR_AS_CONNECTED) {
    rc = data_connect(sess, xa, ref_conn, &request->addr);
    if (rc) {
      ndmda_belay(sess);
      return rc; /* already tattled */
    }
  }

  error = ndmda_data_start_backup(sess);
  if (error != NDMP9_NO_ERR) {
    /* TODO: undo everything */
    ndmda_belay(sess);
    NDMADR_RAISE(error, "start_backup");
  }

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_DATA_START_RECOVER
 */
int ndmp_sxa_data_start_recover(struct ndm_session* sess,
                                struct ndmp_xa_buf* xa,
                                struct ndmconn* ref_conn)
{
  ndmp9_error error;
  int rc;

  if (!sess->data_acb) { NDMADR_RAISE(NDMP9_CONNECT_ERR, "No Data Agent"); }

  NDMS_WITH(ndmp9_data_start_recover)
  rc = data_ok_bu_type(sess, xa, ref_conn, request->bu_type);
  if (rc) { return rc; }

  if (request->addr.addr_type != NDMP9_ADDR_AS_CONNECTED) {
    rc = data_can_connect_and_start(sess, xa, ref_conn, &request->addr,
                                    NDMP9_MOVER_MODE_WRITE);
  } else {
    rc = data_can_start(sess, xa, ref_conn, NDMP9_MOVER_MODE_WRITE);
  }
  if (rc) { return rc; /* already tattled */ }

  strncpy(sess->data_acb->bu_type, request->bu_type,
          sizeof(sess->data_acb->bu_type) - 1);
  sess->data_acb->bu_type[sizeof(sess->data_acb->bu_type) - 1] = '\0';

  error
      = data_copy_environment(sess, request->env.env_val, request->env.env_len);
  if (error != NDMP9_NO_ERR) {
    ndmda_belay(sess);
    NDMADR_RAISE(error, "copy-env");
  }

  error = data_copy_nlist(sess, request->nlist.nlist_val,
                          request->nlist.nlist_len);

  if (error != NDMP9_NO_ERR) {
    ndmda_belay(sess);
    NDMADR_RAISE(error, "copy-nlist");
  }

  if (request->addr.addr_type != NDMP9_ADDR_AS_CONNECTED) {
    rc = data_connect(sess, xa, ref_conn, &request->addr);
    if (rc) {
      ndmda_belay(sess);
      return rc; /* already tattled */
    }
  }

  error = ndmda_data_start_recover(sess);
  if (error != NDMP9_NO_ERR) {
    /* TODO: undo everything */
    ndmda_belay(sess);
    NDMADR_RAISE(error, "start_recover");
  }

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_DATA_ABORT
 */
int ndmp_sxa_data_abort(struct ndm_session* sess,
                        struct ndmp_xa_buf* xa,
                        struct ndmconn* ref_conn)
{
  struct ndm_data_agent* da = sess->data_acb;

  if (!sess->data_acb) { return 0; }

  NDMS_WITH_VOID_REQUEST(ndmp9_data_abort)
  if (da->data_state.state != NDMP9_DATA_STATE_ACTIVE)
    NDMADR_RAISE_ILLEGAL_STATE("data_state !ACTIVE");

  ndmda_data_abort(sess);

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_DATA_GET_ENV
 */
int ndmp_sxa_data_get_env(struct ndm_session* sess,
                          struct ndmp_xa_buf* xa,
                          struct ndmconn* ref_conn)
{
  struct ndm_data_agent* da = sess->data_acb;
  ndmp9_pval* env;

  NDMS_WITH_VOID_REQUEST(ndmp9_data_get_env)
  if (da->data_state.state == NDMP9_DATA_STATE_IDLE) {
    NDMADR_RAISE_ILLEGAL_STATE("data_state IDLE");
  }
  if (da->data_state.operation != NDMP9_DATA_OP_BACKUP) {
    NDMADR_RAISE_ILLEGAL_STATE("data_op !BACKUP");
  }

  ndmda_sync_environment(sess);

  ndmalogf(sess, ref_conn->chan.name, 6, "n_env=%d", da->env_tab.n_env);

  env = ndma_enumerate_env_list(&da->env_tab);
  if (!env) { NDMADR_RAISE(NDMP9_NO_MEM_ERR, "Allocating enumerate buffer"); }
  reply->env.env_len = da->env_tab.n_env;
  reply->env.env_val = env;

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_DATA_STOP
 */
int ndmp_sxa_data_stop(struct ndm_session* sess,
                       struct ndmp_xa_buf* xa,
                       struct ndmconn* ref_conn)
{
  struct ndm_data_agent* da = sess->data_acb;

  NDMS_WITH_VOID_REQUEST(ndmp9_data_stop)
  if (da->data_state.state != NDMP9_DATA_STATE_HALTED) {
    NDMADR_RAISE_ILLEGAL_STATE("data_state !HALTED");
  }

  ndmda_data_stop(sess);

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_DATA_START_RECOVER_FILEHIST
 * This is a Traakan extension to NDMPv2 and NDMPv3
 * Adopted for NDMPv4
 */
int ndmp_sxa_data_start_recover_filehist(struct ndm_session* sess,
                                         struct ndmp_xa_buf* xa,
                                         struct ndmconn* ref_conn)
{
  ndmp9_error error;
  int rc;

  if (!sess->data_acb) { NDMADR_RAISE(NDMP9_CONNECT_ERR, "No Data Agent"); }

  NDMS_WITH(ndmp9_data_start_recover)
  rc = data_ok_bu_type(sess, xa, ref_conn, request->bu_type);
  if (rc) { return rc; }

  if (request->addr.addr_type != NDMP9_ADDR_AS_CONNECTED) {
    rc = data_can_connect_and_start(sess, xa, ref_conn, &request->addr,
                                    NDMP9_MOVER_MODE_WRITE);
  } else {
    rc = data_can_start(sess, xa, ref_conn, NDMP9_MOVER_MODE_WRITE);
  }
  if (rc) { return rc; /* already tattled */ }

  strncpy(sess->data_acb->bu_type, request->bu_type,
          sizeof(sess->data_acb->bu_type) - 1);
  sess->data_acb->bu_type[sizeof(sess->data_acb->bu_type) - 1] = '\0';

  error
      = data_copy_environment(sess, request->env.env_val, request->env.env_len);
  if (error != NDMP9_NO_ERR) {
    ndmda_belay(sess);
    NDMADR_RAISE(error, "copy-env");
  }

  error = data_copy_nlist(sess, request->nlist.nlist_val,
                          request->nlist.nlist_len);

  if (error != NDMP9_NO_ERR) {
    ndmda_belay(sess);
    NDMADR_RAISE(error, "copy-nlist");
  }

  if (request->addr.addr_type != NDMP9_ADDR_AS_CONNECTED) {
    rc = data_connect(sess, xa, ref_conn, &request->addr);
    if (rc) {
      ndmda_belay(sess);
      return rc; /* already tattled */
    }
  }

  error = ndmda_data_start_recover_fh(sess);
  if (error != NDMP9_NO_ERR) {
    /* TODO: undo everything */
    ndmda_belay(sess);
    NDMADR_RAISE(error, "start_recover_filehist");
  }

  return 0;
  NDMS_ENDWITH
}


#  ifndef NDMOS_EFFECT_NO_NDMP3_NOR_NDMP4 /* Surrounds NDMPv[34] DATA intfs */
/*
 * NDMP[34]_DATA_CONNECT
 */
int ndmp_sxa_data_connect(struct ndm_session* sess,
                          struct ndmp_xa_buf* xa,
                          struct ndmconn* ref_conn)
{
  if (!sess->data_acb) { NDMADR_RAISE(NDMP9_CONNECT_ERR, "No Data Agent"); }

  NDMS_WITH(ndmp9_data_connect)
  return data_connect(sess, xa, ref_conn, &request->addr);
  NDMS_ENDWITH
}


#    ifdef notyet
static int data_listen_common34(struct ndm_session* sess,
                                struct ndmp_xa_buf* xa,
                                struct ndmconn* ref_conn,
                                ndmp9_addr_type addr_type);

/*
 * NDMP[34]_DATA_LISTEN
 */
int ndmadr_data_listen(struct ndm_session* sess,
                       struct ndmp_xa_buf* xa,
                       struct ndmconn* ref_conn)
{
  struct ndm_data_agent* da = sess->data_acb;
  int rc;

  switch (xa->request.protocol_version) {
    default:
      return NDMADR_UNIMPLEMENTED_VERSION; /* should never happen */

#      ifndef NDMOS_OPTION_NO_NDMP2
    case NDMP2VER:
      /* not part of NDMPv2 */
      return NDMADR_UNSPECIFIED_MESSAGE;
#      endif /* !NDMOS_OPTION_NO_NDMP2 */

#      ifndef NDMOS_OPTION_NO_NDMP3
    case NDMP3VER:
      NDMS_WITH(ndmp3_data_listen)
      ndmp9_addr_type addr_type;

      /* Check args, map along the way */
      switch (request->addr_type) {
        default:
          addr_type = -1;
          break;
        case NDMP3_ADDR_LOCAL:
          addr_type = NDMP9_ADDR_LOCAL;
          break;
        case NDMP3_ADDR_TCP:
          addr_type = NDMP9_ADDR_TCP;
          break;
      }

      rc = data_listen_common34(sess, xa, ref_conn, addr_type);
      if (rc) return rc; /* something went wrong */

      ndmp_9to3_addr(&da->data_state.data_connection_addr,
                     &reply->data_connection_addr);
      /* reply->error already set to NDMPx_NO_ERROR */
      NDMS_ENDWITH
      break;
#      endif /* !NDMOS_OPTION_NO_NDMP3 */

#      ifndef NDMOS_OPTION_NO_NDMP4
    case NDMP4VER:
      NDMS_WITH(ndmp4_data_listen)
      ndmp9_addr_type addr_type;

      /* Check args, map along the way */
      switch (request->addr_type) {
        default:
          addr_type = -1;
          break;
        case NDMP4_ADDR_LOCAL:
          addr_type = NDMP9_ADDR_LOCAL;
          break;
        case NDMP4_ADDR_TCP:
          addr_type = NDMP9_ADDR_TCP;
          break;
      }

      rc = data_listen_common34(sess, xa, ref_conn, addr_type);
      if (rc) return rc; /* something went wrong */

      ndmp_9to4_addr(&da->data_state.data_connection_addr,
                     &reply->data_connection_addr);
      /* reply->error already set to NDMPx_NO_ERROR */
      NDMS_ENDWITH
      break;
#      endif /* !NDMOS_OPTION_NO_NDMP4 */
  }
  return 0;
}

/* this same intf is expected in v4, so _common() now */
static int data_listen_common34(struct ndm_session* sess,
                                struct ndmp_xa_buf* xa,
                                struct ndmconn* ref_conn,
                                ndmp9_addr_type addr_type)
{
  struct ndm_data_agent* da = sess->data_acb;
#      ifndef NDMOS_OPTION_NO_TAPE_AGENT
  struct ndm_tape_agent* ta = sess->tape_acb;
#      endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
  ndmp9_error error;
  char reason[100];

  /* Check args */

  switch (addr_type) {
    default:
      NDMADR_RAISE_ILLEGAL_ARGS("mover_addr_type");
    case NDMP9_ADDR_LOCAL:
#      ifdef NDMOS_OPTION_NO_TAPE_AGENT
      NDMADR_RAISE_ILLEGAL_ARGS("data LOCAL w/o local TAPE agent");
#      endif /* NDMOS_OPTION_NO_TAPE_AGENT */
      break;

    case NDMP9_ADDR_TCP:
      break;
  }

  /* Check states -- this should cover everything */
  if (da->data_state.state != NDMP9_DATA_STATE_IDLE)
    NDMADR_RAISE_ILLEGAL_STATE("data_state !IDLE");
#      ifndef NDMOS_OPTION_NO_TAPE_AGENT
  if (ta->mover_state.state != NDMP9_MOVER_STATE_IDLE)
    NDMADR_RAISE_ILLEGAL_STATE("mover_state !IDLE");
#      endif /* !NDMOS_OPTION_NO_TAPE_AGENT */

  /*
   * Check image stream state -- should already be reflected
   * in the mover and data states. This extra check gives
   * us an extra measure of robustness and sanity
   * check on the implementation.
   */
  error = ndmis_audit_data_listen(sess, addr_type, reason);
  if (error != NDMP9_NO_ERR) NDMADR_RAISE(error, reason);

  error = ndmis_data_listen(sess, addr_type,
                            &da->data_state.data_connection_addr, reason);
  if (error != NDMP9_NO_ERR) NDMADR_RAISE(error, reason);

  error = ndmda_data_listen(sess);
  if (error != NDMP9_NO_ERR) {
    /* TODO: belay ndmis_data_listen() */
    NDMADR_RAISE(error, "!data_listen");
  }

  return 0;
}
#    endif /* notyet */

#  endif /* !NDMOS_EFFECT_NO_NDMP3_NOR_NDMP4  Surrounds NDMPv[34] DATA intfs \
          */


/*
 * NDMPx_DATA helper routines
 */


static int data_ok_bu_type(struct ndm_session* sess,
                           struct ndmp_xa_buf* xa,
                           struct ndmconn* ref_conn,
                           char* bu_type)
{
  ndmp9_config_info* ci;
  ndmp9_butype_info* bu;
  unsigned int i;

  ndmos_sync_config_info(sess);
  if (!sess->config_info) {
    NDMADR_RAISE(NDMP9_NO_MEM_ERR, "Allocating memory for config data");
  }

  ci = sess->config_info;
  for (i = 0; i < ci->butype_info.butype_info_len; i++) {
    bu = &ci->butype_info.butype_info_val[i];

    if (strcmp(bu_type, bu->butype_name) == 0) { return 0; }
  }

  NDMADR_RAISE_ILLEGAL_ARGS("bu_type");
}


/*
 * Data can only start if the mover is ready.
 * Just mode and state checks.
 */

static int data_can_connect_and_start(struct ndm_session* sess,
                                      struct ndmp_xa_buf* xa,
                                      struct ndmconn* ref_conn,
                                      ndmp9_addr* data_addr,
                                      ndmp9_mover_mode mover_mode)
{
  int rc;

  /* Check args */
  switch (mover_mode) {
    default:
      NDMADR_RAISE_ILLEGAL_ARGS("mover_mode");
    case NDMP9_MOVER_MODE_READ:  /* aka BACKUP */
    case NDMP9_MOVER_MODE_WRITE: /* aka RECOVER */
      break;
  }

  rc = data_can_connect(sess, xa, ref_conn, data_addr);
  if (rc) return rc;

#  ifndef NDMOS_OPTION_NO_TAPE_AGENT
  if (data_addr->addr_type == NDMP9_ADDR_LOCAL) {
    struct ndm_tape_agent* ta = sess->tape_acb;
    ndmp9_mover_get_state_reply* ms = &ta->mover_state;

    if (ms->mode != mover_mode)
      NDMADR_RAISE_ILLEGAL_STATE("mover_mode mismatch");
  }
#  endif /* !NDMOS_OPTION_NO_TAPE_AGENT */

  return 0;
}

static int data_can_connect(struct ndm_session* sess,
                            struct ndmp_xa_buf* xa,
                            struct ndmconn* ref_conn,
                            ndmp9_addr* data_addr)
{
  struct ndm_data_agent* da = sess->data_acb;
#  ifndef NDMOS_OPTION_NO_TAPE_AGENT
  struct ndm_tape_agent* ta = sess->tape_acb;
#  endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
  ndmp9_error error;
  char reason[100];

  /* Check args */
  switch (data_addr->addr_type) {
    default:
      NDMADR_RAISE_ILLEGAL_ARGS("addr_type");
    case NDMP9_ADDR_LOCAL:
#  ifdef NDMOS_OPTION_NO_TAPE_AGENT
      NDMADR_RAISE_ILLEGAL_ARGS("mover LOCAL w/o local DATA agent");
#  endif /* NDMOS_OPTION_NO_TAPE_AGENT */
      break;

    case NDMP9_ADDR_TCP:
      break;
  }

  /* Check states -- this should cover everything */
  if (da->data_state.state != NDMP9_DATA_STATE_IDLE)
    NDMADR_RAISE_ILLEGAL_STATE("data_state !IDLE");

#  ifndef NDMOS_OPTION_NO_TAPE_AGENT
  if (data_addr->addr_type == NDMP9_ADDR_LOCAL) {
    ndmp9_mover_get_state_reply* ms = &ta->mover_state;

    if (ms->state != NDMP9_MOVER_STATE_LISTEN)
      NDMADR_RAISE_ILLEGAL_STATE("mover_state !LISTEN");

    if (ms->data_connection_addr.addr_type != NDMP9_ADDR_LOCAL)
      NDMADR_RAISE_ILLEGAL_STATE("mover_addr !LOCAL");

  } else {
    if (ta->mover_state.state != NDMP9_MOVER_STATE_IDLE)
      NDMADR_RAISE_ILLEGAL_STATE("mover_state !IDLE");
  }
#  endif /* !NDMOS_OPTION_NO_TAPE_AGENT */

  /*
   * Check image stream state -- should already be reflected
   * in the mover and data states. This extra check gives
   * us an extra measure of robustness and sanity
   * check on the implementation.
   */
  error = ndmis_audit_data_connect(sess, data_addr->addr_type, reason);
  if (error != NDMP9_NO_ERR) NDMADR_RAISE(error, reason);

  return 0;
}

static int data_can_start(struct ndm_session* sess,
                          struct ndmp_xa_buf* xa,
                          struct ndmconn* ref_conn,
                          ndmp9_mover_mode mover_mode)
{
  struct ndm_data_agent* da = sess->data_acb;
  ndmp9_data_get_state_reply* ds = &da->data_state;
#  ifndef NDMOS_OPTION_NO_TAPE_AGENT
  struct ndm_tape_agent* ta = sess->tape_acb;
#  endif /* !NDMOS_OPTION_NO_TAPE_AGENT */

  /* Check args */
  switch (mover_mode) {
    default:
      NDMADR_RAISE_ILLEGAL_ARGS("mover_mode");
    case NDMP9_MOVER_MODE_READ:  /* aka BACKUP */
    case NDMP9_MOVER_MODE_WRITE: /* aka RECOVER */
      break;
  }

  /* Check states -- this should cover everything */
  if (da->data_state.state != NDMP9_DATA_STATE_CONNECTED)
    NDMADR_RAISE_ILLEGAL_STATE("data_state !CONNECTED");

#  ifndef NDMOS_OPTION_NO_TAPE_AGENT
  if (ds->data_connection_addr.addr_type == NDMP9_ADDR_LOCAL) {
    ndmp9_mover_get_state_reply* ms = &ta->mover_state;

    if (ms->state != NDMP9_MOVER_STATE_ACTIVE)
      NDMADR_RAISE_ILLEGAL_STATE("mover_state !ACTIVE");

    if (ms->data_connection_addr.addr_type != NDMP9_ADDR_LOCAL)
      NDMADR_RAISE_ILLEGAL_STATE("mover_addr !LOCAL");

    if (ms->mode != mover_mode)
      NDMADR_RAISE_ILLEGAL_STATE("mover_mode mismatch");
  } else {
    if (ta->mover_state.state != NDMP9_MOVER_STATE_IDLE)
      NDMADR_RAISE_ILLEGAL_STATE("mover_state !IDLE");
  }
#  endif /* !NDMOS_OPTION_NO_TAPE_AGENT */

  return 0;
}

/*
 * For NDMPv2, called from ndmadr_data_start_{backup,recover,recover_filhist}()
 * For NDMPv[34], called from ndmp_sxa_data_connect()
 */
static int data_connect(struct ndm_session* sess,
                        struct ndmp_xa_buf* xa,
                        struct ndmconn* ref_conn,
                        ndmp9_addr* data_addr)
{
  struct ndm_data_agent* da = sess->data_acb;
  int rc;
  ndmp9_error error;
  char reason[100];

  if (!sess->data_acb) { NDMADR_RAISE(NDMP9_CONNECT_ERR, "No Data Agent"); }

  rc = data_can_connect(sess, xa, ref_conn, data_addr);
  if (rc) return rc;

  /*
   * Audits done, connect already
   */
  error = ndmis_data_connect(sess, data_addr, reason);
  if (error != NDMP9_NO_ERR) NDMADR_RAISE(error, reason);

  da->data_state.data_connection_addr = *data_addr;
  /* alt: da->....data_connection_addr = sess->...peer_addr */

  error = ndmda_data_connect(sess);
  if (error != NDMP9_NO_ERR) {
    /* TODO: belay ndmis_data_connect() */
    NDMADR_RAISE(error, "!data_connect");
  }

  da->data_state.data_connection_addr = *data_addr;

  return 0;
}

static ndmp9_error data_copy_environment(struct ndm_session* sess,
                                         ndmp9_pval* env,
                                         unsigned n_env)
{
  int rc;

  if (n_env > NDM_MAX_ENV) return NDMP9_ILLEGAL_ARGS_ERR;

  rc = ndmda_copy_environment(sess, env, n_env);
  if (rc != 0) return NDMP9_NO_MEM_ERR;

  return NDMP9_NO_ERR;
}

static ndmp9_error data_copy_nlist(struct ndm_session* sess,
                                   ndmp9_name* nlist,
                                   unsigned n_nlist)
{
  int rc;

  if (n_nlist >= NDM_MAX_NLIST) return NDMP9_ILLEGAL_ARGS_ERR;

  rc = ndmda_copy_nlist(sess, nlist, n_nlist);
  if (rc != 0) return NDMP9_NO_MEM_ERR;

  return NDMP9_NO_ERR;
}


#endif /* !NDMOS_OPTION_NO_DATA_AGENT */ /* Surrounds DATA intfs */


#ifndef NDMOS_OPTION_NO_TAPE_AGENT /* Surrounds MOVER intfs */
/*
 * NDMPx_MOVER Interfaces
 ****************************************************************
 */

static ndmp9_error mover_can_proceed(struct ndm_session* sess, int will_write);


/*
 * NDMP[234]_MOVER_GET_STATE
 */
int ndmp_sxa_mover_get_state(struct ndm_session* sess,
                             struct ndmp_xa_buf* xa,
                             [[maybe_unused]] struct ndmconn* ref_conn)
{
  struct ndm_tape_agent* ta = sess->tape_acb;

  NDMS_WITH_VOID_REQUEST(ndmp9_mover_get_state)
  ndmta_mover_sync_state(sess);
  *reply = ta->mover_state;
  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_MOVER_LISTEN
 */
int ndmp_sxa_mover_listen(struct ndm_session* sess,
                          struct ndmp_xa_buf* xa,
                          struct ndmconn* ref_conn)
{
#  ifndef NDMOS_OPTION_NO_DATA_AGENT
  struct ndm_data_agent* da = sess->data_acb;
#  endif /* !NDMOS_OPTION_NO_DATA_AGENT */
  struct ndm_tape_agent* ta = sess->tape_acb;
  ndmp9_error error;
  int will_write;
  char reason[100];

  NDMS_WITH(ndmp9_mover_listen)
  ndmalogf(sess, 0, 6, "mover_listen_common() addr_type=%s mode=%s",
           ndmp9_addr_type_to_str(request->addr_type),
           ndmp9_mover_mode_to_str(request->mode));

  /* Check args */
  switch (request->mode) {
    default:
      NDMADR_RAISE_ILLEGAL_ARGS("mover_mode");

    case NDMP9_MOVER_MODE_READ:
      will_write = 1;
      break;

    case NDMP9_MOVER_MODE_WRITE:
      will_write = 0;
      break;
  }

  switch (request->addr_type) {
    default:
      NDMADR_RAISE_ILLEGAL_ARGS("mover_addr_type");

    case NDMP9_ADDR_LOCAL:
#  ifdef NDMOS_OPTION_NO_DATA_AGENT
      NDMADR_RAISE_ILLEGAL_ARGS("mover LOCAL w/o local DATA agent");
#  endif /* NDMOS_OPTION_NO_DATA_AGENT */
      break;

    case NDMP9_ADDR_TCP:
      break;
  }

  /* Check states -- this should cover everything */
  if (ta->mover_state.state != NDMP9_MOVER_STATE_IDLE) {
    NDMADR_RAISE_ILLEGAL_STATE("mover_state !IDLE");
  }
#  ifndef NDMOS_OPTION_NO_DATA_AGENT
  if (da && da->data_state.state != NDMP9_DATA_STATE_IDLE) {
    NDMADR_RAISE_ILLEGAL_STATE("data_state !IDLE");
  }
#  endif /* !NDMOS_OPTION_NO_DATA_AGENT */

  /* Check that the tape is ready to go */
  error = mover_can_proceed(sess, will_write);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "!mover_can_proceed"); }

  /*
   * Check image stream state -- should already be reflected
   * in the mover and data states. This extra check gives
   * us an extra measure of robustness and sanity
   * check on the implementation.
   */
  error = ndmis_audit_tape_listen(sess, request->addr_type, reason);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, reason); }

  error = ndmis_tape_listen(sess, request->addr_type,
                            &ta->mover_state.data_connection_addr, reason);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, reason); }

  error = ndmta_mover_listen(sess, request->mode);
  if (error != NDMP9_NO_ERR) {
    /* TODO: belay ndmis_tape_listen() */
    NDMADR_RAISE(error, "!mover_listen");
  }

  reply->data_connection_addr = ta->mover_state.data_connection_addr;

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_MOVER_CONTINUE
 */
int ndmp_sxa_mover_continue(struct ndm_session* sess,
                            struct ndmp_xa_buf* xa,
                            struct ndmconn* ref_conn)
{
  struct ndm_tape_agent* ta = sess->tape_acb;
  ndmp9_error error;
  int will_write;

  NDMS_WITH_VOID_REQUEST(ndmp9_mover_continue)
  if (ta->mover_state.state != NDMP9_MOVER_STATE_PAUSED) {
    NDMADR_RAISE_ILLEGAL_STATE("mover_state !PAUSED");
  }

  will_write = ta->mover_state.mode == NDMP9_MOVER_MODE_READ;

  error = mover_can_proceed(sess, will_write);
  if (error != NDMP9_NO_ERR) { NDMADR_RAISE(error, "!mover_can_proceed"); }

  ndmta_mover_continue(sess);

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_MOVER_ABORT
 */
int ndmp_sxa_mover_abort(struct ndm_session* sess,
                         struct ndmp_xa_buf* xa,
                         struct ndmconn* ref_conn)
{
  struct ndm_tape_agent* ta = sess->tape_acb;

  NDMS_WITH_VOID_REQUEST(ndmp9_mover_abort)
  if (ta->mover_state.state != NDMP9_MOVER_STATE_LISTEN
      && ta->mover_state.state != NDMP9_MOVER_STATE_ACTIVE
      && ta->mover_state.state != NDMP9_MOVER_STATE_PAUSED) {
    NDMADR_RAISE_ILLEGAL_STATE("mover_state");
  }

  ndmta_mover_abort(sess);

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_MOVER_STOP
 */
int ndmp_sxa_mover_stop(struct ndm_session* sess,
                        struct ndmp_xa_buf* xa,
                        struct ndmconn* ref_conn)
{
  struct ndm_tape_agent* ta = sess->tape_acb;

  NDMS_WITH_VOID_REQUEST(ndmp9_mover_stop)
  if (ta->mover_state.state != NDMP9_MOVER_STATE_HALTED) {
    NDMADR_RAISE_ILLEGAL_STATE("mover_state !HALTED");
  }

  ndmta_mover_stop(sess);

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_MOVER_SET_WINDOW
 */
int ndmp_sxa_mover_set_window(struct ndm_session* sess,
                              struct ndmp_xa_buf* xa,
                              struct ndmconn* ref_conn)
{
  struct ndm_tape_agent* ta = sess->tape_acb;
  struct ndmp9_mover_get_state_reply* ms = &ta->mover_state;
  uint64_t max_len;
  uint64_t end_win;

  NDMS_WITH(ndmp9_mover_set_window)
  ndmta_mover_sync_state(sess);

  if (ref_conn->protocol_version < NDMP4VER) {
    /*
     * NDMP[23] require the Mover be in LISTEN state.
     * Unclear sequence for MOVER_CONNECT.
     */
    if (ms->state != NDMP9_MOVER_STATE_LISTEN
        && ms->state != NDMP9_MOVER_STATE_PAUSED) {
      NDMADR_RAISE_ILLEGAL_STATE("mover_state !LISTEN/PAUSED");
    }
  } else {
    /*
     * NDMP4 require the Mover be in IDLE state.
     * This always preceeds both MOVER_LISTEN or
     * MOVER_CONNECT.
     */
    if (ms->state != NDMP9_MOVER_STATE_IDLE
        && ms->state != NDMP9_MOVER_STATE_PAUSED) {
      NDMADR_RAISE_ILLEGAL_STATE("mover_state !IDLE/PAUSED");
    }
  }

  if (request->offset % ms->record_size != 0) {
    NDMADR_RAISE_ILLEGAL_ARGS("off !record_size");
  }

  /* TODO: NDMPv4 subtle semantic changes here */

  /* If a maximum length window is required following a mover transition
   * to the PAUSED state, a window length of all ones (binary) minus the
   * current window offset MUST be specified." (NDMPv4 RFC, Section
   * 3.6.2.2) -- we allow length = NDMP_LENGTH_INFINITY too */

  if (request->length != NDMP_LENGTH_INFINITY
      && request->length + request->offset != NDMP_LENGTH_INFINITY) {
    if (request->length % ms->record_size != 0) {
      NDMADR_RAISE_ILLEGAL_ARGS("len !record_size");
    }

    max_len = NDMP_LENGTH_INFINITY - request->offset;
    max_len -= max_len % ms->record_size;
    if (request->length > max_len) { /* learn math fella */
      NDMADR_RAISE_ILLEGAL_ARGS("length too long");
    }
    end_win = request->offset + request->length;
  } else {
    end_win = NDMP_LENGTH_INFINITY;
  }
  ms->window_offset = request->offset;
  /* record_num should probably be one less than this value, but the spec
   * says to divide, so we divide */
  ms->record_num = request->offset / ms->record_size;
  ms->window_length = request->length;
  ta->mover_window_end = end_win;
  ta->mover_window_first_blockno = ta->tape_state.blockno.value;

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_MOVER_READ
 */
int ndmp_sxa_mover_read(struct ndm_session* sess,
                        struct ndmp_xa_buf* xa,
                        struct ndmconn* ref_conn)
{
  struct ndm_tape_agent* ta = sess->tape_acb;
  struct ndmp9_mover_get_state_reply* ms = &ta->mover_state;

  NDMS_WITH(ndmp9_mover_read)
  ndmta_mover_sync_state(sess);

  if (ms->state != NDMP9_MOVER_STATE_ACTIVE) {
    NDMADR_RAISE_ILLEGAL_STATE("mover_state !ACTIVE");
  }

  if (ms->bytes_left_to_read > 0) {
    NDMADR_RAISE_ILLEGAL_STATE("byte_left_to_read");
  }

  if (ms->data_connection_addr.addr_type != NDMP9_ADDR_TCP) {
    NDMADR_RAISE_ILLEGAL_STATE("mover_addr !TCP");
  }

  if (ms->mode != NDMP9_MOVER_MODE_WRITE) {
    NDMADR_RAISE_ILLEGAL_STATE("mover_mode !WRITE");
  }

  ndmta_mover_read(sess, request->offset, request->length);

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_MOVER_CLOSE
 */
int ndmp_sxa_mover_close(struct ndm_session* sess,
                         struct ndmp_xa_buf* xa,
                         struct ndmconn* ref_conn)
{
  struct ndm_tape_agent* ta = sess->tape_acb;

  NDMS_WITH_VOID_REQUEST(ndmp9_mover_close)
  {
    if (ta->mover_state.state == NDMP9_MOVER_STATE_IDLE)
      NDMADR_RAISE_ILLEGAL_STATE("mover_state !IDLE");
  }

  ndmta_mover_close(sess);

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_MOVER_SET_RECORD_SIZE
 */
int ndmp_sxa_mover_set_record_size(struct ndm_session* sess,
                                   struct ndmp_xa_buf* xa,
                                   struct ndmconn* ref_conn)
{
  struct ndm_tape_agent* ta = sess->tape_acb;
  struct ndmp9_mover_get_state_reply* ms = &ta->mover_state;

  NDMS_WITH(ndmp9_mover_set_record_size)
  ndmta_mover_sync_state(sess);

  if (ms->state != NDMP9_MOVER_STATE_IDLE
      && ms->state != NDMP9_MOVER_STATE_PAUSED)
    NDMADR_RAISE_ILLEGAL_STATE("mover_state !IDLE/PAUSED");

  if (!NDMOS_MACRO_OK_TAPE_REC_LEN(request->record_size))
    NDMADR_RAISE_ILLEGAL_ARGS("!ok_tape_rec_len");

  ta->mover_state.record_size = request->record_size;

  return 0;
  NDMS_ENDWITH
}


#  ifndef NDMOS_EFFECT_NO_NDMP3_NOR_NDMP4 /* Surrounds NDMPv[34] MOVER intfs \
                                           */
/*
 * NDMP[34]_MOVER_CONNECT
 */
int ndmp_sxa_mover_connect(struct ndm_session* sess,
                           struct ndmp_xa_buf* xa,
                           struct ndmconn* ref_conn)
{
#    ifndef NDMOS_OPTION_NO_DATA_AGENT
  struct ndm_data_agent* da = sess->data_acb;
#    endif /* !NDMOS_OPTION_NO_DATA_AGENT */
  struct ndm_tape_agent* ta = sess->tape_acb;
  ndmp9_error error;
  int will_write;
  char reason[100];

  NDMS_WITH(ndmp9_mover_connect)

  /* Check args */
  switch (request->mode) {
    default:
      NDMADR_RAISE_ILLEGAL_ARGS("mover_mode");
    case NDMP9_MOVER_MODE_READ:
      will_write = 1;
      break;

    case NDMP9_MOVER_MODE_WRITE:
      will_write = 0;
      break;
  }

  switch (request->addr.addr_type) {
    default:
      NDMADR_RAISE_ILLEGAL_ARGS("mover_addr_type");
    case NDMP9_ADDR_LOCAL:
#    ifdef NDMOS_OPTION_NO_DATA_AGENT
      NDMADR_RAISE_ILLEGAL_ARGS("mover LOCAL w/o local DATA agent");
#    endif /* NDMOS_OPTION_NO_DATA_AGENT */
      break;

    case NDMP9_ADDR_TCP:
      break;
  }

  /* Check states -- this should cover everything */
  if (ta->mover_state.state != NDMP9_MOVER_STATE_IDLE)
    NDMADR_RAISE_ILLEGAL_STATE("mover_state !IDLE");
#    ifndef NDMOS_OPTION_NO_DATA_AGENT
  if (request->addr.addr_type == NDMP9_ADDR_LOCAL) {
    ndmp9_data_get_state_reply* ds = &da->data_state;

    if (ds->state != NDMP9_DATA_STATE_LISTEN)
      NDMADR_RAISE_ILLEGAL_STATE("data_state !LISTEN");

    if (ds->data_connection_addr.addr_type != NDMP9_ADDR_LOCAL)
      NDMADR_RAISE_ILLEGAL_STATE("data_addr !LOCAL");
  } else {
    if (da->data_state.state != NDMP9_DATA_STATE_IDLE)
      NDMADR_RAISE_ILLEGAL_STATE("data_state !IDLE");
  }
#    endif /* !NDMOS_OPTION_NO_DATA_AGENT */

  /* Check that the tape is ready to go */
  error = mover_can_proceed(sess, will_write);
  if (error != NDMP9_NO_ERR) NDMADR_RAISE(error, "!mover_can_proceed");

  /*
   * Check image stream state -- should already be reflected
   * in the mover and data states. This extra check gives
   * us an extra measure of robustness and sanity
   * check on the implementation.
   */
  error = ndmis_audit_tape_connect(sess, request->addr.addr_type, reason);
  if (error != NDMP9_NO_ERR) NDMADR_RAISE(error, reason);

  error = ndmis_tape_connect(sess, &request->addr, reason);
  if (error != NDMP9_NO_ERR) NDMADR_RAISE(error, reason);

  ta->mover_state.data_connection_addr = request->addr;
  /* alt: ta->....data_connection_addr = sess->...peer_addr */

  error = ndmta_mover_connect(sess, request->mode);
  if (error != NDMP9_NO_ERR) {
    /* TODO: belay ndmis_tape_connect() */
    NDMADR_RAISE(error, "!mover_connect");
  }

  return 0;
  NDMS_ENDWITH
}
#  endif /* !NDMOS_EFFECT_NO_NDMP3_NOR_NDMP4  Surrounds NDMPv[34] MOVER intfs \
          */


/*
 * NDMPx_MOVER helper routines
 */

/*
 * MOVER can only proceed from IDLE->LISTEN or PAUSED->ACTIVE
 * if the tape drive is ready.
 */

static ndmp9_error mover_can_proceed(struct ndm_session* sess, int will_write)
{
  struct ndm_tape_agent* ta = sess->tape_acb;

  ndmos_tape_sync_state(sess);
  if (ta->tape_state.state != NDMP9_TAPE_STATE_OPEN)
    return NDMP9_DEV_NOT_OPEN_ERR;

  if (will_write && !NDMTA_TAPE_IS_WRITABLE(ta)) return NDMP9_PERMISSION_ERR;

  return NDMP9_NO_ERR;
}

#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */ /* Surrounds MOVER intfs */


#ifndef NDMOS_OPTION_NO_CONTROL_AGENT /* Surrounds NOTIFY intfs */
/*
 * NDMPx_NOTIFY Interfaces
 ****************************************************************
 */


/*
 * NDMP[234]_NOTIFY_DATA_HALTED
 */
int ndmp_sxa_notify_data_halted(struct ndm_session* sess,
                                struct ndmp_xa_buf* xa,
                                [[maybe_unused]] struct ndmconn* ref_conn)
{
  struct ndm_control_agent* ca = sess->control_acb;

  NDMS_WITH_NO_REPLY(ndmp9_notify_data_halted)
  xa->reply.flags |= NDMNMB_FLAG_NO_SEND;

  ca->pending_notify_data_halted++;

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_NOTIFY_CONNECTED
 */
int ndmp_sxa_notify_connected([[maybe_unused]] struct ndm_session* sess,
                              struct ndmp_xa_buf* xa,
                              [[maybe_unused]] struct ndmconn* ref_conn)
{
  NDMS_WITH_NO_REPLY(ndmp9_notify_connected)
  xa->reply.flags |= NDMNMB_FLAG_NO_SEND;
  /* Just ignore? */
  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_NOTIFY_MOVER_HALTED
 */
int ndmp_sxa_notify_mover_halted(struct ndm_session* sess,
                                 struct ndmp_xa_buf* xa,
                                 [[maybe_unused]] struct ndmconn* ref_conn)
{
  struct ndm_control_agent* ca = sess->control_acb;

  NDMS_WITH_NO_REPLY(ndmp9_notify_mover_halted)
  xa->reply.flags |= NDMNMB_FLAG_NO_SEND;

  ca->pending_notify_mover_halted++;

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_NOTIFY_MOVER_PAUSED
 */
int ndmp_sxa_notify_mover_paused(struct ndm_session* sess,
                                 struct ndmp_xa_buf* xa,
                                 [[maybe_unused]] struct ndmconn* ref_conn)
{
  struct ndm_control_agent* ca = sess->control_acb;

  NDMS_WITH_NO_REPLY(ndmp9_notify_mover_paused)
  xa->reply.flags |= NDMNMB_FLAG_NO_SEND;

  ca->pending_notify_mover_paused++;
  ca->last_notify_mover_paused.reason = request->reason;
  ca->last_notify_mover_paused.seek_position = request->seek_position;

  return 0;
  NDMS_ENDWITH
}


/*
 * NDMP[234]_NOTIFY_DATA_READ
 */
int ndmp_sxa_notify_data_read(struct ndm_session* sess,
                              struct ndmp_xa_buf* xa,
                              [[maybe_unused]] struct ndmconn* ref_conn)
{
  struct ndm_control_agent* ca = sess->control_acb;

  NDMS_WITH_NO_REPLY(ndmp9_notify_data_read)
  xa->reply.flags |= NDMNMB_FLAG_NO_SEND;

  ca->pending_notify_data_read++;
  ca->last_notify_data_read.offset = request->offset;
  ca->last_notify_data_read.length = request->length;

  return 0;
  NDMS_ENDWITH
}
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */ /* Surrounds NOTIFY intfs */


#ifndef NDMOS_OPTION_NO_CONTROL_AGENT /* Surrounds LOG intfs */
/*
 * NDMPx_LOG Interfaces
 ****************************************************************
 */


/*
 * NDMP[234]_LOG_FILE
 */
int ndmp_sxa_log_file(struct ndm_session* sess,
                      struct ndmp_xa_buf* xa,
                      struct ndmconn* ref_conn)
{
  struct ndm_control_agent* ca = sess->control_acb;
  char prefix[32];
  char* tag;
  int lev = 0;

  xa->reply.flags |= NDMNMB_FLAG_NO_SEND;

  NDMS_WITH_NO_REPLY(ndmp9_log_file)
  switch (request->recovery_status) {
    case NDMP9_RECOVERY_SUCCESSFUL:
      tag = "OK";
      lev = 1;
      break;

    case NDMP9_RECOVERY_FAILED_PERMISSION:
      tag = "Bad Permission";
      break;

    case NDMP9_RECOVERY_FAILED_NOT_FOUND:
      tag = "Not found";
      break;

    case NDMP9_RECOVERY_FAILED_NO_DIRECTORY:
      tag = "No directory";
      break;

    case NDMP9_RECOVERY_FAILED_OUT_OF_MEMORY:
      tag = "Out of mem";
      break;

    case NDMP9_RECOVERY_FAILED_IO_ERROR:
      tag = "I/O error";
      break;

    case NDMP9_RECOVERY_FAILED_UNDEFINED_ERROR:
      tag = "General error";
      break;

    default:
      tag = "n";
      break;
  }

  /* count the notification and whether it is good news or not */
  ca->recover_log_file_count++;
  if (lev == 1) {
    ca->recover_log_file_ok++;
  } else {
    ca->recover_log_file_error++;
  }

  snprintf(prefix, sizeof(prefix), "%cLF", ref_conn->chan.name[1]);

  ndmalogf(sess, prefix, lev, "%s: %s", tag, request->name);
  NDMS_ENDWITH

  return 0;
}


#  ifndef NDMOS_OPTION_NO_NDMP2
/*
 * NDMP2_LOG_LOG
 */
int ndmp2_sxa_log_log(struct ndm_session* sess,
                      struct ndmp_xa_buf* xa,
                      struct ndmconn* ref_conn)
{
  char prefix[32];
  char* tag;
  char* bp;
  int lev;

  xa->reply.flags |= NDMNMB_FLAG_NO_SEND;

  NDMS_WITH_NO_REPLY(ndmp2_log_log)
  tag = "n";
  lev = 1;

  snprintf(prefix, sizeof(prefix), "%cLM%s", ref_conn->chan.name[1], tag);

  bp = strrchr(request->entry, '\n');
  if (bp) { *bp = '\0'; }

  ndmalogf(sess, prefix, lev, "LOG_LOG: '%s'", request->entry);
  NDMS_ENDWITH

  return 0;
}

/*
 * NDMP2_LOG_DEBUG
 */
int ndmp2_sxa_log_debug(struct ndm_session* sess,
                        struct ndmp_xa_buf* xa,
                        struct ndmconn* ref_conn)
{
  char prefix[32];
  char* tag;
  char* bp;
  int lev;

  xa->reply.flags |= NDMNMB_FLAG_NO_SEND;

  NDMS_WITH_NO_REPLY(ndmp2_log_debug)
  tag = "d";
  lev = 2;

  snprintf(prefix, sizeof(prefix), "%cLM%s", ref_conn->chan.name[1], tag);

  bp = strrchr(request->message, '\n');
  if (bp) { *bp = '\0'; }

  ndmalogf(sess, prefix, lev, "LOG_DEBUG: '%s'", request->message);
  NDMS_ENDWITH

  return 0;
}

#  endif /* !NDMOS_OPTION_NO_NDMP2 */


/*
 * NDMP[34]_LOG_MESSAGE
 */
int ndmp_sxa_log_message(struct ndm_session* sess,
                         struct ndmp_xa_buf* xa,
                         struct ndmconn* ref_conn)
{
  char prefix[32];
  char* tag;
  char* bp;
  int lev;

  xa->reply.flags |= NDMNMB_FLAG_NO_SEND;

  NDMS_WITH_NO_REPLY(ndmp9_log_message)
  switch (request->log_type) {
    case NDMP9_LOG_NORMAL:
      tag = "n";
      lev = 1;
      break;

    case NDMP9_LOG_DEBUG:
      tag = "d";
      lev = 2;
      break;

    case NDMP9_LOG_ERROR:
      tag = "e";
      lev = 0;
      break;

    case NDMP9_LOG_WARNING:
      tag = "w";
      lev = 0;
      break;

    default:
      tag = "?";
      lev = 0;
      break;
  }

  snprintf(prefix, sizeof(prefix), "%cLM%s", ref_conn->chan.name[1], tag);

  bp = strrchr(request->entry, '\n');
  if (bp) { *bp = '\0'; }

  ndmalogf(sess, prefix, lev, "LOG_MESSAGE: '%s'", request->entry);
  NDMS_ENDWITH

  return 0;
}

#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */ /* Surrounds LOG intfs */


#ifndef NDMOS_OPTION_NO_CONTROL_AGENT /* Surrounds FH intfs */
/*
 * NDMPx_FH Interfaces
 ****************************************************************
 */


/*
 * NDMP2_FH_ADD_UNIX_PATH
 * NDMP[34]_FH_ADD_FILE
 */
int ndmp_sxa_fh_add_file(struct ndm_session* sess,
                         struct ndmp_xa_buf* xa,
                         struct ndmconn* ref_conn)
{
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndmlog* ixlog = &ca->job.index_log;
  int tagc = ref_conn->chan.name[1];
  unsigned int i;
  ndmp9_file* file;

  xa->reply.flags |= NDMNMB_FLAG_NO_SEND;

  NDMS_WITH_NO_REPLY(ndmp9_fh_add_file)
  for (i = 0; i < request->files.files_len; i++) {
    file = &request->files.files_val[i];

    ndmfhdb_add_file(ixlog, tagc, file->unix_path, &file->fstat);
  }
  NDMS_ENDWITH

  return 0;
}


/*
 * NDMP2_FH_ADD_UNIX_DIR
 * NDMP[34]_FH_ADD_DIR
 */
int ndmp_sxa_fh_add_dir(struct ndm_session* sess,
                        struct ndmp_xa_buf* xa,
                        struct ndmconn* ref_conn)
{
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndmlog* ixlog = &ca->job.index_log;
  int tagc = ref_conn->chan.name[1];
  char* raw_name;
  unsigned int i;
  ndmp9_dir* dir;

  xa->reply.flags |= NDMNMB_FLAG_NO_SEND;

  NDMS_WITH_NO_REPLY(ndmp9_fh_add_dir)
  for (i = 0; i < request->dirs.dirs_len; i++) {
    dir = &request->dirs.dirs_val[i];

    raw_name = dir->unix_name;

    switch (ca->job.n_dir_entry) {
      case 0:
        if (strcmp(raw_name, ".") == 0) {
          /* goodness */
          ndmfhdb_add_dirnode_root(ixlog, tagc, dir->node);
          ca->job.root_node = dir->node;
        } else {
          /* ungoodness */
          ndmalogf(sess, 0, 0,
                   "WARNING: First add_dir "
                   "entry is non-conforming");
        }
        break;

      case 1:
        if (strcmp(raw_name, "..") == 0 && dir->parent == dir->node
            && dir->node == ca->job.root_node) {
          /* goodness */
        } else {
          /* ungoodness */
          /* NetApp is non-conforming */
          /* ndmalogf (sess, 0, 0,
                  "WARNING: Second add_dir "
                  "entry is non-conforming"); */
        }
        break;

      default:
        break;
    }

    ndmfhdb_add_dir(ixlog, tagc, dir->unix_name, dir->parent, dir->node);

    ca->job.n_dir_entry++;
  }
  NDMS_ENDWITH

  return 0;
}


/*
 * NDMP2_FH_ADD_UNIX_NODE
 * NDMP[34]_FH_ADD_NODE
 */
int ndmp_sxa_fh_add_node(struct ndm_session* sess,
                         struct ndmp_xa_buf* xa,
                         struct ndmconn* ref_conn)
{
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndmlog* ixlog = &ca->job.index_log;
  int tagc = ref_conn->chan.name[1];
  unsigned int i;
  ndmp9_node* node;

  xa->reply.flags |= NDMNMB_FLAG_NO_SEND;

  NDMS_WITH_NO_REPLY(ndmp9_fh_add_node)
  for (i = 0; i < request->nodes.nodes_len; i++) {
    node = &request->nodes.nodes_val[i];

    ndmfhdb_add_node(ixlog, tagc, node->fstat.node.value, &node->fstat);
  }
  NDMS_ENDWITH

  return 0;
}
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */ /* Surrounds FH intfs */


/*
 * Common helper interfaces
 ****************************************************************
 * These do complicated state checks which are called from
 * several of the interfaces above.
 */


#ifndef NDMOS_OPTION_NO_TAPE_AGENT
/*
 * Shortcut for DATA->MOVER READ requests when NDMP9_ADDR_LOCAL
 * (local MOVER). This is implemented here because of the
 * state (sanity) checks. This should track the
 * NDMP9_MOVER_READ stanza in ndma_dispatch_request().
 */

int ndmta_local_mover_read(struct ndm_session* sess,
                           uint64_t offset,
                           uint64_t length)
{
  struct ndm_tape_agent* ta = sess->tape_acb;
  struct ndmp9_mover_get_state_reply* ms = &ta->mover_state;
  char* errstr = 0;

  if (ms->state != NDMP9_MOVER_STATE_ACTIVE
      && ms->state != NDMP9_MOVER_STATE_LISTEN) {
    errstr = "mover_state !ACTIVE";
    goto senderr;
  }
  if (ms->bytes_left_to_read > 0) {
    errstr = "byte_left_to_read";
    goto senderr;
  }
  if (ms->data_connection_addr.addr_type != NDMP9_ADDR_LOCAL) {
    errstr = "mover_addr !LOCAL";
    goto senderr;
  }
  if (ms->mode != NDMP9_MOVER_MODE_WRITE) {
    errstr = "mover_mode !WRITE";
    goto senderr;
  }

  ms->seek_position = offset;
  ms->bytes_left_to_read = length;
  ta->mover_want_pos = offset;

  return 0;

senderr:
  if (errstr) { ndmalogf(sess, 0, 2, "local_read error why=%s", errstr); }

  return -1;
}
#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */


/*
 * Dispatch Version Table and Dispatch Request Tables (DVT/DRT)
 ****************************************************************
 */

struct ndm_dispatch_request_table* ndma_drt_lookup(
    struct ndm_dispatch_version_table* dvt,
    unsigned protocol_version,
    unsigned message)
{
  struct ndm_dispatch_request_table* drt;

  for (; dvt->protocol_version >= 0; dvt++) {
    if (dvt->protocol_version == (int)protocol_version) break;
  }

  if (dvt->protocol_version < 0) return 0;

  for (drt = dvt->dispatch_request_table; drt->message; drt++) {
    if (drt->message == message) return drt;
  }

  return 0;
}

struct ndm_dispatch_request_table ndma_dispatch_request_table_v0[]
    = {{
           NDMP0_CONNECT_OPEN,
           NDM_DRT_FLAG_OK_NOT_CONNECTED + NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
           ndmp_sxa_connect_open,
       },
       {
           NDMP0_CONNECT_CLOSE,
           NDM_DRT_FLAG_OK_NOT_CONNECTED + NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
           ndmp_sxa_connect_close,
       },
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT /* Surrounds NOTIFY intfs */
       {
           NDMP0_NOTIFY_CONNECTED,
           0,
           ndmp_sxa_notify_connected,
       },
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */ /* Surrounds NOTIFY intfs */
       {0}};


#ifndef NDMOS_OPTION_NO_NDMP2
struct ndm_dispatch_request_table ndma_dispatch_request_table_v2[]
    = {{NDMP2_CONFIG_GET_BUTYPE_ATTR, NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
        ndmp2_sxa_config_get_butype_attr},
       {NDMP2_LOG_LOG, 0, ndmp2_sxa_log_log},
       {NDMP2_LOG_DEBUG, 0, ndmp2_sxa_log_debug},
       {0}};
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
struct ndm_dispatch_request_table ndma_dispatch_request_table_v3[] = {{0}};
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
struct ndm_dispatch_request_table ndma_dispatch_request_table_v4[] = {{0}};
#endif /* !NDMOS_OPTION_NO_NDMP4 */


struct ndm_dispatch_request_table ndma_dispatch_request_table_v9[]
    = {{NDMP9_CONNECT_OPEN,
        NDM_DRT_FLAG_OK_NOT_CONNECTED + NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
        ndmp_sxa_connect_open},
       {NDMP9_CONNECT_CLIENT_AUTH, NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
        ndmp_sxa_connect_client_auth},
       {NDMP9_CONNECT_CLOSE, NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
        ndmp_sxa_connect_close},
       {NDMP9_CONNECT_SERVER_AUTH, NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
        ndmp_sxa_connect_server_auth},
       {NDMP9_CONFIG_GET_HOST_INFO, NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
        ndmp_sxa_config_get_info},
       {NDMP9_CONFIG_GET_CONNECTION_TYPE, NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
        ndmp_sxa_config_get_info},
       {NDMP9_CONFIG_GET_AUTH_ATTR, NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
        ndmp_sxa_config_get_auth_attr},
       {NDMP9_CONFIG_GET_BUTYPE_INFO, NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
        ndmp_sxa_config_get_info},
       {NDMP9_CONFIG_GET_FS_INFO, NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
        ndmp_sxa_config_get_info},
       {NDMP9_CONFIG_GET_TAPE_INFO, NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
        ndmp_sxa_config_get_info},
       {NDMP9_CONFIG_GET_SCSI_INFO, NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
        ndmp_sxa_config_get_info},
       {NDMP9_CONFIG_GET_SERVER_INFO, NDM_DRT_FLAG_OK_NOT_AUTHORIZED,
        ndmp_sxa_config_get_info},
#ifndef NDMOS_OPTION_NO_ROBOT_AGENT /* Surrounds SCSI intfs */
       {NDMP9_SCSI_OPEN, 0, ndmp_sxa_scsi_open},
       {NDMP9_SCSI_CLOSE, 0, ndmp_sxa_scsi_close},
       {NDMP9_SCSI_GET_STATE, 0, ndmp_sxa_scsi_get_state},
       {NDMP9_SCSI_SET_TARGET, 0, ndmp_sxa_scsi_set_target},
       {NDMP9_SCSI_RESET_DEVICE, 0, ndmp_sxa_scsi_reset_device},
       {NDMP9_SCSI_RESET_BUS, 0, ndmp_sxa_scsi_reset_bus},
       {NDMP9_SCSI_EXECUTE_CDB, 0, ndmp_sxa_scsi_execute_cdb},
#endif /* !NDMOS_OPTION_NO_ROBOT_AGENT */ /* Surrounds SCSI intfs */
#ifndef NDMOS_OPTION_NO_TAPE_AGENT        /* Surrounds TAPE intfs */
       {NDMP9_TAPE_OPEN, 0, ndmp_sxa_tape_open},
       {NDMP9_TAPE_CLOSE, 0, ndmp_sxa_tape_close},
       {NDMP9_TAPE_GET_STATE, 0, ndmp_sxa_tape_get_state},
       {NDMP9_TAPE_MTIO, 0, ndmp_sxa_tape_mtio},
       {NDMP9_TAPE_WRITE, 0, ndmp_sxa_tape_write},
       {NDMP9_TAPE_READ, 0, ndmp_sxa_tape_read},
       {NDMP9_TAPE_EXECUTE_CDB, 0, ndmp_sxa_tape_execute_cdb},
#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */ /* Surrounds TAPE intfs */
#ifndef NDMOS_OPTION_NO_DATA_AGENT       /* Surrounds DATA intfs */
       {NDMP9_DATA_GET_STATE, 0, ndmp_sxa_data_get_state},
       {NDMP9_DATA_START_BACKUP, 0, ndmp_sxa_data_start_backup},
       {NDMP9_DATA_START_RECOVER, 0, ndmp_sxa_data_start_recover},
       {NDMP9_DATA_ABORT, 0, ndmp_sxa_data_abort},
       {NDMP9_DATA_GET_ENV, 0, ndmp_sxa_data_get_env},
       {NDMP9_DATA_STOP, 0, ndmp_sxa_data_stop},
#  ifndef NDMOS_EFFECT_NO_NDMP3_NOR_NDMP4 /* Surrounds NDMPv[34] DATA intfs */
#    ifdef notyet
       {NDMP9_DATA_LISTEN, 0, ndmp_sxa_data_listen},
#    endif /* notyet */
       {NDMP9_DATA_CONNECT, 0, ndmp_sxa_data_connect},
#  endif /* NDMOS_EFFECT_NO_NDMP3_NOR_NDMP4  Surrounds NDMPv[34] DATA intfs */
       {NDMP9_DATA_START_RECOVER_FILEHIST, 0,
        ndmp_sxa_data_start_recover_filehist},
#endif /* !NDMOS_OPTION_NO_DATA_AGENT */ /* Surrounds DATA intfs */
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT    /* Surrounds NOTIFY intfs */
       {NDMP9_NOTIFY_DATA_HALTED, 0, ndmp_sxa_notify_data_halted},
       {NDMP9_NOTIFY_CONNECTED, 0, ndmp_sxa_notify_connected},
       {NDMP9_NOTIFY_MOVER_HALTED, 0, ndmp_sxa_notify_mover_halted},
       {NDMP9_NOTIFY_MOVER_PAUSED, 0, ndmp_sxa_notify_mover_paused},
       {NDMP9_NOTIFY_DATA_READ, 0, ndmp_sxa_notify_data_read},
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */ /* Surrounds NOTIFY intfs */
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT       /* Surrounds LOG intfs */
       {NDMP9_LOG_FILE, 0, ndmp_sxa_log_file},
       {NDMP9_LOG_MESSAGE, 0, ndmp_sxa_log_message},
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */ /* Surrounds LOG intfs */
#ifndef NDMOS_OPTION_NO_CONTROL_AGENT       /* Surrounds FH intfs */
       {NDMP9_FH_ADD_FILE, 0, ndmp_sxa_fh_add_file},
       {NDMP9_FH_ADD_DIR, 0, ndmp_sxa_fh_add_dir},
       {NDMP9_FH_ADD_NODE, 0, ndmp_sxa_fh_add_node},
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */ /* Surrounds FH intfs */
#ifndef NDMOS_OPTION_NO_TAPE_AGENT          /* Surrounds MOVER intfs */
       {NDMP9_MOVER_GET_STATE, 0, ndmp_sxa_mover_get_state},
       {NDMP9_MOVER_LISTEN, 0, ndmp_sxa_mover_listen},
       {NDMP9_MOVER_CONTINUE, 0, ndmp_sxa_mover_continue},
       {NDMP9_MOVER_ABORT, 0, ndmp_sxa_mover_abort},
       {NDMP9_MOVER_STOP, 0, ndmp_sxa_mover_stop},
       {NDMP9_MOVER_SET_WINDOW, 0, ndmp_sxa_mover_set_window},
       {NDMP9_MOVER_READ, 0, ndmp_sxa_mover_read},
       {NDMP9_MOVER_CLOSE, 0, ndmp_sxa_mover_close},
       {NDMP9_MOVER_SET_RECORD_SIZE, 0, ndmp_sxa_mover_set_record_size},
       {NDMP9_MOVER_CONNECT, 0, ndmp_sxa_mover_connect},
#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */ /* Surrounds MOVER intfs */
       {0}};


struct ndm_dispatch_version_table ndma_dispatch_version_table[]
    = {{0, ndma_dispatch_request_table_v0},
#ifndef NDMOS_OPTION_NO_NDMP2
       {NDMP2VER, ndma_dispatch_request_table_v2},
#endif /* !NDMOS_OPTION_NO_NDMP2 */
#ifndef NDMOS_OPTION_NO_NDMP3
       {NDMP3VER, ndma_dispatch_request_table_v3},
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
       {NDMP4VER, ndma_dispatch_request_table_v4},
#endif /* !NDMOS_OPTION_NO_NDMP4 */
       {NDMP9VER, ndma_dispatch_request_table_v9},

       {-1, 0}};
