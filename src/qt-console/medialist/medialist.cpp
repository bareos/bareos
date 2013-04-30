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
 *  MediaList Class
 *
 *   Dirk Bartley, March 2007
 *
 */ 

#include "bat.h"
#include <QAbstractEventDispatcher>
#include <QMenu>
#include <math.h>
#include "medialist.h"
#include "mediaedit/mediaedit.h"
#include "mediainfo/mediainfo.h"
#include "joblist/joblist.h"
#include "relabel/relabel.h"
#include "run/run.h"
#include "util/fmtwidgetitem.h"

MediaList::MediaList() : Pages()
{
   setupUi(this);
   m_name = tr("Pools");
   pgInitialize();
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/cartridge.png")));

   /* mp_treeWidget, Storage Tree Tree Widget inherited from ui_medialist.h */
   m_populated = false;
   m_checkcurwidget = true;
   m_closeable = false;
   /* add context sensitive menu items specific to this classto the page
    * selector tree. m_contextActions is QList of QActions */
   m_contextActions.append(actionRefreshMediaList);
}

MediaList::~MediaList()
{
   if (m_populated)
      writeExpandedSettings();
}

/*
 * The main meat of the class!!  The function that querries the director and 
 * creates the widgets with appropriate values.
 */
void MediaList::populateTree()
{
   QTreeWidgetItem *pooltreeitem = NULL;

   if (m_populated) {
      writeExpandedSettings();
   }
   m_populated = true;

   Freeze frz(*mp_treeWidget); /* disable updating*/

   QStringList headerlist = (QStringList()
      << tr("Volume Name") << tr("Id") << tr("Status") << tr("Enabled") << tr("Bytes") << tr("Files")
      << tr("Jobs") << tr("Retention") << tr("Media Type") << tr("Slot") << tr("Use Duration")
      << tr("Max Jobs") << tr("Max Files") << tr("Max Bytes") << tr("Recycle")
      << tr("Last Written") << tr("First Written") << tr("Read Time")
      << tr("Write Time") << tr("Recycle Count") << tr("Recycle Pool"));

   m_checkcurwidget = false;
   mp_treeWidget->clear();
   m_checkcurwidget = true;
   mp_treeWidget->setColumnCount(headerlist.count());
   m_topItem = new QTreeWidgetItem(mp_treeWidget);
   m_topItem->setText(0, tr("Pools"));
   m_topItem->setData(0, Qt::UserRole, 0);
   m_topItem->setExpanded(true);
   
   mp_treeWidget->setHeaderLabels(headerlist);

   QSettings settings(m_console->m_dir->name(), "bat");
   settings.beginGroup("MediaListTreeExpanded");
   QString query;


   /* Comma separated list of pools first */
   bool first = true;
   QString pool_comsep("");
   foreach (QString pool_listItem, m_console->pool_list) {
      if (first) {
         pool_comsep += "'" + pool_listItem + "'";
         first = false;
      }
      else
         pool_comsep += ",'" + pool_listItem + "'";
   }
   /* Now use pool_comsep list to perform just one query */
   if (pool_comsep != "") {
      query =  "SELECT Pool.Name AS pul,"
         " Media.VolumeName AS Media, "
         " Media.MediaId AS Id, Media.VolStatus AS VolStatus,"
         " Media.Enabled AS Enabled, Media.VolBytes AS Bytes,"
         " Media.VolFiles AS FileCount, Media.VolJobs AS JobCount,"
         " Media.VolRetention AS VolumeRetention, Media.MediaType AS MediaType,"
         " Media.InChanger AS InChanger, Media.Slot AS Slot, "
         " Media.VolUseDuration AS UseDuration,"
         " Media.MaxVolJobs AS MaxJobs, Media.MaxVolFiles AS MaxFiles,"
         " Media.MaxVolBytes AS MaxBytes, Media.Recycle AS Recycle,"
         " Media.LastWritten AS LastWritten,"
         " Media.FirstWritten AS FirstWritten,"
         " (VolReadTime/1000000) AS ReadTime, (VolWriteTime/1000000) AS WriteTime,"
         " RecycleCount AS ReCyCount,"
         " RecPool.Name AS RecyclePool"
         " FROM Media"
         " JOIN Pool ON (Media.PoolId=Pool.PoolId)"
         " LEFT OUTER JOIN Pool AS RecPool ON (Media.RecyclePoolId=RecPool.PoolId)"
         " WHERE ";
      query += " Pool.Name IN (" + pool_comsep + ")";
      query += " ORDER BY Pool.Name, Media";

      if (mainWin->m_sqlDebug) {
         Pmsg1(000, "MediaList query cmd : %s\n",query.toUtf8().data());
      }
      QStringList results;
      int counter = 0;
      if (m_console->sql_cmd(query, results)) {
         QStringList fieldlist;
         QString prev_pool("");
         QString this_pool("");

         /* Iterate through the lines of results. */
         foreach (QString resultline, results) {
            fieldlist = resultline.split("\t");
            this_pool = fieldlist.takeFirst();
            if (prev_pool != this_pool) {
               prev_pool = this_pool;
               pooltreeitem = new QTreeWidgetItem(m_topItem);
               pooltreeitem->setText(0, this_pool);
               pooltreeitem->setData(0, Qt::UserRole, 1);
            }
            if(settings.contains(this_pool)) {
               pooltreeitem->setExpanded(settings.value(this_pool).toBool());
            } else {
               pooltreeitem->setExpanded(true);
            }

            if (fieldlist.size() < 18)
               continue; // some fields missing, ignore row

            int index = 0;
            TreeItemFormatter mediaitem(*pooltreeitem, 2);
  
            /* Iterate through fields in the record */
            QStringListIterator fld(fieldlist);

            /* volname */
            mediaitem.setTextFld(index++, fld.next()); 

            /* id */
            mediaitem.setNumericFld(index++, fld.next()); 

            /* status */
            mediaitem.setVolStatusFld(index++, fld.next());

            /* enabled */
            mediaitem.setBoolFld(index++, fld.next());

            /* bytes */
            mediaitem.setBytesFld(index++, fld.next());

            /* files */
            mediaitem.setNumericFld(index++, fld.next()); 

            /* jobs */
            mediaitem.setNumericFld(index++, fld.next()); 

            /* retention */
            mediaitem.setDurationFld(index++, fld.next());

            /* media type */
            mediaitem.setTextFld(index++, fld.next()); 

            /* inchanger + slot */
            int inchanger = fld.next().toInt();
            if (inchanger) {
               mediaitem.setNumericFld(index++, fld.next()); 
            } else {
               /* volume not in changer, show blank slot */
               mediaitem.setNumericFld(index++, ""); 
               fld.next();
            }

            /* use duration */
            mediaitem.setDurationFld(index++, fld.next());

            /* max jobs */
            mediaitem.setNumericFld(index++, fld.next()); 

            /* max files */
            mediaitem.setNumericFld(index++, fld.next()); 

            /* max bytes */
            mediaitem.setBytesFld(index++, fld.next());

            /* recycle */
            mediaitem.setBoolFld(index++, fld.next());

            /* last written */
            mediaitem.setTextFld(index++, fld.next()); 

            /* first written */
            mediaitem.setTextFld(index++, fld.next()); 

            /* read time */
            mediaitem.setDurationFld(index++, fld.next());

            /* write time */
            mediaitem.setDurationFld(index++, fld.next());

            /* Recycle Count */
            mediaitem.setNumericFld(index++, fld.next()); 

            /* recycle pool */
            mediaitem.setTextFld(index++, fld.next()); 

         } /* foreach resultline */
         counter += 1;
      } /* if results from query */
   } /* foreach pool_listItem */
   settings.endGroup();
   /* Resize the columns */
   for(int cnter=0; cnter<headerlist.count(); cnter++) {
      mp_treeWidget->resizeColumnToContents(cnter);
   }
}

