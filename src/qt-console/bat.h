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
/*
 * Kern Sibbald, January 2007
 */
#ifndef _BAT_H_
#define _BAT_H_

#if defined(HAVE_WIN32)
#if !defined(_STAT_H)
#define _STAT_H       /* don't pull in MinGW stat.h */
#define _STAT_DEFINED /* don't pull in MinGW stat.h */
#endif
#endif

#include "hostconfig.h"

#include <QtGui>
#include <QtCore>
#include "bareos.h"
#include "mainwin.h"
#include "bat_conf.h"
#include "jcr.h"
#include "console/console.h"

extern MainWin *mainWin;
extern QApplication *app;
extern CONFIG *my_config;

bool isWin32Path(QString &fullPath);

#endif /* _BAT_H_ */
