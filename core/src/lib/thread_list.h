/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_LIB_THREAD_LIST_H_
#define BAREOS_LIB_THREAD_LIST_H_ 1

#include <functional>

class ConfigurationParser;
struct ThreadListPrivate;

class ThreadList {
  friend class CleanupOwnThreadAndNotify;

 public:
  ThreadList();
  ~ThreadList();

  void Init(int maximum_thread_count,
            std::function<void*(ConfigurationParser* config, void* data)>
                ThreadInvokedHandler,
            std::function<void*(void* arg)> ShutdownCallback = nullptr);

  bool CreateAndAddNewThread(ConfigurationParser* config, void* data);
  bool WaitUntilThreadListIsEmpty();
  std::size_t GetSize() const;

  ThreadList(const ThreadList& ohter) = delete;
  ThreadList(const ThreadList&& ohter) = delete;
  ThreadList& operator=(const ThreadList& rhs) = delete;
  ThreadList& operator=(const ThreadList&& rhs) = delete;

 private:
  std::unique_ptr<ThreadListPrivate> impl_;
};


#endif /* BAREOS_LIB_THREAD_LIST_H_ */
