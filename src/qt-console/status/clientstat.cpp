/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2010 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *   Dirk Bartley, March 2007
 */
 
#include "bat.h"
#include <QAbstractEventDispatcher>
#include <QTableWidgetItem>
#include "clientstat.h"

/* This probably should be on a mutex */
static bool working = false;   /* prevent timer recursion */

/*
 * Constructor for the class
 */
ClientStat::ClientStat(QString &client, QTreeWidgetItem *parentTreeWidgetItem)
   : Pages()
{
   m_client = client;
   setupUi(this);
   pgInitialize(tr("Client Status %1").arg(m_client), parentTreeWidgetItem);
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/status.png")));
   m_cursor = new QTextCursor(textEditHeader->document());

   readSettings();
   dockPage();
   m_timer = new QTimer(this);

   createConnections();
   m_timer->start(1000);
   setCurrent();
}

void ClientStat::getFont()
{
   QFont font = textEditHeader->font();

   QString dirname;
   m_console->getDirResName(dirname);
   QSettings settings(dirname, "bat");
   settings.beginGroup("Console");
   font.setFamily(settings.value("consoleFont", "Courier").value<QString>());
   font.setPointSize(settings.value("consolePointSize", 10).toInt());
   font.setFixedPitch(settings.value("consoleFixedPitch", true).toBool());
   settings.endGroup();
   textEditHeader->setFont(font);
}

/*
 * Write the m_splitter settings in the destructor
 */
ClientStat::~ClientStat()
{
   writeSettings();
}

/*
 * Populate all tables and header widgets
 */
void ClientStat::populateAll()
{
   populateTerminated();
   populateCurrentTab(tabWidget->currentIndex());
}

/*
 *  Timer is triggered, see if is current and repopulate.
 */
void ClientStat::timerTriggered()
{
   double value = timerDisplay->value();
   value -= 1;
   if (value <= 0 && !working) {
      working = true;
      value = spinBox->value();
      bool iscurrent = mainWin->tabWidget->currentIndex() == mainWin->tabWidget->indexOf(this);
      if (((isDocked() && iscurrent) || (!isDocked())) && (checkBox->checkState() == Qt::Checked)) {
         populateAll();
      }
      working = false;
   }
   timerDisplay->display(value);
}


void ClientStat::populateCurrentTab(int index)
{
   if (index == 0)
      populateRunning();
   if (index == 1)
      populateHeader();
}

/*
 * Populate header text widget
 */
void ClientStat::populateHeader()
{
   QString command = QString(".status client=\"" + m_client + "\" header");
   if (mainWin->m_commandDebug)
      Pmsg1(000, "sending command : %s\n",command.toUtf8().data());
   QStringList results;
   textEditHeader->clear();

   if (m_console->dir_cmd(command, results)) {
      foreach (QString line, results) {
         line += "\n";
         textEditHeader->insertPlainText(line);
      }
   }
}

/*
 * Populate teminated table
 */
void ClientStat::populateTerminated()
{
   QString command = QString(".status client=\"" + m_client + "\" terminated");
   if (mainWin->m_commandDebug)
      Pmsg1(000, "sending command : %s\n",command.toUtf8().data());
   QStringList results;
   QBrush blackBrush(Qt::black);

   terminatedTable->clear();
   QStringList headerlist = (QStringList()
      << tr("Job Id") << tr("Job Level") << tr("Job Files")
      << tr("Job Bytes") << tr("Job Status") << tr("Job Time")
      << tr("Job Name"));
   QStringList flaglist = (QStringList()
      << "R" << "L" << "R" << "R" << "LC"
      << "L" << "L");

   terminatedTable->setColumnCount(headerlist.size());
   terminatedTable->setHorizontalHeaderLabels(headerlist);

   if (m_console->dir_cmd(command, results)) {
      int row = 0;
      QTableWidgetItem* p_tableitem;
      terminatedTable->setRowCount(results.size());
      foreach (QString line, results) {
         /* Iterate through the record returned from the query */
         QStringList fieldlist = line.split("\t");
         int column = 0;
         QString statusCode("");
         /* Iterate through fields in the record */
         foreach (QString field, fieldlist) {
            field = field.trimmed();  /* strip leading & trailing spaces */
            p_tableitem = new QTableWidgetItem(field, 1);
            p_tableitem->setForeground(blackBrush);
            p_tableitem->setFlags(0);
            if (flaglist[column].contains("R"))
               p_tableitem->setTextAlignment(Qt::AlignRight);
            if (flaglist[column].contains("C")) {
               if (field == "OK")
                  p_tableitem->setBackground(Qt::green);
               else
                 p_tableitem->setBackground(Qt::red);
            }
            terminatedTable->setItem(results.size() - row - 1, column, p_tableitem);
            column += 1;
         }
         row += 1;
      }
   }
   terminatedTable->resizeColumnsToContents();
   terminatedTable->resizeRowsToContents();
   terminatedTable->verticalHeader()->hide();
}

/*
 * Populate running text
 */
void ClientStat::populateRunning()
{
   QString command = QString(".status client=\"" + m_client + "\" running");
   if (mainWin->m_commandDebug)
      Pmsg1(000, "sending command : %s\n",command.toUtf8().data());
   QStringList results;
   textEditRunning->clear();

   if (m_console->dir_cmd(command, results)) {
      foreach (QString line, results) {
         line += "\n";
         textEditRunning->insertPlainText(line);
      }
   }
}

/*
 * When the treeWidgetItem in the page selector tree is single clicked, Make sure
 * The tree has been populated.
 */
void ClientStat::PgSeltreeWidgetClicked()
{
   if (!m_populated) {
      populateAll();
      m_populated=true;
   }
}

/*
 *  Virtual function override of pages function which is called when this page
 *  is visible on the stack
 */
void ClientStat::currentStackItem()
{
   populateAll();
   timerDisplay->display(spinBox->value());

   if (!m_populated) {
      m_populated=true;
   }
}

/*
 * Function to create connections for context sensitive menu for this and
 * the page selector
 */
void ClientStat::createConnections()
{
   connect(actionRefresh, SIGNAL(triggered()), this, SLOT(populateAll()));
   connect(tabWidget, SIGNAL(currentChanged(int)), this, SLOT(populateCurrentTab(int)));
   connect(m_timer, SIGNAL(timeout()), this, SLOT(timerTriggered()));
   terminatedTable->setContextMenuPolicy(Qt::ActionsContextMenu);
   terminatedTable->addAction(actionRefresh);
}

/*
 * Save user settings associated with this page
 */
void ClientStat::writeSettings()
{
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup(m_groupText);
   settings.setValue(m_splitText, splitter->saveState());
   settings.setValue("refreshInterval", spinBox->value());
   settings.setValue("refreshCheck", checkBox->checkState());
   settings.endGroup();

   settings.beginGroup("OpenOnExit");
   QString toWrite = "ClientStatus_" + m_client;
   settings.setValue(toWrite, 1);
   settings.endGroup();
}

/*
 * Read and restore user settings associated with this page
 */
void ClientStat::readSettings()
{
   m_groupText = "ClientStatPage";
   m_splitText = "splitterSizes_1";
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup(m_groupText);
   if (settings.contains(m_splitText)) { splitter->restoreState(settings.value(m_splitText).toByteArray()); }
   spinBox->setValue(settings.value("refreshInterval", 28).toInt());
   checkBox->setCheckState((Qt::CheckState)settings.value("refreshCheck", Qt::Checked).toInt());
   settings.endGroup();

   timerDisplay->display(spinBox->value());
}
