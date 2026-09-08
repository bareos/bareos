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

#include <algorithm>
#include <chrono>

namespace storagedaemon {

/* How long a single wait for a device lasts.
 *
 * Releasing a device signals the jobs that wait for one, so this is not a
 * polling interval. It only bounds how long a job would sleep if such a
 * signal was ever missed, and it decides how often the job reports that it
 * is still waiting. */
inline constexpr std::chrono::seconds kDefaultDeviceWait{60};

/* How long a job may wait for a device in total before it gives up.
 *
 * Waiting is deliberately generous, because a job queued behind a long
 * backup should not fail just for being patient. It is bounded so that a job
 * which can never get a device does not stay in the reservation loop
 * forever. */
inline constexpr std::chrono::seconds kMaxTotalDeviceWait{5 * 24 * 60 * 60};

/* The least a single wait costs, so that every wait makes progress.
 *
 * A wait that is woken up by a released device may be arbitrarily short, and
 * a wait that is granted no time at all does not sleep in the first place.
 * Charging such waits a token amount keeps the budget strictly decreasing,
 * which is what bounds the reservation loop. The amounts are far too small
 * to shorten the wait of a job that makes any real progress. */
inline constexpr std::chrono::milliseconds kMinSignalledWaitCharge{1};
inline constexpr std::chrono::milliseconds kMinTimedOutWaitCharge{1000};

/* What is left of the time a job may wait for a device.
 *
 * This is a plain remaining duration, without any backoff between the
 * individual waits. A job is woken up as soon as a device is released, so
 * waiting longer each time would not save any work: it would only delay the
 * job past the moment a device became free.
 *
 * The budget is kept in milliseconds although it is configured in seconds,
 * because waits that end on a signal are usually much shorter than a second
 * and must not be charged as if they had taken no time at all. */
struct DeviceWaitBudget {
  std::chrono::milliseconds remaining{kMaxTotalDeviceWait};
};

// How long the next wait may last, never longer than what is left.
constexpr std::chrono::milliseconds NextDeviceWait(
    const DeviceWaitBudget& budget,
    std::chrono::milliseconds interval = kDefaultDeviceWait)
{
  return std::min(budget.remaining, interval);
}

/* Charge time spent waiting against the budget.
 *
 * Returns whether the job may keep waiting. The charge is at least the
 * given minimum, so that repeated wakeups cannot leave the budget untouched
 * and keep the job in the reservation loop forever. */
constexpr bool ConsumeDeviceWait(DeviceWaitBudget& budget,
                                 std::chrono::milliseconds waited,
                                 std::chrono::milliseconds minimum
                                 = kMinSignalledWaitCharge)
{
  constexpr std::chrono::milliseconds none{0};

  const auto charge = std::max(waited, minimum);
  budget.remaining -= std::min(charge, budget.remaining);

  return budget.remaining > none;
}

}  // namespace storagedaemon

#endif  // BAREOS_STORED_DEVICE_WAIT_POLICY_H_
