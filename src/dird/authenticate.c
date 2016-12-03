/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * BAREOS Director -- authenticate.c -- handles authorization of Consoles, Storage and File daemons.
 *
 * Kern Sibbald, May MMI
 *
 * This routine runs as a thread and must be thread reentrant.
 */

#include "bareos.h"
#include "dird.h"

static const int dbglvl = 50;

/*
 * Commands sent to Storage daemon and File daemon and received from the User Agent
 */
static char hello[] =
   "Hello Director %s calling\n";

/*
 * Response from Storage daemon
 */
static char OKhello[] =
   "3000 OK Hello\n";
static char FDOKhello[] =
   "2000 OK Hello\n";
static char FDOKnewHello[] =
   "2000 OK Hello %d\n";

/*
 * Sent to User Agent
 */
static char Dir_sorry[] =
   "1999 You are not authorized.\n";

static bool auth_ua_2(UAContext *uac, char *name, unsigned int protocol,
                      unsigned int tls_client_requirement);
static bool psk_auth(BSOCK *ua, alist *allowed_users,
                     alist *allowed_groups, const char *console_name);
static bool auth_ua_0(UAContext *uac);
static bool start_tls(JCR *jcr, BSOCK *ua, tls_t& tls);

/*
 * Authenticate with a remote Storage daemon
 */
bool authenticate_with_storage_daemon(JCR *jcr, STORERES *store)
{
   BSOCK *sd = jcr->store_bsock;
   char dirname[MAX_NAME_LENGTH];
   bool auth_success = false;

   /*
    * Send my name to the Storage daemon then do authentication
    */
   bstrncpy(dirname, me->hdr.name, sizeof(dirname));
   bash_spaces(dirname);

   if (!sd->fsend(hello, dirname)) {
      Dmsg1(dbglvl, _("Error sending Hello to Storage daemon. ERR=%s\n"), bnet_strerror(sd));
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to Storage daemon. ERR=%s\n"), bnet_strerror(sd));
      return false;
   }

   auth_success = sd->authenticate_outbound_connection(jcr, "Storage daemon",
                                                       store->name(), store->password,
                                                       store->tls);
   if (!auth_success) {
      Dmsg2(dbglvl, "Director unable to authenticate with Storage daemon at \"%s:%d\"\n",
            sd->host(), sd->port());
      Jmsg(jcr, M_FATAL, 0,
           _("Director unable to authenticate with Storage daemon at \"%s:%d\". Possible causes:\n"
             "Passwords or names not the same or\n"
             "TLS negotiation problem or\n"
             "Maximum Concurrent Jobs exceeded on the SD or\n"
             "SD networking messed up (restart daemon).\n"
             "Please see %s for help.\n"),
           sd->host(), sd->port(), MANUAL_AUTH_URL);
      return false;
   }

   Dmsg1(116, ">stored: %s", sd->msg);
   if (sd->recv() <= 0) {
      Jmsg3(jcr, M_FATAL, 0, _("dir<stored: \"%s:%s\" bad response to Hello command: ERR=%s\n"),
            sd->who(), sd->host(), sd->bstrerror());
      return false;
   }

   Dmsg1(110, "<stored: %s", sd->msg);
   if (!bstrncmp(sd->msg, OKhello, sizeof(OKhello))) {
      Dmsg0(dbglvl, _("Storage daemon rejected Hello command\n"));
      Jmsg2(jcr, M_FATAL, 0, _("Storage daemon at \"%s:%d\" rejected Hello command\n"),
            sd->host(), sd->port());
      return false;
   }

   return true;
}

/*
 * Authenticate with a remote File daemon
 */
