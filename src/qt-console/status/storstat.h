#ifndef _STORSTAT_H_
#define _STORSTAT_H_
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
 *   Version $Id: dirstat.h 5372 2007-08-17 12:17:04Z kerns $
 *
 *   Dirk Bartley, March 2007
 */

#include <QtGui>
#include "ui_storstat.h"
#include "console.h"
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
