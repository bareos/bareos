/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2010 Free Software Foundation Europe e.V.

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

/*
 * preRestore -> dialog put up to determine the restore type
 *
 * Kern Sibbald, February MMVII
 */

#include "bat.h"
#include "restore.h"

/* Constructor to have job id list default in */
prerestorePage::prerestorePage(QString &data, unsigned int datatype) : Pages()
{
   m_dataIn = data;
   m_dataInType = datatype;
   buildPage();
}

/* Basic Constructor */
prerestorePage::prerestorePage()
{
   m_dataIn = "";
   m_dataInType = R_NONE;
   buildPage();
}

/*
 * This is really the constructor
 */
void prerestorePage::buildPage()
{
   m_name = tr("Restore");
   setupUi(this);
   pgInitialize();
   m_conn = m_console->notifyOff();
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/restore.png")));

   jobCombo->addItems(m_console->job_list);
   filesetCombo->addItems(m_console->fileset_list);
   clientCombo->addItems(m_console->client_list);
   poolCombo->addItem(tr("Any"));
   poolCombo->addItems(m_console->pool_list);
   storageCombo->addItems(m_console->storage_list);
   /* current or before . .  Start out with current checked */
   recentCheckBox->setCheckState(Qt::Checked);
   beforeDateTime->setDisplayFormat(mainWin->m_dtformat);
   beforeDateTime->setDateTime(QDateTime::currentDateTime());
   beforeDateTime->setEnabled(false);
   selectFilesRadio->setChecked(true);
   if (m_dataInType == R_NONE) {
      selectJobRadio->setChecked(true);
      selectJobIdsRadio->setChecked(false);
      jobIdEdit->setText(tr("Comma separated list of Job Ids"));
      jobIdEdit->setEnabled(false);
   } else if (m_dataInType == R_JOBIDLIST) {
      selectJobIdsRadio->setChecked(true);
      selectJobRadio->setChecked(false);
      jobIdEdit->setText(m_dataIn);
      jobRadioClicked(false);
      QStringList fieldlist;
      if (jobdefsFromJob(fieldlist, m_dataIn) == 1) {
         filesetCombo->setCurrentIndex(filesetCombo->findText(fieldlist[2], Qt::MatchExactly));
         clientCombo->setCurrentIndex(clientCombo->findText(fieldlist[1], Qt::MatchExactly));
         jobCombo->setCurrentIndex(jobCombo->findText(fieldlist[0], Qt::MatchExactly));
      }
   } else if (m_dataInType == R_JOBDATETIME) {
      selectJobRadio->setChecked(true);
      selectJobIdsRadio->setChecked(false);
      jobIdEdit->setText(tr("Comma separated list of Job Ids"));
      jobIdEdit->setEnabled(false);
      recentCheckBox->setCheckState(Qt::Unchecked);
      jobRadioClicked(true);
      QStringList fieldlist;
      if (jobdefsFromJob(fieldlist, m_dataIn) == 1) {
         filesetCombo->setCurrentIndex(filesetCombo->findText(fieldlist[2], Qt::MatchExactly));
         clientCombo->setCurrentIndex(clientCombo->findText(fieldlist[1], Qt::MatchExactly));
         jobCombo->setCurrentIndex(jobCombo->findText(fieldlist[0], Qt::MatchExactly));
         beforeDateTime->setDateTime(QDateTime::fromString(fieldlist[3], mainWin->m_dtformat));
     }
   }
   job_name_change(0);
   connect(jobCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(job_name_change(int)));
   connect(okButton, SIGNAL(pressed()), this, SLOT(okButtonPushed()));
   connect(cancelButton, SIGNAL(pressed()), this, SLOT(cancelButtonPushed()));
   connect(recentCheckBox, SIGNAL(stateChanged(int)), this, SLOT(recentChanged(int)));
   connect(selectJobRadio, SIGNAL(clicked(bool)), this, SLOT(jobRadioClicked(bool)));
   connect(selectJobIdsRadio, SIGNAL(clicked(bool)), this, SLOT(jobidsRadioClicked(bool)));
   connect(jobIdEdit, SIGNAL(editingFinished()), this, SLOT(jobIdEditFinished()));

   dockPage();
   setCurrent();
   this->show();
   if (mainWin->m_miscDebug) Pmsg0(000, "Leave preRestore\n");
}


/*
 * Check to make sure all is ok then start either the select window or the restore
 * run window
 */
