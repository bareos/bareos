/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_MESSAGES_RESOURCE_H_
#define BAREOS_LIB_MESSAGES_RESOURCE_H_

#include "lib/bareos_resource.h"

struct MessageDestinationInfo;

class MessagesResource : public BareosResource {
 public:
  std::string mail_cmd_;                            /* Mail command */
  std::string operator_cmd_;                        /* Operator command */
  std::string timestamp_format_;                    /* Timestamp format */
  std::vector<MessageDestinationInfo*> dest_chain_; /* chain of destinations */
  std::vector<char> send_msg_types_;

 private:
  static pthread_mutex_t mutex_;
  bool in_use_ = false;  /* Set when using to send a message */
  bool closing_ = false; /* Set when closing message resource */

 public:
  MessagesResource();
  virtual ~MessagesResource();
  MessagesResource(const MessagesResource& other) = delete;
  MessagesResource(const MessagesResource&& other) = delete;
  MessagesResource& operator=(const MessagesResource& rhs) = default;
  MessagesResource& operator=(MessagesResource&& rhs) = delete;

  void ClearInUse();
  void SetInUse();
  void SetClosing();
  bool GetClosing();
  void ClearClosing();
  bool IsClosing();
  void WaitNotInUse();
  void lock();
  void unlock();
  bool PrintConfig(PoolMem& buff,
                   const ConfigurationParser& /* unused */,
                   bool hide_sensitive_data = false,
                   bool verbose = false) override;

  void DuplicateResourceTo(MessagesResource& other) const;
  void AddMessageDestination(MessageDestinationCode dest_code,
                             int msg_type,
                             const std::string& where,
                             const std::string& mail_cmd,
                             const std::string& timestamp_format);
  void RemoveMessageDestination(MessageDestinationCode dest_code,
                                int msg_type,
                                const std::string& where);

 private:
  std::vector<MessageDestinationInfo*> DuplicateDestChain() const;
  bool AddToExistingChain(MessageDestinationCode dest_code,
                          int msg_type,
                          const std::string& where);
  void AddToNewChain(MessageDestinationCode dest_code,
                     int msg_type,
                     const std::string& where,
                     const std::string& mail_cmd,
                     const std::string& timestamp_format);
};

#endif /* BAREOS_LIB_MESSAGES_RESOURCE_H_ */
