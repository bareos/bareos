/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
#ifndef BAREOS_QT_TRAY_MONITOR_MONITORITEMTHREAD_H_
#define BAREOS_QT_TRAY_MONITOR_MONITORITEMTHREAD_H_

#include "tray-monitor.h"

#include <QThread>
#include <QList>

class MonitorItem;
class MonitorResource;
class QTimer;

class MonitorItemThread : public QThread {
  Q_OBJECT

 public:
  static MonitorItemThread* instance();
  static void destruct();

  Qt::HANDLE getThreadId();
  QStringList createRes(const cl_opts& cl);
  MonitorResource* getMonitor() const;
  MonitorItem* getDirector() const;
  bool doConnectionTest();

 protected:
  virtual void run() override;
  void dotest();

 private:
  MonitorItemThread(QObject* parent = 0);
  Q_DISABLE_COPY(MonitorItemThread);
  virtual ~MonitorItemThread();

  static MonitorItemThread* monitorItemThreadSingleton;
  static bool already_destroyed;
  MonitorResource* monitor;
  Qt::HANDLE threadId;

  QList<MonitorItem*> items;
  QTimer* refreshTimer;
  bool isRefreshing;

 public slots:
  /* auto-connected slots to the Refresh Timer     */
  void on_RefreshTimer_timeout();
  /* ********************************************* */

  void onRefreshItems();

 signals:
  void refreshItemsReady();
};

#endif  // BAREOS_QT_TRAY_MONITOR_MONITORITEMTHREAD_H_
