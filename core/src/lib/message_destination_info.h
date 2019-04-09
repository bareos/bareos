/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_MESSAGE_DESTINATION_INFO_H_
#define BAREOS_LIB_MESSAGE_DESTINATION_INFO_H_

#include "lib/message_level.h"

enum class MessageDestinationCode : int
{
  kUndefined = 0,
  kSyslog = 1,     // Send msg to syslog
  kMail,           // Email group of messages
  kFile,           // Write messages to a file
  kAppend,         // Append messages to a file
  kStdout,         // Print messages
  kStderr,         // Print messages to stderr
  kDirector,       // Send message to the Director
  kOperator,       // Email a single message to the operator
  kConsole,        // Send msg to UserAgent or console
  KMailOnError,    // Email messages if job errors
  kMailOnSuccess,  // Email messages if job succeeds
  kCatalog         // Store message into catalog
};


struct MessageDestinationInfo {
 public:
  MessageDestinationInfo() = default;
  ~MessageDestinationInfo() = default;
  MessageDestinationInfo(const MessageDestinationInfo& other) = default;

  FILE* file_pointer_ = nullptr;
  MessageDestinationCode dest_code_ = MessageDestinationCode::kUndefined;
  int max_len_ = 0;                 /* Max mail line length */
  int syslog_facility_ = 0;         /* Syslog Facility */
  char msg_types_[NR_MSG_TYPES]{0}; /* Message type mask */
  std::string where_;               /* Filename/program name */
  std::string mail_cmd_;            /* Mail command */
  std::string timestamp_format_;    /* used in logging messages */
  std::string mail_filename_;       /* Unique mail filename */
};

#endif /* BAREOS_LIB_MESSAGE_DESTINATION_INFO_H_ */
