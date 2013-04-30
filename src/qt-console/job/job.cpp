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
 
#include "bat.h"
#include "job.h"
#include "util/fmtwidgetitem.h"
#include "mediainfo/mediainfo.h"
#include "run/run.h"

Job::Job(QString &jobId, QTreeWidgetItem *parentTreeWidgetItem) : Pages()
{
   setupUi(this);
   pgInitialize(tr("Job"), parentTreeWidgetItem);
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/joblog.png")));
   m_cursor = new QTextCursor(textJobLog->document());

   m_jobId = jobId;
   m_timer = NULL;
   getFont();

   connect(pbRefresh, SIGNAL(clicked()), this, SLOT(populateAll()));
   connect(pbDelete, SIGNAL(clicked()), this, SLOT(deleteJob()));
   connect(pbCancel, SIGNAL(clicked()), this, SLOT(cancelJob()));
   connect(pbRun, SIGNAL(clicked()), this, SLOT(rerun()));
   connect(list_Volume, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(showInfoVolume(QListWidgetItem *)));

   populateAll();
   dockPage();
   setCurrent();
}

void Job::rerun()
{
   new runPage(label_Name->text(),
               label_Level->text(),
               label_Pool->text(),
               QString(""),              // storage
               label_Client->text(),
               label_FileSet->text());
}

void Job::showInfoVolume(QListWidgetItem *item)
{
   QString s= item->text();
   QTreeWidgetItem* pageSelectorTreeWidgetItem = mainWin->getFromHash(this);

   MediaInfo *m = new MediaInfo(pageSelectorTreeWidgetItem, s);
   connect(m, SIGNAL(destroyed()), this, SLOT(populateTree()));
}

void Job::deleteJob()
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
   cmd += m_jobId;
   consoleCommand(cmd, false);
   closeStackPage();
}

void Job::cancelJob()
{
   if (QMessageBox::warning(this, "Bat",
                            tr("Are you sure you want to cancel this job?"),
                            QMessageBox::Ok | QMessageBox::Cancel)
       == QMessageBox::Cancel) { return; }

   QString cmd("cancel jobid=");
   cmd += m_jobId;
   consoleCommand(cmd, false);
}

void Job::getFont()
{
   QFont font = textJobLog->font();

   QString dirname;
   m_console->getDirResName(dirname);
   QSettings settings(dirname, "bat");
   settings.beginGroup("Console");
   font.setFamily(settings.value("consoleFont", "Courier").value<QString>());
   font.setPointSize(settings.value("consolePointSize", 10).toInt());
   font.setFixedPitch(settings.value("consoleFixedPitch", true).toBool());
   settings.endGroup();
   textJobLog->setFont(font);
}

void Job::populateAll()
{
// Pmsg0(50, "populateAll()\n");
   populateText();
   populateForm();
   populateVolumes();
}

/*
 * Populate the text in the window
 * TODO: Just append new text instead of clearing the window
 */
void Job::populateText()
{
   textJobLog->clear();
   QString query;
   query = "SELECT Time, LogText FROM Log WHERE JobId='" + m_jobId + "' order by Time";

   /* This could be a log item */
   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "Log query cmd : %s\n", query.toUtf8().data());
   }
  
   QStringList results;
   if (m_console->sql_cmd(query, results)) {

      if (!results.size()) {
         QMessageBox::warning(this, tr("Bat"),
            tr("There were no results!\n"
               "It is possible you may need to add \"catalog = all\" "
               "to the Messages resource for this job.\n"), QMessageBox::Ok);
         return;
      } 

      QString jobstr("JobId "); /* FIXME: should this be translated ? */
      jobstr += m_jobId;

      QString htmlbuf("<html><body><pre>");
  
      /* Iterate through the lines of results. */
      QString field;
      QStringList fieldlist;
      QString lastTime;
      QString lastSvc;
      foreach (QString resultline, results) {
         fieldlist = resultline.split("\t");
         
         if (fieldlist.size() < 2)
            continue;

         QString curTime = fieldlist[0].trimmed();

         field = fieldlist[1].trimmed();
         int colon = field.indexOf(":");
         if (colon > 0) {
            /* string is like <service> <jobId xxxx>: ..." 
             * we split at ':' then remove the jobId xxxx string (always the same) */ 
            QString curSvc(field.left(colon).replace(jobstr,"").trimmed());
            if (curSvc == lastSvc  && curTime == lastTime) {
               curTime.clear();
               curSvc.clear(); 
            } else {
               lastTime = curTime;
               lastSvc = curSvc;
            }
//          htmlbuf += "<td>" + curTime + "</td>";
            htmlbuf += "\n" + curSvc + " ";

            /* rest of string is marked as pre-formatted (here trimming should
             * be avoided, to preserve original formatting) */
            QString msg(field.mid(colon+2));
            if (msg.startsWith( tr("Error:")) ) { /* FIXME: should really be translated ? */
               /* error msg, use a specific class */
               htmlbuf += "</pre><pre class=err>" + msg + "</pre><pre>";
            } else {
               htmlbuf += msg ;
            }
         } else {
            /* non standard string, place as-is */
            if (curTime == lastTime) {
               curTime.clear();
            } else {
               lastTime = curTime;
            }
//          htmlbuf += "<td>" + curTime + "</td>";
            htmlbuf += "\n" + field ;
         }
  
      } /* foreach resultline */

      htmlbuf += "</pre></body></html>";

      /* full text ready. Here a custom sheet is used to align columns */
      QString logSheet(".err {color:#FF0000;}");
      textJobLog->document()->setDefaultStyleSheet(logSheet);
      textJobLog->document()->setHtml(htmlbuf); 
      textJobLog->moveCursor(QTextCursor::Start);

   } /* if results from query */
  
}


