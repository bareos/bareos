/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/

#include "tray-ui.h"

int doconnect(monitoritem* item);
int docmd(monitoritem* item, const char* command);
static int authenticate_daemon(monitoritem* item, JCR *jcr);
/* Imported functions */
int authenticate_director(JCR *jcr, MONITOR *monitor, DIRRES *director);
int authenticate_file_daemon(JCR *jcr, MONITOR *monitor, CLIENT* client);
int authenticate_storage_daemon(JCR *jcr, MONITOR *monitor, STORE* store);
extern bool parse_tmon_config(CONFIG *config, const char *configfile, int exit_code);
void get_list(monitoritem* item, const char *cmd, QStringList &lst);

/* Dummy functions */
int generate_daemon_event(JCR *, const char *) { return 1; }

/* Static variables */
static char *configfile = NULL;
static MONITOR *monitor;
static JCR jcr;
static int nitems = 0;
static monitoritem items[32];
static CONFIG *config;
static TrayUI *tray;

/* Data received from DIR/FD/SD */
//static char OKqstatus[]   = "%c000 OK .status\n";
//static char DotStatusJob[] = "JobId=%d JobStatus=%c JobErrors=%d\n";


void updateStatusIcon(monitoritem* item);
void changeStatusMessage(monitoritem* item, const char *fmt,...);

#define CONFIG_FILE "./tray-monitor.conf"   /* default configuration file */

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s) %s %s %s\n\n"
"Usage: tray-monitor [-c config_file] [-d debug_level]\n"
"       -c <file>     set configuration file to file\n"
"       -d <nn>       set debug level to <nn>\n"
"       -dt           print timestamp in debug output\n"
"       -t            test - read configuration and exit\n"
"       -?            print this message.\n"
"\n"), 2004, VERSION, BDATE, HOST_OS, DISTNAME, DISTVER);
}

int sm_line = 0;

void dotest()
{
   for (int i = 0; i < nitems; i++) {
      const char *cmd;

      switch (items[i].type) {
      case R_DIRECTOR:
         cmd = ".jobs type=B";
         tray->clearText(items[i].get_name());
         docmd(&items[i], cmd);
         break;
      default:
         break;
      }
   }
}

void get_list(monitoritem *item, const char *cmd, QStringList &lst)
{
   int stat;
   
   doconnect(item);
   item->writecmd(cmd);
   while((stat = bnet_recv(item->D_sock)) >= 0) {
      strip_trailing_junk(item->D_sock->msg);
      if (*(item->D_sock->msg)) {
         lst << QString(item->D_sock->msg);
      }
   }
}

void refresh_item()
{
   for (int i = 0; i < nitems; i++) {
      const char *cmd;
      tray->clearText(items[i].get_name());
      switch (items[i].type) {
      case R_DIRECTOR:
         cmd = "status dir";
         break;
      case R_CLIENT:
         cmd = "status";
         break;
      case R_STORAGE:
         cmd = "status";
         break;          
      default:
         exit(1);
         break;
      }
      docmd(&items[i], cmd);
   }
}

/*********************************************************************
 *
 *         Main Bacula Tray Monitor -- User Interface Program
 *
 */
