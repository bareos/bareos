/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Network Utility Routines
 *
 * Kern Sibbald
 */

#include "bareos.h"
#include "jcr.h"

BSOCK::BSOCK()
{
   m_fd = -1;
   m_spool_fd = -1;
   msg = get_pool_memory(PM_BSOCK);
   errmsg = get_pool_memory(PM_MESSAGE);
   m_blocking = true;
   m_use_keepalive = true;
}

BSOCK::~BSOCK()
{
}

/*
 * This is our "class destructor" that ensures that we use
 * smartalloc rather than the system free().
 */
void BSOCK::free_bsock()
{
   destroy();
}

void BSOCK::free_tls()
{
   free_tls_connection(this->tls_conn);
   this->tls_conn = NULL;
}

/*
 * Copy the address from the configuration dlist that gets passed in
 */
void BSOCK::set_source_address(dlist *src_addr_list)
{
   char allbuf[256 * 10];
   IPADDR *addr = NULL;

   Dmsg1(100, "All source addresses %s\n",
         build_addresses_str(src_addr_list, allbuf, sizeof(allbuf)));

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

/*
 * Force read/write to use locking
 */
bool BSOCK::set_locking()
{
   int status;
   if (m_use_locking) {
      return true;                      /* already set */
   }
   if ((status = pthread_mutex_init(&m_mutex, NULL)) != 0) {
      berrno be;
      Qmsg(m_jcr, M_FATAL, 0, _("Could not init bsock mutex. ERR=%s\n"),
         be.bstrerror(status));
      return false;
   }
   m_use_locking = true;
   return true;
}

void BSOCK::clear_locking()
{
   if (!m_use_locking) {
      return;
   }
   m_use_locking = false;
   pthread_mutex_destroy(&m_mutex);
   return;
}

/*
 * Send a signal
 */
bool BSOCK::signal(int signal)
{
   msglen = signal;
   if (signal == BNET_TERMINATE) {
      m_suppress_error_msgs = true;
   }
   return send();
}

/*
 * Despool spooled attributes
 */
bool BSOCK::despool(void update_attr_spool_size(ssize_t size), ssize_t tsize)
{
   int32_t pktsiz;
   size_t nbytes;
   ssize_t last = 0, size = 0;
   int count = 0;
   JCR *jcr = get_jcr();

   if (lseek(m_spool_fd, 0, SEEK_SET) == -1) {
      Qmsg(jcr, M_FATAL, 0, _("attr spool I/O error.\n"));
      return false;
   }

#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
   posix_fadvise(m_spool_fd, 0, 0, POSIX_FADV_WILLNEED);
#endif

   while ((nbytes = read(m_spool_fd, (char *)&pktsiz, sizeof(int32_t))) == sizeof(int32_t)) {
      size += sizeof(int32_t);
      msglen = ntohl(pktsiz);
      if (msglen > 0) {
         if (msglen > (int32_t)sizeof_pool_memory(msg)) {
            msg = realloc_pool_memory(msg, msglen + 1);
         }

         nbytes = read(m_spool_fd, msg, msglen);
         if (nbytes != (size_t)msglen) {
            berrno be;
            Dmsg2(400, "nbytes=%d msglen=%d\n", nbytes, msglen);
            Qmsg1(get_jcr(), M_FATAL, 0, _("read attr spool error. ERR=%s\n"), be.bstrerror());
            update_attr_spool_size(tsize - last);
            return false;
         }

         size += nbytes;
         if ((++count & 0x3F) == 0) {
            update_attr_spool_size(size - last);
            last = size;
         }
      }

      send();
      if (jcr && job_canceled(jcr)) {
         return false;
      }
   }
   update_attr_spool_size(tsize - last);

   return true;
}

/*
 * Return the string for the error that occurred
 * on the socket. Only the first error is retained.
 */
const char *BSOCK::bstrerror()
{
   berrno be;
   if (errmsg == NULL) {
      errmsg = get_pool_memory(PM_MESSAGE);
   }
   pm_strcpy(errmsg, be.bstrerror(b_errno));
   return errmsg;
}

/*
 * Format and send a message
 * Returns: false on error
 *          true  on success
 */
bool BSOCK::fsend(const char *fmt, ...)
{
   va_list arg_ptr;
   int maxlen;

   if (errors || is_terminated()) {
      return false;
   }
   /* This probably won't work, but we vsnprintf, then if we
    * get a negative length or a length greater than our buffer
    * (depending on which library is used), the printf was truncated, so
    * get a bigger buffer and try again.
    */
   for (;;) {
      maxlen = sizeof_pool_memory(msg) - 1;
      va_start(arg_ptr, fmt);
      msglen = bvsnprintf(msg, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (msglen >= 0 && msglen < (maxlen - 5)) {
         break;
      }
      msg = realloc_pool_memory(msg, maxlen + maxlen / 2);
   }
   return send();
}

void BSOCK::set_killable(bool killable)
{
   if (m_jcr) {
      m_jcr->set_killable(killable);
   }
}

/* Commands sent to Director */
static char hello[] =
   "Hello %s calling\n";

/* Response from Director */
static char OKhello[] =
   "1000 OK:";

/*
 * Authenticate Director
 */
bool BSOCK::authenticate_with_director(JCR *jcr, const char *name,
                                       s_password& password, tls_t& tls,
                                       char *response, int response_len,
                                       unsigned int protocol,
                                       const char *user, const char *psk,
                                       auth_backend_types auth_type)
{
   switch (protocol) {
   case 0:
      return authenticate_with_director_0(jcr, name, password, tls, response, response_len);
   case 2:
      return authenticate_with_director_2(jcr, name, password, tls, auth_type, user, psk,
                                          response, response_len);
   default:
      // TODO
      return false;
   }
}

bool BSOCK::authenticate_with_director_0(JCR *jcr,
                                         const char *name, s_password& password,
                                         tls_t& tls,
                                         char *response, int response_len)
{
   char bashed_name[MAX_NAME_LENGTH];
   BSOCK *dir = this;        /* for readability */

   response[0] = 0;

   /*
    * Send my name to the Director then do authentication
    */
   bstrncpy(bashed_name, name, sizeof(bashed_name));
   bash_spaces(bashed_name);

   /*
    * Timeout Hello after 5 mins
    */
   dir->start_timer(60 * 5);
   dir->fsend(hello, bashed_name);

   if (!authenticate_outbound_connection(jcr, "Director", name, password, tls)) {
      goto bail_out;
   }

   Dmsg1(6, ">dird: %s", dir->msg);
   if (dir->recv() <= 0) {
      dir->stop_timer();
      bsnprintf(response, response_len, _("Bad response to Hello command: ERR=%s\n"
                                          "The Director at \"%s:%d\" is probably not running.\n"),
                dir->bstrerror(), dir->host(), dir->port());
      return false;
   }

   dir->stop_timer();
   Dmsg1(10, "<dird: %s", dir->msg);
   if (!bstrncmp(dir->msg, OKhello, sizeof(OKhello) - 1)) {
      bsnprintf(response, response_len, _("Director at \"%s:%d\" rejected Hello command\n"),
                dir->host(), dir->port());
      return false;
   } else {
      bsnprintf(response, response_len, "%s", dir->msg);
   }

   return true;

bail_out:
   dir->stop_timer();
   bsnprintf(response, response_len, _("Authorization problem with Director at \"%s:%d\"\n"
                                       "Most likely the passwords do not agree.\n"
                                       "If you are using TLS, there may have been a certificate "
                                       "validation error during the TLS handshake.\n"
                                       "Please see %s for help.\n"),
             dir->host(), dir->port(), MANUAL_AUTH_URL);

   return false;
}

#define MAX_NAME 127
#define STRINGIFY(a) #a
#define TOSTRING(a) STRINGIFY(a)

bool BSOCK::authenticate_with_director_2(JCR *jcr,
                                         const char *name, const s_password& password, tls_t& tls,
                                         auth_backend_types auth_type,
                                         const char *user, const char *psk,
                                         char *response, int response_len)
{
   static const char new_protocol_hello[] = "Hello %s calling protocol=%u ssl=%u\n";
   const int dbglvl = 50;
   bool auth_success = false;
   char bashed_name[MAX_NAME_LENGTH + 1];
   char remote[MAX_NAME + 1];
   unsigned int protocol;
   int tls_remote_capability;
   int tls_remote_auth_only;
   char version[MAX_NAME + 1];
   char bareos_date[MAX_NAME + 1];

   response[0] = 0;

   bstrncpy(bashed_name, name, sizeof(bashed_name));
   /* replaces blanks by 0x1 */
   bash_spaces(bashed_name);

   int tls_local_need = BNET_TLS_NONE;
   if (get_tls_enable(tls.ctx)) {
      tls_local_need = get_tls_require(tls.ctx) ? BNET_TLS_REQUIRED : BNET_TLS_OK;
   }

   /*
    * Timeout Hello after 5 mins
    */
   start_timer(60 * 5);

   /* console to director: Hello console_name calling protocol=2 ssl=1\n */
   if (fsend(new_protocol_hello, bashed_name, 2, tls_local_need) <= 0) {
      bsnprintf(response, response_len,
                _("Error sending the Hello command: \"%s:%d\": %s.\n"),
                host(), port(), bstrerror());
      Dmsg3(dbglvl, "Error sending the Hello command: \"%s:%d\": %s.\n",
            host(), port(), bstrerror());
      Jmsg(jcr, M_FATAL, 0, _("Error sending the Hello commandd: \"%s:%d\": %s.\n"),
           host(), port(), bstrerror());
      goto bail_out;
   }

   if (recv() <= 0) {
      /* maybe the director does not understand the new protocol =>
         fall back to the old one */
      bsnprintf(response, response_len,
                _("Error waiting for Hello command response: \"%s:%d\": %s.\n"),
                host(), port(), bstrerror());
      Dmsg3(dbglvl, "Error waiting for Hello command response: \"%s:%d\": %s.\n",
            host(), port(), bstrerror());
      Jmsg(jcr, M_FATAL, 0, "Error waiting for Hello command response: \"%s:%d\": %s.\n",
           host(), port(), bstrerror());
      goto bail_out;
   }

   // check the answer received from hello

   /* director to console: 1000 OK: name protocol=2 ssl=1\n */
   if (sscanf(msg, "1000 OK:"
              " %" TOSTRING(MAX_NAME) "s"
              " %" TOSTRING(MAX_NAME) "s"
              " %" TOSTRING(MAX_NAME) "s"
              " protocol=%u"
              " ssl=%u"
              " ssl_auth_only=%u\n",
              remote, version, bareos_date, &protocol, &tls_remote_capability,
              &tls_remote_auth_only) != 6) {
      bsnprintf(response, response_len,
                _("Unexpected response to our hello when connected to \"%s:%d\": %s"),
                host(), port(), msg);
      Dmsg3(dbglvl, "Unexpected response to our hello when connected to \"%s:%d\": %s",
            host(), port(), msg);
      Jmsg(jcr, M_FATAL, 0, "Unexpected response to our hello when connected to \"%s:%d\": %s",
           host(), port(), msg);
      goto bail_out;
   }

   unbash_spaces(version);
   unbash_spaces(bareos_date);

   bsnprintf(response, response_len, "1000 OK: %s Version: %s (%s)\n",
             remote, version, bareos_date);

   if (protocol != 2) {
      bsnprintf(response, response_len,
                _("Non-matching protocol number (%u) from \"%s:\%d\".\n"),
                protocol, host(), port());
      Dmsg3(dbglvl, "Non-matching protocol number (%u) from \"%s:\%d\".\n",
            protocol, host(), port());
      Jmsg(jcr, M_FATAL, 0, "Non-matching protocol number (%u) from \"%s:\%d\".\n",
           protocol, host(), port());
      goto bail_out;
   }

   /* the protocol matches */
   /* let us check the tls capabilities */

   if (tls_local_need == BNET_TLS_REQUIRED &&
       tls_remote_capability == BNET_TLS_NONE) {
      bsnprintf(response, response_len,
                _("Server \"%s:%d\" does not support required TLS.\n"),
                host(), port());
      Dmsg2(dbglvl, "Server \"%s:%d\" does not support required TLS.\n",
            host(), port());
      Jmsg(jcr, M_FATAL, 0, "Server \"%s:%d\" does not support required TLS.\n",
            host(), port());
      goto bail_out;
   }

   if (tls_local_need == BNET_TLS_REQUIRED) {
      /* initialise TLS */
      if (!start_tls(jcr, tls, response, response_len)) {
         auth_success = false;
         goto bail_out;
      }
   }

   switch (auth_type) {
   case AUTH_BACKEND_PAM:
      if (!psk_auth(jcr, user, psk, response, response_len))
         goto bail_out;
      break;
   case AUTH_BACKEND_LOCAL_MD5:
      if (!cram_md5_auth(jcr, password.value, tls_local_need, response, response_len))
         goto bail_out;
      break;
   default:
      break;
   }

   /* all ok */
   if (tls_local_need == BNET_TLS_REQUIRED && tls_remote_auth_only) {
      free_tls();
   }

   auth_success = true;
bail_out:
   stop_timer();

   if (jcr) {
      jcr->authenticated = auth_success;
   }

   return auth_success;
}

bool BSOCK::start_tls(JCR *jcr, tls_t& tls, char *response, int response_len)
{
   const int dbglvl = 50;

   alist *verify_list = NULL;

   if (tls.verify_peer) {
      verify_list = tls.allowed_cns;
   }

   if (fsend("STARTTLS\n") <= 0) {
      bsnprintf(response, response_len,
                _("Error sending the STARTTLS: \"%s:%d\": %s.\n"),
                host(), port(), bstrerror());
      Dmsg3(dbglvl, "Error sending the STARTTLS command: \"%s:%d\": %s.\n",
            host(), port(), bstrerror());
      Jmsg(jcr, M_FATAL, 0, _("Error sending the STARTTLS commandd: \"%s:%d\": %s.\n"),
           host(), port(), bstrerror());
      return false;
   }

   if (recv() <= 0) {
      bsnprintf(response, response_len,
                _("Error waiting for the STARTTLS command response: \"%s:%d\": %s.\n"),
                host(), port(), bstrerror());
      Dmsg3(dbglvl, "Error waiting for STARTTLS command response: \"%s:%d\": %s.\n",
            host(), port(), bstrerror());
      Jmsg(jcr, M_FATAL, 0, "Error waiting for STARTTLS command response: \"%s:%d\": %s.\n",
           host(), port(), bstrerror());
      return false;
   }

   // check the answer received for STARTTLS

   if (!bstrcmp(msg, "1000 OK STARTTLS\n")) {
      bsnprintf(response, response_len,
                _("Unexpected response to our hello when connected to \"%s:%d\": %s"),
                host(), port(), msg);
      Dmsg3(dbglvl, "Unexpected response to our hello when connected to \"%s:%d\": %s",
            host(), port(), msg);
      Jmsg(jcr, M_FATAL, 0, "Unexpected response to our hello when connected to \"%s:%d\": %s",
           host(), port(), msg);
      return false;
   }

   if (!bnet_tls_client(tls.ctx, this, tls.verify_peer, verify_list)) {
      // Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
      bsnprintf(response, response_len, _("TLS negotiation with \"%s:%d\" failed.\n"),
                host(), port());
      Dmsg2(dbglvl, "TLS negotiation with \"%s:\%d\" failed.\n", host(), port());
      Jmsg(jcr, M_FATAL, 0, "TLS negotiation with \"%s:\%d\" failed.\n", host(), port());
      return false;
   }

   return true;
}

bool BSOCK::psk_auth(JCR *jcr, const char *user, const char *pass, char *response, int response_len)
{
   static const unsigned int MAX_USER_PASS = 128;
   static const int dbglvl = 50;
   char b64user[(MAX_USER_PASS + 2) / 3 * 4 + 1];
   size_t userlen;
   char b64pass[(MAX_USER_PASS + 2) / 3 * 4 + 1];
   size_t passlen;
   bool auth_ok = false;

   /* send auth plain\n
      expect +
    */
   if (fsend("auth plain\n") <= 0) {
      bsnprintf(response, response_len, _("Send error to \"%s:%d\": %s.\n"),
                host(), port(), bstrerror());
      Dmsg3(dbglvl, "Send error to \"%s:%d\": %s.\n", host(), port(), bstrerror());
      Jmsg(jcr, M_FATAL, 0, "Send error to \"%s:%d\": %s.\n", host(), port(), bstrerror());
      return false;
   }

   if (recv() <= 0) {
      bsnprintf(response, response_len, _("Read error from \"%s:%d\": %s.\n"),
                host(), port(), bstrerror());
      Dmsg3(dbglvl, "Read error from \"%s:%d\": %s.\n", host(), port(), bstrerror());
      Jmsg(jcr, M_FATAL, 0, "Read error from \"%s:%d\": %s.\n", host(), port(), bstrerror());
      goto bail_out;
   }

   if (!bstrcmp("+\n", msg)) {
      bsnprintf(response, response_len,
                _("Unexpected response to the 'auth plain' command from \"%s:%d\": %s"),
                host(), port(), msg);
      Dmsg3(dbglvl,
            _("Unexpected response to the 'auth plain' command from \"%s:%d\": %s"),
            host(), port(), msg);
      Jmsg(jcr, M_FATAL, 0,
            _("Unexpected response to the 'auth plain' command from \"%s:%d\": %s"),
           host(), port(), msg);
      goto bail_out;
   }

   /* bin_to_base64 should have const char * for 3rd argument */
   userlen = strlen(user);
   bin_to_base64(b64user, sizeof(b64user), (char *)user, userlen, true);
   passlen = strlen(pass);
   bin_to_base64(b64pass, sizeof(b64pass), (char *)pass, passlen, true);

   if (fsend("login %s %s\n", b64user, b64pass) <= 0) {
      bsnprintf(response, response_len, _("Send error to \"%s:%d\": %s.\n"),
                host(), port(), bstrerror());
      Dmsg3(dbglvl, "Send error to \"%s:%d\": %s.\n", host(), port(), bstrerror());
      Jmsg(jcr, M_FATAL, 0, "Send error to \"%s:%d\": %s.\n", host(), port(), bstrerror());
      goto bail_out;
   }

   if (recv() <= 0) {
      bsnprintf(response, response_len, _("Read error from \"%s:%d\": %s.\n"),
                host(), port(), bstrerror());
      Dmsg3(dbglvl, "Read error from \"%s:%d\": %s.\n", host(), port(), bstrerror());
      Jmsg(jcr, M_FATAL, 0, "Read error from \"%s:%d\": %s.\n", host(), port(), bstrerror());
      goto bail_out;
   }

   if (!bstrcmp(msg, "1000 OK auth\n")) {
      bsnprintf(response, response_len, _("Auth plain error from \"%s:%d\": %s.\n"),
                host(), port(), msg);
      Dmsg3(dbglvl, "Auth plain error from \"%s:%d\": %s.\n", host(), port(), msg);
      Jmsg(jcr, M_FATAL, 0, "Auth plain error from \"%s:%d\": %s.\n", host(), port(), msg);
      goto bail_out;
   }

   auth_ok = true;
bail_out:
   return auth_ok;
}

bool BSOCK::cram_md5_auth(JCR *jcr, const char *pass, int tls_local_need,
                          char *response, int response_len)
{
   static const int dbglvl = 50;

   if (fsend("auth cram-md5\n") <= 0) {
      bsnprintf(response, response_len, _("Send error to \"%s:%d\": %s.\n"),
                host(), port(), bstrerror());
      Dmsg3(dbglvl, "Send error to \"%s:%d\": %s.\n", host(), port(), bstrerror());
      Jmsg(jcr, M_FATAL, 0, "Send error to \"%s:%d\": %s.\n", host(), port(), bstrerror());
      return false;
   }

   int tls_remote_need = BNET_TLS_NONE; // not used
   bool compatible = true;
   // we read the challenge, respond, and consume the auth result
   bool auth_success = cram_md5_respond(this, pass, &tls_remote_need, &compatible);
   if (!auth_success) {
      bsnprintf(response, response_len, _("cram_md5_response failed for: \"%s:%d\".\n"),
                host(), port());
      Jmsg(jcr, M_FATAL, 0, "cram_md5_response failed for: \"%s:%d\".\n", host(), port());
      return false;
   }
   /* challenge the remote */
   /* we send the challenge, the peer sends the response, we finish by sending
      1000 OK auth */
   auth_success = cram_md5_challenge(this, pass, tls_local_need, true);
   if (!auth_success) {
      bsnprintf(response, response_len,
                _("cram_md5_challenge failed for \"%s:\%d\".\n"),
                host(), port());
      Jmsg(jcr, M_FATAL, 0, "cram_md5_challenge failed for \"%s:\%d\".\n", host(), port());
   }
   return auth_success;
}

/*
 * Depending on the initiate parameter perform one of the following:
 *
 * - First make him prove his identity and then prove our identity to the Remote.
 * - First prove our identity to the Remote and then make him prove his identity.
 */
bool BSOCK::two_way_authenticate(JCR *jcr, const char *what,
                                 const char *name, s_password &password,
                                 tls_t &tls, bool initiated_by_remote)
{
   btimer_t *tid = NULL;
   const int dbglvl = 50;
   bool compatible = true;
   bool auth_success = false;
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;

   if(password.encoding != p_encoding_md5) {
      Jmsg(jcr, M_FATAL, 0, _("Password encoding is not MD5. You are probably restoring a NDMP Backup with a restore job not configured for NDMP protocol.\n"));
      goto auth_fatal;
   }

   /*
    * TLS Requirement
    */
   if (get_tls_enable(tls.ctx)) {
      tls_local_need = get_tls_require(tls.ctx) ? BNET_TLS_REQUIRED : BNET_TLS_OK;
   }

   if (jcr && job_canceled(jcr)) {
      Dmsg0(dbglvl, "Failed, because job is canceled.\n");
      auth_success = false;     /* force quick exit */
      goto auth_fatal;
   }

   /*
    * Timeout Hello after 10 min
    */
   tid = start_bsock_timer(this, AUTH_TIMEOUT);

   /*
    * See if we initiate the challenge or respond to a challenge.
    */
   if (initiated_by_remote) {
      /*
       * Challenge Remote.
       */
      auth_success = cram_md5_challenge(this, password.value, tls_local_need, compatible);
      if (auth_success) {
          /*
           * Respond to remote challenge
           */
          auth_success = cram_md5_respond(this, password.value, &tls_remote_need, &compatible);
          if (!auth_success) {
             Dmsg1(dbglvl, "Respond cram-get-auth failed with %s\n", who());
          }
      } else {
         Dmsg1(dbglvl, "Challenge cram-auth failed with %s\n", who());
      }
   } else {
      /*
       * Respond to remote challenge
       */
      auth_success = cram_md5_respond(this, password.value, &tls_remote_need, &compatible);
      if (!auth_success) {
         Dmsg1(dbglvl, "cram_respond failed for %s\n", who());
      } else {
         /*
          * Challenge Remote.
          */
         auth_success = cram_md5_challenge(this, password.value, tls_local_need, compatible);
         if (!auth_success) {
            Dmsg1(dbglvl, "cram_challenge failed for %s\n", who());
         }
      }
   }

   if (!auth_success) {
      Jmsg(jcr, M_FATAL, 0, _("Authorization key rejected by %s %s.\n"
                              "Please see %s for help.\n"),
                              what, name, MANUAL_AUTH_URL);
      goto auth_fatal;
   }

   if (jcr && job_canceled(jcr)) {
         Dmsg0(dbglvl, "Failed, because job is canceled.\n");
         auth_success = false;     /* force quick exit */
         goto auth_fatal;
   }

   /*
    * Verify that the remote host is willing to meet our TLS requirements
    */
   if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      Jmsg(jcr, M_FATAL, 0, _("Authorization problem: Remote server did not"
                              " advertize required TLS support.\n"));
      Dmsg2(dbglvl, "remote_need=%d local_need=%d\n", tls_remote_need, tls_local_need);
      auth_success = false;
      goto auth_fatal;
   }

   /*
    * Verify that we are willing to meet the remote host's requirements
    */
   if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      Jmsg(jcr, M_FATAL, 0, _("Authorization problem: Remote server requires TLS.\n"));
      Dmsg2(dbglvl, "remote_need=%d local_need=%d\n", tls_remote_need, tls_local_need);
      auth_success = false;
      goto auth_fatal;
   }

   if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
      alist *verify_list = NULL;

      if (tls.verify_peer) {
         verify_list = tls.allowed_cns;
      }

      /*
       * See if we are handshaking a passive client connection.
       */
      if (initiated_by_remote) {
         if (!bnet_tls_server(tls.ctx, this, verify_list)) {
            Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
            Dmsg0(dbglvl, "TLS negotiation failed.\n");
            auth_success = false;
            goto auth_fatal;
         }
      } else {
         if (!bnet_tls_client(tls.ctx, this, tls.verify_peer, verify_list)) {
            Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
            Dmsg0(dbglvl, "TLS negotiation failed.\n");
            auth_success = false;
            goto auth_fatal;
         }
      }

      if (tls.authenticate) {           /* tls authentication only? */
         free_tls();                    /* yes, shutdown tls */
      }
   }

