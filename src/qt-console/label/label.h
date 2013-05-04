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
 * Kern Sibbald, February MMVII
 */
#ifndef _LABEL_H_
#define _LABEL_H_

#include <QtGui>
#include "ui_label.h"
#include "pages.h"

class labelPage : public Pages, public Ui::labelForm
{
   Q_OBJECT

public:
   labelPage();
   labelPage(QString &defString);
   void showPage(QString &defString);

private slots:
   void okButtonPushed();
   void cancelButtonPushed();
   void automountOnButtonPushed();
   void automountOffButtonPushed();

private:
   int m_conn;
};

#endif /* _LABEL_H_ */
