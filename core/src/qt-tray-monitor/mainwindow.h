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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QApplication>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QMap>
#include <QString>
#include <QStringList>

namespace Ui {
class MainWindow;
}

class QPlainTextEdit;
class MonitorTab;
class MonitorItem;
class SystemTrayIcon;

class MainWindow : public QMainWindow
{
   Q_OBJECT

public:
   static MainWindow* instance();
   static void destruct();

   void addTabs(QStringList tabRefs);

private:
   explicit MainWindow(QWidget *parent = 0);
   Q_DISABLE_COPY(MainWindow);
   ~MainWindow();

   QPlainTextEdit* getTextEdit(const QString& tabRef);
   static MainWindow* mainWindowSingleton;
   static bool already_destroyed;

   Ui::MainWindow *ui;
   QMap<QString,MonitorTab*>* monitorTabMap;
   SystemTrayIcon* systemTrayIcon;

   QStringList tabs;
   int nTabs;
   bool *bRefs;

public slots:
   /* auto-connected slots to the UI                */
   void on_pushButton_Close_clicked();
   /* ********************************************* */

   /* auto-connected slots to the TrayMenu Actions */
   void on_TrayMenu_About_triggered();
   void on_TrayMenu_Quit_triggered();
   void on_TrayMenu_Display_triggered();
   /* ********************************************* */

   /* auto-connected slots to the SystemTrayIcon    */
   void on_SystemTrayIcon_activated(QSystemTrayIcon::ActivationReason);
   /* ********************************************* */

   void onShowStatusbarMessage(QString);
   void onAppendText(QString, QString);
   void onClearText(const QString& tabRef);
   void onStatusChanged(const QString& tabRef, int state);
   void onFdJobIsRunning(bool running);

signals:
   void refreshItems();
};

#endif // MAINWINDOW_H
