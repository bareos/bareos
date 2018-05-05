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

#ifndef MONITORITEM_H_INCLUDED
#define MONITORITEM_H_INCLUDED

#include <QString>
#include <QStringList>
#include <QObject>

#include "include/bareos.h"
#include "tray_conf.h"
#include "jcr.h"
#include "tray-monitor.h"

class MonitorItemPrivate;

class MonitorItem : public QObject
{
   Q_OBJECT

public:

   enum StateEnum
   {
      Idle = 0,
      Running = 1,
      Warn = 2,
      Error = 3
   };

   class JobDefaults
   {
   public:
      QString job_name;
      QString pool_name;
      QString messages_name;
      QString client_name;
      QString StoreName;
      QString where;
      QString level;
      QString type;
      QString fileset_name;
      QString catalog_name;
      bool enabled;
   };

   class Resources {
   public:
      QStringList job_list;
      QStringList pool_list;
      QStringList client_list;
      QStringList storage_list;
      QStringList levels;
      QStringList fileset_list;
      QStringList messages_list;
   };

public:
   MonitorItem(QObject* parent = 0);
   ~MonitorItem();

   char *get_name() const;
   void writecmd(const char* command);
   bool GetJobDefaults(struct JobDefaults &job_defs);
   bool doconnect();
   void disconnect();
   bool docmd(const char* command);
   void connectToMainWindow(QObject* mainWindow);
   void get_list(const char *cmd, QStringList &lst);
   void GetStatus();

   Rescode type() const;
   void* resource() const;
   BareosSocket* DSock() const;
   StateEnum state() const;
   int connectTimeout() const;

   void setType(Rescode type);
   void setResource(void* resource);
   void setDSock(BareosSocket* DSock);
   void setState(StateEnum state);
   void setConnectTimeout(int timeout);

private:
   Q_DISABLE_COPY(MonitorItem);
   MonitorItemPrivate* d;

public slots:
signals:
   void showStatusbarMessage(const QString& message);
   void appendText(const QString& tabRef, const QString& message);
   void clearText(const QString& tabRef);
   void statusChanged(const QString&tabRef, int);
   void jobIsRunning(bool);
};

class MonitorItemPrivate
{
   friend class MonitorItem;

   MonitorItemPrivate()
      : type(R_UNKNOWN)
      , resource(NULL)
      , DSock(NULL)
      , connectTimeout(0)
      , state(MonitorItem::Idle)
      { }

   Rescode type; /* R_DIRECTOR, R_CLIENT or R_STORAGE */
   void* resource; /* DirectorResource*, ClientResource* or StorageResource* */
   BareosSocket* DSock;
   int connectTimeout;

   MonitorItem::StateEnum state;
};

#endif // MONITORITEM_H_INCLUDED
