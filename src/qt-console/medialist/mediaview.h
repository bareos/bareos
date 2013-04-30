#ifndef _MEDIAVIEW_H_
#define _MEDIAVIEW_H_
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

#include <QtGui>
#include "ui_mediaview.h"
#include "console.h"
#include <qstringlist.h>

class MediaView : public Pages, public Ui::MediaViewForm
{
   Q_OBJECT 

public:
   MediaView();
   ~MediaView();

private slots:
   void populateTable();
   void populateForm();
   void PgSeltreeWidgetClicked();
   void currentStackItem();
   void applyPushed();
   void editPushed();
   void purgePushed();
   void prunePushed();
   void deletePushed();
   bool getSelection(QStringList &ret);
   void showInfoForMedia(QTableWidgetItem * item);
   void filterExipired(QStringList &list);
//   void relabelVolume();
//   void allVolumesFromPool();
//   void allVolumes();
//   void volumeFromPool();

private:
   bool m_populated;
   bool m_checkcurwidget;
   QTreeWidgetItem *m_topItem;
};

#endif /* _MEDIAVIEW_H_ */
