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
 *
 * ndmis_connect_status transitions
 *
 *   Event relative to  ------ before ------    ------ after -------
 *   "mine" end point   MINE    PEER    REMO    MINE    PEER    REMO
 *   ====================================================================
 *   LISTEN/LOCAL       IDLE    IDLE    IDLE    LISTEN  IDLE    EXCLUDE
 *   LISTEN/TCP         IDLE    IDLE    IDLE    LISTEN  REMOTE  LISTEN
 *
 *   CONNECT/LOCAL      IDLE    LISTEN  EXCLUDE CONN'ED ACC'ED  EXCLUDE
 *   CONNECT/TCP        IDLE    IDLE    IDLE    CONN'ED REMOTE  CONN'ED
 *
 *   tcp_accept()       LISTEN  REMOTE  LISTEN  ACC'ED  REMOTE  ACC'ED
 *
 */


#if 0
        DATA                                    TAPE
        END     ========== LOCAL ============   END
        POINT                                   POINT
                        REMOTE
#endif


#include "ndmagents.h"


int ndmis_reinit_remote(struct ndm_session* sess)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  struct ndm_tape_agent* ta = sess->tape_acb;

  NDMOS_MACRO_ZEROFILL(&is->remote);

  ndmchan_initialize(&is->remote.listen_chan, "image-stream-listen");
  ndmchan_initialize(&is->remote.sanity_chan, "image-stream-sanity");
  ndmchan_initialize(&is->chan, "image-stream");
  if (!is->buf) {
    is->buflen = ta->mover_state.record_size;
    is->buf = NDMOS_API_MALLOC(is->buflen);
    if (!is->buf) { return -1; }
    NDMOS_MACRO_ZEROFILL_SIZE(is->buf, is->buflen);
  }
  ndmchan_setbuf(&is->chan, is->buf, is->buflen);

  return 0;
}


/*
 * Initialization and Cleanup
 ****************************************************************
 */

/* Initialize -- Set data structure to know value, ignore current value */
int ndmis_initialize(struct ndm_session* sess)
{
  sess->plumb.image_stream = NDMOS_API_MALLOC(sizeof(struct ndm_image_stream));
  if (!sess->plumb.image_stream) return -1;
  NDMOS_MACRO_ZEROFILL(sess->plumb.image_stream);
  NDMOS_MACRO_ZEROFILL(&sess->plumb.image_stream->chan);

  ndmis_reinit_remote(sess);

  sess->plumb.image_stream->data_ep.name = "DATA";
  sess->plumb.image_stream->tape_ep.name = "TAPE";

  return 0;
}

/* Commission -- Get agent ready. Entire session has been initialize()d */
int ndmis_commission(struct ndm_session* sess) { return 0; }

/* Decommission -- Discard agent */
int ndmis_decommission(struct ndm_session* sess) { return 0; }

/* Destroy -- Destroy agent */
int ndmis_destroy(struct ndm_session* sess)
{
  if (!sess->plumb.image_stream) { return 0; }

  if (sess->plumb.image_stream->buf) {
    NDMOS_API_FREE(sess->plumb.image_stream->buf);
  }
  NDMOS_API_FREE(sess->plumb.image_stream);
  sess->plumb.image_stream = NULL;

  return 0;
}

/* Belay -- Cancel partially issued activation/start */
int ndmis_belay(struct ndm_session* sess) { return 0; }


/*
 * Semantic actions -- called from ndma_dispatch()
 ****************************************************************
 */

ndmp9_error ndmis_audit_data_listen(struct ndm_session* sess,
                                    ndmp9_addr_type addr_type,
                                    char* reason)
{
  struct ndm_image_stream* is;
  struct ndmis_end_point* mine_ep;
  struct ndmis_end_point* peer_ep;

  /*
   * We are about to start using an Image Stream so allocate it.
   * Only do this when not allocated yet.
   */
  if (!sess->plumb.image_stream) {
    if (ndmis_initialize(sess)) { return NDMP9_NO_MEM_ERR; }
  }

  is = sess->plumb.image_stream;
  mine_ep = &is->data_ep;
  peer_ep = &is->tape_ep;

  return ndmis_audit_ep_listen(sess, addr_type, reason, mine_ep, peer_ep);
}

