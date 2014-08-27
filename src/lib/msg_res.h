/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2014 Bareos GmbH & Co. KG

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
 * Extracted from other source files by Marco van Wieringen, April 2014
 */

#ifndef _MSGS_RES_H
#define _MSGS_RES_H 1

/*
 * Message resource directives
 * name config_data_type value code flags default_value
 */
static RES_ITEM msgs_items[] = {
   { "name", CFG_TYPE_NAME, ITEM(res_msgs.hdr.name), 0, 0, NULL },
   { "description", CFG_TYPE_STR, ITEM(res_msgs.hdr.desc), 0, 0, NULL },
   { "mailcommand", CFG_TYPE_STR, ITEM(res_msgs.mail_cmd), 0, 0, NULL },
   { "operatorcommand", CFG_TYPE_STR, ITEM(res_msgs.operator_cmd), 0, 0, NULL },
   { "syslog", CFG_TYPE_MSGS, ITEM(res_msgs), MD_SYSLOG, 0, NULL },
   { "mail", CFG_TYPE_MSGS, ITEM(res_msgs), MD_MAIL, 0, NULL },
   { "mailonerror", CFG_TYPE_MSGS, ITEM(res_msgs), MD_MAIL_ON_ERROR, 0, NULL },
   { "mailonsuccess", CFG_TYPE_MSGS, ITEM(res_msgs), MD_MAIL_ON_SUCCESS, 0, NULL },
   { "file", CFG_TYPE_MSGS, ITEM(res_msgs), MD_FILE, 0, NULL },
   { "append", CFG_TYPE_MSGS, ITEM(res_msgs), MD_APPEND, 0, NULL },
   { "stdout", CFG_TYPE_MSGS, ITEM(res_msgs), MD_STDOUT, 0, NULL },
   { "stderr", CFG_TYPE_MSGS, ITEM(res_msgs), MD_STDERR, 0, NULL },
   { "director", CFG_TYPE_MSGS, ITEM(res_msgs), MD_DIRECTOR, 0, NULL },
   { "console", CFG_TYPE_MSGS, ITEM(res_msgs), MD_CONSOLE, 0, NULL },
   { "operator", CFG_TYPE_MSGS, ITEM(res_msgs), MD_OPERATOR, 0, NULL },
   { "catalog", CFG_TYPE_MSGS, ITEM(res_msgs), MD_CATALOG, 0, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

#endif /* _MSGS_RES_H */
