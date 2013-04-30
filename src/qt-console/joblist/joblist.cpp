/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2009 Free Software Foundation Europe e.V.

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
 *   Version $Id$
 *
 *   Dirk Bartley, March 2007
 */
 
#include "bat.h"
#include <QAbstractEventDispatcher>
#include <QTableWidgetItem>
#include "joblist.h"
#include "restore.h"
#include "job/job.h"
#include "joblog/joblog.h"
#ifdef HAVE_QWT
#include "jobgraphs/jobplot.h"
#endif
#include "util/fmtwidgetitem.h"
#include "util/comboutil.h"

/*
 * Constructor for the class
 */
JobList::JobList(const QString &mediaName, const QString &clientName,
          const QString &jobName, const QString &filesetName, QTreeWidgetItem *parentTreeWidgetItem)
   : Pages()
{
   setupUi(this);
   m_name = "Jobs Run"; /* treeWidgetName has a virtual override in this class */
   m_mediaName = mediaName;
   m_clientName = clientName;
   m_jobName = jobName;
   m_filesetName = filesetName;
   pgInitialize("", parentTreeWidgetItem);
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/emblem-system.png")));

   m_resultCount = 0;
   m_populated = false;
   m_closeable = false;
   if ((m_mediaName != "") || (m_clientName != "") || (m_jobName != "") || (m_filesetName != "")) {
      m_closeable=true;
   }
   m_checkCurrentWidget = true;

   /* Set Defaults for check and spin for limits */
   limitCheckBox->setCheckState(mainWin->m_recordLimitCheck ? Qt::Checked : Qt::Unchecked);
   limitSpinBox->setValue(mainWin->m_recordLimitVal);
   daysCheckBox->setCheckState(mainWin->m_daysLimitCheck ? Qt::Checked : Qt::Unchecked);
   daysSpinBox->setValue(mainWin->m_daysLimitVal);

   QGridLayout *gridLayout = new QGridLayout(this);
   gridLayout->setSpacing(6);
   gridLayout->setMargin(9);
   gridLayout->setObjectName(QString::fromUtf8("gridLayout"));

   m_splitter = new QSplitter(Qt::Vertical, this);
   QScrollArea *area = new QScrollArea();
   area->setObjectName(QString::fromUtf8("area"));
   area->setWidget(frame);
   area->setWidgetResizable(true);
   m_splitter->addWidget(area);
   m_splitter->addWidget(mp_tableWidget);

   gridLayout->addWidget(m_splitter, 0, 0, 1, 1);
   createConnections();
   readSettings();
   if (m_closeable) { dockPage(); }
}

/*
 * Write the m_splitter settings in the destructor
 */
JobList::~JobList()
{
   writeSettings();
}

/*
 * The Meat of the class.
 * This function will populate the QTableWidget, mp_tablewidget, with
 * QTableWidgetItems representing the results of a query for what jobs exist on
 * the media name passed from the constructor stored in m_mediaName.
 */
