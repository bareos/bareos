/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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
#include "lib/parse_conf.h"

ConfigurationParser* my_config = nullptr;

/* QCoreApplication* app will be initialized with:
 * - QApplication for normal execution with a GUI or
 * - QCoreApplication for tests when no GUI must be used */
static QCoreApplication* app = nullptr;

static void usage()
{
  std::vector<char> copyright(1024);
  kBareosVersionStrings.FormatCopyrightWithFsfAndPlanetsWithFsfAndPlanets(
      copyright.data(), copyright.size(), 2004);
  QString out =
      QString(_("%1"
                "Usage: tray-monitor [options]\n"
                "        -c <path>   use <path> as configuration file or "
                "directory\n"
                "        -d <nn>     set debug level to <nn>\n"
                "        -dt         print timestamp in debug output\n"
                "        -t          test - read configuration and exit\n"
                "        -rc         test - do connection test\n"
                "        -xc         print configuration and exit\n"
                "        -xs         print configuration file schema in JSON "
                "format "
                "and exit\n"
                "        -?          print this message.\n"
                "\n"))
          .arg(copyright.data());

#if HAVE_WIN32
  QMessageBox::information(0, "Help", out);
#else
  fprintf(stderr, "%s", out.toUtf8().data());
#endif
}

static void ParseCommandLine(int argc, char* argv[], cl_opts& cl)
{
  int ch;
  while ((ch = getopt(argc, argv, "bc:d:th?f:r:s:x:")) != -1) {
    switch (ch) {
      case 'c': /* configuration file */
        if (cl.configfile_) { free(static_cast<void*>(cl.configfile_)); }
        cl.configfile_ = strdup(optarg);
        break;

      case 'd':
        if (*optarg == 't') {
          dbg_timestamp = true;
        } else {
          debug_level = atoi(optarg);
          if (debug_level <= 0) { debug_level = 1; }
        }
        break;

      case 't':
        cl.test_config_only_ = true;
        break;

      case 'x': /* export configuration/schema and exit */
        if (*optarg == 's') {
          cl.export_config_schema_ = true;
        } else if (*optarg == 'c') {
          cl.export_config_ = true;
        } else {
          usage();
          exit(1);
        }
        break;

      case 'r':
        if ((*optarg) == 'c') {
          cl.do_connection_test_only_ = true;
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
  // argv += optind;

  if (argc) {
    usage();
    exit(1);
  }
}

static void setupQtObjects()
{
  MonitorItemThread* thr = MonitorItemThread::instance();
  MainWindow* win = MainWindow::instance();

  QObject::connect(win, SIGNAL(refreshItems()), thr, SLOT(onRefreshItems()),
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

  MonitorItemThread::destruct();  // disconnects network
  MainWindow::destruct();         // destroys the tray-icon

  if (app) {
    delete app;
    app = nullptr;
  }

  if (my_config) {
    delete my_config;
    my_config = nullptr;
  }

  WSACleanup(); /* Cleanup Windows sockets */
}

void intHandler(int) { exit(0); }

static void InitEnvironment(int argc, char* argv[])
{
  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  InitStackDump();
  MyNameIs(argc, argv, "tray-monitor");
  InitMsg(NULL, NULL);
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
int main(int argc, char* argv[])
{
  InitEnvironment(argc, argv);

  cl_opts cl;  // remember some command line options
  ParseCommandLine(argc, argv, cl);

  if (cl.export_config_schema_) {
    PoolMem buffer;

    my_config = InitTmonConfig(cl.configfile_, M_ERROR_TERM);
    PrintConfigSchemaJson(buffer);
    printf("%s\n", buffer.c_str());
    fflush(stdout);
    exit(0);
  }

  // read the config file
  my_config = InitTmonConfig(cl.configfile_, M_ERROR_TERM);
  my_config->ParseConfig();

  if (cl.export_config_) {
    my_config->DumpResources(PrintMessage, NULL);
    exit(0);
  }

  if (InitCrypto() != 0) {
    Emsg0(M_ERROR_TERM, 0, _("Cryptography library initialization failed.\n"));
  }

  if (cl.do_connection_test_only_) {
    /* do not initialize a GUI */
    app = new QCoreApplication(argc, argv);
  } else {
    QApplication* p = new QApplication(argc, argv);
    p->setQuitOnLastWindowClosed(false);
    app = p;
  }

  if (!cl.do_connection_test_only_) { setupQtObjects(); }

  QStringList tabRefs = MonitorItemThread::instance()->createRes(cl);

  int ret = 0;
  if (cl.do_connection_test_only_) {
    ret = MonitorItemThread::instance()->doConnectionTest() ? 0 : 1;
  } else {
    MainWindow::instance()->addTabs(tabRefs);

    if (cl.test_config_only_) { exit(0); }

    MonitorItemThread::instance()->start();

    ret = app->exec();
  }

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
