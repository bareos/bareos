/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.

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
#ifndef _STORSTAT_H_
#define _STORSTAT_H_

#include <QtGui>
#include "ui_storstat.h"
#include "pages.h"

class StorStat : public Pages, public Ui::StorStatForm
{
   Q_OBJECT

public:
   StorStat(QString &, QTreeWidgetItem *);
   ~StorStat();
   virtual void PgSeltreeWidgetClicked();
   virtual void currentStackItem();

public slots:
   void populateHeader();
   void populateTerminated();
   void populateRunning();
   void populateWaitReservation();
   void populateDevices();
   void populateVolumes();
   void populateSpooling();
   void populateAll();

private slots:
   void timerTriggered();
   void populateCurrentTab(int);
   void mountButtonPushed();
   void umountButtonPushed();
   void releaseButtonPushed();
   void labelButtonPushed();

private:
   void createConnections();
   void writeSettings();
   void readSettings();
   bool m_populated;
   QTextCursor *m_cursor;
   void getFont();
   QString m_groupText;
   QString m_splitText;
   QTimer *m_timer;
   QString m_storage;
};

#endif /* _STORSTAT_H_ */
