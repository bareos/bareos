/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2009 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is 
   listed in the file LICENSE.

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

#ifndef _RUN_H_
#define _RUN_H_

#include <QtGui>
#include "ui_run.h"
#include "ui_runcmd.h"
#include "ui_estimate.h"
#include "ui_prune.h"
#include "console.h"

class runPage : public Pages, public Ui::runForm
{
   Q_OBJECT 

public:
   runPage();

   runPage(const QString &defJob);

   runPage(const QString &defJob, 
           const QString &level,
           const QString &pool,
           const QString &storage,
           const QString &client,
           const QString &fileset);

public slots:
   void okButtonPushed();
   void cancelButtonPushed();
   void job_name_change(int index);

private:
   void init();
   int m_conn;
};

class runCmdPage : public Pages, public Ui::runCmdForm
{
   Q_OBJECT 

public:
   runCmdPage(int conn);

public slots:
   void okButtonPushed();
   void cancelButtonPushed();

private:
   void fill();
   int m_conn;
};

class estimatePage : public Pages, public Ui::estimateForm
{
   Q_OBJECT 

public:
   estimatePage();

public slots:
   void okButtonPushed();
   void cancelButtonPushed();
   void job_name_change(int index);

private:
   int m_conn;
   bool m_aButtonPushed;
};

class prunePage : public Pages, public Ui::pruneForm
{
   Q_OBJECT 

public:
   prunePage(const QString &volume, const QString &client);

public slots:
   void okButtonPushed();
   void cancelButtonPushed();
   void volumeChanged();
   void clientChanged();

private:
   int m_conn;
};

#endif /* _RUN_H_ */
