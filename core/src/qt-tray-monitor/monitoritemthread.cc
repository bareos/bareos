/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
#include <QDebug>
#include <QTimer>

#include "mainwindow.h"
#include "monitoritemthread.h"
#include "tray-monitor.h"
#include "monitoritem.h"
#include "tray_conf.h"
#include "lib/parse_conf.h"

MonitorItemThread* MonitorItemThread::monitorItemThreadSingleton;
bool MonitorItemThread::already_destroyed = false;

MonitorItemThread::MonitorItemThread(QObject* parent)
    : QThread(parent)
    , monitor(NULL)
    , refreshTimer(new QTimer(this))
    , isRefreshing(false)
{
  threadId = currentThreadId();

  refreshTimer->setObjectName("RefreshTimer");
  QMetaObject::connectSlotsByName(this);
}

MonitorItemThread::~MonitorItemThread() { return; }

MonitorItemThread* MonitorItemThread::instance()
{
  // improve that the MonitorItemThread is created
  // and deleted only once during program execution
  Q_ASSERT(!already_destroyed);

  if (!monitorItemThreadSingleton) {
    monitorItemThreadSingleton = new MonitorItemThread;
  }
  return monitorItemThreadSingleton;
}

void MonitorItemThread::destruct()
{
  if (monitorItemThreadSingleton) {
    monitorItemThreadSingleton->exit(0);
    monitorItemThreadSingleton->wait(20000);
    delete monitorItemThreadSingleton;
    monitorItemThreadSingleton = NULL;
    already_destroyed = true;
  }
}

Qt::HANDLE MonitorItemThread::getThreadId() { return threadId; }

void MonitorItemThread::run()
{
  /* all this must be run in the same
   * context of the MonitorItemThread  */

  if (monitor) { refreshTimer->start(monitor->RefreshInterval * 1000); }

  exec();

  while (items.count()) {
    MonitorItem* item = items.first();
    item->disconnect();
    delete item;
    items.removeFirst();
  }

  TermMsg();  // this cannot be called twice, however
}

QStringList MonitorItemThread::createRes(const cl_opts& cl)
{
  QStringList tabRefs;

  LockRes(my_config);

  int monitorItems = 0;
  MonitorResource* monitorRes;
  foreach_res (monitorRes, R_MONITOR) {
    monitorItems++;
  }

  if (monitorItems != 1) {
    const std::string& configfile = my_config->get_base_config_path();
    Emsg2(M_ERROR_TERM, 0,
          _("Error: %d Monitor resources defined in %s. "
            "You must define one and only one Monitor resource.\n"),
          monitorItems, configfile.c_str());
  }

  monitor = reinterpret_cast<MonitorResource*>(
      my_config->GetNextRes(R_MONITOR, (BareosResource*)NULL));

  int nitems = 0;

  DirectorResource* dird;
  foreach_res (dird, R_DIRECTOR) {
    MonitorItem* item = new MonitorItem;
    item->setType(R_DIRECTOR);
    item->setResource(dird);
    item->setConnectTimeout(monitor->DIRConnectTimeout);
    if (!cl.do_connection_test_only_) {
      item->connectToMainWindow(MainWindow::instance());
    }
    tabRefs.append(item->get_name());
    items.append(item);
    nitems++;
  }

  ClientResource* filed;
  foreach_res (filed, R_CLIENT) {
    MonitorItem* item = new MonitorItem;
    item->setType(R_CLIENT);
    item->setResource(filed);
    item->setConnectTimeout(monitor->FDConnectTimeout);
    if (!cl.do_connection_test_only_) {
      item->connectToMainWindow(MainWindow::instance());
    }
    tabRefs.append(item->get_name());
    items.append(item);
    nitems++;
  }

  StorageResource* stored;
  foreach_res (stored, R_STORAGE) {
    MonitorItem* item = new MonitorItem;
    item->setType(R_STORAGE);
    item->setResource(stored);
    item->setConnectTimeout(monitor->SDConnectTimeout);
    if (!cl.do_connection_test_only_) {
      item->connectToMainWindow(MainWindow::instance());
    }
    tabRefs.append(item->get_name());
    items.append(item);
    nitems++;
  }

  UnlockRes(my_config);

  if (nitems == 0) {
    Emsg1(M_ERROR_TERM, 0,
          "No Client, Storage or Director resource defined in %s\n"
          "Without that I don't know how to get status from the File, "
          "Storage or Director Daemon :-(\n",
          cl.configfile_);
  }

  // check the refresh intervals for reasonable values
  int interval = monitor->RefreshInterval;
  if ((interval < 1) || (interval > 600)) {
    Emsg2(M_ERROR_TERM, 0,
          "Invalid refresh interval defined in %s\n"
          "This value must be greater or equal to 1 second and less or "
          "equal to 10 minutes (read value: %d).\n",
          cl.configfile_, interval);
  }

  return tabRefs;
}

void MonitorItemThread::onRefreshItems()
{
  if (!isRefreshing) {
    isRefreshing = true;
    for (int i = 0; i < items.count(); i++) { items[i]->GetStatus(); }
    emit refreshItemsReady();
    isRefreshing = false;
  }
}

bool MonitorItemThread::doConnectionTest()
{
  int failed = 0;
  for (int i = 0; i < items.count(); i++) {
    bool success = items[i]->doconnect();
    if (success) {
      items[i]->disconnect();
    } else {
      ++failed;
    }
  }
  return failed ? false : true;
}

void MonitorItemThread::on_RefreshTimer_timeout() { onRefreshItems(); }

MonitorResource* MonitorItemThread::getMonitor() const { return monitor; }

MonitorItem* MonitorItemThread::getDirector() const
{
  // search for the first occurrence of a director

  int count = items.count();
  for (int i = 0; i < count; i++) {
    if (items[i]->type() == R_DIRECTOR) { return items[i]; }
  }
  return NULL;
}


/************* Testing ***************/

void MonitorItemThread::dotest()
{
  const char* cmd;
  int count = items.count();

  for (int i = 0; i < count; i++) {
    switch (items[i]->type()) {
      case R_DIRECTOR:
        cmd = ".jobs type=B";
        items[i]->docmd(cmd);
        break;
      default:
        break;
    }
  }
}
