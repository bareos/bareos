#ifndef _RESTORE_H_
#define _RESTORE_H_

/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2010 Free Software Foundation Europe e.V.

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
 *
 *  Kern Sibbald, February 2007
 */

#include <sys/types.h>

#include <QtGui>
#include "pages.h"
#include "ui_runrestore.h"

class bRestoreTable : public QTableWidget
{
   Q_OBJECT 
private:
   QPoint dragStartPosition;
public:
   bRestoreTable(QWidget *parent)
      : QTableWidget(parent)
   {
   }
   void mousePressEvent(QMouseEvent *event);
   void mouseMoveEvent(QMouseEvent *event);

   void dragEnterEvent(QDragEnterEvent *event);
   void dragMoveEvent(QDragMoveEvent *event);
   void dropEvent(QDropEvent *event);
};

#include "ui_brestore.h"
#include "ui_restore.h"
#include "ui_prerestore.h"

enum {
   R_NONE,
   R_JOBIDLIST,
   R_JOBDATETIME
};

/*
 * The pre-restore dialog selects the Job/Client to be restored
 * It really could use considerable enhancement.
 */
class prerestorePage : public Pages, public Ui::prerestoreForm
{
   Q_OBJECT 

public:
   prerestorePage();
   prerestorePage(QString &data, unsigned int);

private slots:
   void okButtonPushed();
   void cancelButtonPushed();
   void job_name_change(int index);
   void recentChanged(int);
   void jobRadioClicked(bool);
   void jobidsRadioClicked(bool);
   void jobIdEditFinished();

private:
   int m_conn;
   int jobdefsFromJob(QStringList &, QString &);
   void buildPage();
   bool checkJobIdList();
   QString m_dataIn;
   unsigned int m_dataInType;
};

/*  
 * The restore dialog is brought up once we are in the Bacula
 * restore tree routines.  It handles putting up a GUI tree
 * representation of the files to be restored.
 */
class restorePage : public Pages, public Ui::restoreForm
{
   Q_OBJECT 

public:
   restorePage(int conn);
   ~restorePage();
   void fillDirectory();
   char *get_cwd();
   bool cwd(const char *);

private slots:
   void okButtonPushed();
   void cancelButtonPushed();
   void fileDoubleClicked(QTreeWidgetItem *item, int column);
   void directoryItemChanged(QTreeWidgetItem *, QTreeWidgetItem *);
   void upButtonPushed();
   void unmarkButtonPushed();
   void markButtonPushed();
   void addDirectory(QString &);

private:
   int m_conn;
   void writeSettings();
   void readSettings();
   QString m_cwd;
   QHash<QString, QTreeWidgetItem *> m_dirPaths;
   QHash<QTreeWidgetItem *,QString> m_dirTreeItems;
   QRegExp m_rx;
   QString m_splitText;
};

class bRestore : public Pages, public Ui::bRestoreForm
{
   Q_OBJECT 

public:
   bRestore();
   ~bRestore();
   void PgSeltreeWidgetClicked();
   QString m_client;
   QString m_jobids;
   void get_info_from_selection(QStringList &fileids, QStringList &jobids,
                                QStringList &dirids, QStringList &fileindexes);

public slots:
   void setClient();
   void setJob();
   void showInfoForFile(QTableWidgetItem *);
   void applyLocation();
   void clearVersions(QTableWidgetItem *);
   void clearRestoreList();
   void runRestore();
   void refreshView();
private:
   QString m_path;
   int64_t m_pathid;
   QTableWidgetItem *m_current;
   void setupPage();
   bool m_populated;
   void displayFiles(int64_t pathid, QString path);
   void displayFileVersion(QString pathid, QString fnid, 
                           QString client, QString filename);
};

class bRunRestore : public QDialog, public Ui::bRunRestoreForm
{
   Q_OBJECT 
private:
   bRestore *brestore;
   QStringList m_fileids, m_jobids, m_dirids, m_findexes;

public:
   bRunRestore(bRestore *parent);
   ~bRunRestore() {}
   void computeVolumeList();
   int64_t runRestore(QString tablename);

public slots:
   void useRegexp();
   void UFRcb();
   void computeRestore();
};

#endif /* _RESTORE_H_ */
