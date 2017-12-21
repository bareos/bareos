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
#ifndef MONITORITEMTHREAD_H_INCLUDED
#define MONITORITEMTHREAD_H_INCLUDED

#include "tray-monitor.h"

#include <QThread>
#include <QList>

class MonitorItem;
class MONITORRES;
class QTimer;

class MonitorItemThread : public QThread
{
   Q_OBJECT

public:
   static MonitorItemThread* instance();
   static void destruct();

   Qt::HANDLE getThreadId();
   QStringList createRes(const cl_opts& cl);
   MONITORRES* getMonitor() const;
   MonitorItem *getDirector() const;

protected:
   virtual void run();
   void dotest();

private:
   MonitorItemThread(QObject* parent = Q_NULLPTR);
   Q_DISABLE_COPY(MonitorItemThread)
   virtual ~MonitorItemThread();

   static MonitorItemThread* monitorItemThreadSingleton;
   static bool already_destroyed;
   MONITORRES* monitor;
   Qt::HANDLE threadId;

   QList<MonitorItem*> items;
   QTimer* refreshTimer;
   bool isRefreshing;

public Q_SLOTS:
   /* auto-connected slots to the Refresh Timer     */
   void on_RefreshTimer_timeout();
   /* ********************************************* */

   void onRefreshItems();

Q_SIGNALS:
   void refreshItemsReady();
};

#endif // MONITORITEMTHREAD_H_INCLUDED