void JobList::populateTable()
{
   /* Can't do this in constructor because not neccesarily conected in constructor */
   prepareFilterWidgets();
   m_populated = true;

   Freeze frz(*mp_tableWidget); /* disable updating*/

   /* Set up query */
   QString query;
   fillQueryString(query);

   /* Set up the Header for the table */
   QStringList headerlist = (QStringList()
      << tr("Job Id") << tr("Job Name") << tr("Client") << tr("Job Starttime") 
      << tr("Job Type") << tr("Job Level") << tr("Job Files") 
      << tr("Job Bytes") << tr("Job Status")  << tr("Purged") << tr("File Set")
      << tr("Pool Name") << tr("First Volume") << tr("VolCount"));

   m_jobIdIndex = headerlist.indexOf(tr("Job Id"));
   m_purgedIndex = headerlist.indexOf(tr("Purged"));
   m_typeIndex = headerlist.indexOf(tr("Job Type"));
   m_statusIndex = headerlist.indexOf(tr("Job Status"));
   m_startIndex = headerlist.indexOf(tr("Job Starttime"));
   m_filesIndex = headerlist.indexOf(tr("Job Files"));
   m_bytesIndex = headerlist.indexOf(tr("Job Bytes"));
   m_levelIndex = headerlist.indexOf(tr("Job Level"));
   m_nameIndex = headerlist.indexOf(tr("Job Name"));
   m_filesetIndex = headerlist.indexOf(tr("File Set"));
   m_clientIndex = headerlist.indexOf(tr("Client"));

   /* Initialize the QTableWidget */
   m_checkCurrentWidget = false;
   mp_tableWidget->clear();
   m_checkCurrentWidget = true;
   mp_tableWidget->setColumnCount(headerlist.size());
   mp_tableWidget->setHorizontalHeaderLabels(headerlist);
   mp_tableWidget->horizontalHeader()->setHighlightSections(false);
   mp_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
   mp_tableWidget->setSortingEnabled(false); /* rows move on insert if sorting enabled */

   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "Query cmd : %s\n",query.toUtf8().data());
   }

   QStringList results;
   if (m_console->sql_cmd(query, results)) {
      m_resultCount = results.count();

      QStringList fieldlist;
      mp_tableWidget->setRowCount(results.size());

      int row = 0;
      /* Iterate through the record returned from the query */
      QString resultline;
      foreach (resultline, results) {
         fieldlist = resultline.split("\t");
         if (fieldlist.size() < 13)
            continue; /* some fields missing, ignore row */

         TableItemFormatter jobitem(*mp_tableWidget, row);
  
         /* Iterate through fields in the record */
         QStringListIterator fld(fieldlist);
         int col = 0;

         /* job id */
         jobitem.setNumericFld(col++, fld.next());

         /* job name */
         jobitem.setTextFld(col++, fld.next());

         /* client */
         jobitem.setTextFld(col++, fld.next());

         /* job starttime */
         jobitem.setTextFld(col++, fld.next(), true);

         /* job type */
         jobitem.setJobTypeFld(col++, fld.next());

         /* job level */
         jobitem.setJobLevelFld(col++, fld.next());

         /* job files */
         jobitem.setNumericFld(col++, fld.next());

         /* job bytes */
         jobitem.setBytesFld(col++, fld.next());

         /* job status */
         jobitem.setJobStatusFld(col++, fld.next());

         /* purged */
         jobitem.setBoolFld(col++, fld.next());

         /* fileset */
         jobitem.setTextFld(col++, fld.next());

         /* pool name */
         jobitem.setTextFld(col++, fld.next());

         /* First Media */
         jobitem.setTextFld(col++, fld.next());

         /* Medias count */
         jobitem.setNumericFld(col++, fld.next());
         row++;
      }
   } 
   /* set default sorting */
   mp_tableWidget->sortByColumn(m_jobIdIndex, Qt::DescendingOrder);
   mp_tableWidget->setSortingEnabled(true);
   
   /* Resize the columns */
   mp_tableWidget->resizeColumnsToContents();
   mp_tableWidget->resizeRowsToContents();
   mp_tableWidget->verticalHeader()->hide();
   if ((m_mediaName != tr("Any")) && (m_resultCount == 0)){
      /* for context sensitive searches, let the user know if there were no
       * results */
      QMessageBox::warning(this, "Bat",
          tr("The Jobs query returned no results.\n"
         "Press OK to continue?"), QMessageBox::Ok );
   }

   /* make read only */
   mp_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void JobList::prepareFilterWidgets()
{
   if (!m_populated) {
      clientComboBox->addItem(tr("Any"));
      clientComboBox->addItems(m_console->client_list);
      comboSel(clientComboBox, m_clientName);

      QStringList volumeList;
      getVolumeList(volumeList);
      volumeComboBox->addItem(tr("Any"));
      volumeComboBox->addItems(volumeList);
      comboSel(volumeComboBox, m_mediaName);

      jobComboBox->addItem(tr("Any"));
      jobComboBox->addItems(m_console->job_list);
      comboSel(jobComboBox, m_jobName);

      levelComboFill(levelComboBox);

      boolComboFill(purgedComboBox);

      fileSetComboBox->addItem(tr("Any"));
      fileSetComboBox->addItems(m_console->fileset_list);
      comboSel(fileSetComboBox, m_filesetName);

      poolComboBox->addItem(tr("Any"));
      poolComboBox->addItems(m_console->pool_list);

      jobStatusComboFill(statusComboBox);
   }
}

