/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2025 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_MESSAGES_RESOURCE_ITEMS_H_
#define BAREOS_LIB_MESSAGES_RESOURCE_ITEMS_H_

#include "lib/message_destination_info.h"

/* clang-format off */

static ResourceItem msgs_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_msgs, resource_name_), {config::Code{static_cast<int>(MessageDestinationCode::kUndefined)}}},
  { "Description", CFG_TYPE_STR, ITEM(res_msgs, description_), {config::Code{static_cast<int>(MessageDestinationCode::kUndefined)}}},
  { "MailCommand", CFG_TYPE_STDSTR, ITEM(res_msgs, mail_cmd_), {config::Code{static_cast<int>(MessageDestinationCode::kUndefined)}}},
  { "OperatorCommand", CFG_TYPE_STDSTR, ITEM(res_msgs, operator_cmd_), {config::Code{static_cast<int>(MessageDestinationCode::kUndefined)}}},
  { "TimestampFormat", CFG_TYPE_STDSTR, ITEM(res_msgs, timestamp_format_), {config::Code{static_cast<int>(MessageDestinationCode::kUndefined)}}},
  { "Syslog", CFG_TYPE_MSGS, ITEMC(res_msgs), {config::Code{static_cast<int>(MessageDestinationCode::kSyslog)}}},
  { "Mail", CFG_TYPE_MSGS, ITEMC(res_msgs), {config::Code{static_cast<int>(MessageDestinationCode::kMail)}}},
  { "MailOnError", CFG_TYPE_MSGS, ITEMC(res_msgs), {config::Code{static_cast<int>(MessageDestinationCode::KMailOnError)}}},
  { "MailOnSuccess", CFG_TYPE_MSGS, ITEMC(res_msgs), {config::Code{static_cast<int>(MessageDestinationCode::kMailOnSuccess)}}},
  { "File", CFG_TYPE_MSGS, ITEMC(res_msgs), {config::Code{static_cast<int>(MessageDestinationCode::kFile)}}},
  { "Append", CFG_TYPE_MSGS, ITEMC(res_msgs), {config::Code{static_cast<int>(MessageDestinationCode::kAppend)}}},
  { "Stdout", CFG_TYPE_MSGS, ITEMC(res_msgs), {config::Code{static_cast<int>(MessageDestinationCode::kStdout)}}},
  { "Stderr", CFG_TYPE_MSGS, ITEMC(res_msgs), {config::Code{static_cast<int>(MessageDestinationCode::kStderr)}}},
  { "Director", CFG_TYPE_MSGS, ITEMC(res_msgs), {config::Code{static_cast<int>(MessageDestinationCode::kDirector)}}},
  { "Console", CFG_TYPE_MSGS, ITEMC(res_msgs), {config::Code{static_cast<int>(MessageDestinationCode::kConsole)}}},
  { "Operator", CFG_TYPE_MSGS, ITEMC(res_msgs), {config::Code{static_cast<int>(MessageDestinationCode::kOperator)}}},
  { "Catalog", CFG_TYPE_MSGS, ITEMC(res_msgs), {config::Code{static_cast<int>(MessageDestinationCode::kCatalog)}}},
  {}
};

/* clang-format on */

#endif  // BAREOS_LIB_MESSAGES_RESOURCE_ITEMS_H_
