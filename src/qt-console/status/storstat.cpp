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
 *
 *   Dirk Bartley, March 2007
 */
 
#include "bat.h"
#include <QAbstractEventDispatcher>
#include <QTableWidgetItem>
#include "storstat.h"
#include "mount/mount.h"
#include "label/label.h"

static bool working = false;     /* prevent timer recursion */

/*
.status storage=<storage-name> <keyword>
where <storage-name> is the storage name in the Director, and 
<keyword> is one of the following:
header
running
terminated

waitreservation
devices
volumes
spooling
*/

/*
 * Constructor for the class
 */
StorStat::StorStat(QString &storage, QTreeWidgetItem *parentTreeWidgetItem)
   : Pages()
{
   m_storage = storage;
   setupUi(this);
   pgInitialize(tr("Storage Status %1").arg(m_storage), parentTreeWidgetItem);
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/status.png")));
   m_cursor = new QTextCursor(textEditHeader->document());

   m_timer = new QTimer(this);
   readSettings();

   createConnections();
   m_timer->start(1000);
   setCurrent();

   dockPage();
}

void StorStat::getFont()
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
StorStat::~StorStat()
{
   writeSettings();
}

/*
 * Populate all tables and header widgets
 */
void StorStat::populateAll()
{
   populateTerminated();
   populateCurrentTab(tabWidget->currentIndex());
}

/*
 *  Timer is triggered, see if is current and repopulate.
 */
void StorStat::timerTriggered()
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

/*
 * Populate header text widget
 */
