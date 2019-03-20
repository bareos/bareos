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
  /*
   * Members
   */
 public:
  char* mail_cmd;                         /* Mail command */
  char* operator_cmd;                     /* Operator command */
  char* timestamp_format;                 /* Timestamp format */
  DEST* dest_chain;                       /* chain of destinations */
  char SendMsg[NbytesForBits(M_MAX + 1)]; /* Bit array of types */

 private:
  bool in_use_;  /* Set when using to send a message */
  bool closing_; /* Set when closing message resource */

 public:
  /*
   * Methods
   */
  void ClearInUse()
  {
    lock();
    in_use_ = false;
    unlock();
  }
  void SetInUse()
  {
    WaitNotInUse();
    in_use_ = true;
    unlock();
  }
  void SetClosing() { closing_ = true; }
  bool GetClosing() { return closing_; }
  void ClearClosing()
  {
    lock();
    closing_ = false;
    unlock();
  }
  bool IsClosing()
  {
    lock();
    bool rtn = closing_;
    unlock();
    return rtn;
  }

  MessagesResource() = default;
  virtual ~MessagesResource() = default;

  void CopyToStaticMemory(CommonResourceHeader* p) const override
  {
    MessagesResource* r = dynamic_cast<MessagesResource*>(p);
    if (r) { *r = *this; }
  };

  void WaitNotInUse(); /* in message.c */
  void lock();         /* in message.c */
  void unlock();       /* in message.c */
  bool PrintConfig(PoolMem& buff,
                   bool hide_sensitive_data = false,
                   bool verbose = false);
};

#endif /* BAREOS_LIB_MESSAGES_RESOURCE_H_ */
