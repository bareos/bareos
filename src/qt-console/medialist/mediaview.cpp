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
 
#include "bat.h"
#include <QAbstractEventDispatcher>
#include <QMenu>
#include <math.h>
#include "mediaview.h"
#include "mediaedit/mediaedit.h"
#include "mediainfo/mediainfo.h"
#include "joblist/joblist.h"
#include "relabel/relabel.h"
#include "run/run.h"
#include "util/fmtwidgetitem.h"

MediaView::MediaView() : Pages()
{
   setupUi(this);
   m_name = tr("Media");
   pgInitialize();
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/cartridge.png")));
   connect(m_pbApply, SIGNAL(pressed()), this, SLOT(applyPushed()));
   connect(m_pbEdit, SIGNAL(pressed()), this, SLOT(editPushed()));
   connect(m_pbPurge, SIGNAL(pressed()), this, SLOT(purgePushed()));
   connect(m_pbDelete, SIGNAL(pressed()), this, SLOT(deletePushed()));
   connect(m_pbPrune, SIGNAL(pressed()), this, SLOT(prunePushed()));
   connect(m_tableMedia, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), 
           this, SLOT(showInfoForMedia(QTableWidgetItem *)));

   /* mp_treeWidget, Storage Tree Tree Widget inherited from ui_medialist.h */
   m_populated = false;
   m_checkcurwidget = true;
   m_closeable = false;
}

void MediaView::showInfoForMedia(QTableWidgetItem * item)
{
   QTreeWidgetItem* pageSelectorTreeWidgetItem = mainWin->getFromHash(this);
   int row = item->row();
   QString vol = m_tableMedia->item(row, 0)->text();
   new MediaInfo(pageSelectorTreeWidgetItem, vol);
//   connect(j, SIGNAL(destroyed()), this, SLOT(populateTree()));
}

MediaView::~MediaView()
{
}

void MediaView::applyPushed()
{
   populateTable();
}

void MediaView::editPushed()
{
   QStringList sel;
   QString cmd;
   getSelection(sel);
   
   for(int i=0; i<sel.count(); i++) {
      cmd = sel.at(i);
      new MediaEdit(mainWin->getFromHash(this), cmd);
   }
}

void MediaView::purgePushed()
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

   QStringList lst;
   QString cmd;
   getSelection(lst);
   for(int i=0; i<lst.count(); i++) {
      cmd = "purge volume=" + lst.at(i);
      consoleCommand(cmd);
   }
   populateTable();
}

bool MediaView::getSelection(QStringList &list)
{
   int i, nb, nr_rows, row;
   bool *tab;
   QTableWidgetItem *it;
   QList<QTableWidgetItem*> items = m_tableMedia->selectedItems();

   /*
    * See if anything is selected.
    */
   nb = items.count();
   if (!nb) {
      return false;
   }

   /*
    * Create a nibble map for each row so we can see if its
    * selected or not.
    */
   nr_rows = m_tableMedia->rowCount();
   tab = (bool *)malloc (nr_rows * sizeof(bool));
   memset(tab, 0, sizeof(bool) * nr_rows);

   for (i = 0; i < nb; ++i) {
      row = items[i]->row();
      if (!tab[row]) {
         tab[row] = true;
         it = m_tableMedia->item(row, 0);
         list.append(it->text());
      }
   }
   free(tab);

   return list.count() > 0;
}

void MediaView::prunePushed()
{
   QStringList sel;
   QString cmd;
   getSelection(sel);

   for(int i=0; i<sel.count(); i++) {
      cmd = "prune volume=" + sel.at(i);
      consoleCommand(cmd);
   }
}


void MediaView::deletePushed()
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

   QStringList lst;
   QString cmd;
   getSelection(lst);
   for(int i=0; i<lst.count(); i++) {
      cmd = "delete volume=" + lst.at(i);
      consoleCommand(cmd);
   }
   populateTable();
}

