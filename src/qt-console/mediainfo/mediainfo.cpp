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
#include <QAbstractEventDispatcher>
#include <QTableWidgetItem>
#include <QMessageBox>
#include "mediaedit/mediaedit.h"
#include "relabel/relabel.h"
#include "run/run.h"
#include "mediainfo.h"
#include "util/fmtwidgetitem.h"
#include "job/job.h"

/*
 * A constructor 
 */
MediaInfo::MediaInfo(QTreeWidgetItem *parentWidget, QString &mediaName) 
  : Pages()
{
   setupUi(this);
   pgInitialize(tr("Media Info"), parentWidget);
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/cartridge-edit.png")));
   m_mediaName = mediaName;
   connect(pbPrune, SIGNAL(clicked()), this, SLOT(pruneVol()));
   connect(pbPurge, SIGNAL(clicked()), this, SLOT(purgeVol()));
   connect(pbDelete, SIGNAL(clicked()), this, SLOT(deleteVol()));
   connect(pbEdit, SIGNAL(clicked()), this, SLOT(editVol()));
   connect(tableJob, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this, SLOT(showInfoForJob(QTableWidgetItem *)));
   
   dockPage();
   setCurrent();
   populateForm();
}

/*
 * Subroutine to call class to show the log in the database from that job
 */
void MediaInfo::showInfoForJob(QTableWidgetItem * item)
{
   QTreeWidgetItem* pageSelectorTreeWidgetItem = mainWin->getFromHash(this);
   int row = item->row();
   QString jobid = tableJob->item(row, 0)->text();
   new Job(jobid, pageSelectorTreeWidgetItem);
//   connect(j, SIGNAL(destroyed()), this, SLOT(populateTree()));
}

void MediaInfo::pruneVol()
{
   new prunePage(m_mediaName, "");
//   connect(prune, SIGNAL(destroyed()), this, SLOT(populateTree()));
}

// TODO: use same functions as in medialist.cpp
void MediaInfo::purgeVol()
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

   QString cmd("purge volume=");
   cmd += m_mediaName;
   consoleCommand(cmd);
}

void MediaInfo::deleteVol()
{
   if (QMessageBox::warning(this, "Bat",
      tr("Are you sure you want to delete??  !!!.\n"
"This delete command is used to delete a Volume record and all associated catalog"
" records that were created. This command operates only on the Catalog"
" database and has no effect on the actual data written to a Volume. This"
" command can be dangerous and we strongly recommend that you do not use"
" it unless you know what you are doing.  All Jobs and all associated"
" records (File and JobMedia) will be deleted from the catalog."
      "Press OK to proceed with delete operation.?"),
      QMessageBox::Ok | QMessageBox::Cancel)
      == QMessageBox::Cancel) { return; }

   QString cmd("delete volume=");
   cmd += m_mediaName;
   consoleCommand(cmd);
}

void MediaInfo::editVol()
{
   new MediaEdit(mainWin->getFromHash(this), m_mediaId);
//   connect(edit, SIGNAL(destroyed()), this, SLOT(populateTree()));
}

/*
 * Populate the text in the window
 */