void Job::updateRunInfo()
{
   QString cmd;
   QStringList results;
   QStringList lst;
   bool parseit=false;

   cmd = QString(".status client=\"" + m_client + "\" running");
/*
 *  JobId 5 Job backup.2010-12-21_09.28.17_03 is running.
 *   VSS Full Backup Job started: 21-Dec-10 09:28
 *   Files=4 Bytes=610,976 Bytes/sec=87,282 Errors=0
 *   Files Examined=4
 *   Processing file: /tmp/regress/build/po/de.po
 *   SDReadSeqNo=5 fd=5
 *
 *  Or
 *  JobId=5
 *  Job=backup.2010-12-21_09.28.17_03
 *  VSS=1
 *  Files=4
 *  Bytes=610976
 *
 */
   QRegExp jobline("(JobId) (\\d+) Job ");
   QRegExp itemline("([\\w /]+)[:=]\\s*(.+)");
   QRegExp oldline("Files=([\\d,]+) Bytes=([\\d,]+) Bytes/sec=([\\d,]+) Errors=([\\d,]+)");
   QString com(",");
   QString empty("");
   
   if (m_console->dir_cmd(cmd, results)) {
      foreach (QString mline, results) {
         foreach (QString line, mline.split("\n")) { 
            line = line.trimmed();
            if (oldline.indexIn(line) >= 0) {
               if (parseit) {
                  lst = oldline.capturedTexts();
                  label_JobErrors->setText(lst[4]);
                  label_Speed->setText(convertBytesSI(lst[3].replace(com, empty).toULongLong())+"/s");
                  label_JobFiles->setText(lst[1]);
                  label_JobBytes->setText(convertBytesSI(lst[2].replace(com, empty).toULongLong()));
               }
               continue;

            } else if (jobline.indexIn(line) >= 0) {
               lst = jobline.capturedTexts();
               lst.removeFirst();

            } else if (itemline.indexIn(line) >= 0) {
               lst = itemline.capturedTexts();
               lst.removeFirst();

            } else {
               if (mainWin->m_miscDebug) 
                  Pmsg1(0, "bad line=%s\n", line.toUtf8().data());
               continue;
            }
            if (lst.count() < 2) {
               if (mainWin->m_miscDebug) 
                  Pmsg2(0, "bad line=%s count=%d\n", line.toUtf8().data(), lst.count());
            }
            if (lst[0] == "JobId") {
               if (lst[1] == m_jobId) {
                  parseit = true;
               } else {
                  parseit = false;
               }
            }
            if (!parseit) {
               continue;
            }
            
//         } else if (lst[0] == "Job") {
//            grpRun->setTitle(lst[1]);
            
//               
//         } else if (lst[0] == "VSS") {

//         } else if (lst[0] == "Level") {
//            Info->setText(lst[1]);
//
//         } else if (lst[0] == "JobType") {
//
//         } else if (lst[0] == "JobStarted") {
//            Started->setText(lst[1]);

            if (lst[0] == "Errors") {
               label_JobErrors->setText(lst[1]);
               
            } else if (lst[0] == "Bytes/sec") {
               label_Speed->setText(convertBytesSI(lst[1].toULongLong())+"/s");
               
            } else if (lst[0] == "Files") {
               label_JobFiles->setText(lst[1]);
               
            } else if (lst[0] == "Bytes") {
               label_JobBytes->setText(convertBytesSI(lst[1].toULongLong()));
               
            } else if (lst[0] == "Files Examined") {
               label_FilesExamined->setText(lst[1]);
               
            } else if (lst[0] == "Processing file") {
               label_CurrentFile->setText(lst[1]);
            }
         }
      }
   }
}

