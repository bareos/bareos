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
#ifndef _SELECT_H_
#define _SELECT_H_

#include <QtGui>
#include "ui_select.h"
#include <console/console.h>

class selectDialog : public QDialog, public Ui::selectForm
{
   Q_OBJECT

public:
   selectDialog(Console *console, int conn);

public slots:
   void accept();
   void reject();
   void index_change(int index);

private:
   Console *m_console;
   int m_index;
   int m_conn;
};

class yesnoPopUp : public QDialog
{
   Q_OBJECT

public:
   yesnoPopUp(Console *console, int conn);

};

#endif /* _SELECT_H_ */
