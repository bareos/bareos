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

#include "traymenu.h"

#include <QApplication>
#include <QString>
#include <QWidget>
#include <QDebug>

TrayMenu::TrayMenu(QWidget* mainWindow)
   : QMenu(mainWindow)
{
   setObjectName("TrayMenu");

   createAction("Display", "Display",  mainWindow);
   createAction("About",   "About",    mainWindow);
   addSeparator();
   createAction("Quit",    "Quit",     mainWindow);
}

TrayMenu::~TrayMenu()
{
   return;
}

void TrayMenu::createAction(QString objName, QString text, QWidget* mainWindow)
{
   const QString& translate = QApplication::translate("TrayMonitor",
                                                       text.toUtf8(),
                                                       0);

   QAction *action = new QAction(translate, mainWindow);

   /* QActions are connected to the mainWindow with their
    * name-signals-schema i.e. "on_Display_triggered()"
    * using QMetaObject::connectSlotsByName(mainWindow)
    * during SetupUi of the mainWindow */

   const QString& objNameForSignal = QString("TrayMenu_%1").arg(objName);
   action->setObjectName(objNameForSignal);

   addAction(action);
}