void StorStat::populateHeader()
{
   QString command = QString(".status storage=\"" + m_storage + "\" header");
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

void StorStat::populateWaitReservation()
{
   QString command = QString(".status storage=\"" + m_storage + "\" waitreservation");
   if (mainWin->m_commandDebug)
      Pmsg1(000, "sending command : %s\n",command.toUtf8().data());
   QStringList results;
   textEditWaitReservation->clear();

   if (m_console->dir_cmd(command, results)) {
      foreach (QString line, results) {
         line += "\n";
         textEditWaitReservation->insertPlainText(line);
      }
   }
}

void StorStat::populateDevices()
{
   QString command = QString(".status storage=\"" + m_storage + "\" devices");
   if (mainWin->m_commandDebug)
      Pmsg1(000, "sending command : %s\n",command.toUtf8().data());
   QStringList results;
   textEditDevices->clear();

   if (m_console->dir_cmd(command, results)) {
      foreach (QString line, results) {
         line += "\n";
         textEditDevices->insertPlainText(line);
      }
   }
}

void StorStat::populateVolumes()
{
   QString command = QString(".status storage=\"" + m_storage + "\" volumes");
   if (mainWin->m_commandDebug)
      Pmsg1(000, "sending command : %s\n",command.toUtf8().data());
   QStringList results;
   textEditVolumes->clear();

   if (m_console->dir_cmd(command, results)) {
      foreach (QString line, results) {
         line += "\n";
         textEditVolumes->insertPlainText(line);
      }
   }
}

void StorStat::populateSpooling()
{
   QString command = QString(".status storage=\"" + m_storage + "\" spooling");
   if (mainWin->m_commandDebug)
      Pmsg1(000, "sending command : %s\n",command.toUtf8().data());
   QStringList results;
   textEditSpooling->clear();

   if (m_console->dir_cmd(command, results)) {
      foreach (QString line, results) {
         line += "\n";
         textEditSpooling->insertPlainText(line);
      }
   }
}

void StorStat::populateRunning()
{
   QString command = QString(".status storage=\"" + m_storage + "\" running");
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
 * Populate teminated table
 */
void StorStat::populateTerminated()
{
   QString command = QString(".status storage=\"" + m_storage + "\" terminated");
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
            terminatedTable->setItem(row, column, p_tableitem);
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
 * When the treeWidgetItem in the page selector tree is singleclicked, Make sure
 * The tree has been populated.
 */
void StorStat::PgSeltreeWidgetClicked()
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
void StorStat::currentStackItem()
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
void StorStat::createConnections()
{
   connect(actionRefresh, SIGNAL(triggered()), this, SLOT(populateAll()));
   connect(tabWidget, SIGNAL(currentChanged(int)), this, SLOT(populateCurrentTab(int)));
   connect(mountButton, SIGNAL(pressed()), this, SLOT(mountButtonPushed()));
   connect(umountButton, SIGNAL(pressed()), this, SLOT(umountButtonPushed()));
   connect(labelButton, SIGNAL(pressed()), this, SLOT(labelButtonPushed()));
   connect(releaseButton, SIGNAL(pressed()), this, SLOT(releaseButtonPushed()));
   terminatedTable->setContextMenuPolicy(Qt::ActionsContextMenu);
   terminatedTable->addAction(actionRefresh);
   connect(m_timer, SIGNAL(timeout()), this, SLOT(timerTriggered()));
}

/*
 * Save user settings associated with this page
 */
void StorStat::writeSettings()
{
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup(m_groupText);
   settings.setValue(m_splitText, splitter->saveState());
   settings.setValue("refreshInterval", spinBox->value());
   settings.setValue("refreshCheck", checkBox->checkState());
   settings.endGroup();

   settings.beginGroup("OpenOnExit");
   QString toWrite = "StorageStatus_" + m_storage;
   settings.setValue(toWrite, 1);
   settings.endGroup();
}

/*
 * Read and restore user settings associated with this page
 */
void StorStat::readSettings()
{
   m_groupText = "StorStatPage";
   m_splitText = "splitterSizes_0";
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup(m_groupText);
   if (settings.contains(m_splitText)) { splitter->restoreState(settings.value(m_splitText).toByteArray()); }
   spinBox->setValue(settings.value("refreshInterval", 28).toInt());
   checkBox->setCheckState((Qt::CheckState)settings.value("refreshCheck", Qt::Checked).toInt());
   settings.endGroup();

   timerDisplay->display(spinBox->value());
}

/*
 * Populate the text edit window in the current tab
 */
void StorStat::populateCurrentTab(int index)
{
   if (index == 0)
      populateHeader();
   if (index == 1)
      populateWaitReservation();
   if (index == 2)
      populateDevices();
   if (index == 3)
      populateVolumes();
   if (index == 4)
      populateSpooling();
   if (index == 5)
      populateRunning();
}

/*
 * execute mount in console
 */
void StorStat::mountButtonPushed()
{
   int haschanger = 3;

   /* Set up query QString and header QStringList */
   QString query("SELECT AutoChanger AS Changer"
            " FROM Storage WHERE Name='" + m_storage + "'"
            " ORDER BY Name" );

   QStringList results;
   /* This could be a log item */
   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "Storage query cmd : %s\n",query.toUtf8().data());
   }
   if (m_console->sql_cmd(query, results)) {
      int resultCount = results.count();
      if (resultCount == 1){
         QString resultline;
         QString field;
         QStringList fieldlist;
         /* there will only be one of these */
         foreach (resultline, results) {
            fieldlist = resultline.split("\t");
            int index = 0;
            /* Iterate through fields in the record */
            foreach (field, fieldlist) {
               field = field.trimmed();  /* strip leading & trailing spaces */
               haschanger = field.toInt();
               index++;
            }
         }
      }
   }

   Pmsg1(000, "haschanger is : %i\n", haschanger);
   if (haschanger == 0){
      /* no autochanger, just execute the command in the console */
      QString cmd("mount storage=" + m_storage);
      consoleCommand(cmd);
   } else if (haschanger != 3) {
      setConsoleCurrent();
      /* if this storage is an autochanger, lets ask for the slot */
      new mountDialog(m_console, m_storage);
   }
}

/*
 * execute umount in console
 */
void StorStat::umountButtonPushed()
{
   QString cmd("umount storage=" + m_storage);
   consoleCommand(cmd);
}

/* Release a tape in the drive */
void StorStat::releaseButtonPushed()
{
   QString cmd("release storage=");
   cmd += m_storage;
   consoleCommand(cmd);
}

/* Label Media populating current storage by default */
void StorStat::labelButtonPushed()
{
   new labelPage(m_storage);
}  
