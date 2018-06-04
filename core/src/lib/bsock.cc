/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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
/**
 * Network Utility Routines
 *
 * Kern Sibbald
 */

#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/bnet.h"
#include "lib/cram_md5.h"
#include "lib/util.h"
#include "lib/tls_openssl.h"

BareosSocket::BareosSocket() : tls_conn(nullptr) {
   Dmsg0(100, "Contruct BareosSocket\n");
   fd_            = -1;
   spool_fd_      = -1;
   msg             = GetPoolMemory(PM_BSOCK);
   errmsg          = GetPoolMemory(PM_MESSAGE);
   blocking_      = true;
   use_keepalive_ = true;
}

BareosSocket::~BareosSocket() {
   Dmsg0(100, "Destruct BareosSocket\n");
   // FreeTls();
}

void BareosSocket::FreeTls()
{
   if (tls_conn != nullptr) {
      FreeTlsConnection(tls_conn);
      tls_conn = nullptr;
   }
}

/**
 * Copy the address from the configuration dlist that gets passed in
 */
void BareosSocket::SetSourceAddress(dlist *src_addr_list)
{
   char allbuf[256 * 10];
   IPADDR *addr = NULL;

   Dmsg1(100, "All source addresses %s\n",
         BuildAddressesString(src_addr_list, allbuf, sizeof(allbuf)));

   /*
    * Delete the object we already have, if it's allocated
    */
   if (src_addr) {
     free( src_addr);
     src_addr = NULL;
   }

   if (src_addr_list) {
     addr = (IPADDR*) src_addr_list->first();
     src_addr = New(IPADDR(*addr));
   }
}

/**
 * Force read/write to use locking
 */
bool BareosSocket::SetLocking()
{
   int status;
   if (use_locking_) {
      return true;                      /* already set */
   }
   if ((status = pthread_mutex_init(&mutex_, NULL)) != 0) {
      BErrNo be;
      Qmsg(jcr_, M_FATAL, 0, _("Could not init bsock mutex. ERR=%s\n"),
         be.bstrerror(status));
      return false;
   }
   use_locking_ = true;
   return true;
}

void BareosSocket::ClearLocking()
{
   if (!use_locking_) {
      return;
   }
   use_locking_ = false;
   pthread_mutex_destroy(&mutex_);
   return;
}

/**
 * Send a signal
 */
bool BareosSocket::signal(int signal)
{
   message_length = signal;
   if (signal == BNET_TERMINATE) {
      suppress_error_msgs_ = true;
   }
   return send();
}

/**
 * Despool spooled attributes
 */
bool BareosSocket::despool(void UpdateAttrSpoolSize(ssize_t size), ssize_t tsize)
{
   int32_t pktsiz;
   size_t nbytes;
   ssize_t last = 0, size = 0;
   int count = 0;
   JobControlRecord *jcr = get_jcr();

   if (lseek(spool_fd_, 0, SEEK_SET) == -1) {
      Qmsg(jcr, M_FATAL, 0, _("attr spool I/O error.\n"));
      return false;
   }

#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
   posix_fadvise(spool_fd_, 0, 0, POSIX_FADV_WILLNEED);
#endif

   while ((nbytes = read(spool_fd_, (char *)&pktsiz, sizeof(int32_t))) == sizeof(int32_t)) {
      size += sizeof(int32_t);
      message_length = ntohl(pktsiz);
      if (message_length > 0) {
         if (message_length > (int32_t)SizeofPoolMemory(msg)) {
            msg = ReallocPoolMemory(msg, message_length + 1);
         }

         nbytes = read(spool_fd_, msg, message_length);
         if (nbytes != (size_t)message_length) {
            BErrNo be;
            Dmsg2(400, "nbytes=%d message_length=%d\n", nbytes, message_length);
            Qmsg1(get_jcr(), M_FATAL, 0, _("read attr spool error. ERR=%s\n"), be.bstrerror());
            UpdateAttrSpoolSize(tsize - last);
            return false;
         }

         size += nbytes;
         if ((++count & 0x3F) == 0) {
            UpdateAttrSpoolSize(size - last);
            last = size;
         }
      }

      send();
      if (jcr && JobCanceled(jcr)) {
         return false;
      }
   }
   UpdateAttrSpoolSize(tsize - last);

   return true;
}

/**
 * Return the string for the error that occurred
 * on the socket. Only the first error is retained.
 */
const char *BareosSocket::bstrerror()
{
   BErrNo be;
   if (errmsg == NULL) {
      errmsg = GetPoolMemory(PM_MESSAGE);
   }
   PmStrcpy(errmsg, be.bstrerror(b_errno));
   return errmsg;
}

/**
 * Format and send a message
 * Returns: false on error
 *          true  on success
 */