/*
 * Called from the signal of the context sensitive menu!
 */
void MediaList::editVolume()
{
   MediaEdit* edit = new MediaEdit(mainWin->getFromHash(this), m_currentVolumeId);
   connect(edit, SIGNAL(destroyed()), this, SLOT(populateTree()));
}

/*
 * Called from the signal of the context sensitive menu!
 */
void MediaList::showJobs()
{
   QTreeWidgetItem *parentItem = mainWin->getFromHash(this);
   mainWin->createPageJobList(m_currentVolumeName, "", "", "", parentItem);
}

/*
 * Called from the signal of the context sensitive menu!
 */
void MediaList::viewVolume()
{
   QTreeWidgetItem *parentItem = mainWin->getFromHash(this);
   MediaInfo* view = new MediaInfo(parentItem, m_currentVolumeName);
   connect(view, SIGNAL(destroyed()), this, SLOT(populateTree()));

}

/*
 * When the treeWidgetItem in the page selector tree is singleclicked, Make sure
 * The tree has been populated.
 */
void MediaList::PgSeltreeWidgetClicked()
{
   if (!m_populated) {
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
void MediaList::treeItemChanged(QTreeWidgetItem *currentwidgetitem, QTreeWidgetItem *previouswidgetitem )
{
   /* m_checkcurwidget checks to see if this is during a refresh, which will segfault */
   if (m_checkcurwidget) {
      /* The Previous item */
      if (previouswidgetitem) { /* avoid a segfault if first time */
         foreach(QAction* mediaAction, mp_treeWidget->actions()) {
            mp_treeWidget->removeAction(mediaAction);
         }
      }

      int treedepth = currentwidgetitem->data(0, Qt::UserRole).toInt();
      m_currentVolumeName=currentwidgetitem->text(0);
      mp_treeWidget->addAction(actionRefreshMediaList);
      if (treedepth == 2){
         m_currentVolumeId=currentwidgetitem->text(1);
         mp_treeWidget->addAction(actionEditVolume);
         mp_treeWidget->addAction(actionListJobsOnVolume);
         mp_treeWidget->addAction(actionDeleteVolume);
         mp_treeWidget->addAction(actionPruneVolume);
         mp_treeWidget->addAction(actionPurgeVolume);
         mp_treeWidget->addAction(actionRelabelVolume);
         mp_treeWidget->addAction(actionVolumeFromPool);
      } else if (treedepth == 1) {
         mp_treeWidget->addAction(actionAllVolumesFromPool);
      }
   }
}

/* 
 * Setup a context menu 
 * Made separate from populate so that it would not create context menu over and
 * over as the tree is repopulated.
 */
void MediaList::createContextMenu()
{
   mp_treeWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
   connect(mp_treeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)), this, SLOT(viewVolume()));
   connect(actionEditVolume, SIGNAL(triggered()), this, SLOT(editVolume()));
   connect(actionListJobsOnVolume, SIGNAL(triggered()), this, SLOT(showJobs()));
   connect(actionDeleteVolume, SIGNAL(triggered()), this, SLOT(deleteVolume()));
   connect(actionPurgeVolume, SIGNAL(triggered()), this, SLOT(purgeVolume()));
   connect(actionPruneVolume, SIGNAL(triggered()), this, SLOT(pruneVolume()));
   connect(actionRelabelVolume, SIGNAL(triggered()), this, SLOT(relabelVolume()));
   connect(mp_treeWidget, SIGNAL(
           currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)),
           this, SLOT(treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
   /* connect to the action specific to this pages class */
   connect(actionRefreshMediaList, SIGNAL(triggered()), this,
                SLOT(populateTree()));
   connect(actionAllVolumes, SIGNAL(triggered()), this, SLOT(allVolumes()));
   connect(actionAllVolumesFromPool, SIGNAL(triggered()), this, SLOT(allVolumesFromPool()));
   connect(actionVolumeFromPool, SIGNAL(triggered()), this, SLOT(volumeFromPool()));
}

/*
 * Virtual function which is called when this page is visible on the stack
 */
void MediaList::currentStackItem()
{
   if(!m_populated) {
      populateTree();
      /* Create the context menu for the medialist tree */
      createContextMenu();
   }
}

/*
 * Called from the signal of the context sensitive menu to delete a volume!
 */
void MediaList::deleteVolume()
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
   cmd += m_currentVolumeName;
   consoleCommand(cmd);
}

