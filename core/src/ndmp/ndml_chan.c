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


#include "ndmlib.h"


// Initialize a channel. Make sure it won't be confused for active.
void ndmchan_initialize(struct ndmchan* ch, char* name)
{
  NDMOS_MACRO_ZEROFILL(ch);
  ch->name = name ? name : "???";
  ch->fd = -1;
  ch->mode = NDMCHAN_MODE_IDLE;
}

// Set the data buffer
int ndmchan_setbuf(struct ndmchan* ch, char* data, unsigned data_size)
{
  ch->data = data;
  ch->data_size = data_size;

  ch->beg_ix = 0;
  ch->end_ix = 0;

  return 0;
}


// Interfaces for starting a channel in various modes.
int ndmchan_start_mode(struct ndmchan* ch, int fd, int chan_mode)
{
  ch->fd = fd;
  ch->mode = chan_mode;
  return 0;
}

int ndmchan_start_read(struct ndmchan* ch, int fd)
{
  return ndmchan_start_mode(ch, fd, NDMCHAN_MODE_READ);
}

int ndmchan_start_write(struct ndmchan* ch, int fd)
{
  return ndmchan_start_mode(ch, fd, NDMCHAN_MODE_WRITE);
}

int ndmchan_start_readchk(struct ndmchan* ch, int fd)
{
  return ndmchan_start_mode(ch, fd, NDMCHAN_MODE_READCHK);
}

int ndmchan_start_listen(struct ndmchan* ch, int fd)
{
  return ndmchan_start_mode(ch, fd, NDMCHAN_MODE_LISTEN);
}

int ndmchan_start_resident(struct ndmchan* ch)
{
  return ndmchan_start_mode(ch, -1, NDMCHAN_MODE_RESIDENT);
}

int ndmchan_start_pending(struct ndmchan* ch, int fd)
{
  return ndmchan_start_mode(ch, fd, NDMCHAN_MODE_PENDING);
}

// Change a PENDING channel to an active (READ/WRITE) channel
int ndmchan_pending_to_mode(struct ndmchan* ch, int chan_mode)
{
  ch->mode = chan_mode;
  return 0;
}

int ndmchan_pending_to_read(struct ndmchan* ch)
{
  return ndmchan_pending_to_mode(ch, NDMCHAN_MODE_READ);
}

int ndmchan_pending_to_write(struct ndmchan* ch)
{
  return ndmchan_pending_to_mode(ch, NDMCHAN_MODE_WRITE);
}


/*
 * Interfaces for stopping (close()ing) a channel.
 * This is a bit of a hodge-podge. Could probably be cleaner.
 */

void ndmchan_set_eof(struct ndmchan* ch) { ch->eof = 1; }

void ndmchan_close_set_errno(struct ndmchan* ch, int err_no)
{
  ch->eof = 1;
  if (ch->fd >= 0) {
    close(ch->fd);
    ch->fd = -1;
  }
  ch->mode = NDMCHAN_MODE_CLOSED;
  ch->saved_errno = err_no;
  ch->beg_ix = ch->end_ix = 0;
}

void ndmchan_close(struct ndmchan* ch) { ndmchan_close_set_errno(ch, 0); }

void ndmchan_abort(struct ndmchan* ch)
{
  ndmchan_close_set_errno(ch, ch->saved_errno == 0 ? EINTR : ch->saved_errno);
}

void ndmchan_close_as_is(struct ndmchan* ch)
{
  ndmchan_close_set_errno(ch, ch->saved_errno);
}

void ndmchan_cleanup(struct ndmchan* ch)
{
  if (ch->mode != NDMCHAN_MODE_IDLE) { ndmchan_close_as_is(ch); }
}


/*
 * CPU Quantum for a set of channels. There are three
 * phases:
 * 1) Identify the channels to check for ready to do I/O.
 *    For example, a READ channel with no buffer space
 *    need not be checked.
 * 2) Call the OS dependent function that performs the
 *    actual select()/poll()/whatever.
 * 3) Based on the results, perform actual read()/write().
 *    EOF and errors are detected.
 *
 * This is constructed so that applications which can not use
 * ndmchan_quantum() directly have access to the helper functions
 * ndmchan_pre_poll() and ndmchan_post_poll().
 */