ndmp9_error ndmis_audit_tape_listen(struct ndm_session* sess,
                                    ndmp9_addr_type addr_type,
                                    char* reason)
{
  struct ndm_image_stream* is;
  struct ndmis_end_point* mine_ep;
  struct ndmis_end_point* peer_ep;

  /*
   * We are about to start using an Image Stream so allocate it.
   * Only do this when not allocated yet.
   */
  if (!sess->plumb.image_stream) {
    if (ndmis_initialize(sess)) { return NDMP9_NO_MEM_ERR; }
  }

  is = sess->plumb.image_stream;
  mine_ep = &is->tape_ep;
  peer_ep = &is->data_ep;

  return ndmis_audit_ep_listen(sess, addr_type, reason, mine_ep, peer_ep);
}

ndmp9_error ndmis_audit_data_connect(struct ndm_session* sess,
                                     ndmp9_addr_type addr_type,
                                     char* reason)
{
  struct ndm_image_stream* is;
  struct ndmis_end_point* mine_ep;
  struct ndmis_end_point* peer_ep;

  /*
   * We are about to start using an Image Stream so allocate it.
   * Only do this when not allocated yet.
   */
  if (!sess->plumb.image_stream) {
    if (ndmis_initialize(sess)) { return NDMP9_NO_MEM_ERR; }
  }

  is = sess->plumb.image_stream;
  mine_ep = &is->data_ep;
  peer_ep = &is->tape_ep;

  return ndmis_audit_ep_listen(sess, addr_type, reason, mine_ep, peer_ep);
}

ndmp9_error ndmis_audit_tape_connect(struct ndm_session* sess,
                                     ndmp9_addr_type addr_type,
                                     char* reason)
{
  struct ndm_image_stream* is;
  struct ndmis_end_point* mine_ep;
  struct ndmis_end_point* peer_ep;

  /*
   * We are about to start using an Image Stream so allocate it.
   * Only do this when not allocated yet.
   */
  if (!sess->plumb.image_stream) {
    if (ndmis_initialize(sess)) { return NDMP9_NO_MEM_ERR; }
  }

  is = sess->plumb.image_stream;
  mine_ep = &is->tape_ep;
  peer_ep = &is->data_ep;

  return ndmis_audit_ep_listen(sess, addr_type, reason, mine_ep, peer_ep);
}

ndmp9_error ndmis_data_listen(struct ndm_session* sess,
                              ndmp9_addr_type addr_type,
                              ndmp9_addr* ret_addr,
                              char* reason)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  struct ndmis_end_point* mine_ep = &is->data_ep;
  struct ndmis_end_point* peer_ep = &is->tape_ep;

  return ndmis_ep_listen(sess, addr_type, ret_addr, reason, mine_ep, peer_ep);
}

ndmp9_error ndmis_tape_listen(struct ndm_session* sess,
                              ndmp9_addr_type addr_type,
                              ndmp9_addr* ret_addr,
                              char* reason)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  struct ndmis_end_point* mine_ep = &is->tape_ep;
  struct ndmis_end_point* peer_ep = &is->data_ep;

  return ndmis_ep_listen(sess, addr_type, ret_addr, reason, mine_ep, peer_ep);
}

ndmp9_error ndmis_data_connect(struct ndm_session* sess,
                               ndmp9_addr* addr,
                               char* reason)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  struct ndmis_end_point* mine_ep = &is->data_ep;
  struct ndmis_end_point* peer_ep = &is->tape_ep;
  ndmp9_error error;

  error = ndmis_ep_connect(sess, addr, reason, mine_ep, peer_ep);
#ifndef NDMOS_OPTION_NO_TAPE_AGENT
  if (error == NDMP9_NO_ERR) {
    if (peer_ep->connect_status == NDMIS_CONN_ACCEPTED &&
        peer_ep->addr_type == NDMP9_ADDR_LOCAL) {
      ndmta_quantum(sess);
    }
  }
