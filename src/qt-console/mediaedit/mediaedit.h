/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
#ifndef _MEDIAEDIT_H_
#define _MEDIAEDIT_H_

#include <QtGui>
#include "ui_mediaedit.h"
#include <pages.h>

class MediaEdit : public Pages, public Ui::mediaEditForm
{
   Q_OBJECT

public:
   MediaEdit(QTreeWidgetItem *parentWidget, QString &mediaId);

private slots:
   void okButtonPushed();
   void cancelButtonPushed();
   void retentionChanged();
   void durationChanged();
   void useDurationChanged();
   void setSpins(int value);
   void retentionRadioPressed();
   void useDurationRadioPressed();

private:
   void connectSpins();
   void disconnectSpins();
   QString m_mediaName;
   QString m_pool;
   QString m_status;
   int m_slot;
   int m_retention;
   int m_useDuration;
   int m_maxVolJobs;
   int m_maxVolFiles;
   int m_maxVolBytes;
   bool m_recycle;
   bool m_enabled;
   QString m_recyclePool;
};

#endif /* _MEDIAEDIT_H_ */
