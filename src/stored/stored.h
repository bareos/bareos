/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2013 Free Software Foundation Europe e.V.

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
 * Storage daemon specific defines and includes
 */

#ifndef __STORED_H_
#define __STORED_H_

#define STORAGE_DAEMON 1

/* Set to debug mutexes */
//#define SD_DEBUG_LOCK
#ifdef SD_DEBUG_LOCK
const int sd_dbglvl = 3;
#else
const int sd_dbglvl = 300;
#endif

#ifdef HAVE_MTIO_H
#include <mtio.h>
#else
#ifdef HAVE_SYS_MTIO_H
#ifdef HAVE_AIX_OS
#define _MTEXTEND_H 1
#endif
#include <sys/mtio.h>
#else
#ifdef HAVE_SYS_TAPE_H
#include <sys/tape.h>
#else
/* Needed for Mac 10.6 (Snow Leopard) */
#include "lib/bmtio.h"
#endif
#endif
#endif
#include "lib/bsr.h"
#include "ch.h"
#include "lock.h"
#include "block.h"
#include "record.h"
#include "dev.h"
#include "stored_conf.h"
#include "jcr.h"
#include "vol_mgr.h"
#include "reserve.h"

#include "protos.h"
#ifdef HAVE_FNMATCH
#include <fnmatch.h>
#else
#include "lib/fnmatch.h"
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#define NAMELEN(dirent) (strlen((dirent)->d_name))
#endif
#ifndef HAVE_READDIR_R
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
#endif

#include "sd_plugins.h"

extern char SD_IMP_EXP *configfile;
extern bool SD_IMP_EXP forge_on;      /* Proceed inspite of I/O errors */
extern STORES SD_IMP_EXP *me;         /* "Global" daemon resource */
extern CONFIG SD_IMP_EXP *my_config;  /* Our Global config */

#endif /* __STORED_H_ */
