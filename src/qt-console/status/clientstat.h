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
#ifndef _CLIENTSTAT_H_
#define _CLIENTSTAT_H_

#include <QtGui>
#include "ui_clientstat.h"
#include "pages.h"

class ClientStat : public Pages, public Ui::ClientStatForm
{
   Q_OBJECT

public:
   ClientStat(QString&, QTreeWidgetItem*);
   ~ClientStat();
   virtual void PgSeltreeWidgetClicked();
   virtual void currentStackItem();

public slots:
   void populateHeader();
   void populateTerminated();
   void populateRunning();
   void populateAll();
   void populateCurrentTab(int);

private slots:
   void timerTriggered();

private:
   void createConnections();
   void writeSettings();
   void readSettings();
   bool m_populated;
   QTextCursor *m_cursor;
   void getFont();
   QString m_groupText, m_splitText;
   QTimer *m_timer;
   QString m_client;
};

#endif /* _CLIENTSTAT_H_ */
