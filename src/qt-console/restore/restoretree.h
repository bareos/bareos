#ifndef _RESTORETREE_H_
#define _RESTORETREE_H_

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
 *  Kern Sibbald, February 2007
 */

#include <QtGui>
#include "pages.h"
#include "ui_restoretree.h"


/*  
 * A restore tree to view files in the catalog
 */
class restoreTree : public Pages, public Ui::restoreTreeForm
{
   Q_OBJECT 

public:
   restoreTree();
   ~restoreTree();
   virtual void PgSeltreeWidgetClicked();
   virtual void currentStackItem();
   enum folderCheckState
   {
      FolderUnchecked = 0,
      FolderGreenChecked = 1,
      FolderWhiteChecked = 2,
      FolderBothChecked = 3
   };

private slots:
   void refreshButtonPushed();
   void restoreButtonPushed();
   void jobComboChanged(int);
   void directoryCurrentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *);
   void fileCurrentItemChanged(QTableWidgetItem *,QTableWidgetItem *);
   void directoryItemExpanded(QTreeWidgetItem *);
   void setCheckofChildren(QTreeWidgetItem *item, Qt::CheckState);
   void directoryItemChanged(QTreeWidgetItem *, int);
   void fileTableItemChanged(QTableWidgetItem *);
   void versionTableItemChanged(QTableWidgetItem *);
   void updateRefresh();
   void jobTableCellClicked(int, int);

private:
   void populateDirectoryTree();
   void populateJobTable();
   void parseDirectory(const QString &dir_in);
   void setupPage();
   void writeSettings();
   void readSettings();
   void fileExceptionInsert(QString &, QString &, Qt::CheckState);
   void fileExceptionRemove(QString &, QString &);
   void directoryTreeDisconnectedSet(QTreeWidgetItem *, Qt::CheckState);
   void fileTableDisconnectedSet(QTableWidgetItem *, Qt::CheckState, bool color);
   void versionTableDisconnectedSet(QTableWidgetItem *, Qt::CheckState);
   void updateFileTableChecks();
   void updateVersionTableChecks();
   void directoryIconStateInsert(QString &, Qt::CheckState);
   void directoryIconStateRemove();
   void directorySetIcon(int operation, int change, QString &, QTreeWidgetItem* item);
   void fullPathtoSubPaths(QStringList &, QString &);
   int mostRecentVersionfromFullPath(QString &);
   void setJobsCheckedList();
   int queryFileIndex(QString &fullPath, int jobID);

   QSplitter *m_splitter;
   QString m_groupText;
   QString m_splitText1;
   QString m_splitText2;
   bool m_populated;
   bool m_dropdownChanged;
   bool m_slashTrap;
   QHash<QString, QTreeWidgetItem *> m_dirPaths;
   QString m_checkedJobs, m_prevJobCombo, m_prevClientCombo, m_prevFileSetCombo;
   int m_prevLimitSpinBox, m_prevDaysSpinBox;
   Qt::CheckState m_prevLimitCheckState, m_prevDaysCheckState;
   QString m_JobsCheckedList;
   int m_debugCnt;
   bool m_debugTrap;
   QList<Qt::CheckState> m_fileCheckStateList;
   QList<Qt::CheckState> m_versionCheckStateList;
   QHash<QString, Qt::CheckState> m_fileExceptionHash;
   QMultiHash<QString, QString> m_fileExceptionMulti;
   QHash<QString, int> m_versionExceptionHash;
   QHash<QString, int> m_directoryIconStateHash;
   QHash<QString, int> m_directoryPathIdHash;
   int m_toggleUpIndex, m_toggleDownIndex, m_nullFileNameId;
};

#endif /* _RESTORETREE_H_ */