void JobList::fillQueryString(QString &query)
{
   query = "";
   int volumeIndex = volumeComboBox->currentIndex();
   if (volumeIndex != -1)
      m_mediaName = volumeComboBox->itemText(volumeIndex);
   QString distinct = "";
   if (m_mediaName != tr("Any")) { distinct = "DISTINCT "; }
   query += "SELECT " + distinct + "Job.JobId AS JobId, Job.Name AS JobName, " 
            " Client.Name AS Client,"
            " Job.Starttime AS JobStart, Job.Type AS JobType,"
            " Job.Level AS BackupLevel, Job.Jobfiles AS FileCount,"
            " Job.JobBytes AS Bytes, Job.JobStatus AS Status,"
            " Job.PurgedFiles AS Purged, FileSet.FileSet,"
            " Pool.Name AS Pool,"
            " (SELECT Media.VolumeName FROM JobMedia JOIN Media ON JobMedia.MediaId=Media.MediaId WHERE JobMedia.JobId=Job.JobId ORDER BY JobMediaId LIMIT 1) AS FirstVolume,"
            " (SELECT count(DISTINCT MediaId) FROM JobMedia WHERE JobMedia.JobId=Job.JobId) AS Volumes"
            " FROM Job"
            " JOIN Client ON (Client.ClientId=Job.ClientId)"
            " LEFT OUTER JOIN FileSet ON (FileSet.FileSetId=Job.FileSetId) "
            " LEFT OUTER JOIN Pool ON Job.PoolId = Pool.PoolId ";
   QStringList conditions;
   if (m_mediaName != tr("Any")) {
      query += " LEFT OUTER JOIN JobMedia ON (JobMedia.JobId=Job.JobId) "
               " LEFT OUTER JOIN Media ON (JobMedia.MediaId=Media.MediaId) ";
      conditions.append("Media.VolumeName='" + m_mediaName + "'");
   }

   comboCond(conditions, clientComboBox, "Client.Name");
   comboCond(conditions, jobComboBox, "Job.Name");
   levelComboCond(conditions, levelComboBox, "Job.Level");
   jobStatusComboCond(conditions, statusComboBox, "Job.JobStatus");
   boolComboCond(conditions, purgedComboBox, "Job.PurgedFiles");
   comboCond(conditions, fileSetComboBox, "FileSet.FileSet");
   comboCond(conditions, poolComboBox, "Pool.Name");

   /* If Limit check box For limit by days is checked  */
   if (daysCheckBox->checkState() == Qt::Checked) {
      QDateTime stamp = QDateTime::currentDateTime().addDays(-daysSpinBox->value());
      QString since = stamp.toString(Qt::ISODate);
      conditions.append("Job.Starttime > '" + since + "'");
   }
   if (filterCopyCheckBox->checkState() == Qt::Checked) {
      conditions.append("Job.Type != 'c'" );
   }
   if (filterMigrationCheckBox->checkState() == Qt::Checked) {
      conditions.append("Job.Type != 'g'" );
   }
   bool first = true;
   foreach (QString condition, conditions) {
      if (first) {
         query += " WHERE " + condition;
         first = false;
      } else {
         query += " AND " + condition;
      }
   }
   /* Descending */
   query += " ORDER BY Job.JobId DESC";
   /* If Limit check box for limit records returned is checked  */
   if (limitCheckBox->checkState() == Qt::Checked) {
      QString limit;
      limit.setNum(limitSpinBox->value());
      query += " LIMIT " + limit;
   }
}