bool authenticate_with_file_daemon(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;
   CLIENTRES *client = jcr->res.client;
   char dirname[MAX_NAME_LENGTH];
   bool auth_success = false;

   if (jcr->authenticated) {
      /*
       * already authenticated
       */
      return true;
   }

   /*
    * Send my name to the File daemon then do authentication
    */
   bstrncpy(dirname, me->name(), sizeof(dirname));
   bash_spaces(dirname);

   if (!fd->fsend(hello, dirname)) {
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to File daemon at \"%s:%d\". ERR=%s\n"),
           fd->host(), fd->port(), fd->bstrerror());
      return false;
   }
   Dmsg1(dbglvl, "Sent: %s", fd->msg);

   auth_success = fd->authenticate_outbound_connection(jcr, "File Daemon",
                                                       client->name(), client->password,
                                                       client->tls);
   if (!auth_success) {
      Dmsg2(dbglvl, "Unable to authenticate with File daemon at \"%s:%d\"\n", fd->host(), fd->port());
      Jmsg(jcr, M_FATAL, 0,
            _("Unable to authenticate with File daemon at \"%s:%d\". Possible causes:\n"
              "Passwords or names not the same or\n"
              "TLS negotiation failed or\n"
              "Maximum Concurrent Jobs exceeded on the FD or\n"
              "FD networking messed up (restart daemon).\n"
              "Please see %s for help.\n"),
            fd->host(), fd->port(), MANUAL_AUTH_URL);
      return false;
   }

   Dmsg1(116, ">filed: %s", fd->msg);
   if (fd->recv() <= 0) {
      Dmsg1(dbglvl, _("Bad response from File daemon to Hello command: ERR=%s\n"),
            bnet_strerror(fd));
      Jmsg(jcr, M_FATAL, 0, _("Bad response from File daemon at \"%s:%d\" to Hello command: ERR=%s\n"),
           fd->host(), fd->port(), fd->bstrerror());
      return false;
   }

   Dmsg1(110, "<filed: %s", fd->msg);
   jcr->FDVersion = 0;
   if (!bstrncmp(fd->msg, FDOKhello, sizeof(FDOKhello)) &&
       sscanf(fd->msg, FDOKnewHello, &jcr->FDVersion) != 1) {
      Dmsg0(dbglvl, _("File daemon rejected Hello command\n"));
      Jmsg(jcr, M_FATAL, 0, _("File daemon at \"%s:%d\" rejected Hello command\n"),
           fd->host(), fd->port());
      return false;
   }

   return true;
}

/*
 * Authenticate File daemon connection
 */
bool authenticate_file_daemon(BSOCK *fd, char *client_name)
{
   CLIENTRES *client;
   bool auth_success = false;

   unbash_spaces(client_name);
   client = (CLIENTRES *)GetResWithName(R_CLIENT, client_name);
   if (client) {
      if (is_connect_from_client_allowed(client)) {
         auth_success = fd->authenticate_inbound_connection(NULL, "File Daemon",
                                                            client_name, client->password,
                                                            client->tls);
      }
   }

   /*
    * Authorization Completed
    */
   if (!auth_success) {
      fd->fsend("%s", _(Dir_sorry));
      Emsg4(M_ERROR, 0, _("Unable to authenticate client \"%s\" at %s:%s:%d.\n"),
            client_name, fd->who(), fd->host(), fd->port());
      sleep(5);
      return false;
   }
   fd->fsend("1000 OK: %s Version: %s (%s)\n", my_name, VERSION, BDATE);

   return true;
}

/*
 * Count the number of established console connections.
 */
static inline bool count_console_connections()
{
   JCR *jcr;
   unsigned int cnt = 0;

   foreach_jcr(jcr) {
      if (jcr->is_JobType(JT_CONSOLE)) {
         cnt++;
      }
   }
   endeach_jcr(jcr);

   return (cnt >= me->MaxConsoleConnections) ? false : true;
}

#define MAX_USER_PASS 128
#define MAX_B64_USER_PASS 176
#define STRINGIFY(a) #a
#define TOSTRING(a) STRINGIFY(a)

/*
 * Authenticate user agent.
 */
bool authenticate_user_agent(UAContext *uac)
{
   char name[MAX_NAME_LENGTH + 1];
   unsigned int tls_client_requirement;
   unsigned int protocol;

   if (sscanf(uac->UA_sock->msg,
              "Hello %" TOSTRING(MAX_NAME_LENGTH) "s "
              "calling protocol=%u ssl=%u\n",
              name, &protocol, &tls_client_requirement) == 3) {
      switch (protocol) {
      case 0:
         return auth_ua_0(uac);
      case 2:
         return auth_ua_2(uac, name, protocol, tls_client_requirement);
      default:
         //
         return false;
      }
   } else
      return auth_ua_0(uac);
}