#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
  return error;
}

ndmp9_error ndmis_tape_connect(struct ndm_session* sess,
                               ndmp9_addr* addr,
                               char* reason)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  struct ndmis_end_point* mine_ep = &is->tape_ep;
  struct ndmis_end_point* peer_ep = &is->data_ep;

  return ndmis_ep_connect(sess, addr, reason, mine_ep, peer_ep);
}

int ndmis_ep_start(struct ndm_session* sess,
                   int chan_mode,
                   struct ndmis_end_point* mine_ep,
                   struct ndmis_end_point* peer_ep)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;

  if (mine_ep->connect_status != NDMIS_CONN_CONNECTED &&
      mine_ep->connect_status != NDMIS_CONN_ACCEPTED) {
    return -1;
  }

  if (mine_ep->transfer_mode != NDMCHAN_MODE_IDLE) { return -2; }

  if (mine_ep->addr_type == NDMP9_ADDR_LOCAL) {
    ndmchan_start_resident(&is->chan);
    if (chan_mode == NDMCHAN_MODE_WRITE) {
      peer_ep->transfer_mode = NDMCHAN_MODE_READ;
    } else {
      peer_ep->transfer_mode = NDMCHAN_MODE_WRITE;
    }
  } else if (chan_mode == NDMCHAN_MODE_WRITE) {
    ndmchan_pending_to_write(&is->chan);
  } else if (chan_mode == NDMCHAN_MODE_READ) {
    ndmchan_pending_to_read(&is->chan);
  } else {
    return -3;
  }

  mine_ep->transfer_mode = chan_mode;

  return 0;
}

int ndmis_data_start(struct ndm_session* sess, int chan_mode)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  struct ndmis_end_point* mine_ep = &is->data_ep;
  struct ndmis_end_point* peer_ep = &is->tape_ep;

  return ndmis_ep_start(sess, chan_mode, mine_ep, peer_ep);
}

int ndmis_tape_start(struct ndm_session* sess, int chan_mode)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  struct ndmis_end_point* mine_ep = &is->tape_ep;
  struct ndmis_end_point* peer_ep = &is->data_ep;

  return ndmis_ep_start(sess, chan_mode, mine_ep, peer_ep);
}

int ndmis_data_close(struct ndm_session* sess)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;

  if (is) {
    return ndmis_ep_close(sess, &is->data_ep, &is->tape_ep);
  } else {
    return 0;
  }
}

int ndmis_tape_close(struct ndm_session* sess)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;

  if (is) {
    return ndmis_ep_close(sess, &is->tape_ep, &is->data_ep);
  } else {
    return 0;
  }
}


/*
 * Quantum -- get a bit of work done
 ****************************************************************
 */

int ndmis_quantum(struct ndm_session* sess)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  struct ndmis_end_point* mine_ep;
  int rc;

  if (is->remote.connect_status != NDMIS_CONN_LISTEN)
    return 0; /* did nothing */

  if (!is->remote.listen_chan.ready) return 0; /* did nothing */

  /* now this is going to get hard */

  if (is->data_ep.connect_status == NDMIS_CONN_LISTEN) {
    mine_ep = &is->data_ep;
    /* assert (is->tape_ep.connect_status == NDMIS_CONN_REMOTE); */
  } else if (is->tape_ep.connect_status == NDMIS_CONN_LISTEN) {
    mine_ep = &is->tape_ep;
    /* assert (is->data_ep.connect_status == NDMIS_CONN_REMOTE); */
  } else {
    assert(0);
    return -1;
  }

  rc = ndmis_tcp_accept(sess);
  if (rc == 0) {
    mine_ep->connect_status = NDMIS_CONN_ACCEPTED;
    is->remote.connect_status = NDMIS_CONN_ACCEPTED;
  } else {
    mine_ep->connect_status = NDMIS_CONN_BOTCHED;
    is->remote.connect_status = NDMIS_CONN_BOTCHED;
  }

  return 1; /* did something */
}