void MediaView::populateForm()
{
   m_cbPool->clear();
   m_cbPool->addItem("");
   m_cbPool->addItems(m_console->pool_list);

   m_cbStatus->clear();
   m_cbStatus->addItem("");
   m_cbStatus->addItems(m_console->volstatus_list);

   m_cbMediaType->clear();
   m_cbMediaType->addItem("");
   m_cbMediaType->addItems(m_console->mediatype_list);

   m_cbLocation->clear();
   m_cbLocation->addItem("");
   m_cbLocation->addItems(m_console->location_list);
}

/* 
 * If chkExpired button is checked, we can remove all non Expired
 * entries
 */
void MediaView::filterExipired(QStringList &list)
{
   utime_t t, now = time(NULL);
   QString resultline, stat, LastWritten;
   QStringList fieldlist;

   /* We should now in advance how many rows we will have */
   if (m_chkExpired->isChecked()) {
      for (int i=list.size() -1; i >= 0; i--) {
         fieldlist = list.at(i).split("\t");
         ASSERT(fieldlist.size() != 9);
         LastWritten = fieldlist.at(7);
         if (LastWritten == "") {
            list.removeAt(i);

         } else {
            stat = fieldlist.at(8);
            t = str_to_utime(LastWritten.toAscii().data());
            t = t + stat.toULongLong();
            if (t > now) {
               list.removeAt(i);
            }
         }
      }
   }
}

/*
 * The main meat of the class!!  The function that querries the director and 
 * creates the widgets with appropriate values.
 */
void MediaView::populateTable()
{
   utime_t t;
   time_t ttime;
   QString stat, resultline, query;
   QString str_usage;
   QHash<QString, float> hash_size;
   QStringList fieldlist, results;
   char buf[256];
   float usage;
   struct tm tm;

   m_populated = true;

   Freeze frz(*m_tableMedia); /* disable updating*/
   QStringList where;
   QString cmd;
   if (m_cbPool->currentText() != "") {
      cmd = " Pool.Name = '" + m_cbPool->currentText() + "'";
      where.append(cmd);
   } 

   if (m_cbStatus->currentText() != "") {
      cmd = " Media.VolStatus = '" + m_cbStatus->currentText() + "'";
      where.append(cmd);
   }

   if (m_cbStatus->currentText() != "") {
      cmd = " Media.VolStatus = '" + m_cbStatus->currentText() + "'";
      where.append(cmd);
   }

   if (m_cbMediaType->currentText() != "") {
      cmd = " Media.MediaType = '" + m_cbMediaType->currentText() + "'";
      where.append(cmd);
   }

   if (m_cbLocation->currentText() != "") {
      cmd = " Location.Location = '" + m_cbLocation->currentText() + "'";
      where.append(cmd);
   }

   if (m_textName->text() != "") {
      cmd = " Media.VolumeName like '%" + m_textName->text() + "%'";
      where.append(cmd);
   }

   if (where.size() > 0) {
      cmd = " WHERE " + where.join(" AND ");
   } else {
      cmd = "";
   }

   query =
      "SELECT AVG(VolBytes) AS size, COUNT(1) as nb, "
      "MediaType  FROM Media "
      "WHERE VolStatus IN ('Full', 'Used') "
      "GROUP BY MediaType";

   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "MediaView query cmd : %s\n",query.toUtf8().data());
   }
   if (m_console->sql_cmd(query, results)) {
      foreach (resultline, results) {
         fieldlist = resultline.split("\t");
         if (fieldlist.at(1).toInt() >= 1) { 
            //           MediaType
            hash_size[fieldlist.at(2)] 
               = fieldlist.at(0).toFloat(); 
         }
      }
   }      
   
   m_tableMedia->clearContents();
   query = 
      "SELECT VolumeName, InChanger, "
      "Slot, MediaType, VolStatus, VolBytes, Pool.Name,  "
      "LastWritten, Media.VolRetention "
      "FROM Media JOIN Pool USING (PoolId) "
      "LEFT JOIN Location ON (Media.LocationId=Location.LocationId) "
      + cmd + 
      " ORDER BY VolumeName LIMIT " + m_sbLimit->cleanText();

   m_tableMedia->sortByColumn(0, Qt::AscendingOrder);
   m_tableMedia->setSortingEnabled(false); /* Don't sort during insert */
   results.clear();
   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "MediaView query cmd : %s\n",query.toUtf8().data());
   }
   if (m_console->sql_cmd(query, results)) {
      int row=0;
      filterExipired(results);
      m_tableMedia->setRowCount(results.size());

      foreach (resultline, results) { // should have only one result
         int index = 0;
         QString VolBytes, MediaType, LastWritten, VolStatus;
         fieldlist = resultline.split("\t");
         if (fieldlist.size() != 10) {
            continue;
         }
         QStringListIterator fld(fieldlist);
         TableItemFormatter mediaitem(*m_tableMedia, row);

         /* VolumeName */
         mediaitem.setTextFld(index++, fld.next()); 
         
         /* Online */
         mediaitem.setInChanger(index++, fld.next());

         /* Slot */
         mediaitem.setNumericFld(index++, fld.next());

         MediaType = fld.next();
         VolStatus = fld.next();

         /* Volume bytes */
         VolBytes = fld.next();
         mediaitem.setBytesFld(index++, VolBytes);

         /* Usage */
         usage = 0;
         if (hash_size.contains(MediaType) &&
             hash_size[MediaType] != 0) {
            usage = VolBytes.toLongLong() * 100 / hash_size[MediaType];
         }
         mediaitem.setPercent(index++, usage);

         /* Volstatus */
         mediaitem.setVolStatusFld(index++, VolStatus);

         /* Pool */
         mediaitem.setTextFld(index++, fld.next());

         /* MediaType */
         mediaitem.setTextFld(index++, MediaType);

         LastWritten = fld.next();
         buf[0] = 0;
         if (LastWritten != "") {
            stat = fld.next();  // VolUseDuration
            t = str_to_utime(LastWritten.toAscii().data());
            t = t + stat.toULongLong();
            ttime = t;
            localtime_r(&ttime, &tm);         
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
         }
         
         /* LastWritten */
         mediaitem.setTextFld(index++, LastWritten);

         /* When expired */
         mediaitem.setTextFld(index++, buf);
         row++;
      }
   }
   m_tableMedia->resizeColumnsToContents();
   m_tableMedia->resizeRowsToContents();
   m_tableMedia->verticalHeader()->hide();
   m_tableMedia->setSortingEnabled(true);

   /* make read only */
   m_tableMedia->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

