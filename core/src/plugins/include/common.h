/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2010-2011 Bacula Systems(R) SA
   Copyright (C) 2016-2025 Bareos GmbH & Co. KG

   This program is Free Software; you can modify it under the terms of
   version three of the GNU Affero General Public License as published by the
   Free Software Foundation, which is listed in the file LICENSE.

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
 * @file
 * You can include this file to your plugin to have
 * access to some common tools and utilities provided by Bareos
 */

#ifndef BAREOS_PLUGINS_INCLUDE_COMMON_H_
#define BAREOS_PLUGINS_INCLUDE_COMMON_H_


#define JT_BACKUP 'B'  /* Backup Job */
#define JT_RESTORE 'R' /* Restore Job */

#include "include/job_level.h"

#define Dmsg(context, level, ...)                                             \
  if (bareos_core_functions && context) {                                     \
    bareos_core_functions->DebugMessage(context, __FILE__, __LINE__, level,   \
                                        __VA_ARGS__);                         \
  } else {                                                                    \
    fprintf(stderr,                                                           \
            "Dmsg: bareos_core_functions(%p) and context(%p) need to be set " \
            "before Dmsg call\n",                                             \
            bareos_core_functions, context);                                  \
  }

#define Jmsg(context, type, ...)                                              \
  if (bareos_core_functions && context) {                                     \
    bareos_core_functions->JobMessage(context, __FILE__, __LINE__, type, 0,   \
                                      __VA_ARGS__);                           \
  } else {                                                                    \
    fprintf(stderr,                                                           \
            "Jmsg: bareos_core_functions(%p) and context(%p) need to be set " \
            "before Jmsg call\n",                                             \
            bareos_core_functions, context);                                  \
  }
#endif  // BAREOS_PLUGINS_INCLUDE_COMMON_H_