/*
 * Populate the text in the window
 */
void Job::populateForm()
{
   QString stat, err;
   char buf[256];
   QString query = 
      "SELECT JobId, Job.Name, Level, Client.Name, Pool.Name, FileSet,"
      "SchedTime, StartTime, EndTime, EndTime-StartTime AS Duration, "
      "JobBytes, JobFiles, JobErrors, JobStatus, PurgedFiles "
      "FROM Job JOIN Client USING (ClientId) "
        "LEFT JOIN Pool ON (Job.PoolId = Pool.PoolId) "
        "LEFT JOIN FileSet ON (Job.FileSetId = FileSet.FileSetId)"
      "WHERE JobId=" + m_jobId; 
   QStringList results;
   if (m_console->sql_cmd(query, results)) {
      QString resultline, duration;
      QStringList fieldlist;

      foreach (resultline, results) { // should have only one result
         fieldlist = resultline.split("\t");
         QStringListIterator fld(fieldlist);
         label_JobId->setText(fld.next());
         label_Name->setText(fld.next());
         
         label_Level->setText(job_level_to_str(fld.next()[0].toAscii()));

         m_client = fld.next();
         label_Client->setText(m_client);
         label_Pool->setText(fld.next());
         label_FileSet->setText(fld.next());
         label_SchedTime->setText(fld.next());
         label_StartTime->setText(fld.next());
         label_EndTime->setText(fld.next());
         duration = fld.next();
         /* 
          * Note: if we have a negative duration, it is because the EndTime
          *  is zero (i.e. the Job is still running).  We should use 
          *  duration = StartTime - current_time
          */
         if (duration.left(1) == "-") {
            duration = "0.0";
         }
         label_Duration->setText(duration);

         label_JobBytes->setText(convertBytesSI(fld.next().toULongLong()));
         label_JobFiles->setText(fld.next());
         err = fld.next();
         label_JobErrors->setText(err);

         stat = fld.next();
         if (stat == "T" && err.toInt() > 0) {
            stat = "W";
         }
         if (stat == "R") {
            pbDelete->setVisible(false);
            pbCancel->setVisible(true);
            grpRun->setVisible(true);
            if (!m_timer) {
               m_timer = new QTimer(this);
               connect(m_timer, SIGNAL(timeout()), this, SLOT(populateAll()));
               m_timer->start(30000);
            }
            updateRunInfo();
         } else {
            pbDelete->setVisible(true);
            pbCancel->setVisible(false);
            grpRun->setVisible(false);
            if (m_timer) {
               m_timer->stop();
               delete m_timer;
               m_timer = NULL;
            }
         }
         label_JobStatus->setPixmap(QPixmap(":/images/" + stat + ".png"));
         jobstatus_to_ascii_gui(stat[0].toAscii(), buf, sizeof(buf));
         stat = buf;
         label_JobStatus->setToolTip(stat);

         chkbox_PurgedFiles->setCheckState(fld.next().toInt()?Qt::Checked:Qt::Unchecked);
      }
   }
}
  
void Job::populateVolumes()
{

   QString query = 
      "SELECT DISTINCT VolumeName, InChanger, Slot "
      "FROM Job JOIN JobMedia USING (JobId) JOIN Media USING (MediaId) "
      "WHERE JobId=" + m_jobId + " ORDER BY VolumeName "; 
   if (mainWin->m_sqlDebug) Pmsg1(0, "Query cmd : %s\n",query.toUtf8().data());
         

   QStringList results;
   if (m_console->sql_cmd(query, results)) {
      QString resultline;
      QStringList fieldlist;
      list_Volume->clear();
      foreach (resultline, results) { // should have only one result
         fieldlist = resultline.split("\t");
         QStringListIterator fld(fieldlist);
//         QListWidgetItem(QIcon(":/images/inchanger" + fld.next() + ".png"), 
//                         fld.next(), list_Volume);
         list_Volume->addItem(fld.next());
      }
   }
}

//QListWidgetItem ( const QIcon & icon, const QString & text, QListWidget * parent = 0, int type = Type )