/*
 * ndmis_end_point oriented helper routines
 ****************************************************************
 */

ndmp9_error ndmis_audit_ep_listen(struct ndm_session* sess,
                                  ndmp9_addr_type addr_type,
                                  char* reason,
                                  struct ndmis_end_point* mine_ep,
                                  struct ndmis_end_point* peer_ep)
{
  ndmp9_error error = NDMP9_NO_ERR;
  char* reason_end;

  sprintf(reason, "IS %s_LISTEN: ", mine_ep->name);
  reason_end = reason;
  while (*reason_end) reason_end++;

  if (mine_ep->connect_status != NDMIS_CONN_IDLE) {
    sprintf(reason_end, "%s not idle", mine_ep->name);
    error = NDMP9_ILLEGAL_STATE_ERR;
    goto out;
  }
  if (peer_ep->connect_status != NDMIS_CONN_IDLE) {
    sprintf(reason_end, "%s not idle", peer_ep->name);
    error = NDMP9_ILLEGAL_STATE_ERR;
    goto out;
  }

  switch (addr_type) {
    case NDMP9_ADDR_LOCAL:
      break;

    case NDMP9_ADDR_TCP:
      break;

    default:
      strcpy(reason_end, "unknown addr_type");
      error = NDMP9_ILLEGAL_ARGS_ERR;
      goto out;
  }

out:
  if (error == NDMP9_NO_ERR)
    strcpy(reason_end, "OK");
  else
    ndmalogf(sess, 0, 2, "listen %s messy mcs=%d pcs=%d", mine_ep->name,
             mine_ep->connect_status, peer_ep->connect_status);

  return error;
}

ndmp9_error ndmis_audit_ep_connect(struct ndm_session* sess,
                                   ndmp9_addr_type addr_type,
                                   char* reason,
                                   struct ndmis_end_point* mine_ep,
                                   struct ndmis_end_point* peer_ep)
{
  ndmp9_error error = NDMP9_NO_ERR;
  char* reason_end;

  sprintf(reason, "IS %s_CONNECT: ", mine_ep->name);
  reason_end = reason;
  while (*reason_end) reason_end++;

  if (mine_ep->connect_status != NDMIS_CONN_IDLE) {
    sprintf(reason_end, "%s not idle", mine_ep->name);
    error = NDMP9_ILLEGAL_STATE_ERR;
    goto out;
  }

  switch (addr_type) {
    case NDMP9_ADDR_LOCAL:
      if (peer_ep->connect_status != NDMIS_CONN_LISTEN) {
        sprintf(reason_end, "LOCAL %s not LISTEN", peer_ep->name);
        error = NDMP9_ILLEGAL_STATE_ERR;
        goto out;
      }
      if (peer_ep->addr_type != NDMP9_ADDR_LOCAL) {
        sprintf(reason_end, "LOCAL %s not LOCAL", peer_ep->name);
        error = NDMP9_ILLEGAL_STATE_ERR;
        goto out;
      }
      break;

    case NDMP9_ADDR_TCP:
      if (peer_ep->connect_status != NDMIS_CONN_IDLE) {
        sprintf(reason_end, "LOCAL %s not IDLE", peer_ep->name);
        error = NDMP9_ILLEGAL_STATE_ERR;
        goto out;
      }
      break;

    default:
      strcpy(reason_end, "unknown addr_type");
      error = NDMP9_ILLEGAL_ARGS_ERR;
      goto out;
  }

out:
  if (error == NDMP9_NO_ERR) strcpy(reason_end, "OK");

  return error;
}


