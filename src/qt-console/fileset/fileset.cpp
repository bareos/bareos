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
 *  FileSet Class
 *
 *   Dirk Bartley, March 2007
 *
 */ 

#include "bat.h"
#include <QAbstractEventDispatcher>
#include <QMenu>
#include "fileset/fileset.h"
#include "util/fmtwidgetitem.h"

FileSet::FileSet() : Pages()
{
   setupUi(this);
   m_name = tr("FileSets");
   pgInitialize();
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/system-file-manager.png")));

   /* tableWidget, FileSet Tree Tree Widget inherited from ui_fileset.h */
   m_populated = false;
   m_checkcurwidget = true;
   m_closeable = false;
   readSettings();
   /* add context sensitive menu items specific to this classto the page
    * selector tree. m_contextActions is QList of QActions */
   m_contextActions.append(actionRefreshFileSet);
}

FileSet::~FileSet()
{
   writeSettings();
}

/*
 * The main meat of the class!!  The function that querries the director and 
 * creates the widgets with appropriate values.
 */
void FileSet::populateTable()
{

   Freeze frz(*tableWidget); /* disable updating*/

   m_checkcurwidget = false;
   tableWidget->clear();
   m_checkcurwidget = true;

   QStringList headerlist = (QStringList() << tr("FileSet Name") << tr("FileSet Id")
       << tr("Create Time"));

   tableWidget->setColumnCount(headerlist.count());
   tableWidget->setHorizontalHeaderLabels(headerlist);
   tableWidget->horizontalHeader()->setHighlightSections(false);
   tableWidget->verticalHeader()->hide();
   tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
   tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
   tableWidget->setSortingEnabled(false); /* rows move on insert if sorting enabled */

   QString fileset_comsep("");
   bool first = true;
   QStringList notFoundList = m_console->fileset_list;
   foreach(QString filesetName, m_console->fileset_list) {
      if (first) {
         fileset_comsep += "'" + filesetName + "'";
         first = false;
      }
      else
         fileset_comsep += ",'" + filesetName + "'";
   }

   int row = 0;
   tableWidget->setRowCount(m_console->fileset_list.count());
   if (fileset_comsep != "") {
      /* Set up query QString and header QStringList */
      QString query("");
      query += "SELECT FileSet AS Name, FileSetId AS Id, CreateTime"
           " FROM FileSet"
           " WHERE FileSetId IN (SELECT MAX(FileSetId) FROM FileSet WHERE";
      query += " FileSet IN (" + fileset_comsep + ")";
      query += " GROUP BY FileSet) ORDER BY FileSet";

      QStringList results;
      if (mainWin->m_sqlDebug) {
         Pmsg1(000, "FileSet query cmd : %s\n",query.toUtf8().data());
      }
      if (m_console->sql_cmd(query, results)) {
         QStringList fieldlist;

         /* Iterate through the record returned from the query */
         foreach (QString resultline, results) {
            fieldlist = resultline.split("\t");

            /* remove this fileSet from notFoundList */
            int indexOf = notFoundList.indexOf(fieldlist[0]);
            if (indexOf != -1) { notFoundList.removeAt(indexOf); }

            TableItemFormatter item(*tableWidget, row);
  
            /* Iterate through fields in the record */
            QStringListIterator fld(fieldlist);
            int col = 0;

            /* name */
            item.setTextFld(col++, fld.next());

            /* id */
            item.setNumericFld(col++, fld.next());

            /* creation time */
            item.setTextFld(col++, fld.next());

            row++;
         }
      }
   }
   foreach(QString filesetName, notFoundList) {
      TableItemFormatter item(*tableWidget, row);
      item.setTextFld(0, filesetName);
      row++;
   }
   
   /* set default sorting */
   tableWidget->sortByColumn(headerlist.indexOf(tr("FileSet Name")), Qt::AscendingOrder);
   tableWidget->setSortingEnabled(true);
   
   /* Resize rows and columns */
   tableWidget->resizeColumnsToContents();
   tableWidget->resizeRowsToContents();

   /* make read only */
   int rcnt = tableWidget->rowCount();
   int ccnt = tableWidget->columnCount();
   for(int r=0; r < rcnt; r++) {
      for(int c=0; c < ccnt; c++) {
         QTableWidgetItem* item = tableWidget->item(r, c);
         if (item) {
            item->setFlags(Qt::ItemFlags(item->flags() & (~Qt::ItemIsEditable)));
         }
      }
   }
   m_populated = true;
}

