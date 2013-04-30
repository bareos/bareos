#ifndef _DIRCOMM_H_
#define _DIRCOMM_H_
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2009 Free Software Foundation Europe e.V.

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
 *   Kern Sibbald, January 2007
 */

#include <QtGui>
#include "pages.h"
#include "ui_console.h"
#include <QObject>

#ifndef MAX_NAME_LENGTH
#define MAX_NAME_LENGTH 128
#endif

class DIRRES;
class BSOCK;
class JCR;
class CONRES;

//class DirComm : public QObject
class DirComm : public QObject
{
   Q_OBJECT
   friend class Console;
public:
   DirComm(Console *parent, int conn);
   ~DirComm();
   Console *m_console;
   int  sock_read();
   bool authenticate_director(JCR *jcr, DIRRES *director, CONRES *cons, 
          char *buf, int buflen);
   bool is_connected() { return m_sock != NULL; };
   bool is_ready() { return is_connected() && m_at_prompt && m_at_main_prompt; };
   char *msg();
   bool notify(bool enable); // enables/disables socket notification - returns the previous state
   bool is_notify_enabled() const;
   bool is_in_command() const { return m_in_command > 0; };
   void terminate();
   bool connect_dir();                     
   int read(void);
   int write(const char *msg);
   int write(QString msg);

public slots:
   void notify_read_dir(int fd);

private:
   BSOCK *m_sock;   
   bool m_at_prompt;
   bool m_at_main_prompt;
   bool m_sent_blank;
   bool m_notify;
   int  m_in_command;
   QSocketNotifier *m_notifier;
   bool m_api_set;
   int m_conn;
   bool m_in_select;
};

#endif /* _DIRCOMM_H_ */
