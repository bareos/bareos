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
#include <QMenu>
#include "content.h"
#include "label/label.h"
#include "mediainfo/mediainfo.h"
#include "mount/mount.h"
#include "util/fmtwidgetitem.h"
#include "status/storstat.h"

// 
// TODO: List tray
//       List drives in autochanger
//       use user selection to add slot= argument
// 

Content::Content(QString storage, QTreeWidgetItem *parentWidget) : Pages()
{
   setupUi(this);
   pgInitialize(storage, parentWidget);
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/package-x-generic.png")));

   m_populated = false;
   m_firstpopulation = true;
   m_checkcurwidget = true;
   m_currentStorage = storage;

   connect(pbUpdate, SIGNAL(clicked()), this,
           SLOT(consoleUpdateSlots()));

   connect(pbLabel, SIGNAL(clicked()), this,
           SLOT(consoleLabelStorage()));

   connect(pbMount, SIGNAL(clicked()), this,
           SLOT(consoleMountStorage()));

   connect(pbUnmount, SIGNAL(clicked()), this,
           SLOT(consoleUnMountStorage()));

   connect(pbStatus, SIGNAL(clicked()), this,
           SLOT(statusStorageWindow()));

   connect(pbRelease, SIGNAL(clicked()), this,
           SLOT(consoleRelease()));

   connect(tableContent, SIGNAL(itemDoubleClicked(QTableWidgetItem*)), this,
           SLOT(showMediaInfo(QTableWidgetItem *)));

   dockPage();
   setCurrent();
}


/*
 * Subroutine to call class to show the log in the database from that job
 */
void Content::showMediaInfo(QTableWidgetItem * item)
{
   QTreeWidgetItem* pageSelectorTreeWidgetItem = mainWin->getFromHash(this);
   int row = item->row();
   QString vol = tableContent->item(row, 1)->text();
   if (vol != "") {
      new MediaInfo(pageSelectorTreeWidgetItem, vol);
   }
}

void table_get_selection(QTableWidget *table, QString &sel)
{
   QTableWidgetItem *item;
   int current;

   /* The QT selection returns each cell, so you
    * have x times the same row number...
    * We take only one instance
    */
   int s = table->rowCount();
   bool *tab = (bool *)malloc(s * sizeof(bool));
   memset(tab, 0, s); 

   foreach (item, table->selectedItems()) {
      current = item->row();
      tab[current]=true;
   }

   sel += "=";

   for(int i=0; i<s; i++) {
      if (tab[i]) {
         sel += table->item(i, 0)->text();
         sel += ",";
      }
   }
   sel.chop(1);                 // remove trailing , or useless =
   free(tab);
}

/* Label Media populating current storage by default */
void Content::consoleLabelStorage()
{
   QString sel;
   table_get_selection(tableContent, sel);
   if (sel == "") {
      new labelPage(m_currentStorage);
   } else {
      QString cmd = "label barcodes slots";
      cmd += sel;
      cmd += " storage=" + m_currentStorage;
      consoleCommand(cmd);
   }
}

/* Mount currently selected storage */
void Content::consoleMountStorage()
{
   setConsoleCurrent();
   /* if this storage is an autochanger, lets ask for the slot */
   new mountDialog(m_console, m_currentStorage);
}

/* Unmount Currently selected storage */
void Content::consoleUnMountStorage()
{
   QString cmd("umount storage=");
   cmd += m_currentStorage;
   consoleCommand(cmd);
}

void Content::statusStorageWindow()
{
   /* if one exists, then just set it current */
   bool found = false;
   foreach(Pages *page, mainWin->m_pagehash) {
      if (mainWin->currentConsole() == page->console()) {
         if (page->name() == tr("Storage Status %1").arg(m_currentStorage)) {
            found = true;
            page->setCurrent();
         }
      }
   }
   if (!found) {
      QTreeWidgetItem *parentItem = mainWin->getFromHash(this);
      new StorStat(m_currentStorage, parentItem);
   }
}

/*
 * The main meat of the class!!  The function that querries the director and 
 * creates the widgets with appropriate values.
 */