/*
 * Called from the signal of the context sensitive menu to purge!
 */
void MediaList::purgeVolume()
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
   cmd += m_currentVolumeName;
   consoleCommand(cmd);
   populateTree();
}

/*
 * Called from the signal of the context sensitive menu to prune!
 */
void MediaList::pruneVolume()
{
   new prunePage(m_currentVolumeName, "");
}

/*
 * Called from the signal of the context sensitive menu to relabel!
 */
void MediaList::relabelVolume()
{
   setConsoleCurrent();
   new relabelDialog(m_console, m_currentVolumeName);
}

/*
 * Called from the signal of the context sensitive menu to purge!
 */
void MediaList::allVolumesFromPool()
{
   QString cmd = "update volume AllFromPool=" + m_currentVolumeName;
   consoleCommand(cmd);
   populateTree();
}

void MediaList::allVolumes()
{
   QString cmd = "update volume allfrompools";
   consoleCommand(cmd);
   populateTree();
}

/*
 * Called from the signal of the context sensitive menu to purge!
 */
void MediaList::volumeFromPool()
{
   QTreeWidgetItem *currentItem = mp_treeWidget->currentItem();
   QTreeWidgetItem *parent = currentItem->parent();
   QString pool = parent->text(0);
   QString cmd;
   cmd = "update volume=" + m_currentVolumeName + " frompool=" + pool;
   consoleCommand(cmd);
   populateTree();
}

/*
 * Write settings to save expanded states of the pools
 */
void MediaList::writeExpandedSettings()
{
   if (m_topItem) {
      QSettings settings(m_console->m_dir->name(), "bat");
      settings.beginGroup("MediaListTreeExpanded");
      int childcount = m_topItem->childCount();
      for (int cnt=0; cnt<childcount; cnt++) {
         QTreeWidgetItem *poolitem = m_topItem->child(cnt);
         settings.setValue(poolitem->text(0), poolitem->isExpanded());
      }
      settings.endGroup();
   }
}