void prerestorePage::okButtonPushed()
{
   if (!selectJobRadio->isChecked()) {
      if (!checkJobIdList()) {
         return;
      }
   }
   QString cmd;

   this->hide();


   cmd = QString("restore");
   cmd += " fileset=\"" + filesetCombo->currentText() + "\"";
   cmd += " client=\"" + clientCombo->currentText() + "\"";
   if (selectJobRadio->isChecked()) {
      if (poolCombo->currentText() != tr("Any") ){
         cmd += " pool=\"" + poolCombo->currentText() + "\"";
      }
      cmd += " storage=\"" + storageCombo->currentText() + "\"";
      if (recentCheckBox->checkState() == Qt::Checked) {
         cmd += " current";
      } else {
         QDateTime stamp = beforeDateTime->dateTime();
         QString before = stamp.toString(mainWin->m_dtformat);
         cmd += " before=\"" + before + "\"";
      }
   } else {
      cmd += " jobid=\"" + jobIdEdit->text() + "\"";
   }
   if (selectFilesRadio->isChecked()) {
      if (!selectJobIdsRadio->isChecked())
         cmd += " select";
   } else {
      cmd += " all done";
   }

   if (mainWin->m_commandDebug) {
      Pmsg1(000, "preRestore command \'%s\'\n", cmd.toUtf8().data());
   }
   /*
    * Send off command that looks something like:
    *
    * restore fileset="Full Set" client="timmy-fd"
    *        storage="File" current select
    */
   m_console->write_dir(m_conn, cmd.toUtf8().data());

   /* Note, do not turn notifier back on here ... */
   if (selectFilesRadio->isChecked()) {
      setConsoleCurrent();
      closeStackPage();
      /* wait will be exited in the restore page constructor */
      mainWin->waitEnter();
   } else {
      closeStackPage();
      mainWin->resetFocus();
   }
   m_console->notify(m_conn, true);
   if (mainWin->m_miscDebug) Pmsg0(000, "preRestore OK pressed\n");
}


/*
 * Destroy the instace of the class
 */
void prerestorePage::cancelButtonPushed()
{
   mainWin->set_status(tr("Canceled"));
   this->hide();
   m_console->notify(m_conn, true);
   closeStackPage();
}


/*
 * Handle updating the other widget with job defaults when the job combo is changed.
 */
void prerestorePage::job_name_change(int index)
{
   job_defaults job_defs;

   (void)index;
   job_defs.job_name = jobCombo->currentText();
   if (m_console->get_job_defaults(m_conn, job_defs)) {
      filesetCombo->setCurrentIndex(filesetCombo->findText(job_defs.fileset_name, Qt::MatchExactly));
      clientCombo->setCurrentIndex(clientCombo->findText(job_defs.client_name, Qt::MatchExactly));
      poolCombo->setCurrentIndex(poolCombo->findText(tr("Any"), Qt::MatchExactly));
      storageCombo->setCurrentIndex(storageCombo->findText(job_defs.store_name, Qt::MatchExactly));
   }
}

/*
 * Handle the change of enabled of input widgets when the recent checkbox state
 * is changed.
 */
void prerestorePage::recentChanged(int state)
{
   if ((state == Qt::Unchecked) && (selectJobRadio->isChecked())) {
      beforeDateTime->setEnabled(true);
   } else {
      beforeDateTime->setEnabled(false);
   }
}


/*
 * For when jobs list is to be used, return a list which is the needed items from
 * the job record
 */
int prerestorePage::jobdefsFromJob(QStringList &fieldlist, QString &jobId)
{
   QString job, client, fileset;
   QString query("");
   query = "SELECT DISTINCT Job.Name AS JobName, Client.Name AS Client,"
   " FileSet.FileSet AS FileSet, Job.EndTime AS JobEnd,"
   " Job.Type AS JobType"
   " From Job, Client, FileSet"
   " WHERE Job.FileSetId=FileSet.FileSetId AND Job.ClientId=Client.ClientId"
   " AND JobId=\'" + jobId + "\'";
   if (mainWin->m_sqlDebug) { Pmsg1(000, "query = %s\n", query.toUtf8().data()); }
   QStringList results;
   if (m_console->sql_cmd(m_conn, query, results)) {
      QString field;

      /* Iterate through the lines of results, there should only be one. */
      foreach (QString resultline, results) {
         fieldlist = resultline.split("\t");
      } /* foreach resultline */
   } /* if results from query */

   /* ***FIXME*** This should not ever be getting more than one */
   return results.count() >= 1;
}

/*
 * Function to handle when the jobidlist line edit input loses focus or is entered
 */
