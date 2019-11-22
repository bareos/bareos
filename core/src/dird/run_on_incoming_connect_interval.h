/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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

#ifndef BAREOS_SRC_DIRD_RUN_ON_INCOMING_CONNECT_INTERVAL_H_
#define BAREOS_SRC_DIRD_RUN_ON_INCOMING_CONNECT_INTERVAL_H_

#include <string>

class BareosDb;

namespace directordaemon {

class Scheduler;
class JobResource;

class RunOnIncomingConnectInterval {
 public:
  explicit RunOnIncomingConnectInterval(std::string client_name);

  ~RunOnIncomingConnectInterval() = default;
  RunOnIncomingConnectInterval(const RunOnIncomingConnectInterval&) = delete;
  RunOnIncomingConnectInterval(RunOnIncomingConnectInterval&&) = delete;
  RunOnIncomingConnectInterval operator=(const RunOnIncomingConnectInterval&) =
      delete;
  RunOnIncomingConnectInterval operator=(RunOnIncomingConnectInterval&&) =
      delete;

  void Run();

  // testing
  RunOnIncomingConnectInterval(std::string client_name,
                               Scheduler& scheduler,
                               BareosDb* db);

 private:
  time_t FindLastJobStart(JobResource* job);
  void RunJobIfIntervalExceeded(JobResource* job, time_t last_start_time);

  std::string client_name_;
  Scheduler& scheduler_;
  BareosDb* db_{nullptr};
};

}  // namespace directordaemon

#endif  // BAREOS_SRC_DIRD_RUN_ON_INCOMING_CONNECT_INTERVAL_H_
