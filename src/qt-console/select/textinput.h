/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.

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
#ifndef _TEXTENTRY_H_
#define _TEXTENTRY_H_

#include <QtGui>
#include "ui_textinput.h"
#include <console/console.h>

class textInputDialog : public QDialog, public Ui::textInputForm
{
   Q_OBJECT

public:
   textInputDialog(Console *console, int conn);

public slots:
   void accept();
   void reject();

private:
   Console *m_console;
   int m_conn;
};

#endif /* _TEXTENTRY_H_ */