bool BareosSocket::fsend(const char *fmt, ...)
{
   va_list arg_ptr;
   int maxlen;

   if (errors || IsTerminated()) {
      return false;
   }
   /* This probably won't work, but we vsnprintf, then if we
    * get a negative length or a length greater than our buffer
    * (depending on which library is used), the printf was truncated, so
    * get a bigger buffer and try again.
    */
   for (;;) {
      maxlen = SizeofPoolMemory(msg) - 1;
      va_start(arg_ptr, fmt);
      message_length = Bvsnprintf(msg, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (message_length >= 0 && message_length < (maxlen - 5)) {
         break;
      }
      msg = ReallocPoolMemory(msg, maxlen + maxlen / 2);
   }
   return send();
}

void BareosSocket::SetKillable(bool killable)
{
   if (jcr_) {
      jcr_->SetKillable(killable);
   }
}

/** Commands sent to Director */
static char hello[] =
   "Hello %s calling\n";

/** Response from Director */
static char OKhello[] =
   "1000 OK:";

/**
 * Authenticate with Director
 */
bool BareosSocket::AuthenticateWithDirector(JobControlRecord *jcr,
                                       const char *identity,
                                       s_password &password,
                                       char *response,
                                       int response_len,
                                       TlsResource *tls_configuration) {
   char bashed_name[MAX_NAME_LENGTH];
   BareosSocket *dir = this;        /* for readability */

   response[0] = 0;

   /*
    * Send my name to the Director then do authentication
    */
   bstrncpy(bashed_name, identity, sizeof(bashed_name));
   BashSpaces(bashed_name);

   /*
    * Timeout Hello after 5 mins
    */
   dir->StartTimer(60 * 5);
   dir->fsend(hello, bashed_name);

   if (!AuthenticateOutboundConnection(jcr, "Director", identity, password, tls_configuration)) {
      goto bail_out;
   }

   Dmsg1(6, ">dird: %s", dir->msg);
   if (dir->recv() <= 0) {
      dir->StopTimer();
      Bsnprintf(response, response_len, _("Bad response to Hello command: ERR=%s\n"
                                          "The Director at \"%s:%d\" is probably not running.\n"),
                dir->bstrerror(), dir->host(), dir->port());
      return false;
   }

   dir->StopTimer();
   Dmsg1(10, "<dird: %s", dir->msg);
   if (!bstrncmp(dir->msg, OKhello, sizeof(OKhello) - 1)) {
      Bsnprintf(response, response_len, _("Director at \"%s:%d\" rejected Hello command\n"),
                dir->host(), dir->port());
      return false;
   } else {
      Bsnprintf(response, response_len, "%s", dir->msg);
   }

   return true;

bail_out:
   dir->StopTimer();
   Bsnprintf(response, response_len, _("Authorization problem with Director at \"%s:%d\"\n"
                                       "Most likely the passwords do not agree.\n"
                                       "If you are using TLS, there may have been a certificate "
                                       "validation error during the TLS handshake.\n"
                                       "Please see %s for help.\n"),
             dir->host(), dir->port(), MANUAL_AUTH_URL);

   return false;
}

/**
 * Depending on the initiate parameter perform one of the following:
 *
 * - First make him prove his identity and then prove our identity to the Remote.
 * - First prove our identity to the Remote and then make him prove his identity.
 */
bool BareosSocket::two_way_authenticate(JobControlRecord *jcr,
                                 const char *what,
                                 const char *identity,
                                 s_password &password,
                                 TlsResource *tls_configuration,
                                 bool initiated_by_remote)
{
   const int debuglevel    = 50;
   bool auth_success  = false;

   btimer_t *tid = nullptr;
   TlsBase *selected_local_tls = nullptr;
   uint32_t local_tls_policy = GetLocalTlsPolicyFromConfiguration(tls_configuration);
   CramMd5Handshake cram_md5_handshake(this, password.value, local_tls_policy);

   if (jcr && JobCanceled(jcr)) {
      Dmsg0(debuglevel, "Failed, because job is canceled.\n");
      goto auth_fatal;
   }

   if (password.encoding != p_encoding_md5) {
      Jmsg(jcr, M_FATAL, 0, _("Password encoding is not MD5. You are probably restoring a NDMP Backup "
                              "with a restore job not configured for NDMP protocol.\n"));
      goto auth_fatal;
   }

   tid = StartBsockTimer(this, AUTH_TIMEOUT);

   auth_success = cram_md5_handshake.DoHandshake(initiated_by_remote);

   if (!auth_success) {
      Jmsg(jcr, M_FATAL, 0,
           _("Authorization key rejected by %s %s.\n"
             "Please see %s for help.\n"),
           what, identity, MANUAL_AUTH_URL);
      goto auth_fatal;
   }

   if (jcr && JobCanceled(jcr)) {
      Dmsg0(debuglevel, "Failed, because job is canceled.\n");
      auth_success = false; /* force quick exit */
      goto auth_fatal;
   }

   selected_local_tls = SelectTlsFromPolicy(tls_configuration, cram_md5_handshake.RemoteTlsPolicy());
   if (selected_local_tls != nullptr) {
      if (initiated_by_remote) {
         std::shared_ptr<TLS_CONTEXT> tls_ctx = selected_local_tls->CreateServerContext(
             std::make_shared<PskCredentials>(identity, password.value));
         if (jcr) {
            jcr->tls_ctx = tls_ctx;
         }
         alist *verify_list = NULL;
         if (selected_local_tls->GetVerifyPeer()) {
            verify_list = selected_local_tls->GetVerifyList();
         }
         if (!BnetTlsServer(tls_ctx, this, verify_list)) {
            Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
            Dmsg0(debuglevel, "TLS negotiation failed.\n");
            auth_success = false;
            goto auth_fatal;
         }
      } else {
         std::shared_ptr<TLS_CONTEXT> tls_ctx = selected_local_tls->CreateClientContext(
             std::make_shared<PskCredentials>(identity, password.value));
         if (jcr) {
            jcr->tls_ctx = tls_ctx;
         }
         if (!BnetTlsClient(tls_ctx,
                              this,
                              selected_local_tls->GetVerifyPeer(),
                              selected_local_tls->GetVerifyList())) {
            Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
            Dmsg0(debuglevel, "TLS negotiation failed.\n");
            auth_success = false;
            goto auth_fatal;
         }
      }

      if (selected_local_tls->GetAuthenticate()) { /* tls authentication only? */
         FreeTls();          /* yes, shutdown tls */
      }
   }
   if (!initiated_by_remote) {
      TlsLogConninfo(jcr, GetTlsConnection(), host(), port(), who());
   }
auth_fatal:
   if (tid) {
      StopBsockTimer(tid);
      tid = NULL;
   }

   if (jcr) {
      jcr->authenticated = auth_success;
   }

   return auth_success;
}

bool BareosSocket::AuthenticateOutboundConnection(
   JobControlRecord *jcr,
   const char *what,
   const char *identity,
   s_password &password,
   TlsResource *tls_configuration) {
   return two_way_authenticate(jcr, what, identity, password, tls_configuration, false);
}

bool BareosSocket::AuthenticateInboundConnection(
   JobControlRecord *jcr,
   const char *what,
   const char *identity,
   s_password &password,
   TlsResource *tls_configuration) {
   return two_way_authenticate(jcr, what, identity, password, tls_configuration, true);
}


/**
 * Try to limit the bandwidth of a network connection
 */
void BareosSocket::ControlBwlimit(int bytes)
{
   btime_t now, temp;
   int64_t usec_sleep;

   /*
    * If nothing written or read nothing todo.
    */
   if (bytes == 0) {
      return;
   }

   /*
    * See if this is the first time we enter here.
    */
   now = GetCurrentBtime();
   if (last_tick_ == 0) {
      nb_bytes_ = bytes;
      last_tick_ = now;
      return;
   }

   /*
    * Calculate the number of microseconds since the last check.
    */
   temp = now - last_tick_;

   /*
    * Less than 0.1ms since the last call, see the next time
    */
   if (temp < 100) {
      nb_bytes_ += bytes;
      return;
   }

   /*
    * Keep track of how many bytes are written in this timeslice.
    */
   nb_bytes_ += bytes;
   last_tick_ = now;
   if (debug_level >= 400) {
      Dmsg3(400, "ControlBwlimit: now = %lld, since = %lld, nb_bytes = %d\n", now, temp, nb_bytes_);
   }

   /*
    * Take care of clock problems (>10s)
    */
   if (temp > 10000000) {
      return;
   }

   /*
    * Remove what was authorised to be written in temp usecs.
    */
   nb_bytes_ -= (int64_t)(temp * ((double)bwlimit_ / 1000000.0));
   if (nb_bytes_ < 0) {
      /*
       * If more was authorized then used but bursting is not enabled
       * reset the counter as these bytes cannot be used later on when
       * we are exceeding our bandwidth.
       */
      if (!use_bursting_) {
         nb_bytes_ = 0;
      }
      return;
   }

   /*
    * What exceed should be converted in sleep time
    */
   usec_sleep = (int64_t)(nb_bytes_ /((double)bwlimit_ / 1000000.0));
   if (usec_sleep > 100) {
      if (debug_level >= 400) {
         Dmsg1(400, "ControlBwlimit: sleeping for %lld usecs\n", usec_sleep);
      }

      /*
       * Sleep the right number of usecs.
       */
      while (1) {
         Bmicrosleep(0, usec_sleep);
         now = GetCurrentBtime();

         /*
          * See if we slept enough or that Bmicrosleep() returned early.
          */
         if ((now - last_tick_) < usec_sleep) {
            usec_sleep -= (now - last_tick_);
            continue;
         } else {
            last_tick_ = now;
            break;
         }
      }

      /*
       * Subtract the number of bytes we could have sent during the sleep
       * time given the bandwidth limit set. We only do this when we are
       * allowed to burst e.g. use unused bytes from previous timeslices
       * to get an overall bandwidth limiting which may sometimes be below
       * the bandwidth and sometimes above it but the average will be near
       * the set bandwidth.
       */
      if (use_bursting_) {
         nb_bytes_ -= (int64_t)(usec_sleep * ((double)bwlimit_ / 1000000.0));
      } else {
         nb_bytes_ = 0;
      }
   }
}
