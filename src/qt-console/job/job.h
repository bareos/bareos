#ifndef _JOB_H_
#define _JOB_H_
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.

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
#include "ui_job.h"
#include "console.h"

class Job : public Pages, public Ui::JobForm
{
   Q_OBJECT 

public:
   Job(QString &jobId, QTreeWidgetItem *parentTreeWidgetItem);

public slots:
   void populateAll();
   void deleteJob();
   void cancelJob();
   void showInfoVolume(QListWidgetItem *);
   void rerun();

private slots:

private:
   void updateRunInfo();
   void populateText();
   void populateForm();
   void populateVolumes();
   void getFont();
   QTextCursor *m_cursor;
   QString m_jobId;
   QString m_client;
   QTimer *m_timer;
};

#endif /* _JOB_H_ */