static bool
auth_ua_2(UAContext *uac, char *name, unsigned int protocol,
          unsigned int tls_client_requirement)
{
   static const char HELLO_OK[]    = "1000 OK: %s %s %s protocol=%u ssl=%u ssl_auth_only=%u\n";
   static const char UNKNOWN_CONSOLE[]    = "1999 Unknown console: %s\n";
   static const char AUTH_FAILED[] = "1999 Authorization failed.\n";
   bool compatible = true;
   char version[MAX_NAME_LENGTH + 1];
   char bareos_date[MAX_NAME_LENGTH + 1];

   s_password *password;
   int unused;

   BSOCK *ua = uac->UA_sock;
   CONRES *cons = NULL;
   bool auth_success = false;
   bool tls_active = false;
   JCR *jcr = ua->jcr();
   int tls_capability = BNET_TLS_NONE;
   tls_t *tls = NULL;

   if (!count_console_connections()) {
      ua->fsend("%s", AUTH_FAILED);
      Emsg0(M_ERROR, 0, _("Number of console connections exhausted, please increase MaximumConsoleConnections\n"));
      return false;
   }

   bool is_default_console = bstrcmp(name, "*UserAgent*");
   if (!is_default_console) {
      unbash_spaces(name);
      cons = (CONRES *)GetResWithName(R_CONSOLE, name);
      if (!cons) {
         Jmsg(jcr, M_FATAL, 0, _("Unknown console %s.\n"), name);
         Emsg3(M_ERROR, 0, _("Unknown console: %s \"%s:%d\"\n"),
               name, ua->host(), ua->port());
         bash_spaces(name);
         ua->fsend(UNKNOWN_CONSOLE, name);
         goto auth_fatal;
      }
      password = &cons->password;
      tls = &cons->tls;
   } else {
      password = &me->password;
      tls = &me->tls;
   }

   if (get_tls_enable(tls->ctx))
      tls_capability = get_tls_require(tls->ctx) ? BNET_TLS_REQUIRED : BNET_TLS_OK;

   ua->start_timer(60 * 5);

   bstrncpy(version, VERSION, sizeof(version));
   bash_spaces(version);
   bstrncpy(bareos_date, BDATE, sizeof(bareos_date));
   bash_spaces(bareos_date);

   if (!ua->fsend(HELLO_OK, my_name, version, bareos_date, protocol,
                  tls_capability, tls->authenticate)) {
      Jmsg(jcr, M_FATAL, 0, _("Bnet send error.\n"));
      Emsg2(M_ERROR, 0, _("Bnet send error \"%s:%d\"\n"),
            ua->host(), ua->port());
      goto auth_fatal;
   }

   if (jcr && job_canceled(jcr)) {
      Dmsg0(dbglvl, "Failed, because job is canceled.\n");
      auth_success = false;     /* force quick exit */
      goto auth_fatal;
   }

   /* wait for either STARTTLS or auth */
   if (ua->recv() <= 0) {
      Jmsg(jcr, M_FATAL, 0, _("Bnet recv error.\n"));
      Emsg2(M_ERROR, 0, _("Bnet recv error \"%s:%d\"\n"),
            ua->host(), ua->port());
      Dmsg0(dbglvl, _("Socket read error.\n"));
      goto auth_fatal;
   }

   if (bstrcmp("STARTTLS\n", ua->msg)) {
      if (tls_capability == BNET_TLS_NONE) {
         if (!ua->fsend("1999 TLS is not supported\n")) {
            Jmsg(jcr, M_FATAL, 0, _("Bnet send error.\n"));
            Emsg2(M_ERROR, 0, _("Bnet send error \"%s:%d\"\n"),
                  ua->host(), ua->port());
         }
         goto auth_fatal;
      }
      if (!ua->fsend("1000 OK STARTTLS\n")) {
         Jmsg(jcr, M_FATAL, 0, _("Bnet send error.\n"));
         Emsg2(M_ERROR, 0, _("Bnet send error \"%s:%d\"\n"),
               ua->host(), ua->port());
         goto auth_fatal;
      }

      if (!start_tls(jcr, ua, *tls)) {
         goto auth_fatal;
      }

      tls_active = true;

      if (ua->recv() <= 0) {
         Jmsg(jcr, M_FATAL, 0, _("Bnet recv error.\n"));
         Emsg2(M_ERROR, 0, _("Bnet recv error \"%s:%d\"\n"),
               ua->host(), ua->port());
         Dmsg0(dbglvl, _("Socket read error.\n"));
         goto auth_fatal;
      }
   }

   if (jcr && job_canceled(jcr)) {
      Dmsg0(dbglvl, "Failed, because job is canceled.\n");
      auth_success = false;     /* force quick exit */
      goto auth_fatal;
   }

   if (bstrcmp(ua->msg, "auth cram-md5\n")) {
      /* we send the challenge, the client sends the response, we send the result */
      if (!cram_md5_challenge(ua, password->value, tls_capability, true)) {
         Jmsg(jcr, M_FATAL, 0, _("cram_md5_challenge failed.\n"));
         goto auth_fatal;
      }
      /* the client sends the challenge, we read it, send the response, we read the result */
      if (!cram_md5_respond(ua, password->value, &unused, &compatible)) {
         Dmsg0(dbglvl, _("cram_md5_respond failed.\n"));
         goto auth_fatal;
      }
   } else if (bstrcmp(ua->msg, "auth plain\n")) {
      /* we send '+' (continue), the client sends the user:pass, we send the result */
      if (is_default_console) {
         /* plain authententication is not allowed for the default console */
         if (!ua->fsend("1999 Plain authentication for the default console is not allowed\n")) {
            Jmsg(jcr, M_FATAL, 0, _("Bnet send error.\n"));
            Emsg2(M_ERROR, 0, _("Bnet send error \"%s:%d\"\n"),
                  ua->host(), ua->port());
         }
         goto auth_fatal;
      }
      if (!tls_active) {
         /* reject plain authentication over unsecure transport */
         if (!ua->fsend("1999 Plain authentication over unsecure transport is not allowed\n")) {
            Jmsg(jcr, M_FATAL, 0, _("Bnet send error.\n"));
            Emsg2(M_ERROR, 0, _("Bnet send error \"%s:%d\"\n"),
                  ua->host(), ua->port());
         }
         goto auth_fatal;
      }
      if (!cons || cons->authtype != AUTH_BACKEND_PAM) {
         /* this console may not use pam authentication */
         if (!ua->fsend("1999 This console does not support plain authentication\n")) {
            Jmsg(jcr, M_FATAL, 0, _("Bnet send error.\n"));
            Emsg2(M_ERROR, 0, _("Bnet send error \"%s:%d\"\n"),
                  ua->host(), ua->port());
         }
         goto auth_fatal;
      }
      if (!ua->fsend("+\n")) {
         Jmsg(jcr, M_FATAL, 0, _("Bnet send error.\n"));
         Emsg2(M_ERROR, 0, _("Bnet send error \"%s:%d\"\n"),
               ua->host(), ua->port());
         goto auth_fatal;
      }

      if (ua->recv() <= 0) {
         Jmsg(jcr, M_FATAL, 0, _("Bnet recv error.\n"));
         Emsg2(M_ERROR, 0, _("Bnet recv error \"%s:%d\"\n"),
               ua->host(), ua->port());
         Dmsg0(dbglvl, _("Socket read error.\n"));
         goto auth_fatal;
      }

      /* we send the result */
      if (!psk_auth(ua, cons->allowed_users, cons->allowed_groups, name)) {
         Jmsg(jcr, M_FATAL, 0, _("PSK authentication failed.\n"));
         goto auth_fatal;
      }
   } else {
      if (!ua->fsend("1999 Unsupported command\n")) {
         Jmsg(jcr, M_FATAL, 0, _("Bnet send error.\n"));
         Emsg2(M_ERROR, 0, _("Bnet send error \"%s:%d\"\n"),
               ua->host(), ua->port());
      }
      goto auth_fatal;
   }

   if (jcr && job_canceled(jcr)) {
      Dmsg0(dbglvl, "Failed, because job is canceled.\n");
      auth_success = false;     /* force quick exit */
      goto auth_fatal;
   }

   /* client is authenticated */
   /* psk_auth has sent AUTH_OK */
   /* now we authenticate ourselves with the client */
   /* we read the challenge, send the response, get the result */
   /* auth_success = cram_md5_respond(ua, password->value, &unused, &compatible); */
   /* if (!auth_success) { */
   /*    Dmsg0(dbglvl, _("cram_md5_respond failed.\n")); */
   /*    goto auth_fatal; */
   /* } */

   /* if (jcr && job_canceled(jcr)) { */
   /*    Dmsg0(dbglvl, "Failed, because job is canceled.\n"); */
   /*    auth_success = false;     /\* force quick exit *\/ */
   /*    goto auth_fatal; */
   /* } */

   /* tear-down TLS */
   if (tls_active && tls->authenticate) {
      ua->free_tls();
   }

   auth_success = true;
   if (cons)
      uac->cons = cons;
auth_fatal:
   ua->stop_timer();
   if (jcr)
      jcr->authenticated = auth_success;
   return auth_success;
}

