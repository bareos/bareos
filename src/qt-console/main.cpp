/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.

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
/*
 *  Main program for bat (qt-console)
 *
 *   Kern Sibbald, January MMVII
 *
 */ 


#include "bat.h"
#include <QApplication>
#include <QTranslator>

/*
 * We need Qt version 4.8.4 or later to be able to comple correctly
 */
#if QT_VERSION < 0x040804
#error "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
#error "You need Qt version 4.8.4 or later to build Bat"
#error "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
#endif

MainWin *mainWin;
QApplication *app;

/* Forward referenced functions */
void terminate_console(int sig);                                
static void usage();
static int check_resources();

extern bool parse_bat_config(CONFIG *config, const char *configfile, int exit_code);
extern void message_callback(int /* type */, char *msg);


#define CONFIG_FILE "bat.conf"     /* default configuration file */

/* Static variables */
static CONFIG *config;
static char *configfile = NULL;

int main(int argc, char *argv[])
{
   int ch;
   bool no_signals = true;
   bool test_config = false;


   app = new QApplication(argc, argv);        
   app->setQuitOnLastWindowClosed(true);
   QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));
     
   QTranslator qtTranslator;
   qtTranslator.load(QString("qt_") + QLocale::system().name(),QLibraryInfo::location(QLibraryInfo::TranslationsPath));
   app->installTranslator(&qtTranslator);

   QTranslator batTranslator;
   batTranslator.load(QString("bat_") + QLocale::system().name(),QLibraryInfo::location(QLibraryInfo::TranslationsPath));
   app->installTranslator(&batTranslator);

   register_message_callback(message_callback);

#ifdef xENABLE_NLS
   setlocale(LC_ALL, "");
   bindtextdomain("bacula", LOCALEDIR);
   textdomain("bacula");
#endif

#ifdef HAVE_WIN32
   set_trace(true);          /* output to trace file */
#endif

   init_stack_dump();
   my_name_is(argc, argv, "bat");
   lmgr_init_thread();
   init_msg(NULL, NULL);
   working_directory  = "/tmp";

   struct sigaction sigignore;
   sigignore.sa_flags = 0;
   sigignore.sa_handler = SIG_IGN;
   sigfillset(&sigignore.sa_mask);
   sigaction(SIGPIPE, &sigignore, NULL);
   sigaction(SIGUSR2, &sigignore, NULL);


   while ((ch = getopt(argc, argv, "bc:d:r:st?")) != -1) {
      switch (ch) {
      case 'c':                    /* configuration file */
         if (configfile != NULL) {
            free(configfile);
         }
         configfile = bstrdup(optarg);
         break;

      case 'd':
         debug_level = atoi(optarg);
         if (debug_level <= 0)
            debug_level = 1;
         break;

      case 's':                    /* turn off signals */
         no_signals = true;
         break;

      case 't':
         test_config = true;
         break;

      case '?':
      default:
         usage();
      }
   }
   argc -= optind;
   argv += optind;


   if (!no_signals) {
      init_signals(terminate_console);
   }

   if (argc) {
      usage();
   }

   OSDependentInit();
#ifdef HAVE_WIN32
   WSA_Init();                        /* Initialize Windows sockets */
#endif

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   config = new_config_parser();
   parse_bat_config(config, configfile, M_ERROR_TERM);

   if (init_crypto() != 0) {
      Emsg0(M_ERROR_TERM, 0, _("Cryptography library initialization failed.\n"));
   }

   if (!check_resources()) {
      Emsg1(M_ERROR_TERM, 0, _("Please correct configuration file: %s\n"), configfile);
   }
   if (test_config) {
      exit(0);
   }

   mainWin = new MainWin;
   mainWin->show();

   return app->exec();
}

void terminate_console(int /*sig*/)
{
// WSA_Cleanup();                  /* TODO: check when we have to call it */
   exit(0);
}

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s) %s %s %s\n\n"
"Usage: bat [-s] [-c config_file] [-d debug_level] [config_file]\n"
"       -c <file>   set configuration file to file\n"
"       -dnn        set debug level to nn\n"
"       -s          no signals\n"
"       -t          test - read configuration and exit\n"
"       -?          print this message.\n"
"\n"), 2007, VERSION, BDATE, HOST_OS, DISTNAME, DISTVER);

   exit(1);
}

/*
 * Make a quick check to see that we have all the
 * resources needed.
 */
static int check_resources()
{
   bool ok = true;
   DIRRES *director;
   int numdir;
   bool tls_needed;

   LockRes();

   numdir = 0;
   foreach_res(director, R_DIRECTOR) {
      numdir++;
      /* tls_require implies tls_enable */
      if (director->tls_require) {
         if (have_tls) {
            director->tls_enable = true;
         } else {
            Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bacula.\n"));
            ok = false;
            continue;
         }
      }
      tls_needed = director->tls_enable || director->tls_authenticate;

      if ((!director->tls_ca_certfile && !director->tls_ca_certdir) && tls_needed) {
         Emsg2(M_FATAL, 0, _("Neither \"TLS CA Certificate\""
                             " or \"TLS CA Certificate Dir\" are defined for Director \"%s\" in %s."
                             " At least one CA certificate store is required.\n"),
                             director->hdr.name, configfile);
         ok = false;
      }
   }
   
   if (numdir == 0) {
      Emsg1(M_FATAL, 0, _("No Director resource defined in %s\n"
                          "Without that I don't how to speak to the Director :-(\n"), configfile);
      ok = false;
   }

   CONRES *cons;
   /* Loop over Consoles */
   foreach_res(cons, R_CONSOLE) {
      /* tls_require implies tls_enable */
      if (cons->tls_require) {
         if (have_tls) {
            cons->tls_enable = true;
         } else {
            Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bacula.\n"));
            ok = false;
            continue;
         }
      }
      tls_needed = cons->tls_enable || cons->tls_authenticate;

      if ((!cons->tls_ca_certfile && !cons->tls_ca_certdir) && tls_needed) {
         Emsg2(M_FATAL, 0, _("Neither \"TLS CA Certificate\""
                             " or \"TLS CA Certificate Dir\" are defined for Console \"%s\" in %s.\n"),
                             cons->hdr.name, configfile);
         ok = false;
      }
   }

   UnlockRes();

   return ok;
}
