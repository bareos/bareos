#ifndef _STORAGE_H_
#define _STORAGE_H_
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.

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

#include <QtGui>
#include "ui_storage.h"
#include "console.h"
#include "pages.h"

class Storage : public Pages, public Ui::StorageForm
{
   Q_OBJECT 

public:
   Storage();
   ~Storage();
   virtual void PgSeltreeWidgetClicked();
   virtual void currentStackItem();

public slots:
   void treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *);

private slots:
   void populateTree();
   void consoleStatusStorage();
   void consoleLabelStorage();
   void consoleMountStorage();
   void consoleUnMountStorage();
   void consoleUpdateSlots();
   void consoleUpdateSlotsScan();
   void consoleRelease();
   void statusStorageWindow();
   void contentWindow();

private:
   void createContextMenu();
   void mediaList(QTreeWidgetItem *parent, const QString &storageID);
   void settingsOpenStatus(QString& storage);
   QString m_currentStorage;
   bool m_currentAutoChanger;
   bool m_populated;
   bool m_firstpopulation;
   bool m_checkcurwidget;
   void writeExpandedSettings();
   QTreeWidgetItem *m_topItem;
};

void table_get_selection(QTableWidget *table, QString &sel);

#endif /* _STORAGE_H_ */
