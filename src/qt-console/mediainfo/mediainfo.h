/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2009 Free Software Foundation Europe e.V.

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
#ifndef _MEDIAINFO_H_
#define _MEDIAINFO_H_

#include <QtGui>
#include "ui_mediainfo.h"
#include <pages.h>

class MediaInfo : public Pages, public Ui::mediaInfoForm
{
   Q_OBJECT

public:
   MediaInfo(QTreeWidgetItem *parentWidget, QString &mediaId);

private slots:
   void pruneVol();
   void purgeVol();
   void deleteVol();
   void editVol();
   void importVol();
   void exportVol();
   void showInfoForJob(QTableWidgetItem * item);

private:
   void populateForm();
   QString m_mediaName;
   QString m_mediaId;
};

#endif /* _MEDIAINFO_H_ */