void prerestorePage::jobIdEditFinished()
{
   checkJobIdList();
}

bool prerestorePage::checkJobIdList()
{
   /* Need to check and make sure the text is a comma separated list of integers */
   QString line = jobIdEdit->text();
   if (line.contains(" ")) {
      QMessageBox::warning(this, "Bat",
         tr("There can be no spaces in the text for the joblist.\n"
         "Press OK to continue?"), QMessageBox::Ok );
      return false;
   }
   QStringList joblist = line.split(",", QString::SkipEmptyParts);
   bool allintokay = true, alljobok = true, allisjob = true;
   QString jobName(""), clientName("");
   foreach (QString job, joblist) {
      bool intok;
      job.toInt(&intok, 10);
      if (intok) {
         /* are the integers representing a list of jobs all with the same job
          * and client */
         QStringList fields;
         if (jobdefsFromJob(fields, job) == 1) {
            if (jobName == "")
               jobName = fields[0];
            else if (jobName != fields[0])
               alljobok = false;
            if (clientName == "")
               clientName = fields[1];
            else if (clientName != fields[1])
               alljobok = false;
         } else {
            allisjob = false;
         }
      } else {
         allintokay = false;
      }
   }
   if (!allintokay){
      QMessageBox::warning(this, "Bat",
         tr("The string is not a comma separated list of integers.\n"
         "Press OK to continue?"), QMessageBox::Ok );
      return false;
   }
   if (!allisjob){
      QMessageBox::warning(this, tr("Bat"),
         tr("At least one of the jobs is not a valid job of type \"Backup\".\n"
         "Press OK to continue?"), QMessageBox::Ok );
      return false;
   }
   if (!alljobok){
      QMessageBox::warning(this, "Bat",
         tr("All jobs in the list must be of the same jobName and same client.\n"
         "Press OK to continue?"), QMessageBox::Ok );
      return false;
   }
   return true;
}

/*
 * Handle the change of enabled of input widgets when the job radio buttons
 * are changed.
 */
void prerestorePage::jobRadioClicked(bool checked)
{
   if (checked) {
      jobCombo->setEnabled(true);
      filesetCombo->setEnabled(true);
      clientCombo->setEnabled(true);
      poolCombo->setEnabled(true);
      storageCombo->setEnabled(true);
      recentCheckBox->setEnabled(true);
      if (!recentCheckBox->isChecked()) {
         beforeDateTime->setEnabled(true);
      }
      jobIdEdit->setEnabled(false);
      selectJobRadio->setChecked(true);
      selectJobIdsRadio->setChecked(false);
   } else {
      jobCombo->setEnabled(false);
      filesetCombo->setEnabled(false);
      clientCombo->setEnabled(false);
      poolCombo->setEnabled(false);
      storageCombo->setEnabled(false);
      recentCheckBox->setEnabled(false);
      beforeDateTime->setEnabled(false);
      jobIdEdit->setEnabled(true);
      selectJobRadio->setChecked(false);
      selectJobIdsRadio->setChecked(true);
   }
   if (mainWin->m_miscDebug) {
      Pmsg2(000, "jobRadio=%d jobidsRadio=%d\n", selectJobRadio->isChecked(),
         selectJobIdsRadio->isChecked());
   }
}

void prerestorePage::jobidsRadioClicked(bool checked)
{
   if (checked) {
      jobCombo->setEnabled(false);
      filesetCombo->setEnabled(false);
      clientCombo->setEnabled(false);
      poolCombo->setEnabled(false);
      storageCombo->setEnabled(false);
      recentCheckBox->setEnabled(false);
      beforeDateTime->setEnabled(false);
      jobIdEdit->setEnabled(true);
      selectJobRadio->setChecked(false);
      selectJobIdsRadio->setChecked(true);
   } else {
      jobCombo->setEnabled(true);
      filesetCombo->setEnabled(true);
      clientCombo->setEnabled(true);
      poolCombo->setEnabled(true);
      storageCombo->setEnabled(true);
      recentCheckBox->setEnabled(true);
      if (!recentCheckBox->isChecked()) {
         beforeDateTime->setEnabled(true);
      }
      jobIdEdit->setEnabled(false);
      selectJobRadio->setChecked(true);
      selectJobIdsRadio->setChecked(false);
   }
   if (mainWin->m_miscDebug) {
      Pmsg2(000, "jobRadio=%d jobidsRadio=%d\n", selectJobRadio->isChecked(),
         selectJobIdsRadio->isChecked());
   }
}