static bool
psk_auth(BSOCK *ua, alist *allowed_users, alist *allowed_groups,
         const char *console_name)
{
   static const char AUTH_OK[]     = "1000 OK auth\n";
   static const char AUTH_FAILED[] = "1999 Authorization failed.\n";
   char user[MAX_USER_PASS + 1];
   int userlen;
   char pass[MAX_USER_PASS + 1];
   int passlen;
   char b64user[4 * (MAX_USER_PASS + 2) / 3 + 1];
   char b64pass[4 * (MAX_USER_PASS + 2) / 3 + 1];
   bool check_acl = false;

   b64user[0] = '\0';

   if (sscanf(ua->msg,
              "login %" TOSTRING(MAX_B64_USER_PASS) "s "
              "%" TOSTRING(MAX_B64_USER_PASS) "s\n",
              b64user, b64pass) != 2) {
      if (sscanf(ua->msg,
                 "login %" TOSTRING(MAX_B64_USER_PASS) "s\n",
                 b64pass) != 1) {
         Emsg2(M_ERROR, 0, _("No login command received from \"%s:%d\"\n"),
               ua->host(), ua->port());
         Dmsg0(dbglvl, _("No login command received.\n"));
         return false;
      }
   }

   if (b64user[0] != '\0') {
      userlen = base64_to_bin(user, MAX_USER_PASS, b64user, strlen(b64user));
      user[userlen] = '\0';
      check_acl = true;
   }
   passlen = base64_to_bin(pass, MAX_USER_PASS, b64pass, strlen(b64pass));
   pass[passlen] = '\0';

   Dmsg1(dbglvl, _("Authenticating %s.\n"),
         check_acl ? user : console_name);
   bool auth_success = pam_authenticate_useragent(
      check_acl ? user : console_name, pass,
      allowed_users, allowed_groups, check_acl, ua->host());
   if (auth_success)
      ua->fsend(AUTH_OK);
   else {
      ua->fsend(AUTH_FAILED);
      sleep(5);
   }
   return auth_success;
}

