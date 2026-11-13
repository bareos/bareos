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

#ifndef BAREOS_STORED_DEVICE_INIT_H_
#define BAREOS_STORED_DEVICE_INIT_H_

#include <array>
#include <string_view>

namespace storagedaemon {

/* Initialization state of a single device.
 *
 * Device initialization happens in the background while the storage daemon
 * already answers network requests, so every device carries its own state. */
enum class DeviceInitState
{
  Pending,      /**< not looked at yet */
  Initializing, /**< being created/opened/mounted right now */
  Ready,        /**< initialization finished successfully */
  Failed        /**< initialization failed */
};

constexpr std::string_view DeviceInitStateToString(DeviceInitState state)
{
  switch (state) {
    case DeviceInitState::Pending:
      return "pending";
    case DeviceInitState::Initializing:
      return "initializing";
    case DeviceInitState::Ready:
      return "ready";
    case DeviceInitState::Failed:
      return "failed";
  }
  return "unknown";
}

// True while the device object must not be accessed by other threads.
constexpr bool DeviceInitInProgress(DeviceInitState state)
{
  return state == DeviceInitState::Pending
         || state == DeviceInitState::Initializing;
}

/* Commands from the director that neither use nor inspect a device and may
 * therefore be answered while device initialization is still running. */
inline constexpr std::array kDeviceIndependentSdDirCommands{
    std::string_view{"status"},           std::string_view{".status"},
    std::string_view{"setdebug="},        std::string_view{"resolve"},
    std::string_view{"cancel"},           std::string_view{"stats"},
    std::string_view{"getSecureEraseCmd"}};

/* Whether a director command has to wait until background device
 * initialization has finished. */
constexpr bool SdDirCommandNeedsDeviceInit(std::string_view cmd)
{
  for (auto known : kDeviceIndependentSdDirCommands) {
    if (known == cmd) { return false; }
  }
  return true;
}

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_DEVICE_INIT_H_
