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
 *  Help Window class
 *  It reads an html file and displays it in a "browser" window.
 *
 *  Kern Sibbald, May MMVII
 */
#ifndef _HELP_H_
#define _HELP_H_

#include "bat.h"
#include "ui_help.h"

class Help : public QWidget, public Ui::helpForm
{
   Q_OBJECT

public:
   Help(const QString &path, const QString &file, QWidget *parent = NULL);
   virtual ~Help() { };
   static void displayFile(const QString &file);

public slots:
   void updateTitle();

private:
};

#endif /* _HELP_H_ */
