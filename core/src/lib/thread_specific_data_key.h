/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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

#ifndef BAREOS_SRC_LIB_THREAD_SPECIFIC_DATA_KEY_H_
#define BAREOS_SRC_LIB_THREAD_SPECIFIC_DATA_KEY_H_

#include "lib/berrno.h"

#include <mutex>

class ThreadSpecificDataKey {
 public:
  static pthread_key_t Key()
  {
    init_once();
    return key_;
  }

 private:
  static pthread_key_t key_;
  static std::once_flag once_flag;

  static void init_once()
  {
    try {
      static std::once_flag once_flag;
      std::call_once(once_flag, CreateKey);
    } catch (const std::system_error& e) {
      Jmsg1(nullptr, M_ABORT, 0,
            _("Could not call CreateThreadSpecificDataKey: %s\n"), e.what());
    }
  }

  static void CreateKey()
  {
    int status = pthread_key_create(&key_, nullptr);
    if (status != 0) {
      BErrNo be;
      Jmsg1(nullptr, M_ABORT, 0, _("pthread key create failed: ERR=%s\n"),
            be.bstrerror(status));
    }
  }
};

pthread_key_t ThreadSpecificDataKey::key_;

#endif  // BAREOS_SRC_LIB_THREAD_SPECIFIC_DATA_KEY_H_