/*
 * When the treeWidgetItem in the page selector tree is singleclicked, Make sure
 * The tree has been populated.
 */
void JobList::PgSeltreeWidgetClicked()
{
   if (!m_populated) {
      populateTable();
      /* Lets make sure the splitter is not all the way to size index 0 == 0 */
      QList<int> sizes = m_splitter->sizes();
      if (sizes[0] == 0) {
         int frameMax = frame->maximumHeight();
         int sizeSum = 0;
         foreach(int size, sizes) { sizeSum += size; }
         int tabHeight = mainWin->tabWidget->geometry().height();
         sizes[0] = frameMax;
         sizes[1] = tabHeight - frameMax;
         m_splitter->setSizes(sizes);
      }
   }
   if (!isOnceDocked()) {
      dockPage();
   }
}

/*
 *  Virtual function override of pages function which is called when this page
 *  is visible on the stack
 */
void JobList::currentStackItem()
{
/*   if (!m_populated) populate every time user comes back to this object */
      populateTable();
}

/*
 * Virtual Function to return the name for the medialist tree widget
 */
void JobList::treeWidgetName(QString &desc)
{
   if (m_mediaName != "" ) {
     desc = tr("Jobs Run on Volume %1").arg(m_mediaName);
   } else if (m_clientName != "" ) {
     desc = tr("Jobs Run from Client %1").arg(m_clientName);
   } else if (m_jobName != "" ) {
     desc = tr("Jobs Run of Job %1").arg(m_jobName);
   } else if (m_filesetName != "" ) {
     desc = tr("Jobs Run with fileset %1").arg(m_filesetName);
   } else {
     desc = tr("Jobs Run");
   }
}

/*
 * Function to create connections for context sensitive menu for this and
 * the page selector
 */
void JobList::createConnections()
{
   /* connect to the action specific to this pages class that shows up in the 
    * page selector tree */
   connect(actionRefreshJobList, SIGNAL(triggered()), this, SLOT(populateTable()));
   connect(refreshButton, SIGNAL(pressed()), this, SLOT(populateTable()));
#ifdef HAVE_QWT
   connect(graphButton, SIGNAL(pressed()), this, SLOT(graphTable()));
#else
   graphButton->setEnabled(false);
   graphButton->setVisible(false);
#endif
   /* for the selectionChanged to maintain m_currentJob and a delete selection */
   connect(mp_tableWidget, SIGNAL(itemSelectionChanged()), this, SLOT(selectionChanged()));
   connect(mp_tableWidget, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(showInfoForJob()));

   /* Do what is required for the local context sensitive menu */


   /* setContextMenuPolicy is required */
   mp_tableWidget->setContextMenuPolicy(Qt::ActionsContextMenu);

   connect(actionListFilesOnJob, SIGNAL(triggered()), this, SLOT(consoleListFilesOnJob()));
   connect(actionListJobMedia, SIGNAL(triggered()), this, SLOT(consoleListJobMedia()));
   connect(actionDeleteJob, SIGNAL(triggered()), this, SLOT(consoleDeleteJob()));
   connect(actionRestartJob, SIGNAL(triggered()), this, SLOT(consoleRestartJob()));
   connect(actionPurgeFiles, SIGNAL(triggered()), this, SLOT(consolePurgeFiles()));
   connect(actionRestoreFromJob, SIGNAL(triggered()), this, SLOT(preRestoreFromJob()));
   connect(actionRestoreFromTime, SIGNAL(triggered()), this, SLOT(preRestoreFromTime()));
   connect(actionShowLogForJob, SIGNAL(triggered()), this, SLOT(showLogForJob()));
   connect(actionShowInfoForJob, SIGNAL(triggered()), this, SLOT(showInfoForJob()));
   connect(actionCancelJob, SIGNAL(triggered()), this, SLOT(consoleCancelJob()));
   connect(actionListJobTotals, SIGNAL(triggered()), this, SLOT(consoleListJobTotals()));
   connect(m_splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(splitterMoved(int, int)));

   m_contextActions.append(actionRefreshJobList);
   m_contextActions.append(actionListJobTotals);
}

