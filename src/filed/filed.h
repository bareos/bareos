/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2010 Free Software Foundation Europe e.V.

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
 * Bacula File Daemon specific configuration and defines
 *
 *     Kern Sibbald, Jan MMI
 *
 */

/*
 * Number of acl errors to report per job.
 */
#define ACL_REPORT_ERR_MAX_PER_JOB      25

/*
 * Number of xattr errors to report per job.
 */
#define XATTR_REPORT_ERR_MAX_PER_JOB    25

/*
 * Return codes from acl subroutines.
 */
typedef enum {
   bacl_exit_fatal = -1,
   bacl_exit_error = 0,
   bacl_exit_ok = 1
} bacl_exit_code;

/*
 * Return codes from xattr subroutines.
 */
typedef enum {
   bxattr_exit_fatal = -1,
   bxattr_exit_error = 0,
   bxattr_exit_ok = 1
} bxattr_exit_code;

#define FILE_DAEMON 1
#include "lib/htable.h"
#include "filed_conf.h"
#include "fd_plugins.h"
#include "findlib/find.h"
#include "acl.h"
#include "xattr.h"
#include "jcr.h"
#include "protos.h"                   /* file daemon prototypes */
#include "lib/runscript.h"
#include "lib/breg.h"
#ifdef HAVE_LIBZ
#include <zlib.h>                     /* compression headers */
#else
#define uLongf uint32_t
#endif
#ifdef HAVE_LZO
#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>
#endif

extern CLIENT *me;                    /* "Global" Client resource */

void terminate_filed(int sig);

