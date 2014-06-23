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

#include "systemtrayicon.h"
#include "traymenu.h"
#include <QObject>
#include <QMainWindow>
#include <QTimer>
#include <QDebug>

SystemTrayIcon::SystemTrayIcon(QMainWindow* mainWindow)
   : QSystemTrayIcon(mainWindow)
   , iconIdx(0)
   , timer(new QTimer(this))
{
   // this object name is used for auto-connection to the MainWindow
   setObjectName("SystemTrayIcon");

   TrayMenu* menu = new TrayMenu(mainWindow);
   setContextMenu(menu);

   icons << ":/images/bareos_1.png" << ":/images/bareos_2.png" << ":/images/W.png";
   setIcon(QIcon(icons[iconIdx]));
   setToolTip("Bareos Tray Monitor");

   timer->setInterval(700);
   connect(timer, SIGNAL(timeout()), this, SLOT(setIconInternal()));
}

SystemTrayIcon::~SystemTrayIcon()
{
   timer->stop();
}

void SystemTrayIcon::setNewIcon(int icon){

   iconIdx = icon;
   setIcon(QIcon(icons[icon]));
}

void  SystemTrayIcon::setIconInternal()
{
   setIcon(QIcon(icons[iconIdx++]));
   iconIdx %= 2; // 0 if 1 / 1 if 0
}

void SystemTrayIcon::animateIcon(bool on)
{
   if (on) {
      if (!timer->isActive()) {
         timer->start();
      }
   } else { //off
      if (timer->isActive()) {
         timer->stop();
      }
      if(iconIdx != 2){  // blink if there's no error
         iconIdx = 0;
         setIconInternal();
      }
   }
}