ndmp9_error ndmis_ep_listen(struct ndm_session* sess,
                            ndmp9_addr_type addr_type,
                            ndmp9_addr* ret_addr,
                            char* reason,
                            struct ndmis_end_point* mine_ep,
                            struct ndmis_end_point* peer_ep)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  char* reason_end;
  ndmp9_error error;

  error = ndmis_audit_ep_listen(sess, addr_type, reason, mine_ep, peer_ep);
  if (error != NDMP9_NO_ERR) return error;

  reason_end = reason;
  while (*reason_end && *reason_end != ':') reason_end++;
  *reason_end++ = ':';
  *reason_end++ = ' ';
  *reason_end = 0;

  NDMOS_MACRO_ZEROFILL(ret_addr);
  ret_addr->addr_type = addr_type;

  switch (addr_type) {
    case NDMP9_ADDR_LOCAL:
      mine_ep->addr_type = NDMP9_ADDR_LOCAL;
      mine_ep->connect_status = NDMIS_CONN_LISTEN;
      is->remote.connect_status = NDMIS_CONN_EXCLUDE;
      break;

    case NDMP9_ADDR_TCP:
      if (ndmis_tcp_listen(sess, ret_addr) != 0) {
        strcpy(reason_end, "TCP listen() failed");
        error = NDMP9_CONNECT_ERR;
        goto out;
      }
      mine_ep->addr_type = NDMP9_ADDR_TCP;
      mine_ep->connect_status = NDMIS_CONN_LISTEN;
      peer_ep->connect_status = NDMIS_CONN_REMOTE;
      break;

    default:
      reason = "unknown addr_type (bad)";
      error = NDMP9_ILLEGAL_ARGS_ERR;
      goto out;
  }

out:
  if (error == NDMP9_NO_ERR) strcpy(reason_end, "OK");

  return error;
}

ndmp9_error ndmis_ep_connect(struct ndm_session* sess,
                             ndmp9_addr* addr,
                             char* reason,
                             struct ndmis_end_point* mine_ep,
                             struct ndmis_end_point* peer_ep)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  ndmp9_addr_type addr_type = addr->addr_type;
  char* reason_end;
  ndmp9_error error;

  error = ndmis_audit_ep_connect(sess, addr_type, reason, mine_ep, peer_ep);
  if (error != NDMP9_NO_ERR) return error;

  reason_end = reason;
  while (*reason_end && *reason_end != ':') reason_end++;
  *reason_end++ = ':';
  *reason_end++ = ' ';
  *reason_end = 0;

  switch (addr_type) {
    case NDMP9_ADDR_LOCAL:
      mine_ep->addr_type = NDMP9_ADDR_LOCAL;
      mine_ep->connect_status = NDMIS_CONN_CONNECTED;
      peer_ep->connect_status = NDMIS_CONN_ACCEPTED;
      is->remote.connect_status = NDMIS_CONN_EXCLUDE;
      break;

    case NDMP9_ADDR_TCP:
      if (ndmis_tcp_connect(sess, addr) != 0) {
        strcpy(reason_end, "TCP connect() failed");
        error = NDMP9_CONNECT_ERR;
        goto out;
      }
      mine_ep->addr_type = NDMP9_ADDR_TCP;
      mine_ep->connect_status = NDMIS_CONN_CONNECTED;
      peer_ep->connect_status = NDMIS_CONN_REMOTE;
      break;

    default:
      reason = "unknown addr_type (bad)";
      error = NDMP9_ILLEGAL_ARGS_ERR;
      goto out;
  }

out:
  return error;
}

