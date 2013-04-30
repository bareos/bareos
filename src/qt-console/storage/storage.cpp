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
 *  Storage Class
 *
 *   Dirk Bartley, March 2007
 *
 */ 

#include "bat.h"
#include <QAbstractEventDispatcher>
#include <QMenu>
#include "storage.h"
#include "content.h"
#include "label/label.h"
#include "mount/mount.h"
#include "status/storstat.h"
#include "util/fmtwidgetitem.h"

Storage::Storage() : Pages()
{
   setupUi(this);
   pgInitialize(tr("Storage"));
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/package-x-generic.png")));

   /* mp_treeWidget, Storage Tree Tree Widget inherited from ui_storage.h */
   m_populated = false;
   m_firstpopulation = true;
   m_checkcurwidget = true;
   m_closeable = false;
   m_currentStorage = "";
   /* add context sensitive menu items specific to this classto the page
    * selector tree. m_contextActions is QList of QActions */
   m_contextActions.append(actionRefreshStorage);
}

Storage::~Storage()
{
   if (m_populated)
      writeExpandedSettings();
}

/*
 * The main meat of the class!!  The function that querries the director and 
 * creates the widgets with appropriate values.
 */
void Storage::populateTree()
{
   if (m_populated)
      writeExpandedSettings();
   m_populated = true;

   Freeze frz(*mp_treeWidget); /* disable updating */

   m_checkcurwidget = false;
   mp_treeWidget->clear();
   m_checkcurwidget = true;

   QStringList headerlist = (QStringList() << tr("Name") << tr("Id")
        << tr("Changer") << tr("Slot") << tr("Status") << tr("Enabled") << tr("Pool") 
        << tr("Media Type") );

   m_topItem = new QTreeWidgetItem(mp_treeWidget);
   m_topItem->setText(0, tr("Storage"));
   m_topItem->setData(0, Qt::UserRole, 0);
   m_topItem->setExpanded(true);

   mp_treeWidget->setColumnCount(headerlist.count());
   mp_treeWidget->setHeaderLabels(headerlist);

   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup("StorageTreeExpanded");

   bool first = true;
   QString storage_comsep("");
   QString storageName;
   foreach(storageName, m_console->storage_list){
      if (first) {
         storage_comsep += "'" + storageName + "'";
         first = false;
      }
      else
         storage_comsep += ",'" + storageName + "'";
   }
   if (storage_comsep != "") {

      /* Set up query QString and header QStringList */
      QString query("SELECT"
               " Name AS StorageName,"
               " StorageId AS ID, AutoChanger AS Changer"
               " FROM Storage "
               " WHERE StorageId IN (SELECT MAX(StorageId) FROM Storage WHERE");
      query += " Name IN (" + storage_comsep + ")";
      query += " GROUP BY Name) ORDER BY Name";

      QStringList results;
      /* This could be a log item */
      if (mainWin->m_sqlDebug) {
         Pmsg1(000, "Storage query cmd : %s\n",query.toUtf8().data());
      }
      if (m_console->sql_cmd(query, results)) {

         QStringList fieldlist;
         foreach (QString resultline, results) {
            fieldlist = resultline.split("\t");
            storageName = fieldlist.takeFirst();
            if (m_firstpopulation) {
               settingsOpenStatus(storageName);
            }
            TreeItemFormatter storageItem(*m_topItem, 1);
            storageItem.setTextFld(0, storageName);
            if(settings.contains(storageName))
               storageItem.widget()->setExpanded(settings.value(storageName).toBool());
            else
               storageItem.widget()->setExpanded(true);

            int index = 1;
            QStringListIterator fld(fieldlist);
 
            /* storage id */
            storageItem.setNumericFld(index++, fld.next());

            /* changer */
            QString changer = fld.next();
            storageItem.setBoolFld(index++, changer);

            if (changer == "1")
               mediaList(storageItem.widget(), fieldlist.first());
         }
      }
   }
   /* Resize the columns */
   for(int cnter=0; cnter<headerlist.size(); cnter++) {
      mp_treeWidget->resizeColumnToContents(cnter);
   }
   m_firstpopulation = false;
}

/*
 * For autochangers    A query to show the tapes in the changer.
 */
