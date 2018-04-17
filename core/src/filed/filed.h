/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2014 Bareos GmbH & Co. KG

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
 * Bareos File Daemon specific configuration and defines
 *
 * Kern Sibbald, Jan MMI
 */

#ifndef __FILED_H_
#define __FILED_H_

#define FILE_DAEMON 1
#include "filed_conf.h"
#ifdef HAVE_WIN32
#include "vss.h"
#endif
#include "jcr.h"
#include "lib/breg.h"
#include "lib/htable.h"
#include "lib/runscript.h"
#include "findlib/find.h"
#include "fd_plugins.h"
#include "ch.h"
#include "backup.h"
#include "restore.h"

extern ClientResource *me;                 /* "Global" Client resource */
extern ConfigurationParser *my_config;             /* Our Global config */

void terminate_filed(int sig);

/*
 * File Daemon protocol version
 */
const int FD_PROTOCOL_VERSION = 54;

#endif /* __FILED_H_ */
