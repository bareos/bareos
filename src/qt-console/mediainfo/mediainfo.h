#ifndef _MEDIAINFO_H_
#define _MEDIAINFO_H_
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
 */

#include <QtGui>
#include "ui_mediainfo.h"
#include "console.h"
#include "pages.h"

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
   void showInfoForJob(QTableWidgetItem * item);

private:
   void populateForm();
   QString m_mediaName;
   QString m_mediaId;
};

#endif /* _MEDIAINFO_H_ */
