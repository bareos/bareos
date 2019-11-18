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

#ifndef BAREOS_SRC_DIRD_SCHEDULER_TIME_ADAPTER_H_
#define BAREOS_SRC_DIRD_SCHEDULER_TIME_ADAPTER_H_

#include "include/bareos.h"

namespace directordaemon {

class TimeSource {
 public:
  virtual time_t SystemTime() = 0;
  virtual void SleepFor(std::chrono::seconds wait_interval) = 0;
  virtual void Terminate() = 0;
  virtual ~TimeSource() = default;
};

class SchedulerTimeAdapter {
 public:
  std::unique_ptr<TimeSource> time_source_;
  time_t default_wait_interval_{60};
  virtual ~SchedulerTimeAdapter() = default;

 protected:
  SchedulerTimeAdapter(std::unique_ptr<TimeSource> time_source)
      : time_source_(std::move(time_source))
  {
  }
};

} /* namespace directordaemon */

#endif  // BAREOS_SRC_DIRD_SCHEDULER_TIME_ADAPTER_H_
