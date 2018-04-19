/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.
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

#include <QDebug>
#include <QList>
#include <QPointer>
#include <QApplication>
#include <QMessageBox>

#include "mainwindow.h"
#include "tray-monitor.h"
#include "authenticate.h"
#include "monitoritem.h"
#include "monitoritemthread.h"
#include "lib/bsignal.h"

ConfigurationParser *my_config = NULL;             /* Our Global config */

/* Imported function from tray_conf.cpp */
extern bool parse_tmon_config(ConfigurationParser *config, const char *configfile, int exit_code);

/* Static variables */
static QApplication* app = NULL;

static void usage()
{
   QString out;

   out = out.sprintf(_(PROG_COPYRIGHT
                       "\nVersion: %s (%s) %s %s %s\n\n"
                       "Usage: tray-monitor [options]\n"
                       "        -c <path>   use <path> as configuration file or directory\n"
                       "        -d <nn>     set debug level to <nn>\n"
                       "        -dt         print timestamp in debug output\n"
                       "        -t          test - read configuration and exit\n"
                       "        -xc         print configuration and exit\n"
                       "        -xs         print configuration file schema in JSON format and exit\n"
                       "        -?          print this message.\n"
                       "\n"),
                     2004, VERSION, BDATE, HOST_OS, DISTNAME, DISTVER);

#if HAVE_WIN32
   QMessageBox::information(0, "Help", out);
#else
   fprintf(stderr, "%s", out.toUtf8().data());
#endif
}

static void parse_command_line(int argc, char* argv[], cl_opts& cl)
{
   int ch;
   while ((ch = getopt(argc, argv, "bc:d:th?f:s:x:")) != -1) {
      switch (ch) {
      case 'c':                    /* configuration file */
         if (cl.configfile) {
            free(static_cast<void*>(cl.configfile));
         }
         cl.configfile = bstrdup(optarg);
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
         cl.test_config_only = true;
         break;

      case 'x':                    /* export configuration/schema and exit */
         if (*optarg == 's') {
            cl.export_config_schema = true;
         } else if (*optarg == 'c') {
            cl.export_config = true;
         } else {
            usage();
            exit(1);
         }
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
}

static void setupQtObjects()
{
    MonitorItemThread* thr = MonitorItemThread::instance();
    MainWindow* win = MainWindow::instance();

    QObject::connect(win, SIGNAL(refreshItems()),
                     thr, SLOT(onRefreshItems()),
                     Qt::QueuedConnection);

    // move the thread exec handler
    // into its own context
    thr->moveToThread(thr);
}

static void cleanup()
{
   static bool terminated = false;

   if (terminated) {
   // don't call it twice
      return;
   }

   terminated = true;

   MonitorItemThread::destruct(); //disconnects network
   MainWindow::destruct(); //destroys the tray-icon

   if(app) {
      delete app;
      app = NULL;
   }

   if (my_config) {
      my_config->free_resources();
      free(my_config);
      my_config = NULL;
   }

   WSACleanup(); /* Cleanup Windows sockets */
}

void intHandler(int)
{
   exit(0);
}

static void init_environment(int argc, char* argv[])
{
   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");

   init_stack_dump();
   my_name_is(argc, argv, "tray-monitor");
   init_msg(NULL, NULL);
   signal(SIGINT, intHandler);
   working_directory = "/tmp";
   OSDependentInit();
   WSA_Init(); /* Initialize Windows sockets */
}

/*********************************************************************
 *
 *         Main Bareos Tray Monitor -- User Interface Program
 *
 */
int main(int argc, char *argv[])
{
   init_environment(argc, argv);

   cl_opts cl; // remember some command line options
   parse_command_line(argc, argv, cl);

   if (cl.export_config_schema) {
      PoolMem buffer;

      my_config = new_config_parser();
      init_tmon_config(my_config, cl.configfile, M_ERROR_TERM);
      print_config_schema_json(buffer);
      printf("%s\n", buffer.c_str());
      fflush(stdout);
      exit(0);
   }

   // read the config file
   my_config = new_config_parser();
   parse_tmon_config(my_config, cl.configfile, M_ERROR_TERM);

   if (cl.export_config) {
      my_config->dump_resources(prtmsg, NULL);
      exit(0);
   }

   // this is the Qt core application
   // with its message handler
   app = new QApplication(argc, argv);
   app->setQuitOnLastWindowClosed(false);

   setupQtObjects();

   // create the monitoritems
   QStringList tabRefs = MonitorItemThread::instance()->createRes(cl);
   MainWindow::instance()->addTabs(tabRefs);

   // exit() if it was only a test
   if (cl.test_config_only) {
      exit(0);
   }

   MonitorItemThread::instance()->start();

   // launch the QApplication message handler
   int ret = app->exec();

   // cleanup everything before finishing
   cleanup();

   return ret;
}
/* This is a replacement for the std::exit() handler */
void exit(int status)
{
   /* avoid to call the std::exit() cleanup handlers
    * via atexit() since exit() destroys objects that
    * are used by the QApplication class and this would
    * lead to a sementation fault when QApplication
    * in turn wants to destroy its child-objects. */

   // first do the Qt cleanup
   cleanup();

   // do the kernel cleanup
   _exit(status);
}