void Content::populateContent()
{
   char buf[200];
   time_t tim;
   struct tm tm;

   QStringList results_all;
   QString cmd("status slots drive=0 storage=\"" + m_currentStorage + "\"");
   m_console->dir_cmd(cmd, results_all);

   Freeze frz(*tableContent); /* disable updating*/
   Freeze frz2(*tableTray);
   Freeze frz3(*tableDrive);

   tableContent->clearContents();
   tableTray->clearContents();
   tableTray->clearContents();

   // take only valid records, TODO: Add D to get drive status
   QStringList results = results_all.filter(QRegExp("^[IS]\\|[0-9]+\\|"));
   tableContent->setRowCount(results.size());

   QStringList io_results = results_all.filter(QRegExp("^I\\|[0-9]+\\|"));
   tableTray->setRowCount(io_results.size());

   QString resultline;
   QStringList fieldlist;
   int row = 0, row_io=0;

   foreach (resultline, results) {
      fieldlist = resultline.split("|");
      if (fieldlist.size() < 10) {
         Pmsg1(0, "Discarding %s\n", resultline.data());
         continue; /* some fields missing, ignore row */
      }

      int index=0;
      QStringListIterator fld(fieldlist);
      TableItemFormatter slotitem(*tableContent, row);

      /* Slot type */
      if (fld.next() == "I") {
         TableItemFormatter ioitem(*tableTray, row_io++);
         ioitem.setNumericFld(0, fieldlist[1]);
         ioitem.setTextFld(1, fieldlist[3]);
      }

      /* Slot */
      slotitem.setNumericFld(index++, fld.next()); 

      /* Real Slot */
      if (fld.next() != "") {

         /* Volume */
         slotitem.setTextFld(index++, fld.next());
         
         /* Bytes */
         slotitem.setBytesFld(index++, fld.next());
         
         /* Status */
         slotitem.setVolStatusFld(index++, fld.next());
         
         /* MediaType */
         slotitem.setTextFld(index++, fld.next());
         
         /* Pool */
         slotitem.setTextFld(index++, fld.next());
         
         tim = fld.next().toInt();
         if (tim > 0) {
            /* LastW */
            localtime_r(&tim, &tm);
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
            slotitem.setTextFld(index++, QString(buf));
            
            /* Expire */
            tim = fld.next().toInt();
            localtime_r(&tim, &tm);
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
            slotitem.setTextFld(index++, QString(buf));
         }
      }
      row++;
   }

   tableContent->verticalHeader()->hide();
   tableContent->sortByColumn(0, Qt::AscendingOrder);
   tableContent->setSortingEnabled(true);
   tableContent->resizeColumnsToContents();
   tableContent->resizeRowsToContents();

   tableContent->setEditTriggers(QAbstractItemView::NoEditTriggers);
   m_populated = true;

   tableTray->verticalHeader()->hide();
   tableTray->setEditTriggers(QAbstractItemView::NoEditTriggers);

   tableDrive->verticalHeader()->hide();
   QStringList drives = results_all.filter(QRegExp("^D\\|[0-9]+\\|"));
   tableDrive->setRowCount(drives.size());

   row = 0;
   foreach (resultline, drives) {
      fieldlist = resultline.split("|");
      if (fieldlist.size() < 4) {
         Pmsg1(0, "Discarding %s\n", resultline.data());
         continue; /* some fields missing, ignore row */
      }

      int index=0;
      QStringListIterator fld(fieldlist);
      TableItemFormatter slotitem(*tableDrive, row);

      /* Drive type */
      fld.next();

      /* Number */
      slotitem.setNumericFld(index++, fld.next()); 

      /* Slot */
      fld.next();
      
      /* Volume */
      slotitem.setTextFld(index++, fld.next());
         
      row++;
   }

   tableDrive->resizeRowsToContents();
   tableDrive->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

/*
 * Virtual function which is called when this page is visible on the stack
 */
void Content::currentStackItem()
{
   if(!m_populated) {
      populateContent();
   }
}

void Content::treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)
{

}

/* Update Slots */
void Content::consoleUpdateSlots()
{
   QString sel = "";
   table_get_selection(tableContent, sel);
   
   QString cmd("update slots");
   if (sel != "") {
      cmd += sel;
   }
   cmd += " drive=0 storage=" + m_currentStorage;

   Pmsg1(0, "cmd=%s\n", cmd.toUtf8().data());

   consoleCommand(cmd);
   populateContent();
}

/* Release a tape in the drive */
void Content::consoleRelease()
{
   QString cmd("release storage=");
   cmd += m_currentStorage;
   consoleCommand(cmd);
}