auth_fatal:
   if (tid) {
      stop_bsock_timer(tid);
      tid = NULL;
   }

   if (jcr) {
      jcr->authenticated = auth_success;
   }

   return auth_success;
}


/*
 * Try to limit the bandwidth of a network connection
 */
void BSOCK::control_bwlimit(int bytes)
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
   now = get_current_btime();
   if (m_last_tick == 0) {
      m_nb_bytes = bytes;
      m_last_tick = now;
      return;
   }

   /*
    * Calculate the number of microseconds since the last check.
    */
   temp = now - m_last_tick;

   /*
    * Less than 0.1ms since the last call, see the next time
    */
   if (temp < 100) {
      m_nb_bytes += bytes;
      return;
   }

   /*
    * Keep track of how many bytes are written in this timeslice.
    */
   m_nb_bytes += bytes;
   m_last_tick = now;
   if (debug_level >= 400) {
      Dmsg3(400, "control_bwlimit: now = %lld, since = %lld, nb_bytes = %d\n", now, temp, m_nb_bytes);
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
   m_nb_bytes -= (int64_t)(temp * ((double)m_bwlimit / 1000000.0));
   if (m_nb_bytes < 0) {
      /*
       * If more was authorized then used but bursting is not enabled
       * reset the counter as these bytes cannot be used later on when
       * we are exceeding our bandwidth.
       */
      if (!m_use_bursting) {
         m_nb_bytes = 0;
      }
      return;
   }

   /*
    * What exceed should be converted in sleep time
    */
   usec_sleep = (int64_t)(m_nb_bytes /((double)m_bwlimit / 1000000.0));
   if (usec_sleep > 100) {
      if (debug_level >= 400) {
         Dmsg1(400, "control_bwlimit: sleeping for %lld usecs\n", usec_sleep);
      }

      /*
       * Sleep the right number of usecs.
       */
      while (1) {
         bmicrosleep(0, usec_sleep);
         now = get_current_btime();

         /*
          * See if we slept enough or that bmicrosleep() returned early.
          */
         if ((now - m_last_tick) < usec_sleep) {
            usec_sleep -= (now - m_last_tick);
            continue;
         } else {
            m_last_tick = now;
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
      if (m_use_bursting) {
         m_nb_bytes -= (int64_t)(usec_sleep * ((double)m_bwlimit / 1000000.0));
      } else {
         m_nb_bytes = 0;
      }
   }
}
