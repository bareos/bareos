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

#ifndef BAREOS_STORED_CONNECT_WAIT_H_
#define BAREOS_STORED_CONNECT_WAIT_H_

#include "include/bareos.h"

namespace storagedaemon {

// Fallback used when Client Connect Wait is unset or not usable.
inline constexpr utime_t kDefaultClientConnectWait = 1800;

/* Fallback used when Ndmp Connect Wait is unset or not usable.
 *
 * The director tells the NDMP data mover to connect right after the storage
 * daemon reserved the device, so the connection normally arrives within
 * seconds. Waiting as long as for a file daemon would keep a tape drive
 * reserved for half an hour just because a filer is unreachable. */
inline constexpr utime_t kDefaultNdmpConnectWait = 180;

/* Select how long the storage daemon waits for the data connection of a job.
 *
 * NDMP jobs get their own, shorter deadline so an unreachable data mover
 * frees the reserved device quickly instead of blocking it for the much
 * longer file daemon timeout. Values that cannot be used as a deadline fall
 * back to the respective default. */
constexpr utime_t SelectConnectWait(bool is_ndmp,
                                    utime_t client_wait,
                                    utime_t ndmp_connect_wait)
{
  if (is_ndmp) {
    return ndmp_connect_wait > 0 ? ndmp_connect_wait : kDefaultNdmpConnectWait;
  }

  return client_wait > 0 ? client_wait : kDefaultClientConnectWait;
}

}  // namespace storagedaemon

#endif  // BAREOS_STORED_CONNECT_WAIT_H_
