/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2013 Free Software Foundation Europe e.V.

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
 * Storage daemon specific defines and includes
 *
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
# ifdef HAVE_SYS_MTIO_H
# include <sys/mtio.h>
# else
#   ifdef HAVE_SYS_TAPE_H
#   include <sys/tape.h>
#   else
    /* Needed for Mac 10.6 (Snow Leopard) */
#   include "lib/bmtio.h"
#   endif
# endif
#endif
#include "lock.h"
#include "block.h"
#include "record.h"
#include "dev.h"
#include "stored_conf.h"
#include "bsr.h"
#include "jcr.h"
#include "vol_mgr.h"
#include "reserve.h"
#include "protos.h"
#ifdef HAVE_LIBZ
#include <zlib.h>                     /* compression headers */
#else
#define uLongf uint32_t
#endif
#ifdef HAVE_LZO
#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>
#endif
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

#include "vtape.h"
#include "sd_plugins.h"

/* Daemon globals from stored.c */
extern STORES *me;                    /* "Global" daemon resource */
extern bool forge_on;                 /* proceed inspite of I/O errors */
extern pthread_mutex_t device_release_mutex;
extern pthread_cond_t wait_device_release; /* wait for any device to be released */                           

#endif /* __STORED_H_ */
