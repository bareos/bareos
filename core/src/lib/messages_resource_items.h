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

#ifndef BAREOS_LIB_MSG_RES_H_
#define BAREOS_LIB_MSG_RES_H_ 1

#include "lib/message_destination_info.h"

/* clang-format off */

static ResourceItem msgs_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_msgs, resource_name_), static_cast<int>(MessageDestinationCode::kUndefined), 0, NULL, NULL, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_msgs, description_), static_cast<int>(MessageDestinationCode::kUndefined), 0, NULL, NULL, NULL },
   { "MailCommand", CFG_TYPE_STDSTR, ITEM(res_msgs, mail_cmd_), static_cast<int>(MessageDestinationCode::kUndefined), 0, NULL, NULL, NULL },
   { "OperatorCommand", CFG_TYPE_STDSTR, ITEM(res_msgs, operator_cmd_), static_cast<int>(MessageDestinationCode::kUndefined), 0, NULL, NULL, NULL },
   { "TimestampFormat", CFG_TYPE_STDSTR, ITEM(res_msgs, timestamp_format_), static_cast<int>(MessageDestinationCode::kUndefined), 0, NULL, NULL, NULL },
   { "Syslog", CFG_TYPE_MSGS, ITEMC(res_msgs), static_cast<int>(MessageDestinationCode::kSyslog), 0, NULL, NULL, NULL },
   { "Mail", CFG_TYPE_MSGS, ITEMC(res_msgs), static_cast<int>(MessageDestinationCode::kMail), 0, NULL, NULL, NULL },
   { "MailOnError", CFG_TYPE_MSGS, ITEMC(res_msgs), static_cast<int>(MessageDestinationCode::KMailOnError), 0, NULL, NULL, NULL },
   { "MailOnSuccess", CFG_TYPE_MSGS, ITEMC(res_msgs), static_cast<int>(MessageDestinationCode::kMailOnSuccess), 0, NULL, NULL, NULL },
   { "File", CFG_TYPE_MSGS, ITEMC(res_msgs), static_cast<int>(MessageDestinationCode::kFile), 0, NULL, NULL, NULL },
   { "Append", CFG_TYPE_MSGS, ITEMC(res_msgs), static_cast<int>(MessageDestinationCode::kAppend), 0, NULL, NULL, NULL },
   { "Stdout", CFG_TYPE_MSGS, ITEMC(res_msgs), static_cast<int>(MessageDestinationCode::kStdout), 0, NULL, NULL, NULL },
   { "Stderr", CFG_TYPE_MSGS, ITEMC(res_msgs), static_cast<int>(MessageDestinationCode::kStderr), 0, NULL, NULL, NULL },
   { "Director", CFG_TYPE_MSGS, ITEMC(res_msgs), static_cast<int>(MessageDestinationCode::kDirector), 0, NULL, NULL, NULL },
   { "Console", CFG_TYPE_MSGS, ITEMC(res_msgs), static_cast<int>(MessageDestinationCode::kConsole), 0, NULL, NULL, NULL },
   { "Operator", CFG_TYPE_MSGS, ITEMC(res_msgs), static_cast<int>(MessageDestinationCode::kOperator), 0, NULL, NULL, NULL },
   { "Catalog", CFG_TYPE_MSGS, ITEMC(res_msgs), static_cast<int>(MessageDestinationCode::kCatalog), 0, NULL, NULL, NULL },
   {nullptr, 0, 0, nullptr, static_cast<int>(MessageDestinationCode::kUndefined), 0, nullptr, nullptr, nullptr}
};

/* clang-format on */

#endif /* BAREOS_LIB_MSG_RES_H_ */
