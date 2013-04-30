#ifndef _BAT_H_
#define _BAT_H_

/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.

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
 *   Kern Sibbald, January 2007
 */

#if defined(HAVE_WIN32)
#if !defined(_STAT_H)
#define _STAT_H       /* don't pull in MinGW stat.h */
#define _STAT_DEFINED /* don't pull in MinGW stat.h */
#endif
#endif

#if defined(HAVE_WIN32)
#if defined(HAVE_MINGW)
#include "mingwconfig.h"
#else
#include "winconfig.h"
#endif
#else
#include "config.h"
#endif
#define __CONFIG_H

#include <QtGui>
#include <QtCore>
#include "bacula.h"
#include "mainwin.h"
#include "bat_conf.h"
#include "jcr.h"
#include "console.h"

extern MainWin *mainWin;
extern QApplication *app;

bool isWin32Path(QString &fullPath);

#endif /* _BAT_H_ */