int ndmis_ep_close(struct ndm_session* sess,
                   struct ndmis_end_point* mine_ep,
                   struct ndmis_end_point* peer_ep)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  char* save_name = mine_ep->name;

  switch (mine_ep->connect_status) {
    case NDMIS_CONN_IDLE:
      return 0;

    case NDMIS_CONN_BOTCHED:
    case NDMIS_CONN_REMOTE:
    case NDMIS_CONN_EXCLUDE:
      goto messy;

    case NDMIS_CONN_LISTEN:
      switch (mine_ep->addr_type) {
        default:
          goto messy;

        case NDMP9_ADDR_LOCAL:
          ndmis_reinit_remote(sess);
          if (peer_ep->connect_status != NDMIS_CONN_IDLE) goto messy;
          break;

        case NDMP9_ADDR_TCP:
          ndmis_tcp_close(sess);
          if (peer_ep->connect_status != NDMIS_CONN_REMOTE) goto messy;
          peer_ep->connect_status = NDMIS_CONN_IDLE;
          break;
      }
      break;

    case NDMIS_CONN_ACCEPTED:
      switch (mine_ep->addr_type) {
        default:
          goto messy;

        case NDMP9_ADDR_LOCAL:
          if (peer_ep->connect_status != NDMIS_CONN_CONNECTED) goto messy;
          peer_ep->connect_status = NDMIS_CONN_DISCONNECTED;
          is->chan.eof = 1;
          if (mine_ep->transfer_mode == NDMCHAN_MODE_READ)
            is->chan.error = 1; /* EPIPE */
          break;

        case NDMP9_ADDR_TCP:
          ndmis_tcp_close(sess);
          if (peer_ep->connect_status != NDMIS_CONN_REMOTE) goto messy;
          peer_ep->connect_status = NDMIS_CONN_IDLE;
          break;
      }
      break;

    case NDMIS_CONN_CONNECTED:
      switch (mine_ep->addr_type) {
        default:
          goto messy;

        case NDMP9_ADDR_LOCAL:
          if (peer_ep->connect_status != NDMIS_CONN_ACCEPTED) goto messy;
          peer_ep->connect_status = NDMIS_CONN_DISCONNECTED;
          is->chan.eof = 1;
          if (mine_ep->transfer_mode == NDMCHAN_MODE_READ)
            is->chan.error = 1; /* EPIPE */
          break;

        case NDMP9_ADDR_TCP:
          ndmis_tcp_close(sess);
          if (peer_ep->connect_status != NDMIS_CONN_REMOTE) goto messy;
          peer_ep->connect_status = NDMIS_CONN_IDLE;
          break;
      }
      break;

    case NDMIS_CONN_DISCONNECTED: /* peer close()d first */
      ndmis_reinit_remote(sess);
      break;

    case NDMIS_CONN_CLOSED:
      goto messy;
  }

  NDMOS_MACRO_ZEROFILL(mine_ep);
  mine_ep->name = save_name;

  return 0;

messy:
  ndmalogf(sess, 0, 2, "close %s messy mcs=%d pcs=%d", mine_ep->name,
           mine_ep->connect_status, peer_ep->connect_status);
  NDMOS_MACRO_ZEROFILL(mine_ep);
  mine_ep->name = save_name;
  return -1;
}

/*
 * ADDR_TCP helper routines
 ****************************************************************
 */


/*
 * ndmis_tcp_listen()
 *
 * The tricky part of listen()ing is determining the IP
 * address to offer, which ultimately will be used by
 * the other (peer) side for connect()ing.
 *
 * We can't just bind() with INADDR_ANY (0's) because
 * that results in a local socket with INADDR_ANY, and
 * any inbound connection to the right port will be
 * accept()ed by the networking system. That doesn't
 * help us here, though, because we have to have a
 * real IP address to offer. INADDR_ANY ain't sufficient.
 *
 * There is also the issue of systems with multiple
 * network connections. We of course would like to
 * use the network data link that is most advantageous
 * on both sides. This may vary from job run to job
 * run, and so any method of specifying just one
 * is IP address ain't sufficient.
 *
 * The approach here uses the existing control connections,
 * which normally precede the image stream connection,
 * as cues for which IP address to use. So, for example,
 * if a TAPE or DATA host has four network connections,
 * the CONTROL agent can coax them to use a specific one
 * of the four by connecting to the IP address of the
 * network wanted for the image stream.
 *
 * If the clever rules don't work out, the fallback is to
 * look up the host name. Right now we use ndmhost_lookup()
 * of sess->local_info.host_name because both must work
 * before things would progress to this point.
 */