int ndmchan_quantum(struct ndmchan* chtab[], unsigned n_chtab, int milli_timo)
{
  int rc;

  ndmchan_pre_poll(chtab, n_chtab);

  rc = ndmos_chan_poll(chtab, n_chtab, milli_timo);
  if (rc <= 0) return rc;

  rc = ndmchan_post_poll(chtab, n_chtab);

  return rc;
}

int ndmchan_pre_poll(struct ndmchan* chtab[], unsigned n_chtab)
{
  struct ndmchan* ch;
  unsigned int i, n_check;

  n_check = 0;
  for (i = 0; i < n_chtab; i++) {
    ch = chtab[i];
    ch->ready = 0;
    ch->check = 0;

    if (ch->error) continue;

    switch (ch->mode) {
      default:
      case NDMCHAN_MODE_IDLE:
      case NDMCHAN_MODE_PENDING:
      case NDMCHAN_MODE_RESIDENT:
      case NDMCHAN_MODE_CLOSED:
        continue;

      case NDMCHAN_MODE_LISTEN:
      case NDMCHAN_MODE_READCHK:
        break;

      case NDMCHAN_MODE_READ:
        if (ch->eof) continue;
        if (ndmchan_n_avail(ch) == 0) continue;
        break;

      case NDMCHAN_MODE_WRITE:
        if (ndmchan_n_ready(ch) == 0) continue;
        break;
    }

    ch->check = 1;
    n_check++;
  }

  return n_check;
}

int ndmchan_post_poll(struct ndmchan* chtab[], unsigned n_chtab)
{
  struct ndmchan* ch;
  unsigned int i;
  int rc, len, n_ready;

  n_ready = 0;

  for (i = 0; i < n_chtab; i++) {
    ch = chtab[i];

    if (!ch->ready) continue;

    switch (ch->mode) {
      case NDMCHAN_MODE_READ:
        len = ndmchan_n_avail(ch);
        if (len <= 0) continue;

        n_ready++;
        rc = read(ch->fd, &ch->data[ch->end_ix], len);
        if (rc < 0) {
          if (errno != NDMOS_CONST_EWOULDBLOCK) {
            ch->error = ch->eof = 1;
            ch->saved_errno = errno;
            if (!ch->saved_errno) ch->saved_errno = -1;
          } else {
            /* no bytes read */
          }
        } else if (rc == 0) {
          ch->eof = 1;
          ch->error = 0;
          ch->saved_errno = 0;
        } else {
          ch->end_ix += rc;
        }
        break;

      case NDMCHAN_MODE_WRITE:
        len = ndmchan_n_ready(ch);
        if (len <= 0) continue;

        n_ready++;
        rc = write(ch->fd, &ch->data[ch->beg_ix], len);
        if (rc < 0) {
          if (errno != NDMOS_CONST_EWOULDBLOCK) {
            ch->eof = 1;
            ch->error = 1;
            ch->saved_errno = errno;
            if (!ch->saved_errno) ch->saved_errno = -1;
          } else {
            /* no bytes written */
            /* EWOULDBLOCK but ready? */
          }
        } else if (rc == 0) {
          /* NDMOS_CONST_EWOULDBLOCK? */
          ch->eof = 1;
          ch->error = 1;
          ch->saved_errno = 0;
        } else {
          ch->beg_ix += rc;
        }
        break;
    }
  }

  return n_ready;
}


// Channel data buffer space manipulation.

void ndmchan_compress(struct ndmchan* ch)
{
  unsigned len = ch->end_ix - ch->beg_ix;

  if (ch->beg_ix > 0 && len > 0) {
    bcopy(&ch->data[ch->beg_ix], ch->data, len);
  } else {
    if (len > ch->data_size) len = 0;
  }
  ch->beg_ix = 0;
  ch->end_ix = len;
}

int ndmchan_n_avail(struct ndmchan* ch)
{
  if (ch->end_ix == ch->beg_ix) ch->end_ix = ch->beg_ix = 0;

  if (ch->end_ix >= ch->data_size) { ndmchan_compress(ch); }
  return ch->data_size - ch->end_ix;
}

int ndmchan_n_avail_record(struct ndmchan* ch, uint32_t size)
{
  if (ch->end_ix == ch->beg_ix) ch->end_ix = ch->beg_ix = 0;

  if (ch->end_ix >= ch->data_size - size) { ndmchan_compress(ch); }
  return ch->data_size - ch->end_ix;
}

