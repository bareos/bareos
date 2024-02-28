/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2007 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2024 Bareos GmbH & Co. KG

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
// Kern Sibbald, February MMVI
/**
 * @file
 * Definitions for reservation system.
 */

/**
 *   Use Device command from Director
 *   The DIR tells us what Device Name to use, the Media Type,
 *      the Pool Name, and the Pool Type.
 *
 *    Ensure that the device exists and is opened, then store
 *      the media and pool info in the JobControlRecord.  This class is used
 *      only temporarily in this file.
 */

#ifndef BAREOS_STORED_RESERVE_H_
#define BAREOS_STORED_RESERVE_H_

#include <vector>
#include <string>
#include <string_view>

namespace storagedaemon {

struct director_storage {
  bool append;
  std::vector<std::string> device_names{};
  std::string name{}, media_type{}, pool_name{}, pool_type{};

  director_storage(bool append_,
                   std::string_view sname_,
                   std::string_view mtype_,
                   std::string_view pname_,
                   std::string_view ptype_)
      : append{append_}
      , name{sname_}
      , media_type{mtype_}
      , pool_name{pname_}
      , pool_type{ptype_}
  {
  }
};

/* Reserve context */
class ReserveContext {
 public:
  JobControlRecord* jcr;
  const char* device_name;
  director_storage* store;
  DeviceResource* device_resource;
  Device* low_use_drive;            /**< Low use drive candidate */
  int num_writers;                  /**< for selecting low use drive */
  bool try_low_use_drive;           /**< see if low use drive available */
  bool any_drive;                   /**< Accept any drive if set */
  bool PreferMountedVols;           /**< Prefer volumes already mounted */
  bool exact_match;                 /**< Want exact volume */
  bool have_volume;                 /**< Have DIR suggested vol name */
  bool suitable_device;             /**< at least one device is suitable */
  bool autochanger_only;            /**< look at autochangers only */
  bool append;                      /**< set if append device */
  char VolumeName[MAX_NAME_LENGTH]; /**< Vol name suggested by DIR */
};


void InitReservationsLock();
void TermReservationsLock();
void LockReservations();
bool TryReserveAfterUse(JobControlRecord* jcr, bool append);
void UnlockReservations();
void LockVolumes();
void UnlockVolumes();
void LockReadVolumes();
void UnlockReadVolumes();
void UnreserveDevice(DeviceControlRecord* dcr);
int SearchResForDevice(ReserveContext& rctx);
void ClearReserveMessages(JobControlRecord* jcr);

bool use_cmd(JobControlRecord* jcr);

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_RESERVE_H_
