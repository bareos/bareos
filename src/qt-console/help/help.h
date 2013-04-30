#ifndef _HELP_H_
#define _HELP_H_

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
 *  Help Window class
 *
 *    It reads an html file and displays it in a "browser" window.
 *
 *   Kern Sibbald, May MMVII
 *
 *  $Id$
 */ 

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
