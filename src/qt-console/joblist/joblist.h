#ifndef _JOBLIST_H_
#define _JOBLIST_H_
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
#include "ui_joblist.h"
#include "console.h"
#include "pages.h"

class JobList : public Pages, public Ui::JobListForm
{
   Q_OBJECT 

public:
   JobList(const QString &medianame, const QString &clientname, 
           const QString &jobname, const QString &filesetname, QTreeWidgetItem *);
   ~JobList();
   virtual void PgSeltreeWidgetClicked();
   virtual void currentStackItem();
   int m_resultCount;

public slots:
   void populateTable();
   virtual void treeWidgetName(QString &);
   void selectionChanged();

private slots:
   void consoleListFilesOnJob();
   void consoleListJobMedia();
   void consoleListJobTotals();
   void consoleDeleteJob();
   void consoleRestartJob();
   void consolePurgeFiles();
   void preRestoreFromJob();
   void preRestoreFromTime();
   void showLogForJob();
   void showInfoForJob(QTableWidgetItem * item=NULL);
   void consoleCancelJob();
   void graphTable();
   void splitterMoved(int pos, int index);

private:
   void createConnections();
   void writeSettings();
   void readSettings();
   void prepareFilterWidgets();
   void fillQueryString(QString &query);
   QSplitter *m_splitter;

   QString m_groupText;
   QString m_splitText;
   QString m_mediaName;
   QString m_clientName;
   QString m_jobName;
   QString m_filesetName;
   QString m_currentJob;
   QString m_levelName;

   bool m_populated;
   bool m_checkCurrentWidget;
   int m_jobIdIndex;
   int m_purgedIndex;
   int m_typeIndex;
   int m_levelIndex;
   int m_clientIndex;
   int m_nameIndex;
   int m_filesetIndex;
   int m_statusIndex;
   int m_startIndex;
   int m_bytesIndex;
   int m_filesIndex;
   int m_selectedJobsCount;
   QString m_selectedJobs;
   QStringList m_selectedJobsList;
};

#endif /* _JOBLIST_H_ */