/*
 * Functions to respond to local context sensitive menu sending console commands
 * If I could figure out how to make these one function passing a string, Yaaaaaa
 */
void JobList::consoleListFilesOnJob()
{
   QString cmd("list files jobid=");
   cmd += m_currentJob;
   if (mainWin->m_longList) { cmd.prepend("l"); }
   consoleCommand(cmd);
}
void JobList::consoleListJobMedia()
{
   QString cmd("list jobmedia jobid=");
   cmd += m_currentJob;
   if (mainWin->m_longList) { cmd.prepend("l"); }
   consoleCommand(cmd);
}

void JobList::consoleListJobTotals()
{
   QString cmd("list jobtotals");
   if (mainWin->m_longList) { cmd.prepend("l"); }
   consoleCommand(cmd);
}

void JobList::consoleDeleteJob()
{
   if (QMessageBox::warning(this, "Bat",
      tr("Are you sure you want to delete??  !!!.\n"
"This delete command is used to delete a Job record and all associated catalog"
" records that were created. This command operates only on the Catalog"
" database and has no effect on the actual data written to a Volume. This"
" command can be dangerous and we strongly recommend that you do not use"
" it unless you know what you are doing.  The Job and all its associated"
" records (File and JobMedia) will be deleted from the catalog."
      "Press OK to proceed with delete operation.?"),
      QMessageBox::Ok | QMessageBox::Cancel)
      == QMessageBox::Cancel) { return; }

   QString cmd("delete job jobid=");
   cmd += m_selectedJobs;
   consoleCommand(cmd, false);
   populateTable();
}

void JobList::consoleRestartJob()
{
   QString cmd;

   cmd = tr("run job=\"%1\" client=\"%2\" level=%3").arg(m_jobName).arg(m_clientName).arg(m_levelName);
   if (m_filesetName != "" && m_filesetName != "*None*") {
      cmd += tr(" fileset=\"%1\"").arg(m_filesetName);
   }

   if (mainWin->m_commandDebug) Pmsg1(000, "Run cmd : %s\n",cmd.toUtf8().data());
   consoleCommand(cmd, false);
   populateTable();
}



void JobList::consolePurgeFiles()
{
   if (QMessageBox::warning(this, "Bat",
      tr("Are you sure you want to purge ??  !!!.\n"
"The Purge command will delete associated Catalog database records from Jobs and"
" Volumes without considering the retention period. Purge  works only on the"
" Catalog database and does not affect data written to Volumes. This command can"
" be dangerous because you can delete catalog records associated with current"
" backups of files, and we recommend that you do not use it unless you know what"
" you are doing.\n"
      "Press OK to proceed with the purge operation?"),
      QMessageBox::Ok | QMessageBox::Cancel)
      == QMessageBox::Cancel) { return; }

   m_console->m_warningPrevent = true;
   foreach(QString job, m_selectedJobsList) {
      QString cmd("purge files jobid=");
      cmd += job;
      consoleCommand(cmd, false);
   }
   m_console->m_warningPrevent = false;
   populateTable();
}

/*
 * Subroutine to call preRestore to restore from a select job
 */
void JobList::preRestoreFromJob()
{
   new prerestorePage(m_currentJob, R_JOBIDLIST);
}

/*
 * Subroutine to call preRestore to restore from a select job
 */