/*
 * When the treeWidgetItem in the page selector tree is singleclicked, Make sure
 * The tree has been populated.
 */
void MediaView::PgSeltreeWidgetClicked()
{
   if (!m_populated) {
      populateForm();
      populateTable();
   }
   if (!isOnceDocked()) {
      dockPage();
   }
}

/*
 * Virtual function which is called when this page is visible on the stack
 */
void MediaView::currentStackItem()
{
   if(!m_populated) {
      populateForm();
      populateTable();
   }
}

// /*
//  * Called from the signal of the context sensitive menu to relabel!
//  */
// void MediaView::relabelVolume()
// {
//    setConsoleCurrent();
//    new relabelDialog(m_console, m_currentVolumeName);
// }
// 
// /*
//  * Called from the signal of the context sensitive menu to purge!
//  */
// void MediaView::allVolumesFromPool()
// {
//    QString cmd = "update volume AllFromPool=" + m_currentVolumeName;
//    consoleCommand(cmd);
//    populateTable();
// }
// 
// void MediaView::allVolumes()
// {
//    QString cmd = "update volume allfrompools";
//    consoleCommand(cmd);
//    populateTable();
// }
// 
//  /*
//   * Called from the signal of the context sensitive menu to purge!
//   */
//  void MediaView::volumeFromPool()
//  {
//     QTreeWidgetItem *currentItem = mp_treeWidget->currentItem();
//     QTreeWidgetItem *parent = currentItem->parent();
//     QString pool = parent->text(0);
//     QString cmd;
//     cmd = "update volume=" + m_currentVolumeName + " frompool=" + pool;
//     consoleCommand(cmd);
//     populateTable();
//  }
//  