int ndmis_tcp_listen(struct ndm_session* sess, struct ndmp9_addr* listen_addr)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  ndmp9_tcp_addr* tcp_addr = &listen_addr->ndmp9_addr_u.tcp_addr;
  struct ndmconn* conn;
  struct sockaddr c_sa;
  struct sockaddr l_sa;
  struct sockaddr_in* sin;
  socklen_t len;
  int listen_sock = -1;
  char* what = "???";

  /*
   * Get the IP address thru which the CONTROL agent connected
   * to this session. The CONTROL agent may influence the
   * network used for the image-stream on multi-homed hosts
   * simply by connecting to the prefered IP address.
   */
  what = "determine-conn";
  conn = sess->plumb.control;
  if (!conn || conn->conn_type != NDMCONN_TYPE_REMOTE) {
    /*
     * If CONTROL is resident, try the other
     * control connections in hopes of finding
     * a clue about what IP address to offer.
     */
    conn = sess->plumb.data;
    if (!conn || conn->conn_type != NDMCONN_TYPE_REMOTE) {
      conn = sess->plumb.tape;
      if (!conn || conn->conn_type != NDMCONN_TYPE_REMOTE) { conn = 0; }
    }
  }

  if (conn) {
    /*
     * We found a connection to use for determining
     * what IP address to offer.
     */
    len = sizeof c_sa;
    if (getsockname(ndmconn_fileno(conn), &c_sa, &len) < 0) {
      /* we'll try the fallback rules */
      conn = 0;
    }
  }

  if (!conn) {
    /*
     * For whatever reason, we can't determine a good
     * IP address from the connections. Try the boring
     * fallback rules.
     */
    ndmos_sync_config_info(sess);

    sin = (struct sockaddr_in*)&c_sa;

    what = "ndmhost_lookup";
    if (ndmhost_lookup(sess->config_info->hostname, sin) != 0) goto fail;
  }

  /* c_sa is a sockaddr_in for the IP address to use */

  what = "socket";
  listen_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sock < 0) goto fail;

  /* could bind() to more restrictive addr based on c_sa */
  NDMOS_MACRO_SET_SOCKADDR(&l_sa, 0, 0);
  what = "bind";
  if (bind(listen_sock, &l_sa, sizeof l_sa) < 0) goto fail;

  what = "listen";
  if (listen(listen_sock, 1) < 0) goto fail;

  ndmos_condition_listen_socket(sess, listen_sock);

  /* Get the port */
  what = "getsockname-listen";
  len = sizeof l_sa;
  if (getsockname(listen_sock, &l_sa, &len) < 0) goto fail;

  // Fill in the return address

  listen_addr->addr_type = NDMP9_ADDR_TCP;
  tcp_addr = &listen_addr->ndmp9_addr_u.tcp_addr;

  /* IP addr from CONTROL connection, or where ever c_sa came from */
  sin = (struct sockaddr_in*)&c_sa;
  tcp_addr->ip_addr = ntohl(sin->sin_addr.s_addr);

  /* port from the bind() and getsockname() above */
  sin = (struct sockaddr_in*)&l_sa;
  tcp_addr->port = ntohs(sin->sin_port);

  // Start the listen channel

  ndmchan_start_listen(&is->remote.listen_chan, listen_sock);

  is->remote.connect_status = NDMIS_CONN_LISTEN;
  is->remote.listen_addr = *listen_addr;

  return 0;

fail:
  ndmalogf(sess, 0, 2, "ndmis_tcp_listen(): %s failed", what);
  if (listen_sock >= 0) close(listen_sock);

  return -1;
}

int ndmis_tcp_accept(struct ndm_session* sess)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  char* what = "???";
  ndmp9_tcp_addr* tcp_addr;
  struct sockaddr sa;
  struct sockaddr_in* sin = (struct sockaddr_in*)&sa;
  socklen_t len;
  int accept_sock = -1;

  what = "remote-conn-stat";
  if (is->remote.connect_status != NDMIS_CONN_LISTEN) goto fail;

  what = "remote-list-ready";
  if (!is->remote.listen_chan.ready) goto fail;

  what = "accept";
  len = sizeof sa;
  accept_sock = accept(is->remote.listen_chan.fd, &sa, &len);

  ndmchan_cleanup(&is->remote.listen_chan);

  if (accept_sock < 0) {
    is->remote.connect_status = NDMIS_CONN_BOTCHED;
    goto fail;
  }

  /* write what we know, ndmis...addrs() will update if possible */
  is->remote.peer_addr.addr_type = NDMP9_ADDR_TCP;
  tcp_addr = &is->remote.peer_addr.ndmp9_addr_u.tcp_addr;
  tcp_addr->ip_addr = ntohl(sin->sin_addr.s_addr);
  tcp_addr->port = ntohs(sin->sin_port);

  ndmis_tcp_green_light(sess, accept_sock, NDMIS_CONN_ACCEPTED);

  return 0;