int ndmchan_n_avail_total(struct ndmchan* ch)
{
  if (ch->end_ix == ch->beg_ix) ch->end_ix = ch->beg_ix = 0;

  if (ch->end_ix >= ch->data_size) { ndmchan_compress(ch); }
  return ch->data_size - ch->end_ix + ch->beg_ix;
}

int ndmchan_n_ready(struct ndmchan* ch) { return ch->end_ix - ch->beg_ix; }


// Interfaces for interpreting channel state, obtaining pointers, lengths

enum ndmchan_read_interpretation ndmchan_read_interpret(struct ndmchan* ch,
                                                        char** data_p,
                                                        unsigned* n_ready_p)
{
  unsigned n_ready;

  n_ready = *n_ready_p = ndmchan_n_ready(ch);
  *data_p = &ch->data[ch->beg_ix];

  if (ch->error) {
    if (n_ready == 0) {
      return NDMCHAN_RI_DONE_ERROR;
    } else {
      return NDMCHAN_RI_DRAIN_ERROR;
    }
  }

  if (ch->eof) {
    if (n_ready == 0) {
      return NDMCHAN_RI_DONE_EOF;
    } else {
      return NDMCHAN_RI_DRAIN_EOF;
    }
  }

  if (n_ready == 0) { return NDMCHAN_RI_EMPTY; }

  if (n_ready == ch->data_size) { return NDMCHAN_RI_READY_FULL; }

  return NDMCHAN_RI_READY;
}

enum ndmchan_write_interpretation ndmchan_write_interpret(struct ndmchan* ch,
                                                          char** data_p,
                                                          unsigned* n_avail_p)
{
  unsigned n_avail;

  n_avail = *n_avail_p = ndmchan_n_avail(ch);
  *data_p = &ch->data[ch->end_ix];

  if (ch->error) {
    /* We don't use WI_DRAIN_ERROR. If it's kaput, it's kaput */
    return NDMCHAN_WI_DONE_ERROR;
  }

  if (ch->eof) {
    if (n_avail == ch->data_size) {
      return NDMCHAN_WI_DONE_EOF;
    } else {
      return NDMCHAN_WI_DRAIN_EOF;
    }
  }

  if (n_avail == 0) { return NDMCHAN_WI_FULL; }

  if (n_avail == ch->data_size) { return NDMCHAN_WI_AVAIL_EMPTY; }

  return NDMCHAN_WI_AVAIL;
}


// Pretty printer
void ndmchan_pp(struct ndmchan* ch, char* buf)
{
  int show_ra = 0;
  char* bp = buf;
  char* p;

  sprintf(bp, "name=%s", ch->name);
  while (*bp) bp++;

  switch (ch->mode) {
    case NDMCHAN_MODE_IDLE:
      p = "idle";
      break;
    case NDMCHAN_MODE_RESIDENT:
      p = "resident";
      show_ra = 1;
      break;
    case NDMCHAN_MODE_READ:
      p = "read";
      show_ra = 1;
      break;
    case NDMCHAN_MODE_WRITE:
      p = "write";
      show_ra = 1;
      break;
    case NDMCHAN_MODE_READCHK:
      p = "readchk";
      break;
    case NDMCHAN_MODE_LISTEN:
      p = "listen";
      break;
    case NDMCHAN_MODE_PENDING:
      p = "pending";
      break;
    case NDMCHAN_MODE_CLOSED:
      p = "closed";
      break;
    default:
      p = "mode=???";
      break;
  }

  sprintf(bp, " %s ", p);
  while (*bp) bp++;

  if (show_ra) {
    sprintf(bp, "ready=%d avail=%d ", ndmchan_n_ready(ch), ndmchan_n_avail(ch));
    while (*bp) bp++;
  }

  if (ch->ready) strcat(bp, "-rdy");
  if (ch->check) strcat(bp, "-chk");
  if (ch->eof) strcat(bp, "-eof");
  if (ch->error) strcat(bp, "-err");
}


#ifdef NDMOS_OPTION_USE_SELECT_FOR_CHAN_POLL
// Here because it is almost always used