void Storage::mediaList(QTreeWidgetItem *parent, const QString &storageID)
{
   QString query("SELECT Media.VolumeName AS Media, Media.Slot AS Slot,"
                 " Media.VolStatus AS VolStatus, Media.Enabled AS Enabled,"
                 " Pool.Name AS MediaPool, Media.MediaType AS MediaType" 
                 " From Media"
                 " JOIN Pool ON (Media.PoolId=Pool.PoolId)"
                 " WHERE Media.StorageId='" + storageID + "'"
                 " AND Media.InChanger<>0"
                 " ORDER BY Media.Slot");

   QStringList results;
   /* This could be a log item */
   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "Storage query cmd : %s\n",query.toUtf8().data());
   }
   if (m_console->sql_cmd(query, results)) {
      QString resultline;
      QString field;
      QStringList fieldlist;
 
      foreach (resultline, results) {
         fieldlist = resultline.split("\t");
         if (fieldlist.size() < 6)
             continue; 

         /* Iterate through fields in the record */
         QStringListIterator fld(fieldlist);
         int index = 0;
         TreeItemFormatter fmt(*parent, 2);

         /* volname */
         fmt.setTextFld(index++, fld.next()); 
 
         /* skip the next two columns, unused by media */
         index += 2;

         /* slot */
         fmt.setNumericFld(index++, fld.next());

         /* status */
         fmt.setVolStatusFld(index++, fld.next());

         /* enabled */
         fmt.setBoolFld(index++, fld.next()); 

         /* pool */
         fmt.setTextFld(index++, fld.next()); 

         /* media type */
         fmt.setTextFld(index++, fld.next()); 

      }
   }
}

/*
 * When the treeWidgetItem in the page selector tree is singleclicked, Make sure
 * The tree has been populated.
 */
