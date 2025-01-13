/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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

#include <signal.h>
#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif

#include "mainwindow.h"
#include "tray-monitor.h"
#include "authenticate.h"
#include "monitoritem.h"
#include "monitoritemthread.h"
#include "lib/bsignal.h"
#include "lib/parse_conf.h"
#include "lib/cli.h"
#include "lib/crypto.h"
#include "include/exit_codes.h"

ConfigurationParser* my_config = nullptr;

/* QCoreApplication* app will be initialized with:
 * - QApplication for normal execution with a GUI or
 * - QCoreApplication for tests when no GUI must be used */
static QCoreApplication* app = nullptr;

static void ParseCommandLine(int argc, char* argv[], cl_opts& cl)
{
  CLI::App tray_monitor_app;
  InitCLIApp(tray_monitor_app, "The Bareos Tray Monitor.", 2004);

  tray_monitor_app
      .add_option(
          "-c,--config",
          [&cl](std::vector<std::string> val) {
            if (cl.configfile_) { free(static_cast<void*>(cl.configfile_)); }
            cl.configfile_ = strdup(val.front().c_str());
            return true;
          },
          "Use <path> as configuration file or directory.")
      ->check(CLI::ExistingPath)
      ->type_name("<path>");

  AddDebugOptions(tray_monitor_app);

  tray_monitor_app.add_flag("-t,--test-config", cl.test_config_only_,
                            "Test - read configuration and exit.");

  CLI::Option* xc
      = tray_monitor_app.add_flag("--xc,--export-config", cl.export_config_,
                                  "Print configuration resources and exit.");


  tray_monitor_app
      .add_flag("--xs,--export-schema", cl.export_config_schema_,
                "Print configuration schema in JSON format and exit.")
      ->excludes(xc);

  AddDeprecatedExportOptionsHelp(tray_monitor_app);

  tray_monitor_app.add_flag("--rc,--test-connection",
                            cl.do_connection_test_only_,
                            "Test connection only.");

  try {
    (tray_monitor_app).parse((argc), (argv));
  } catch (const CLI::ParseError& e) {
    exit((tray_monitor_app).exit(e));
  }
}

static std::pair<int, const char*> debug_header(QtMsgType type)
{
  switch (type) {
    case QtDebugMsg:
      return {500, "Qt-Debug"};
    case QtInfoMsg:
      return {200, "Qt-Info"};
    case QtWarningMsg:
      return {100, "Qt-Warning"};
    case QtCriticalMsg:
      return {40, "Qt-Critical"};
    case QtFatalMsg:
      return {20, "Qt-Fatal"};
  }

  return {5, "Qt-Unknown"};
}

QtMessageHandler original_handler{nullptr};

static void QtMessageOutputHandler(QtMsgType type,
                                   const QMessageLogContext& context,
                                   const QString& msg)
{
  QByteArray localMsg = msg.toLocal8Bit();
  const char* file = context.file ? context.file : "<unknown>";
  const char* function = context.function ? context.function : nullptr;

  auto [level, name] = debug_header(type);

  if (function) {
    d_msg(file, context.line, level, "%s %s: %s\n", function, name,
          localMsg.constData());
  } else {
    d_msg(file, context.line, level, "%s: %s\n", name, localMsg.constData());
  }

  if (original_handler) { original_handler(type, context, msg); }
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
    exit(BEXIT_SUCCESS);
  }

  // read the config file
  my_config = InitTmonConfig(cl.configfile_, M_CONFIG_ERROR);
  my_config->ParseConfigOrExit();

  if (cl.export_config_) {
    my_config->DumpResources(PrintMessage, NULL);
    exit(0);
  }

  if (InitCrypto() != 0) {
    Emsg0(M_ERROR_TERM, 0, T_("Cryptography library initialization failed.\n"));
  }

  original_handler = qInstallMessageHandler(QtMessageOutputHandler);

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
