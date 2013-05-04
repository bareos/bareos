/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.

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
#ifndef _JOBLOG_H_
#define _JOBLOG_H_

#include <QtGui>
#include "ui_joblog.h"
#include <pages.h>

class JobLog : public Pages, public Ui::JobLogForm
{
   Q_OBJECT

public:
   JobLog(QString &jobId, QTreeWidgetItem *parentTreeWidgetItem);

public slots:

private slots:

private:
   void populateText();
   void getFont();
   QTextCursor *m_cursor;
   QString m_jobId;
};

#endif /* _JOBLOG_H_ */