void Storage::PgSeltreeWidgetClicked()
{
   if(!m_populated) {
      populateTree();
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
void Storage::treeItemChanged(QTreeWidgetItem *currentwidgetitem, QTreeWidgetItem *previouswidgetitem )
{
   /* m_checkcurwidget checks to see if this is during a refresh, which will segfault */
   if (m_checkcurwidget) {
      /* The Previous item */
      if (previouswidgetitem) { /* avoid a segfault if first time */
         int treedepth = previouswidgetitem->data(0, Qt::UserRole).toInt();
         if (treedepth == 1){
            mp_treeWidget->removeAction(actionStatusStorageInConsole);
            mp_treeWidget->removeAction(actionStatusStorageWindow);
            mp_treeWidget->removeAction(actionLabelStorage);
            mp_treeWidget->removeAction(actionMountStorage);
            mp_treeWidget->removeAction(actionUnMountStorage);
            mp_treeWidget->removeAction(actionUpdateSlots);
            mp_treeWidget->removeAction(actionUpdateSlotsScan);
            mp_treeWidget->removeAction(actionRelease);
         }
      }

      int treedepth = currentwidgetitem->data(0, Qt::UserRole).toInt();
      if (treedepth == 1){
         /* set a hold variable to the storage name in case the context sensitive
          * menu is used */
         m_currentStorage = currentwidgetitem->text(0);
         m_currentAutoChanger = currentwidgetitem->text(2) == tr("Yes");
         mp_treeWidget->addAction(actionStatusStorageInConsole);
         mp_treeWidget->addAction(actionStatusStorageWindow);
         mp_treeWidget->addAction(actionLabelStorage);
         mp_treeWidget->addAction(actionMountStorage);
         mp_treeWidget->addAction(actionUnMountStorage);
         mp_treeWidget->addAction(actionRelease);
         QString text;
         text = tr("Status Storage \"%1\"").arg(m_currentStorage);;
         actionStatusStorageInConsole->setText(text);
         text = tr("Status Storage \"%1\" in Window").arg(m_currentStorage);;
         actionStatusStorageWindow->setText(text);
         text = tr("Label media in Storage \"%1\"").arg(m_currentStorage);
         actionLabelStorage->setText(text);
         text = tr("Mount media in Storage \"%1\"").arg(m_currentStorage);
         actionMountStorage->setText(text);
         text = tr("\"UN\" Mount media in Storage \"%1\"").arg(m_currentStorage);
         text = tr("Release media in Storage \"%1\"").arg(m_currentStorage);
         actionRelease->setText(text);
         if (m_currentAutoChanger) {
            mp_treeWidget->addAction(actionUpdateSlots);
            mp_treeWidget->addAction(actionUpdateSlotsScan);
            text = tr("Barcode Scan media in Storage \"%1\"").arg(m_currentStorage);
            actionUpdateSlots->setText(text);
            text = tr("Read scan media in Storage \"%1\"").arg( m_currentStorage);
            actionUpdateSlotsScan->setText(text);
         }
      }
   }
}

/* 
 * Setup a context menu 
 * Made separate from populate so that it would not create context menu over and
 * over as the tree is repopulated.
 */
void Storage::createContextMenu()
{
   mp_treeWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
   mp_treeWidget->addAction(actionRefreshStorage);
   connect(mp_treeWidget, SIGNAL(
           currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
           this, SLOT(treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
   /* connect to the action specific to this pages class */
   connect(actionRefreshStorage, SIGNAL(triggered()), this,
                SLOT(populateTree()));
   connect(actionStatusStorageInConsole, SIGNAL(triggered()), this,
                SLOT(consoleStatusStorage()));
   connect(actionLabelStorage, SIGNAL(triggered()), this,
                SLOT(consoleLabelStorage()));
   connect(actionMountStorage, SIGNAL(triggered()), this,
                SLOT(consoleMountStorage()));
   connect(actionUnMountStorage, SIGNAL(triggered()), this,
                SLOT(consoleUnMountStorage()));
   connect(actionUpdateSlots, SIGNAL(triggered()), this,
                SLOT(consoleUpdateSlots()));
   connect(actionUpdateSlotsScan, SIGNAL(triggered()), this,
                SLOT(consoleUpdateSlotsScan()));
   connect(actionRelease, SIGNAL(triggered()), this,
                SLOT(consoleRelease()));
   connect(actionStatusStorageWindow, SIGNAL(triggered()), this,
                SLOT(statusStorageWindow()));
   connect(mp_treeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
           this, SLOT(contentWindow()));

}

void Storage::contentWindow()
{
   if (m_currentStorage != "" && m_currentAutoChanger) { 
      QTreeWidgetItem *parentItem = mainWin->getFromHash(this);
      new Content(m_currentStorage, parentItem);
   }
}

/*
 * Virtual function which is called when this page is visible on the stack
 */
void Storage::currentStackItem()
{
   if(!m_populated) {
      populateTree();
      /* Create the context menu for the storage tree */
      createContextMenu();
   }
}

/*
 *  Functions to respond to local context sensitive menu sending console commands
 *  If I could figure out how to make these one function passing a string, Yaaaaaa
 */
void Storage::consoleStatusStorage()
{
   QString cmd("status storage=");
   cmd += m_currentStorage;
   consoleCommand(cmd);
}

/* Label Media populating current storage by default */
void Storage::consoleLabelStorage()
{
   new labelPage(m_currentStorage);
}

/* Mount currently selected storage */
void Storage::consoleMountStorage()
{
   if (m_currentAutoChanger == 0){
      /* no autochanger, just execute the command in the console */
      QString cmd("mount storage=");
      cmd += m_currentStorage;
      consoleCommand(cmd);
   } else {
      setConsoleCurrent();
      /* if this storage is an autochanger, lets ask for the slot */
      new mountDialog(m_console, m_currentStorage);
   }
}

/* Unmount Currently selected storage */
void Storage::consoleUnMountStorage()
{
   QString cmd("umount storage=");
   cmd += m_currentStorage;
   consoleCommand(cmd);
}

/* Update Slots */
void Storage::consoleUpdateSlots()
{
   QString cmd("update slots storage=");
   cmd += m_currentStorage;
   consoleCommand(cmd);
}

/* Update Slots Scan*/
void Storage::consoleUpdateSlotsScan()
{
   QString cmd("update slots scan storage=");
   cmd += m_currentStorage;
   consoleCommand(cmd);
}

/* Release a tape in the drive */
void Storage::consoleRelease()
{
   QString cmd("release storage=");
   cmd += m_currentStorage;
   consoleCommand(cmd);
}

/*
 *  Open a status storage window
 */
void Storage::statusStorageWindow()
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
 * Write settings to save expanded states of the pools
 */
void Storage::writeExpandedSettings()
{
   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup("StorageTreeExpanded");
   int childcount = m_topItem->childCount();
   for (int cnt=0; cnt<childcount; cnt++) {
      QTreeWidgetItem *item = m_topItem->child(cnt);
      settings.setValue(item->text(0), item->isExpanded());
   }
   settings.endGroup();
}

/*
 * If first time, then check to see if there were status pages open the last time closed
 * if so open
 */
void Storage::settingsOpenStatus(QString &storage)
{
   QSettings settings(m_console->m_dir->name(), "bat");

   settings.beginGroup("OpenOnExit");
   QString toRead = "StorageStatus_" + storage;
   if (settings.value(toRead) == 1) {
      if (mainWin->m_sqlDebug) {
         Pmsg1(000, "Do open Storage Status window for : %s\n", storage.toUtf8().data());
      }
      new StorStat(storage, mainWin->getFromHash(this));
      setCurrent();
      mainWin->getFromHash(this)->setExpanded(true);
   } else {
      if (mainWin->m_sqlDebug) {
         Pmsg1(000, "Do NOT open Storage Status window for : %s\n", storage.toUtf8().data());
      }
   }
   settings.endGroup();
}