fail:
  ndmalogf(sess, 0, 2, "ndmis_tcp_accept(): %s failed", what);

  return -1;
}

int ndmis_tcp_connect(struct ndm_session* sess, struct ndmp9_addr* connect_addr)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  ndmp9_tcp_addr* tcp_addr = &connect_addr->ndmp9_addr_u.tcp_addr;
  char* what = "???";
  struct sockaddr sa;
  int connect_sock;

  NDMOS_MACRO_SET_SOCKADDR(&sa, tcp_addr->ip_addr, tcp_addr->port);

  what = "socket";
  connect_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (connect_sock < 0) goto fail;

  what = "connect";
  if (connect(connect_sock, &sa, sizeof sa) < 0) goto fail;


  /* write what we know, ndmis...addrs() will update if possible */
  is->remote.peer_addr = *connect_addr;

  ndmis_tcp_green_light(sess, connect_sock, NDMIS_CONN_CONNECTED);

  return 0;

fail:
  ndmalogf(sess, 0, 2, "ndmis_tcp_connect(): %s failed", what);
  if (connect_sock >= 0) close(connect_sock);

  return -1;
}

int ndmis_tcp_green_light(struct ndm_session* sess,
                          int sock,
                          ndmis_connect_status new_status)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;

  ndmos_condition_image_stream_socket(sess, sock);

  ndmchan_start_pending(&is->chan, sock);

  is->remote.connect_status = new_status;

  ndmis_tcp_get_local_and_peer_addrs(sess);

  return 0;
}

int ndmis_tcp_close(struct ndm_session* sess)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;

  switch (is->remote.connect_status) {
    case NDMIS_CONN_LISTEN:
      ndmchan_cleanup(&is->remote.listen_chan);
      break;

    case NDMIS_CONN_CONNECTED:
    case NDMIS_CONN_ACCEPTED:
      ndmchan_cleanup(&is->chan);
      break;

    default:
      break;
  }

  ndmis_reinit_remote(sess);

  return 0;
}

int ndmis_tcp_get_local_and_peer_addrs(struct ndm_session* sess)
{
  struct ndm_image_stream* is = sess->plumb.image_stream;
  char* what = "???";
  struct sockaddr sa;
  struct sockaddr_in* sin = (struct sockaddr_in*)&sa;
  ndmp9_tcp_addr* tcp_addr;
  socklen_t len;
  int rc = 0;

  len = sizeof sa;
  what = "getpeername";
  if (getpeername(is->chan.fd, &sa, &len) < 0) {
    /* this is best effort */
    ndmalogf(sess, 0, 2, "ndmis_tcp..._addrs(): %s failed", what);
    rc = -1;
  } else {
    is->remote.peer_addr.addr_type = NDMP9_ADDR_TCP;
    tcp_addr = &is->remote.peer_addr.ndmp9_addr_u.tcp_addr;
    tcp_addr->ip_addr = ntohl(sin->sin_addr.s_addr);
    tcp_addr->port = ntohs(sin->sin_port);
  }

  len = sizeof sa;
  what = "getsockname";
  if (getsockname(is->chan.fd, &sa, &len) < 0) {
    /* this is best effort */
    ndmalogf(sess, 0, 2, "ndmis_tcp..._addrs(): %s failed", what);
    rc = -1;
  } else {
    is->remote.local_addr.addr_type = NDMP9_ADDR_TCP;
    tcp_addr = &is->remote.peer_addr.ndmp9_addr_u.tcp_addr;
    tcp_addr->ip_addr = ntohl(sin->sin_addr.s_addr);
    tcp_addr->port = ntohs(sin->sin_port);
  }

  return rc;
}
