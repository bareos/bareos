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

#ifndef BAREOS_QT_TRAY_MONITOR_SYSTEMTRAYICON_H_
#define BAREOS_QT_TRAY_MONITOR_SYSTEMTRAYICON_H_

#include <QSystemTrayIcon>

class QMainWindow;
class QTimer;

class SystemTrayIcon : public QSystemTrayIcon {
  Q_OBJECT

 public:
  SystemTrayIcon(QMainWindow* mainWindow);
  virtual ~SystemTrayIcon();

  void animateIcon(bool on);

 private:
  Q_DISABLE_COPY(SystemTrayIcon);
  SystemTrayIcon();
  QStringList icons;

  int iconIdx;
  QTimer* timer;

 protected:
 public slots:
  void setNewIcon(int icon);
  void setIconInternal();
};

#endif  // BAREOS_QT_TRAY_MONITOR_SYSTEMTRAYICON_H_
