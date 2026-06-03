/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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

#include <QColor>
#include <QIcon>
#include <QList>
#include <QPixmap>

#include <QTimer>
#include <QSystemTrayIcon>

class QMainWindow;

enum class IconType
{
  Normal,
  Error,
};

class SystemTrayIcon : public QSystemTrayIcon {
  Q_OBJECT

 public:
  SystemTrayIcon(QMainWindow* mainWindow);

  void animateIcon(bool on);

 private:
  Q_DISABLE_COPY(SystemTrayIcon);

  IconType currentIcon{IconType::Normal};
  size_t animationFrameIdx{0};
  bool animationRequested{false};
  QIcon normalIcon;
  QIcon errorIcon;
  QList<QIcon> animationIcons;
  std::unique_ptr<QTimer> timer;

  void updateIcon();

 protected:
 public slots:
  void setNewIcon(IconType type);
  void setIconInternal();
};

#endif  // BAREOS_QT_TRAY_MONITOR_SYSTEMTRAYICON_H_
