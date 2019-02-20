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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "systemtrayicon.h"

#include <QDebug>
#include <QPlainTextEdit>
#include <QTimer>
#include <QThread>

#include "tray-monitor.h"
#include "monitoritemthread.h"
#include "monitoritem.h"
#include "monitortab.h"

MainWindow* MainWindow::mainWindowSingleton = NULL;
bool MainWindow::already_destroyed = false;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , monitorTabMap(new QMap<QString, MonitorTab*>)
    , systemTrayIcon(new SystemTrayIcon(this))
{
  /* Init the SystemTrayIcon first to have all its signals
   * configured to be auto-connected via connectSlotsByName(MainWindow)
   * during ui->setupUi(this). */
  Q_ASSERT(systemTrayIcon->objectName() == "SystemTrayIcon");

  // This will setup the tab-window and auto-connect signals and slots.
  ui->setupUi(this);
  setWindowTitle(tr("Bareos Tray Monitor"));
  setWindowIcon(QIcon(":images/bareos_1.png"));
  ui->pushButton_Close->setIcon(QIcon(":images/f.png"));

  // Prepare the tabWidget
  while (ui->tabWidget->count()) { ui->tabWidget->removeTab(0); }

  nTabs = 100;
  bRefs = new bool[nTabs];
  for (int i = 0; i < nTabs; i++) bRefs[i] = true;


  // Now show the tray icon, but leave the MainWindow hidden.
  systemTrayIcon->show();
}

MainWindow::~MainWindow()
{
  delete ui;
  delete monitorTabMap;
  delete bRefs;
}

MainWindow* MainWindow::instance()
{
  // improve that the MainWindow is created
  // and deleted only once during program execution
  Q_ASSERT(!already_destroyed);

  if (!mainWindowSingleton) { mainWindowSingleton = new MainWindow; }

  return mainWindowSingleton;
}

void MainWindow::destruct()
{
  if (mainWindowSingleton) {
    delete mainWindowSingleton;
    mainWindowSingleton = NULL;
  }
}

void MainWindow::addTabs(QStringList tabRefs)
{
  tabs = tabRefs;  //
  nTabs = tabRefs.size();

  for (int i = 0; i < tabRefs.count(); i++) {
    MonitorTab* tab = new MonitorTab(tabRefs[i], this);
    monitorTabMap->insert(tabRefs[i], tab);  // tabRefs[i] used as reference
    ui->tabWidget->addTab(tab->getTabWidget(), tabRefs[i]);
  }
}

void MainWindow::on_TrayMenu_About_triggered()
{
  QString fmt =
      QString(
          "<br><h2>Bareos Tray Monitor %1</h2>"
          "<p>For more information, see: www.bareos.com"
          "<p>Copyright &copy; 2004-2011 Free Software Foundation Europe e.V."
          "<p>Copyright &copy; 2011-2012 Planets Communications B.V."
          "<p>Copyright &copy; 2013-%2 Bareos GmbH & Co. KG"
          "<p>BAREOS &reg; is a registered trademark of Bareos GmbH & Co. KG"
          "<p>Licensed under GNU AGPLv3.")
          .arg(VERSION)
          .arg(BYEAR);

  QMessageBox::about(this, tr("About Bareos Tray Monitor"), fmt);
}

void MainWindow::on_TrayMenu_Quit_triggered() { QApplication::quit(); }

void MainWindow::on_TrayMenu_Display_triggered()
{
  if (isVisible()) {
    hide();
  } else {
    show();
    raise();
    emit refreshItems();
  }
}

void MainWindow::on_SystemTrayIcon_activated(
    QSystemTrayIcon::ActivationReason r)
{
  if (r == QSystemTrayIcon::Trigger) { on_TrayMenu_Display_triggered(); }
}

void MainWindow::on_pushButton_Close_clicked() { hide(); }

QPlainTextEdit* MainWindow::getTextEdit(const QString& tabRef)
{
  MonitorTab* tab = monitorTabMap->value(tabRef);

  return (tab) ? tab->getTextEdit() : 0;
}

void MainWindow::onClearText(const QString& tabRef)
{
  QPlainTextEdit* w = getTextEdit(tabRef);

  if (w) { w->clear(); }
}

void MainWindow::onAppendText(QString tabRef, QString line)
{
  QPlainTextEdit* w = getTextEdit(tabRef);

  if (w) { w->appendPlainText(line); }
}

void MainWindow::onShowStatusbarMessage(QString message)
{
  ui->statusbar->showMessage(message, 3000);
}

void MainWindow::onStatusChanged(const QString& tabRef, int state)
{
  int n = tabs.indexOf(tabRef);

  MonitorTab* tab = monitorTabMap->value(tabRef);

  if (tab) {
    int idx = ui->tabWidget->indexOf(tab->getTabWidget());
    if (idx < 0) { return; }

    switch (state) {
      case MonitorItem::Error:

        bRefs[n] = false;

        systemTrayIcon->setNewIcon(2);  // red Icon on Error
        ui->tabWidget->setTabIcon(idx, QIcon(":images/W.png"));
        break;

      case MonitorItem::Running:

        bRefs[n] = true;
        if (bRefs[(n + 1) % nTabs] && bRefs[(n + 2) % nTabs])  // if all Tabs OK
          systemTrayIcon->setNewIcon(0);  // shows blue Icon

        ui->tabWidget->setTabIcon(idx, QIcon());
        break;

      default:
        break;
    } /* switch(state) */
  }   /* if (tab) */
}

void MainWindow::onFdJobIsRunning(bool running)
{
  systemTrayIcon->animateIcon(running);
}
