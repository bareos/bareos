/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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

#include "bat.h"
#include <QAbstractEventDispatcher>
#include <QMenu>
#include "content.h"
#include "label/label.h"
#include "mediainfo/mediainfo.h"
#include "mount/mount.h"
#include "util/fmtwidgetitem.h"
#include "status/storstat.h"

/*
 * Create a new content subpage.
 */
Content::Content(QString storage, QTreeWidgetItem *parentWidget) : Pages()
{
   setupUi(this);
   pgInitialize(storage, parentWidget);
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/package-x-generic.png")));

   m_populated = false;
   m_needs_repopulate = false;
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

   connect(pbImport, SIGNAL(clicked()), this,
           SLOT(consoleImportStorage()));

   connect(pbExport, SIGNAL(clicked()), this,
           SLOT(consoleExportStorage()));

   connect(pbMoveUp, SIGNAL(clicked()), this,
           SLOT(consoleMoveUpStorage()));

   connect(pbMoveDown, SIGNAL(clicked()), this,
           SLOT(consoleMoveDownStorage()));

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

static int table_get_selection(QTableWidget *table, QString &sel)
{
   int i, size, cnt;
   int start_range = -1,
       end_range = -1;
   bool *sel_rows;
   QTableWidgetItem *item;

   /*
    * When there are now rows in this table there is also nothing selected.
    */
   size = table->rowCount();
   if (size == 0) {
      return 0;
   }

   /*
    * The QT selection returns each cell, so you
    * have x times the same row number...
    * We take only one instance
    */
   sel_rows = (bool *)malloc(size * sizeof(bool));
   memset(sel_rows, 0, size * sizeof(bool));

   /*
    * Fill an internal array with per slot a marker if its selected or not.
    */
   foreach (item, table->selectedItems()) {
      sel_rows[item->row()] = true;
   }

   /*
    * A selection starts with a = and then the slot selection.
    */
   sel += "=";

   /*
    * Try to be smart if we can use ranges
    */
   cnt = 0;
   for (i = 0; i < size; i++) {
      if (sel_rows[i]) {
         /*
          * Keep track of the number of elements selected.
          */
         cnt++;

         /*
          * If no range is started, mark this as the start of
          * a new one and continue with the next item.
          */
         if (start_range == -1) {
            start_range = i;
            end_range = i;
            continue;
         } else {
            /*
             * See if this is the next item in the range.
             */
            if (i == end_range + 1) {
               end_range = i;
               continue;
            } else {
               /*
                * Not the next in a range flush the previous range if any
                * or when just a single slot also flush it.
                *
                * See if this is a proper range.
                */
               if (start_range != -1 && start_range != end_range) {
                  sel += table->item(start_range, 0)->text() + "-" + table->item(end_range, 0)->text();
                  sel += ",";
                  start_range = i;
                  end_range = i;
                  continue;
               } else {
                  /*
                   * Only a single slot was selected not a range.
                   */
                  sel += table->item(start_range, 0)->text();
                  sel += ",";
                  start_range = i;
                  end_range = i;
                  continue;
               }

            }
         }
      } else {
         /*
          * Slot is not selected so the range (if started ends here)
          *
          * See if this is a proper range.
          */
         if (start_range != -1 && start_range != end_range) {
            sel += table->item(start_range, 0)->text() + "-" + table->item(end_range, 0)->text();
            sel += ",";
            start_range = -1;
            end_range = -1;
         } else if (start_range != -1) {
            /*
             * Only a single slot was selected not a range.
             */
            sel += table->item(start_range, 0)->text();
            sel += ",";
            start_range = -1;
            end_range = -1;
         }
      }
   }

   /*
    * See if there are any unflushed ranges. e.g. when we run out of slots
    * but the range is not flushed to the cmdline.
    *
    * See if this is a proper range.
    */
   if (start_range != -1 && start_range != end_range) {
      sel += table->item(start_range, 0)->text() + "-" + table->item(end_range, 0)->text();
      sel += ",";
   } else if (start_range != -1) {
      /*
       * Only a single slot was selected not a range.
       */
      sel += table->item(start_range, 0)->text();
      sel += ",";
   }

   /*
    * remove trailing , or useless =
    */
   sel.chop(1);

   free(sel_rows);
   return cnt;
}

/*
 * Label Media populating current storage by default
 */
void Content::consoleLabelStorage()
{
   int drive_cnt, sel_cnt;
   QString sel, drive_sel, cmd;

   sel_cnt = table_get_selection(tableContent, sel);
   if (sel_cnt == 0) {
      new labelPage(m_currentStorage);
   } else {
      /*
       * See if there is more then 1 drive in this changer.
       * If there is the user should select which drive to
       * use for the label by selecting that drive in the
       * drives pane.
       */
      if (tableDrive->rowCount() > 1) {
         drive_cnt = table_get_selection(tableDrive, drive_sel);
         switch (drive_cnt) {
         case 0:
            QMessageBox::warning(this,
                                 tr("Drive Selection"),
                                 tr("Please select drive in drive pane"),
                                 QMessageBox::Ok, QMessageBox::Ok);
            return;
         case 1:
            cmd = QString("label barcodes slots%1 storage=\"%2\" drive%3")
                          .arg(sel)
                          .arg(m_currentStorage)
                          .arg(drive_sel);
            break;
         default:
            QMessageBox::warning(this,
                                 tr("Drive Selection"),
                                 tr("Please select only one drive in drive pane"),
                                 QMessageBox::Ok, QMessageBox::Ok);
            return;
         }
      } else {
         cmd = QString("label barcodes slots%1 storage=\"%2\"")
                       .arg(sel)
                       .arg(m_currentStorage);
      }
   }

   consoleCommand(cmd);

   /*
    * Volumes have moved so (re)populate the view.
    */
   m_needs_repopulate = true;
}

/*
 * Mount currently selected storage
 */
void Content::consoleMountStorage()
{
   int drive_cnt, sel_cnt;
   QString sel, drive_sel, cmd;

   /*
    * See if there is more then 1 drive in this changer.
    * If there is the user should select which drive to
    * use for the mount by selecting that drive in the
    * drives pane. If she/he does not we fall back to
    * the normal behaviour of showing the mountDialog.
    */
   if (tableDrive->rowCount() > 1) {
      drive_cnt = table_get_selection(tableDrive, drive_sel);
      switch (drive_cnt) {
      case 0:
      case 1:
         /*
          * See if One slot is selected in the content table.
          */
         sel_cnt = table_get_selection(tableContent, sel);
         if (sel_cnt == 1) {
            switch (drive_cnt) {
            case 0:
               cmd = QString("mount storage=\"%1\" slot%2 drive=0")
                             .arg(m_currentStorage)
                             .arg(sel);
               consoleCommand(cmd);

               /*
                * Volumes have moved so (re)populate the view.
                */
               m_needs_repopulate = true;
               return;
            case 1:
               cmd = QString("mount storage=\"%1\" slot%2 drive%3")
                             .arg(m_currentStorage)
                             .arg(sel)
                             .arg(drive_sel);
               consoleCommand(cmd);

               /*
                * Volumes have moved so (re)populate the view.
                */
               m_needs_repopulate = true;
               return;
            default:
               break;
            }
         }
         break;
      default:
         break;
      }
   }

   /*
    * Fallback to old interactive mount dialog.
    */
   setConsoleCurrent();

   /*
    * If this storage is an autochanger, lets ask for the slot
    */
   new mountDialog(m_console, m_currentStorage);

   /*
    * Volumes have moved so (re)populate the view.
    */
   m_needs_repopulate = true;
}

/*
 * Unmount Currently selected storage
 */
void Content::consoleUnMountStorage()
{
   int drive_cnt;
   QString drive_sel, cmd;

   /*
    * See if there is more then 1 drive in this changer.
    * If there is the user should select which drive to
    * use for the unmount by selecting that drive in the
    * drives pane.
    */
   if (tableDrive->rowCount() > 1) {
      drive_cnt = table_get_selection(tableDrive, drive_sel);
      switch (drive_cnt) {
      case 0:
         QMessageBox::warning(this,
                              tr("Drive Selection"),
                              tr("Please select drive in drive pane"),
                              QMessageBox::Ok, QMessageBox::Ok);
         return;
      case 1:
         cmd = QString("umount storage=\"%1\" drive%2")
                       .arg(m_currentStorage)
                       .arg(drive_sel);
         break;
      default:
         QMessageBox::warning(this,
                              tr("Drive Selection"),
                              tr("Please select only one drive in drive pane"),
                              QMessageBox::Ok, QMessageBox::Ok);
         return;
      }
   } else {
      cmd = QString("umount storage=\"%1\"")
                    .arg(m_currentStorage);
   }

   consoleCommand(cmd);

   /*
    * Volumes have moved so (re)populate the view.
    */
   m_needs_repopulate = true;
}

/*
 * Import currently selected slots
 */
void Content::consoleImportStorage()
{
   int src_cnt, dst_cnt;
   QString srcslots, dstslots, cmd;

   cmd = QString("import storage=\"%1\"")
                 .arg(m_currentStorage);

   src_cnt = table_get_selection(tableTray, srcslots);
   if (src_cnt > 0) {
      cmd += " srcslots" + srcslots;
   }
   dst_cnt = table_get_selection(tableContent, dstslots);
   if (dst_cnt > 0) {
      cmd += " dstslots" + dstslots;
   }

   Pmsg1(0, "cmd=%s\n", cmd.toUtf8().data());

   consoleCommand(cmd);

   /*
    * Volumes have moved so (re)populate the view.
    */
   m_needs_repopulate = true;
}

/*
 * Export slots currently selected slots
 */
void Content::consoleExportStorage()
{
   int src_cnt, dst_cnt;
   QString srcslots, dstslots, cmd;

   cmd = QString("export storage=\"%1\"")
                 .arg(m_currentStorage);

   src_cnt = table_get_selection(tableContent, srcslots);
   if (src_cnt > 0) {
      cmd += " srcslots" + srcslots;
   }
   dst_cnt = table_get_selection(tableTray, dstslots);
   if (dst_cnt > 0) {
      cmd += " dstslots" + dstslots;
   }

   Pmsg1(0, "cmd=%s\n", cmd.toUtf8().data());

   consoleCommand(cmd);

   /*
    * Volumes have moved so (re)populate the view.
    */
   m_needs_repopulate = true;
}

/*
 * Move currently selected slots for this we need
 * exactly two slot ranges.
 */
void Content::consoleMoveUpStorage()
{
   int sel_cnt;
   QString srcslots, dstslots, selslots, cmd;

   cmd = QString("move storage=\"%1\"")
                 .arg(m_currentStorage);

   /*
    * For a move operation no import/export slots may be selected.
    */
   sel_cnt = table_get_selection(tableTray, selslots);
   if (sel_cnt > 0) {
      QMessageBox::warning(this,
                           tr("Slot Selection"),
                           tr("Please deselect any import/export slots which are invalid for a move operation"),
                           QMessageBox::Ok, QMessageBox::Ok);
      return;
   }

   /*
    * See what slots are selected and try to split it into exactly two ranges.
    */
   sel_cnt = table_get_selection(tableContent, selslots);
   if (sel_cnt > 0) {
      QStringList fieldlist;

      fieldlist = selslots.split(",");
      /*
       * There are not 2 ranges selected this ain't gonna work for a move operation.
       */
      if (fieldlist.size() != 2) {
         QMessageBox::warning(this,
                              tr("Slot Selection"),
                              tr("Please select two slot ranges for the move operation"),
                              QMessageBox::Ok, QMessageBox::Ok);
         return;
      }

      QStringListIterator fld(fieldlist);

      dstslots = fld.next();
      srcslots = fld.next();

      cmd += " srcslots=" + srcslots;
      cmd += " dstslots" + dstslots;
   } else {
      /*
       * Nothing selected this ain't gonna work for a move operation.
       */
      QMessageBox::warning(this,
                           tr("Slot Selection"),
                           tr("Please select two slot ranges for the move operation"),
                           QMessageBox::Ok, QMessageBox::Ok);
      return;
   }

   Pmsg1(0, "cmd=%s\n", cmd.toUtf8().data());

   consoleCommand(cmd);

   /*
    * Volumes have moved so (re)populate the view.
    */
   m_needs_repopulate = true;
}

void Content::consoleMoveDownStorage()
{
   int sel_cnt;
   QString srcslots, dstslots, selslots, cmd;

   cmd = QString("move storage=\"%1\"")
                 .arg(m_currentStorage);

   /*
    * For a move operation no import/export slots may be selected.
    */
   sel_cnt = table_get_selection(tableTray, selslots);
   if (sel_cnt > 0) {
      QMessageBox::warning(this,
                           tr("Slot Selection"),
                           tr("Please deselect any import/export slots which are invalid for a move operation"),
                           QMessageBox::Ok, QMessageBox::Ok);
      return;
   }

   /*
    * See what slots are selected and try to split it into exactly two ranges.
    */
   sel_cnt = table_get_selection(tableContent, selslots);
   if (sel_cnt > 0) {
      QStringList fieldlist;

      fieldlist = selslots.split(",");
      /*
       * There are not 2 ranges selected this ain't gonna work for a move operation.
       */
      if (fieldlist.size() != 2) {
         QMessageBox::warning(this,
                              tr("Slot Selection"),
                              tr("Please select two slot ranges for the move operation"),
                              QMessageBox::Ok, QMessageBox::Ok);
         return;
      }

      QStringListIterator fld(fieldlist);

      srcslots = fld.next();
      dstslots = fld.next();

      cmd += " srcslots" + srcslots;
      cmd += " dstslots=" + dstslots;
   } else {
      /*
       * Nothing selected this ain't gonna work for a move operation.
       */
      QMessageBox::warning(this,
                           tr("Slot Selection"),
                           tr("Please select two slot ranges for the move operation"),
                           QMessageBox::Ok, QMessageBox::Ok);
      return;
   }

   Pmsg1(0, "cmd=%s\n", cmd.toUtf8().data());

   consoleCommand(cmd);

   /*
    * Volumes have moved so (re)populate the view.
    */
   m_needs_repopulate = true;
}

/*
 * Status Slots
 */
void Content::statusStorageWindow()
{
   /*
    * If one exists, then just set it current
    */
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
 * Update Slots
 */
void Content::consoleUpdateSlots()
{
   int sel_cnt;
   QString sel, cmd;

   sel_cnt = table_get_selection(tableContent, sel);
   if (sel_cnt > 0) {
      cmd = QString("update slots%1 storage=\"%2\"")
                    .arg(sel)
                    .arg(m_currentStorage);
   } else {
      cmd = QString("update slots storage=\"%1\"")
                    .arg(m_currentStorage);
   }

   Pmsg1(0, "cmd=%s\n", cmd.toUtf8().data());

   consoleCommand(cmd);

   /*
    * Volumes have moved so (re)populate the view.
    */
   m_needs_repopulate = true;
}

/*
 * Release a tape in the drive
 */
void Content::consoleRelease()
{
   int drive_cnt;
   QString drive_sel, cmd;

   /*
    * See if there is more then 1 drive in this changer.
    * If there is the user should select which drive to
    * use for the release by selecting that drive in the
    * drives pane.
    */
   if (tableDrive->rowCount() > 1) {
      drive_cnt = table_get_selection(tableDrive, drive_sel);
      switch (drive_cnt) {
      case 0:
         QMessageBox::warning(this,
                              tr("Drive Selection"),
                              tr("Please select drive in drive pane"),
                              QMessageBox::Ok, QMessageBox::Ok);
         return;
      case 1:
         cmd = QString("release storage=\"%1\" drive%2")
                       .arg(m_currentStorage)
                       .arg(drive_sel);
         break;
      default:
         QMessageBox::warning(this,
                              tr("Drive Selection"),
                              tr("Please select only one drive in drive pane"),
                              QMessageBox::Ok, QMessageBox::Ok);
         return;
      }
   } else {
      cmd = QString("release storage=\"%1\"")
                    .arg(m_currentStorage);
   }

   consoleCommand(cmd);

   /*
    * Volumes have moved so (re)populate the view.
    */
   m_needs_repopulate = true;
}

/*
 * The main meat of the class!!  The function that querries the director and
 * creates the widgets with appropriate values.
 */
void Content::populateContent()
{
   char buf[30];
   time_t tim;

   QStringList results_all;
   QString cmd;

   cmd = QString("status slots storage=\"%1\"")
                 .arg(m_currentStorage);
   m_console->dir_cmd(cmd, results_all);

   Freeze frz(*tableContent); /* disable updating */
   Freeze frz2(*tableTray);
   Freeze frz3(*tableDrive);

   /*
    * Populate Content pane.
    */
   tableContent->clearContents();

   QStringList results = results_all.filter(QRegExp("^S\\|[0-9]+\\|"));
   tableContent->setRowCount(results.size());

   QString resultline;
   QStringList fieldlist;
   int row = 0;

   foreach (resultline, results) {
      fieldlist = resultline.split("|");
      if (fieldlist.size() < 10) {
         Pmsg1(0, "Discarding %s\n", resultline.data());
         continue; /* some fields missing, ignore row */
      }

      int index = 0;
      QStringListIterator fld(fieldlist);
      TableItemFormatter slotitem(*tableContent, row);

       /* Slot type. */
      fld.next();

      /* Slot */
      slotitem.setNumericFld(index++, fld.next());

      /* Filled Slot */
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
            bstrutime(buf, sizeof(buf), tim);
            slotitem.setTextFld(index++, QString(buf));

            /* Expire */
            tim = fld.next().toInt();
            bstrutime(buf, sizeof(buf), tim);
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

   /*
    * Populate Input/Ouput pane.
    */
   tableTray->clearContents();

   QStringList io_results = results_all.filter(QRegExp("^I\\|[0-9]+\\|"));
   tableTray->setRowCount(io_results.size());

   int row_io = 0;
   foreach (resultline, io_results) {
      fieldlist = resultline.split("|");
      if (fieldlist.size() < 10) {
         Pmsg1(0, "Discarding %s\n", resultline.data());
         continue; /* some fields missing, ignore row */
      }

      QStringListIterator fld(fieldlist);

      /* Slot type. */
      fld.next();

      TableItemFormatter ioitem(*tableTray, row_io++);
      ioitem.setNumericFld(0, fieldlist[1]);
      ioitem.setTextFld(1, fieldlist[3]);
   }

   tableTray->verticalHeader()->hide();
   tableTray->setEditTriggers(QAbstractItemView::NoEditTriggers);

   /*
    * Populate Drive pane.
    */
   tableDrive->verticalHeader()->hide();

   tableDrive->clearContents();

   QStringList drives = results_all.filter(QRegExp("^D\\|[0-9]+\\|"));
   tableDrive->setRowCount(drives.size());

   int row_drive = 0;
   foreach (resultline, drives) {
      fieldlist = resultline.split("|");
      if (fieldlist.size() < 4)
         continue; /* some fields missing, ignore row */

      int index = 0;
      QStringListIterator fld(fieldlist);
      TableItemFormatter slotitem(*tableDrive, row_drive);

      /* Drive type */
      fld.next();

      /* Number */
      slotitem.setNumericFld(index++, fld.next());

      /* Slot */
      fld.next();

      /* Volume */
      slotitem.setTextFld(index++, fld.next());

      row_drive++;
   }

   tableDrive->resizeRowsToContents();
   tableDrive->setEditTriggers(QAbstractItemView::NoEditTriggers);

   m_populated = true;
}

/*
 * Virtual function which is called when this page is visible on the stack
 */
void Content::currentStackItem()
{
   if (!m_populated || m_needs_repopulate) {
      populateContent();
   }
}

void Content::treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)
{
}