static bool auth_ua_0(UAContext *uac)
{
   char name[MAX_NAME_LENGTH];
   CONRES *cons = NULL;
   BSOCK *ua = uac->UA_sock;
   bool auth_success = false;

   if (sscanf(ua->msg, "Hello %127s calling\n", name) != 1) {
      ua->msg[100] = 0;                 /* terminate string */
      Emsg4(M_ERROR, 0, _("UA Hello from %s:%s:%d is invalid. Got: %s\n"), ua->who(),
            ua->host(), ua->port(), ua->msg);
      return false;
   }
   name[sizeof(name)-1] = 0;            /* terminate name */

   if (!count_console_connections()) {
      ua->fsend("%s", _(Dir_sorry));
      Emsg0(M_ERROR, 0, _("Number of console connections exhausted, please increase MaximumConsoleConnections\n"));
      return false;
   }

   if (bstrcmp(name, "*UserAgent*")) {  /* default console */
      auth_success = ua->authenticate_inbound_connection(NULL, "Console",
                                                         "*UserAgent*", me->password,
                                                         me->tls);
   } else {
      unbash_spaces(name);
      cons = (CONRES *)GetResWithName(R_CONSOLE, name);
      if (cons) {
         auth_success = ua->authenticate_inbound_connection(NULL, "Console",
                                                            name, cons->password,
                                                            cons->tls);

         if (auth_success) {
            uac->cons = cons;           /* save console resource pointer */
         }
      }
   }

   /*
    * Authorization Completed
    */
   if (!auth_success) {
      ua->fsend("%s", _(Dir_sorry));
      Emsg4(M_ERROR, 0, _("Unable to authenticate console \"%s\" at %s:%s:%d.\n"),
            name, ua->who(), ua->host(), ua->port());
      sleep(5);
      return false;
   }
   ua->fsend(_("1000 OK: %s Version: %s (%s)\n"), my_name, VERSION, BDATE);

   return true;
}

static bool start_tls(JCR *jcr, BSOCK *ua, tls_t& tls)
{
   alist *verify_list = NULL;

   if (tls.verify_peer) {
      verify_list = tls.allowed_cns;
   }

   if (!bnet_tls_server(tls.ctx, ua, verify_list)) {
      Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
      Dmsg0(dbglvl, "TLS negotiation failed.\n");
      return false;
   }

   return true;
}