void JobList::preRestoreFromTime()
{
   new prerestorePage(m_currentJob, R_JOBDATETIME);
}

/*
 * Subroutine to call class to show the log in the database from that job
 */
void JobList::showLogForJob()
{
   QTreeWidgetItem* pageSelectorTreeWidgetItem = mainWin->getFromHash(this);
   new JobLog(m_currentJob, pageSelectorTreeWidgetItem);
}

/*
 * Subroutine to call class to show the log in the database from that job
 */
void JobList::showInfoForJob(QTableWidgetItem * /*item*/)
{
   QTreeWidgetItem* pageSelectorTreeWidgetItem = mainWin->getFromHash(this);
   new Job(m_currentJob, pageSelectorTreeWidgetItem);
}

/*
 * Cancel a running job
 */
void JobList::consoleCancelJob()
{
   QString cmd("cancel jobid=");
   cmd += m_currentJob;
   consoleCommand(cmd);
}

/*
 * Graph this table
 */
void JobList::graphTable()
{
#ifdef HAVE_QWT
   JobPlotPass pass;
   pass.recordLimitCheck = limitCheckBox->checkState();
   pass.daysLimitCheck = daysCheckBox->checkState();
   pass.recordLimitSpin = limitSpinBox->value();
   pass.daysLimitSpin = daysSpinBox->value();
   pass.jobCombo = jobComboBox->currentText();
   pass.clientCombo = clientComboBox->currentText();
   pass.volumeCombo = volumeComboBox->currentText();
   pass.fileSetCombo = fileSetComboBox->currentText();
   pass.purgedCombo = purgedComboBox->currentText();
   pass.levelCombo = levelComboBox->currentText();
   pass.statusCombo = statusComboBox->currentText();
   pass.use = true;
   QTreeWidgetItem* pageSelectorTreeWidgetItem = mainWin->getFromHash(this);
   new JobPlot(pageSelectorTreeWidgetItem, pass);
#endif
}

/*
 * Save user settings associated with this page
 */
void JobList::writeSettings()
{
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup(m_groupText);
   settings.setValue(m_splitText, m_splitter->saveState());
   settings.setValue("FilterCopyCheckState", filterCopyCheckBox->checkState());
   settings.setValue("FilterMigrationCheckState", filterMigrationCheckBox->checkState());
   settings.endGroup();
}

/*
 * Read and restore user settings associated with this page
 */
void JobList::readSettings()
{
   m_groupText = "JobListPage";
   m_splitText = "splitterSizes_2";
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup(m_groupText);
   if (settings.contains(m_splitText)) {
      m_splitter->restoreState(settings.value(m_splitText).toByteArray());
   }
   filterCopyCheckBox->setCheckState((Qt::CheckState)settings.value("FilterCopyCheckState").toInt());
   filterMigrationCheckBox->setCheckState((Qt::CheckState)settings.value("FilterMigrationCheckState").toInt());
   settings.endGroup();
}

/*
 * Function to fill m_selectedJobsCount and m_selectedJobs with selected values
 */
