/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2010 Free Software Foundation Europe e.V.
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
#ifndef _MEDIALIST_H_
#define _MEDIALIST_H_

#include <QtGui>
#include "ui_medialist.h"
#include <qstringlist.h>
#include <pages.h>

class MediaList : public Pages, public Ui::MediaListForm
{
   Q_OBJECT

public:
   MediaList();
   ~MediaList();
   virtual void PgSeltreeWidgetClicked();
   virtual void currentStackItem();

public slots:
   void treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *);

private slots:
   void populateTree();
   void showJobs();
   void viewVolume();
   void editVolume();
   void deleteVolume();
   void purgeVolume();
   void pruneVolume();
   void importVolume();
   void exportVolume();
   void relabelVolume();
   void allVolumesFromPool();
   void allVolumes();
   void volumeFromPool();

private:
   void createContextMenu();
   void writeExpandedSettings();
   QString m_currentVolumeName;
   QString m_currentVolumeId;
   bool m_populated;
   bool m_needs_repopulate;
   bool m_checkcurwidget;
   QTreeWidgetItem *m_topItem;
};

#endif /* _MEDIALIST_H_ */