void MediaInfo::populateForm()
{
   utime_t t;
   time_t ttime;

   QString stat, LastWritten;
   struct tm tm;
   char buf[256];
   QString query = 
      "SELECT MediaId, VolumeName, Pool.Name, MediaType, FirstWritten,"
      "LastWritten, VolMounts, VolBytes, Media.Enabled,"
      "Location.Location, VolStatus, RecyclePool.Name, Media.Recycle, "
      "VolReadTime/1000000, VolWriteTime/1000000, Media.VolUseDuration, "
      "Media.MaxVolJobs, Media.MaxVolFiles, Media.MaxVolBytes, "
      "Media.VolRetention,InChanger,Slot "
      "FROM Media JOIN Pool USING (PoolId) LEFT JOIN Pool AS RecyclePool "
      "ON (Media.RecyclePoolId=RecyclePool.PoolId) "
      "LEFT JOIN Location ON (Media.LocationId=Location.LocationId) "
      "WHERE Media.VolumeName='" + m_mediaName + "'";

   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "MediaInfo query cmd : %s\n",query.toUtf8().data());
   }
   QStringList results;
   if (m_console->sql_cmd(query, results)) {
      QString resultline;
      QStringList fieldlist;

      foreach (resultline, results) { // should have only one result
         fieldlist = resultline.split("\t");
         QStringListIterator fld(fieldlist);
         m_mediaId = fld.next();

         label_VolumeName->setText(fld.next());
         label_Pool->setText(fld.next());
         label_MediaType->setText(fld.next());
         label_FirstWritten->setText(fld.next());
         LastWritten = fld.next();
         label_LastWritten->setText(LastWritten);
//         label_VolFiles->setText(fld.next());
         label_VolMounts->setText(fld.next());
         label_VolBytes->setText(convertBytesSI(fld.next().toULongLong()));
         label_Enabled->setPixmap(QPixmap(":/images/inflag" + fld.next() + ".png"));
         label_Location->setText(fld.next());
         label_VolStatus->setText(fld.next());
         label_RecyclePool->setText(fld.next());
         chkbox_Recycle->setCheckState(fld.next().toInt()?Qt::Checked:Qt::Unchecked);         
         edit_utime(fld.next().toULongLong(), buf, sizeof(buf));
         label_VolReadTime->setText(QString(buf));

         edit_utime(fld.next().toULongLong(), buf, sizeof(buf));
         label_VolWriteTime->setText(QString(buf));

         edit_utime(fld.next().toULongLong(), buf, sizeof(buf));
         label_VolUseDuration->setText(QString(buf));

         label_MaxVolJobs->setText(fld.next());
         label_MaxVolFiles->setText(fld.next());
         label_MaxVolBytes->setText(fld.next());

         stat = fld.next();
         edit_utime(stat.toULongLong(), buf, sizeof(buf));
         label_VolRetention->setText(QString(buf));

         if (LastWritten != "") {
            t = str_to_utime(LastWritten.toAscii().data());
            t = t + stat.toULongLong();
            ttime = t;
            localtime_r(&ttime, &tm);         
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
            label_Expire->setText(QString(buf));
         }
         label_Online->setPixmap(QPixmap(":/images/inflag"+fld.next()+".png"));
//         label_VolFiles->setText(fld.next());
//         label_VolErrors->setText(fld.next());

//         stat=fld.next();

//         jobstatus_to_ascii_gui(stat[0].toAscii(), buf, sizeof(buf));
//         stat = buf;
//       
      }
   }

   query = 
      "SELECT DISTINCT JobId, Name, StartTime, Type, Level, JobFiles,"
      "JobBytes,JobStatus "
      "FROM Job JOIN JobMedia USING (JobId) JOIN Media USING (MediaId) "
      "WHERE Media.VolumeName = '" + m_mediaName + "'";

   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "MediaInfo query cmd : %s\n",query.toUtf8().data());
   }
   results.clear();
   if (m_console->sql_cmd(query, results)) {
      QString resultline;
      QStringList fieldlist;
      int row = 0;
      tableJob->setRowCount(results.size());
      foreach (resultline, results) {
         fieldlist = resultline.split("\t");
         QStringListIterator fld(fieldlist);
         int index=0;
         TableItemFormatter jobitem(*tableJob, row);

         /* JobId */
         jobitem.setNumericFld(index++, fld.next()); 

         /* job name */
         jobitem.setTextFld(index++, fld.next());

         /* job starttime */
         jobitem.setTextFld(index++, fld.next(), true);

         /* job type */
         jobitem.setJobTypeFld(index++, fld.next());

         /* job level */
         jobitem.setJobLevelFld(index++, fld.next());

         /* job files */
         jobitem.setNumericFld(index++, fld.next());

         /* job bytes */
         jobitem.setBytesFld(index++, fld.next());

         /* job status */
         jobitem.setJobStatusFld(index++, fld.next());
         row++;
      }
   }

   tableJob->resizeColumnsToContents();
   tableJob->resizeRowsToContents();
   tableJob->verticalHeader()->hide();

   /* make read only */
   tableJob->setEditTriggers(QAbstractItemView::NoEditTriggers);
}
