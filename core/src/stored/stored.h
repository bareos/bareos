/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2013 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
/**
 * @file
 * Storage daemon specific defines and includes
 */

#ifndef BAREOS_STORED_STORED_H_
#define BAREOS_STORED_STORED_H_

#define STORAGE_DAEMON 1

const int sd_debuglevel = 300;

#if __has_include(<mtio.h>)
#  include <mtio.h>
#else
#  if __has_include(<sys/mtio.h>)
#    ifdef HAVE_AIX_OS
#      define _MTEXTEND_H 1
#    endif
#    include <sys/mtio.h>
#  else
#    if __has_include(<sys/tape.h>)
#      include <sys/tape.h>
#    else
/* Needed for Mac 10.6 (Snow Leopard) */
#      include "lib/bmtio.h"
#    endif
#  endif
#endif
#include "stored/bsr.h"
#include "include/ch.h"
#include "lock.h"
#include "block.h"
#include "record.h"
#include "dev.h"
#include "stored_conf.h"
#include "include/jcr.h"
#include "vol_mgr.h"
#include "reserve.h"

#include "lib/fnmatch.h"
#include <dirent.h>
#define NAMELEN(dirent) (strlen((dirent)->d_name))
#ifndef HAVE_READDIR_R
int Readdir_r(DIR* dirp, struct dirent* entry, struct dirent** result);
#endif

#include "sd_plugins.h"

namespace storagedaemon {
BAREOS_IMPORT bool forge_on; /* Proceed inspite of I/O errors */
uint32_t new_VolSessionId();

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_STORED_H_
