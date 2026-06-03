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
#include "lib/version.h"

MainWindow* MainWindow::mainWindowSingleton = NULL;
bool MainWindow::already_destroyed = false;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , monitorTabMap(new QMap<QString, tab>)
    , systemTrayIcon(new SystemTrayIcon(this))
{
  /* Init the SystemTrayIcon first to have all its signals
   * configured to be auto-connected via connectSlotsByName(MainWindow)
   * during ui->setupUi(this). */
  Q_ASSERT(systemTrayIcon->objectName() == "SystemTrayIcon");

  // This will setup the tab-window and auto-connect signals and slots.
  ui->setupUi(this);
  setWindowTitle(tr("Bareos Tray Monitor"));
  setWindowIcon(systemTrayIcon->icon());
  ui->pushButton_Close->setIcon(QIcon(":/images/f.png"));

  // Prepare the tabWidget
  while (ui->tabWidget->count()) { ui->tabWidget->removeTab(0); }

  // Now show the tray icon, but leave the MainWindow hidden.
  systemTrayIcon->show();
}

MainWindow::~MainWindow()
{
  delete ui;
  delete monitorTabMap;
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
  for (int i = 0; i < tabRefs.count(); i++) {
    MonitorTab* mtab = new MonitorTab(tabRefs[i], this);
    tab actual_tab = {mtab, i, MonitorItem::Idle};

    monitorTabMap->insert(tabRefs[i], actual_tab);
    ui->tabWidget->addTab(mtab->getTabWidget(), tabRefs[i]);
  }
}

void MainWindow::on_TrayMenu_About_triggered()
{
  QString fmt
      = QString(
            "<br><h2>Bareos Tray Monitor %1</h2>"
            "<p>For more information, see: www.bareos.com"
            "<p>Copyright &copy; 2004-2011 Free Software Foundation Europe e.V."
            "<p>Copyright &copy; 2011-2012 Planets Communications B.V."
            "<p>Copyright &copy; 2013-%2 Bareos GmbH & Co. KG"
            "<p>BAREOS &reg; is a registered trademark of Bareos GmbH & Co. KG"
            "<p>Licensed under GNU AGPLv3.")
            .arg(kBareosVersionStrings.Full)
            .arg(kBareosVersionStrings.Year);

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
  if (auto iter = monitorTabMap->find(tabRef); iter != monitorTabMap->end()) {
    if (iter->tab) { return iter->tab->getTextEdit(); }
  }

  return nullptr;
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
  auto iter = monitorTabMap->find(tabRef);
  if (iter == monitorTabMap->end()) { return; }

  iter->state = state;
  switch (state) {
    case MonitorItem::Error: {
      ui->tabWidget->setTabIcon(iter->tab_index, QIcon(":images/W.png"));
    } break;
    default: {
      ui->tabWidget->setTabIcon(iter->tab_index, QIcon());
    } break;
  }

  int total_status = MonitorItem::Idle;

  for (auto& [_1, _2, tab_state] : *monitorTabMap) {
    if (tab_state > total_status) { total_status = tab_state; }
  }

  switch (total_status) {
    case MonitorItem::Error: {
      systemTrayIcon->setNewIcon(IconType::Error);  // red Icon on Error
    } break;

    case MonitorItem::Running: {
      systemTrayIcon->setNewIcon(IconType::Normal);  // shows blue Icon
    } break;

    case MonitorItem::Idle: {
      systemTrayIcon->setNewIcon(IconType::Normal);  // shows blue Icon
    } break;

    default:
      break;
  } /* switch(state) */
}

void MainWindow::onFdJobIsRunning(bool running)
{
  systemTrayIcon->animateIcon(running);
}