int ndmos_chan_poll(struct ndmchan* chtab[], unsigned n_chtab, int milli_timo)
{
  struct ndmchan* ch;
  fd_set rfds, wfds;
  int nfd = 0, rc;
  unsigned i;
  struct timeval timo;

  FD_ZERO(&rfds);
  FD_ZERO(&wfds);

  timo.tv_sec = milli_timo / 1000;
  timo.tv_usec = (milli_timo % 1000) * 1000;

  for (i = 0; i < n_chtab; i++) {
    ch = chtab[i];
    if (!ch->check) continue;

    switch (ch->mode) {
      case NDMCHAN_MODE_LISTEN:
      case NDMCHAN_MODE_READCHK:
      case NDMCHAN_MODE_READ:
        FD_SET(ch->fd, &rfds);
        break;

      case NDMCHAN_MODE_WRITE:
        FD_SET(ch->fd, &wfds);
        break;
    }
    if (nfd < ch->fd + 1) nfd = ch->fd + 1;
  }

  rc = select(nfd, &rfds, &wfds, (void*)0, &timo);
  if (rc <= 0) return rc;

  for (i = 0; i < n_chtab; i++) {
    ch = chtab[i];
    if (!ch->check) continue;

    switch (ch->mode) {
      case NDMCHAN_MODE_LISTEN:
      case NDMCHAN_MODE_READCHK:
      case NDMCHAN_MODE_READ:
        if (FD_ISSET(ch->fd, &rfds)) ch->ready = 1;
        break;

      case NDMCHAN_MODE_WRITE:
        if (FD_ISSET(ch->fd, &wfds)) ch->ready = 1;
        break;
    }
  }

  return rc;
}

#endif /* NDMOS_OPTION_USE_SELECT_FOR_CHAN_POLL */

#ifdef NDMOS_OPTION_USE_POLL_FOR_CHAN_POLL
/*
 * Here because it is common, and because poll(2) is
 * INFINITELY SUPERIOR to select(2).
 */

#ifdef HAVE_POLL
#include <poll.h>
#endif

int ndmos_chan_poll(struct ndmchan* chtab[], unsigned n_chtab, int milli_timo)
{
  struct ndmchan* ch;
  struct pollfd* pfdtab;
  int n_pfdtab;
  int rc, i;

  /*
   * See how many filedescriptors we need to check.
   */
  n_pfdtab = 0;
  for (i = 0; i < n_chtab; i++) {
    if (!chtab[i]->check) continue;

    n_pfdtab++;
  }

  pfdtab = (struct pollfd*)NDMOS_API_MALLOC(sizeof(struct pollfd) * n_pfdtab);
  if (!pfdtab) return -1;
  NDMOS_API_BZERO(pfdtab, sizeof(struct pollfd) * n_pfdtab);

  n_pfdtab = 0;
  for (i = 0; i < n_chtab; i++) {
    ch = chtab[i];
    if (!ch->check) continue;

    switch (ch->mode) {
      case NDMCHAN_MODE_LISTEN:
      case NDMCHAN_MODE_READCHK:
      case NDMCHAN_MODE_READ:
        pfdtab[n_pfdtab].fd = ch->fd;
        pfdtab[n_pfdtab].events = POLLIN;
        break;

      case NDMCHAN_MODE_WRITE:
        pfdtab[n_pfdtab].fd = ch->fd;
        pfdtab[n_pfdtab].events = POLLOUT;
        break;
    }
    n_pfdtab++;
  }

  rc = poll(pfdtab, n_pfdtab, milli_timo);
  if (rc <= 0) {
    NDMOS_API_FREE(pfdtab);
    return rc;
  }

  n_pfdtab = 0;
  for (i = 0; i < n_chtab; i++) {
    ch = chtab[i];
    if (!ch->check) continue;

    switch (ch->mode) {
      case NDMCHAN_MODE_LISTEN:
      case NDMCHAN_MODE_READCHK:
      case NDMCHAN_MODE_READ:
        if (pfdtab[n_pfdtab].revents & POLLIN) ch->ready = 1;
        if (pfdtab[n_pfdtab].revents & POLLHUP) ch->eof = 1;
        break;

      case NDMCHAN_MODE_WRITE:
        if (pfdtab[n_pfdtab].revents & POLLOUT) ch->ready = 1;
        break;
    }
    n_pfdtab++;
  }

  NDMOS_API_FREE(pfdtab);

  return rc;
}

#endif /* NDMOS_OPTION_USE_POLL_FOR_CHAN_POLL */