int main(int argc, char *argv[])
{   
   int ch, i, dir_index=-1;
   bool test_config = false;
   DIRRES* dird;
   CLIENT* filed;
   STORE* stored;

   setlocale(LC_ALL, "");
   bindtextdomain("bacula", LOCALEDIR);
   textdomain("bacula");

   init_stack_dump();
   my_name_is(argc, argv, "tray-monitor");
   lmgr_init_thread();
   init_msg(NULL, NULL);
   working_directory = "/tmp";

   struct sigaction sigignore;
   sigignore.sa_flags = 0;
   sigignore.sa_handler = SIG_IGN;
   sigfillset(&sigignore.sa_mask);
   sigaction(SIGPIPE, &sigignore, NULL);

   while ((ch = getopt(argc, argv, "bc:d:th?f:s:")) != -1) {
      switch (ch) {
      case 'c':                    /* configuration file */
         if (configfile != NULL) {
            free(configfile);
         }
         configfile = bstrdup(optarg);
         break;

      case 'd':
         if (*optarg == 't') {
            dbg_timestamp = true;
         } else {
            debug_level = atoi(optarg);
            if (debug_level <= 0) {
               debug_level = 1;
            }
         }
         break;

      case 't':
         test_config = true;
         break;

      case 'h':
      case '?':
      default:
         usage();
         exit(1);
      }
   }
   argc -= optind;
   //argv += optind;

   if (argc) {
      usage();
      exit(1);
   }

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   config = new_config_parser();
   parse_tmon_config(config, configfile, M_ERROR_TERM);

   LockRes();
   nitems = 0;
   foreach_res(monitor, R_MONITOR) {
      nitems++;
   }

   if (nitems != 1) {
      Emsg2(M_ERROR_TERM, 0,
         _("Error: %d Monitor resources defined in %s. You must define one and only one Monitor resource.\n"), nitems, configfile);
   }

   nitems = 0;
   foreach_res(dird, R_DIRECTOR) {
      dir_index=nitems;
      items[nitems].type = R_DIRECTOR;
      items[nitems].resource = dird;
      items[nitems].D_sock = NULL;
      items[nitems].state = warn;
      items[nitems].oldstate = warn;
      nitems++;
   }
   foreach_res(filed, R_CLIENT) {
      items[nitems].type = R_CLIENT;
      items[nitems].resource = filed;
      items[nitems].D_sock = NULL;
      items[nitems].state = warn;
      items[nitems].oldstate = warn;
      nitems++;
   }
   foreach_res(stored, R_STORAGE) {
      items[nitems].type = R_STORAGE;
      items[nitems].resource = stored;
      items[nitems].D_sock = NULL;
      items[nitems].state = warn;
      items[nitems].oldstate = warn;
      nitems++;
   }
   UnlockRes();

   if (nitems == 0) {
      Emsg1(M_ERROR_TERM, 0, _("No Client, Storage or Director resource defined in %s\n"
"Without that I don't how to get status from the File, Storage or Director Daemon :-(\n"), configfile);
   }

   if (test_config) {
      exit(0);
   }

   (void)WSA_Init();                /* Initialize Windows sockets */

   LockRes();
   monitor = (MONITOR*)GetNextRes(R_MONITOR, (RES *)NULL);
   UnlockRes();

   if ((monitor->RefreshInterval < 1) || (monitor->RefreshInterval > 600)) {
      Emsg2(M_ERROR_TERM, 0, _("Invalid refresh interval defined in %s\n"
"This value must be greater or equal to 1 second and less or equal to 10 minutes (read value: %d).\n"), configfile, monitor->RefreshInterval);
   }

   sm_line = 0;
   QApplication    app(argc, argv);
   app.setQuitOnLastWindowClosed(false);
   tray = new TrayUI();
   tray->setupUi(tray);
   tray->spinRefresh->setValue(monitor->RefreshInterval);
   if (dir_index >= 0) {
      tray->addDirector(&items[dir_index]);
   }

   for (i = 0; i < nitems; i++) {
      const char *cmd;
      tray->addTab(items[i].get_name());
      switch (items[i].type) {
      case R_DIRECTOR:
         tray->addDirector(&items[i]);
         cmd = "status dir";
         break;
      case R_CLIENT:
         cmd = "status";
         break;
      case R_STORAGE:
         cmd = "status";
         break;          
      default:
         exit(1);
         break;
      }
      docmd(&items[i], cmd);
   }

   tray->startTimer();

   app.exec();

   for (i = 0; i < nitems; i++) {
      if (items[i].D_sock) {
         items[i].writecmd("quit");
         if (items[i].D_sock) {
            bnet_sig(items[i].D_sock, BNET_TERMINATE); /* send EOF */
            bnet_close(items[i].D_sock);
         }
      }
   }


   (void)WSACleanup();               /* Cleanup Windows sockets */
   
   config->free_resources();
   free(config);
   config = NULL;
   term_msg();
   return 0;
}

static int authenticate_daemon(monitoritem* item, JCR *jcr) {
   switch (item->type) {
   case R_DIRECTOR:
      return authenticate_director(jcr, monitor, (DIRRES*)item->resource);
   case R_CLIENT:
      return authenticate_file_daemon(jcr, monitor, (CLIENT*)item->resource);
   case R_STORAGE:
      return authenticate_storage_daemon(jcr, monitor, (STORE*)item->resource);
   default:
      printf(_("Error, currentitem is not a Client or a Storage..\n"));
      return FALSE;
   }
   return false;
}

void changeStatusMessage(monitoritem*, const char *fmt,...) {
   char buf[512];
   va_list arg_ptr;

   va_start(arg_ptr, fmt);
   bvsnprintf(buf, sizeof(buf), (char *)fmt, arg_ptr);
   va_end(arg_ptr);
   tray->statusbar->showMessage(QString(buf));
}