void JobList::selectionChanged()
{
   QList<int> rowList;
   QList<QTableWidgetItem *> sitems = mp_tableWidget->selectedItems();
   foreach (QTableWidgetItem *sitem, sitems) {
      int row = sitem->row();
      if (!rowList.contains(row)) {
         rowList.append(row);
      }
   }

   m_selectedJobs = "";
   m_selectedJobsList.clear();
   bool first = true;
   foreach(int row, rowList) {
      QTableWidgetItem * sitem = mp_tableWidget->item(row, m_jobIdIndex);
      if (!first) m_selectedJobs.append(",");
      else first = false;
      m_selectedJobs.append(sitem->text());
      m_selectedJobsList.append(sitem->text());
   }
   m_selectedJobsCount = rowList.count();
   if (m_selectedJobsCount > 1) {
      QString text = QString( tr("Delete list of %1 Jobs")).arg(m_selectedJobsCount);
      actionDeleteJob->setText(text);
      text = QString( tr("Purge Files from list of %1 Jobs")).arg(m_selectedJobsCount);
      actionPurgeFiles->setText(text);
   } else {
      actionDeleteJob->setText(tr("Delete Single Job"));
      actionPurgeFiles->setText(tr("Purge Files from single job"));
   }

   /* remove all actions */
   foreach(QAction* mediaAction, mp_tableWidget->actions()) {
      mp_tableWidget->removeAction(mediaAction);
   }

   /* Add Actions */
   mp_tableWidget->addAction(actionRefreshJobList);
   if (m_selectedJobsCount == 1) {
      mp_tableWidget->addAction(actionListFilesOnJob);
      mp_tableWidget->addAction(actionListJobMedia);
      mp_tableWidget->addAction(actionRestartJob);
      mp_tableWidget->addAction(actionRestoreFromJob);
      mp_tableWidget->addAction(actionRestoreFromTime);
      mp_tableWidget->addAction(actionShowLogForJob);
      mp_tableWidget->addAction(actionShowInfoForJob);
   }
   if (m_selectedJobsCount >= 1) {
      mp_tableWidget->addAction(actionDeleteJob);
      mp_tableWidget->addAction(actionPurgeFiles);
   }

   /* Make Connections */
   if (m_checkCurrentWidget) {
      int row = mp_tableWidget->currentRow();
      QTableWidgetItem* jobitem = mp_tableWidget->item(row, 0);
      m_currentJob = jobitem->text();    /* get JobId */
      jobitem = mp_tableWidget->item(row, m_clientIndex);
      m_clientName = jobitem->text();    /* get Client Name */
      jobitem = mp_tableWidget->item(row, m_nameIndex);
      m_jobName = jobitem->text();    /* get Job Name */
      jobitem = mp_tableWidget->item(row, m_levelIndex);
      m_levelName = jobitem->text();    /* get level */
      jobitem = mp_tableWidget->item(row, m_filesetIndex);
      if (jobitem) {
         m_filesetName = jobitem->text();    /* get FileSet Name */
      } else {
         m_filesetName = "";
      }

      /* include purged action or not */
      jobitem = mp_tableWidget->item(row, m_purgedIndex);
      QString purged = jobitem->text();
/*      mp_tableWidget->removeAction(actionPurgeFiles);
      if (purged == tr("No") ) {
         mp_tableWidget->addAction(actionPurgeFiles);
      }*/

      /* include restore from time and job action or not */
      jobitem = mp_tableWidget->item(row, m_typeIndex);
      QString type = jobitem->text();
      if (m_selectedJobsCount == 1) {
         mp_tableWidget->removeAction(actionRestoreFromJob);
         mp_tableWidget->removeAction(actionRestoreFromTime);
         if (type == tr("Backup")) {
            mp_tableWidget->addAction(actionRestoreFromJob);
            mp_tableWidget->addAction(actionRestoreFromTime);
         }
      }

      /* include cancel action or not */
      jobitem = mp_tableWidget->item(row, m_statusIndex);
      QString status = jobitem->text();
      mp_tableWidget->removeAction(actionCancelJob);
      if (status == tr("Running") || status == tr("Created, not yet running")) {
         mp_tableWidget->addAction(actionCancelJob);
      }
   }
}

/*
 *  Function to prevent the splitter from making index 0 of the size larger than it
 *  needs to be
 */
void JobList::splitterMoved(int /*pos*/, int /*index*/)
{
   int frameMax = frame->maximumHeight();
   QList<int> sizes = m_splitter->sizes();
   int sizeSum = 0;
   foreach(int size, sizes) { sizeSum += size; }
   if (sizes[0] > frameMax) {
      sizes[0] = frameMax;
      sizes[1] = sizeSum - frameMax;
      m_splitter->setSizes(sizes);
   }
}
