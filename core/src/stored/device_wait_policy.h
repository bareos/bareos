/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_DEVICE_WAIT_POLICY_H_
#define BAREOS_STORED_DEVICE_WAIT_POLICY_H_

#include <cstdint>

namespace storagedaemon {

/* Budget for waiting until a device becomes available.
 *
 * A job waits in rounds. Every round grants wait_sec seconds, of which
 * rem_wait_sec are still left. When a round is used up the next one lasts
 * twice as long, capped at max_wait, until max_num_wait rounds were spent. */
struct DeviceWaitTimes {
  int32_t min_wait{};
  int32_t max_wait{};
  int32_t max_num_wait{};
  int32_t wait_sec{};
  int32_t rem_wait_sec{};
  int32_t num_wait{};
};

/* Start a new waiting round with twice the length of the previous one.
 *
 * Returns whether another round may be started, so a job that can never get
 * a device stops waiting instead of retrying forever. */
constexpr bool DoubleJobWaitTime(DeviceWaitTimes& times)
{
  times.wait_sec *= 2;
  if (times.wait_sec > times.max_wait) { times.wait_sec = times.max_wait; }

  times.num_wait++;
  times.rem_wait_sec = times.wait_sec;

  return times.num_wait < times.max_num_wait;
}

/* Account for seconds spent waiting for a device.
 *
 * Returns whether the job may keep waiting. Once the current round is used
 * up the next one is started, and waiting ends when the budget is exhausted.
 * A round that cannot make progress, because it grants no time at all, does
 * not stall the budget either. */
constexpr bool ConsumeDeviceWaitTime(DeviceWaitTimes& times, int32_t seconds)
{
  if (seconds > 0) { times.rem_wait_sec -= seconds; }

  if (times.rem_wait_sec > 0) { return true; }

  return DoubleJobWaitTime(times);
}

}  // namespace storagedaemon

#endif  // BAREOS_STORED_DEVICE_WAIT_POLICY_H_
