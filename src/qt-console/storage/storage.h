/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.
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
/*
 * Dirk Bartley, March 2007
 */
#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <QtGui>
#include "ui_storage.h"
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
   void consoleImportStorage();
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
   bool m_needs_repopulate;
   bool m_firstpopulation;
   bool m_checkcurwidget;
   void writeExpandedSettings();
   QTreeWidgetItem *m_topItem;
};

void table_get_selection(QTableWidget *table, QString &sel);

#endif /* _STORAGE_H_ */
