/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Kern Sibbald, January MM
 *
 * Extracted from other source files by Marco van Wieringen, June 2013
 */

#ifndef BAREOS_LIB_GENERIC_RES_H_
#define BAREOS_LIB_GENERIC_RES_H_ 1

#include "lib/keyword_table_s.h"

#ifndef DIRECTOR_DAEMON
/*
 * Used for message destinations.
 */
struct s_mdestination {
  int code;
  const char* destination;
  bool where;
};

/*
 * Used for message types.
 */
struct s_mtypes {
  const char* name;
  uint32_t token;
};

/*
 * Various message destinations.
 */
static s_mdestination msg_destinations[] = {
    {MD_SYSLOG, "syslog", false},
    {MD_MAIL, "mail", true},
    {MD_FILE, "file", true},
    {MD_APPEND, "append", true},
    {MD_STDOUT, "stdout", false},
    {MD_STDERR, "stderr", false},
    {MD_DIRECTOR, "director", true},
    {MD_OPERATOR, "operator", true},
    {MD_CONSOLE, "console", false},
    {MD_MAIL_ON_ERROR, "mailonerror", true},
    {MD_MAIL_ON_SUCCESS, "mailonsuccess", true},
    {MD_CATALOG, "catalog", false},
    {0, NULL}};

/*
 * Various message types
 */
static struct s_mtypes msg_types[] = {
    {"debug", M_DEBUG},       {"abort", M_ABORT},
    {"fatal", M_FATAL},       {"error", M_ERROR},
    {"warning", M_WARNING},   {"info", M_INFO},
    {"saved", M_SAVED},       {"notsaved", M_NOTSAVED},
    {"skipped", M_SKIPPED},   {"mount", M_MOUNT},
    {"Terminate", M_TERM},    {"restored", M_RESTORED},
    {"security", M_SECURITY}, {"alert", M_ALERT},
    {"volmgmt", M_VOLMGMT},   {"audit", M_AUDIT},
    {"all", M_MAX + 1},       {NULL, 0}};
#endif /* DIRECTOR_DAEMON */

/*
 * Tape Label types permitted in Pool records
 *
 * tape_label label_code = token
 */
static s_kw tapelabels[] = {{"bareos", B_BAREOS_LABEL},
                            {"ansi", B_ANSI_LABEL},
                            {"ibm", B_IBM_LABEL},
                            {NULL, 0}};

#endif /* BAREOS_LIB_GENERIC_RES_H_ */
