/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2008 Free Software Foundation Europe e.V.
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
/*
 * Kern Sibbald, May MMI
 */
/**
 * @file
 * handles authorization of Consoles, Storage and File daemons.
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

/**
 * Authenticate with a remote Storage daemon
 */
bool authenticate_with_storage_daemon(JCR *jcr, STORERES *store)
{
   BSOCK *sd = jcr->store_bsock;
   char dirname[MAX_NAME_LENGTH];
   bool auth_success = false;

   /**
    * Send my name to the Storage daemon then do authentication
    */
   bstrncpy(dirname, me->hdr.name, sizeof(dirname));
   bash_spaces(dirname);

   if (!sd->fsend(hello, dirname)) {
      Dmsg1(dbglvl, _("Error sending Hello to Storage daemon. ERR=%s\n"), bnet_strerror(sd));
      Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to Storage daemon. ERR=%s\n"), bnet_strerror(sd));
      return false;
   }

   std::vector<std::reference_wrapper<tls_base_t>> tls_resources{store->tls_cert, store->tls_psk};
   auth_success = sd->authenticate_outbound_connection(
       jcr, "Storage daemon", dirname, store->password, tls_resources);
   if (!auth_success) {
      Dmsg2(dbglvl,
            "Director unable to authenticate with Storage daemon at \"%s:%d\"\n",
            sd->host(),
            sd->port());
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

/**
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

   /**
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

std::vector<std::reference_wrapper<tls_base_t>> tls_resources{client->tls_cert, client->tls_psk};
auth_success =
    fd->authenticate_outbound_connection(jcr, "File Daemon", dirname, client->password, tls_resources);

if (!auth_success) {
   Dmsg2(dbglvl, "Unable to authenticate with File daemon at \"%s:%d\"\n", fd->host(), fd->port());
   Jmsg(jcr,
        M_FATAL,
        0,
        _("Unable to authenticate with File daemon at \"%s:%d\". Possible causes:\n"
          "Passwords or names not the same or\n"
          "TLS negotiation failed or\n"
          "Maximum Concurrent Jobs exceeded on the FD or\n"
          "FD networking messed up (restart daemon).\n"
          "Please see %s for help.\n"),
        fd->host(),
        fd->port(),
        MANUAL_AUTH_URL);
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

/**
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
      std::vector<std::reference_wrapper<tls_base_t > > tls_resources{client->tls_cert, client->tls_psk};
         auth_success = fd->authenticate_inbound_connection(
             NULL, "File Daemon", client_name, client->password, tls_resources);
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

/**
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

/**
 * Authenticate user agent.
 */
bool authenticate_user_agent(UAContext *uac)
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

   if (bstrcmp(name, "*UserAgent*")) { /* default console */
      std::vector<std::reference_wrapper<tls_base_t > > tls_resources{me->tls_cert, me->tls_psk};
      auth_success = ua->authenticate_inbound_connection(
          NULL, "Console", "*UserAgent*", me->password, tls_resources);
   } else {
      unbash_spaces(name);
      cons = (CONRES *)GetResWithName(R_CONSOLE, name);
      if (cons) {
            std::vector<std::reference_wrapper<tls_base_t > > tls_resources{cons->tls_cert, cons->tls_psk};
         auth_success =
             ua->authenticate_inbound_connection(NULL, "Console", name, cons->password, tls_resources);

         if (auth_success) {
            uac->cons = cons; /* save console resource pointer */
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
