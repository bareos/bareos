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

/* Maximum duration of one wait for a device before the caller rechecks its
 * state and reports that it is still waiting. */
inline constexpr std::chrono::seconds kDefaultDeviceWait{60};

/* How long a job may wait for a device in total before it gives up.
 *
 * Waiting is deliberately generous, because a job queued behind a long
 * backup should not fail just for being patient. It is bounded so that a job
 * which can never get a device does not stay in the reservation loop
 * forever. */
inline constexpr std::chrono::seconds kMaxTotalDeviceWait{5 * 24 * 60 * 60};

/* What is left of the time a job may wait for a device. */
struct DeviceWaitBudget {
  std::chrono::milliseconds remaining{kMaxTotalDeviceWait};

  /* Charge elapsed time, with a small minimum to ensure the budget progresses
   * even when a duration rounds down to zero. */
  constexpr bool Consume(std::chrono::milliseconds elapsed)
  {
    constexpr std::chrono::milliseconds minimum_charge{1};

    const auto charge = std::max(elapsed, minimum_charge);
    remaining -= std::min(charge, remaining);

    return remaining > std::chrono::milliseconds{0};
  }
};

}  // namespace storagedaemon

#endif  // BAREOS_STORED_DEVICE_WAIT_POLICY_H_