/*
 * When the treeWidgetItem in the page selector tree is singleclicked, Make sure
 * The tree has been populated.
 */
void FileSet::PgSeltreeWidgetClicked()
{
   if (!m_populated) {
      populateTable();
      createContextMenu();
   }
   if (!isOnceDocked()) {
      dockPage();
   }
}

/*
 * Added to set the context menu policy based on currently active treeWidgetItem
 * signaled by currentItemChanged
 */
void FileSet::tableItemChanged(QTableWidgetItem *currentwidgetitem, QTableWidgetItem *previouswidgetitem)
{
   /* m_checkcurwidget checks to see if this is during a refresh, which will segfault */
   if (m_checkcurwidget && currentwidgetitem) {
      int currentRow = currentwidgetitem->row();
      QTableWidgetItem *currentrowzeroitem = tableWidget->item(currentRow, 0);
      m_currentlyselected = currentrowzeroitem->text();

      /* The Previous item */
      if (previouswidgetitem) { /* avoid a segfault if first time */
         tableWidget->removeAction(actionStatusFileSetInConsole);
         tableWidget->removeAction(actionShowJobs);
      }

      if (m_currentlyselected.length() != 0) {
         /* set a hold variable to the fileset name in case the context sensitive
          * menu is used */
         tableWidget->addAction(actionStatusFileSetInConsole);
         tableWidget->addAction(actionShowJobs);
      }
   }
}

/* 
 * Setup a context menu 
 * Made separate from populate so that it would not create context menu over and
 * over as the tree is repopulated.
 */
void FileSet::createContextMenu()
{
   tableWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
   tableWidget->addAction(actionRefreshFileSet);
   connect(tableWidget, SIGNAL(
           currentItemChanged(QTableWidgetItem *, QTableWidgetItem *)),
           this, SLOT(tableItemChanged(QTableWidgetItem *, QTableWidgetItem *)));
   /* connect to the action specific to this pages class */
   connect(actionRefreshFileSet, SIGNAL(triggered()), this,
                SLOT(populateTable()));
   connect(actionStatusFileSetInConsole, SIGNAL(triggered()), this,
                SLOT(consoleShowFileSet()));
   connect(actionShowJobs, SIGNAL(triggered()), this,
                SLOT(showJobs()));
}

/*
 * Function responding to actionListJobsofFileSet which calls mainwin function
 * to create a window of a list of jobs of this fileset.
 */
void FileSet::consoleShowFileSet()
{
   QString cmd("show fileset=\"");
   cmd += m_currentlyselected + "\"";
   consoleCommand(cmd);
}

/*
 * Virtual function which is called when this page is visible on the stack
 */
void FileSet::currentStackItem()
{
   if (!m_populated) {
      populateTable();
      /* Create the context menu for the fileset table */
      createContextMenu();
   }
}

/*
 * Save user settings associated with this page
 */
void FileSet::writeSettings()
{
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup("FileSet");
   settings.setValue("geometry", saveGeometry());
   settings.endGroup();
}

/*
 * Read and restore user settings associated with this page
 */
void FileSet::readSettings()
{ 
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup("FileSet");
   restoreGeometry(settings.value("geometry").toByteArray());
   settings.endGroup();
}

/*
 * Create a JobList object pre-populating a fileset
 */
void FileSet::showJobs()
{
   QTreeWidgetItem *parentItem = mainWin->getFromHash(this);
   mainWin->createPageJobList("", "", "", m_currentlyselected, parentItem);
}
