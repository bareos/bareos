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

class MessagesResource : public BareosResource {
 public:
  std::string mail_cmd_;                          /* Mail command */
  std::string operator_cmd_;                      /* Operator command */
  std::string timestamp_format_;                  /* Timestamp format */
  std::vector<DEST*> dest_chain_;                 /* chain of destinations */
  char send_msg_types_[NbytesForBits(M_MAX + 1)]; /* Bit array of types */

 private:
  static pthread_mutex_t mutex_;
  bool in_use_;  /* Set when using to send a message */
  bool closing_; /* Set when closing message resource */

 public:
  MessagesResource();
  virtual ~MessagesResource();

  void ShallowCopyTo(BareosResource* p) const override;

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
};

#endif /* BAREOS_LIB_MESSAGES_RESOURCE_H_ */