int doconnect(monitoritem* item) 
{
   if (!item->D_sock) {
      memset(&jcr, 0, sizeof(jcr));

      DIRRES* dird;
      CLIENT* filed;
      STORE* stored;

      switch (item->type) {
      case R_DIRECTOR:
         dird = (DIRRES*)item->resource;
         changeStatusMessage(item, _("Connecting to Director %s:%d"), dird->address, dird->DIRport);
         item->D_sock = bnet_connect(NULL, monitor->DIRConnectTimeout, 
                                     0, 0, _("Director daemon"), dird->address, NULL, dird->DIRport, 0);
         jcr.dir_bsock = item->D_sock;
         break;
      case R_CLIENT:
         filed = (CLIENT*)item->resource;
         changeStatusMessage(item, _("Connecting to Client %s:%d"), filed->address, filed->FDport);
         item->D_sock = bnet_connect(NULL, monitor->FDConnectTimeout, 
                                     0, 0, _("File daemon"), filed->address, NULL, filed->FDport, 0);
         jcr.file_bsock = item->D_sock;
         break;
      case R_STORAGE:
         stored = (STORE*)item->resource;
         changeStatusMessage(item, _("Connecting to Storage %s:%d"), stored->address, stored->SDport);
         item->D_sock = bnet_connect(NULL, monitor->SDConnectTimeout, 
                                     0, 0, _("Storage daemon"), stored->address, NULL, stored->SDport, 0);
         jcr.store_bsock = item->D_sock;
         break;
      default:
         printf(_("Error, currentitem is not a Client, a Storage or a Director..\n"));
         return 0;
      }

      if (item->D_sock == NULL) {
         changeStatusMessage(item, _("Cannot connect to daemon."));
         item->state = error;
         item->oldstate = error;
         return 0;
      }

      if (!authenticate_daemon(item, &jcr)) {
         item->state = error;
         item->oldstate = error;
         changeStatusMessage(item, _("Authentication error : %s"), item->D_sock->msg);
         item->D_sock = NULL;
         return 0;
      }

      switch (item->type) {
      case R_DIRECTOR:
         changeStatusMessage(item, _("Opened connection with Director daemon."));
         break;
      case R_CLIENT:
         changeStatusMessage(item, _("Opened connection with File daemon."));
         break;
      case R_STORAGE:
         changeStatusMessage(item, _("Opened connection with Storage daemon."));
         break;
      default:
         printf(_("Error, currentitem is not a Client, a Storage or a Director..\n"));
         return 0;
         break;
      }

      if (item->type == R_DIRECTOR) { /* Read connection messages... */
         docmd(item, ""); /* Usually invalid, but no matter */
      }
   }
   return 1;
}

int docmd(monitoritem* item, const char* command) 
{
   int stat;
   //qDebug() << "docmd(" << item->get_name() << "," << command << ")";
   if (!doconnect(item)) {
      return 0;
   }

   if (command[0] != 0)
      item->writecmd(command);

   while(1) {
      if ((stat = bnet_recv(item->D_sock)) >= 0) {
         strip_trailing_newline(item->D_sock->msg);
         tray->appendText(item->get_name(), item->D_sock->msg);
      }
      else if (stat == BNET_SIGNAL) {
         if (item->D_sock->msglen == BNET_EOD) {
            // qDebug() << "<< EOD >>";
            return 1;
         }
         else if (item->D_sock->msglen == BNET_SUB_PROMPT) {
            // qDebug() << "<< PROMPT >>";
            return 0;
         }
         else if (item->D_sock->msglen == BNET_HEARTBEAT) {
            bnet_sig(item->D_sock, BNET_HB_RESPONSE);
         }
         else {
            qDebug() << bnet_sig_to_ascii(item->D_sock);
         }
      }
      else { /* BNET_HARDEOF || BNET_ERROR */
         item->D_sock = NULL;
         item->state = error;
         item->oldstate = error;
         changeStatusMessage(item, _("Error : BNET_HARDEOF or BNET_ERROR"));
         //fprintf(stderr, _("<< ERROR >>\n"));
         return 0;
      }

      if (is_bnet_stop(item->D_sock)) {
         item->D_sock = NULL;
         item->state = error;
         item->oldstate = error;
         changeStatusMessage(item, _("Error : Connection closed."));
         //fprintf(stderr, "<< STOP >>\n");
         return 0;            /* error or term */
      }
   }
}
