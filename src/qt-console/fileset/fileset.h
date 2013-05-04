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
/*
 * Dirk Bartley, March 2007
 */
#ifndef _FILESET_H_
#define _FILESET_H_

#include <QtGui>
#include "ui_fileset.h"
#include "pages.h"

class FileSet : public Pages, public Ui::FileSetForm
{
   Q_OBJECT

public:
   FileSet();
   ~FileSet();
   virtual void PgSeltreeWidgetClicked();
   virtual void currentStackItem();

public slots:
   void tableItemChanged(QTableWidgetItem *, QTableWidgetItem *);

private slots:
   void populateTable();
   void consoleShowFileSet();
   void showJobs();

private:
   void writeSettings();
   void readSettings();
   void createContextMenu();
   QString m_currentlyselected;
   bool m_populated;
   bool m_checkcurwidget;
};

#endif /* _FILESET_H_ */
