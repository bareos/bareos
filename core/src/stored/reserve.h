/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2007 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2022 Bareos GmbH & Co. KG

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

template <typename T> class alist;

namespace storagedaemon {

class DirectorStorage {
 public:
  alist<const char*>* device;
  bool append;
  char name[MAX_NAME_LENGTH];
  char media_type[MAX_NAME_LENGTH];
  char pool_name[MAX_NAME_LENGTH];
  char pool_type[MAX_NAME_LENGTH];
};

/* Reserve context */
class ReserveContext {
 public:
  JobControlRecord* jcr;
  const char* device_name;
  DirectorStorage* store;
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
  bool notify_dir;                  /**< Notify DIR about device */
  bool append;                      /**< set if append device */
  char VolumeName[MAX_NAME_LENGTH]; /**< Vol name suggested by DIR */
};


void InitReservationsLock();
void TermReservationsLock();
void _lockReservations(const char* = "**Unknown**", int = 0);
void _unLockReservations();
void _lockVolumes(const char* = "**Unknown**", int = 0);
void _unLockVolumes();
void _lockReadVolumes(const char* file = "**Unknown**", int line = 0);
void _unLockReadVolumes();
void UnreserveDevice(DeviceControlRecord* dcr);
bool FindSuitableDeviceForJob(JobControlRecord* jcr, ReserveContext& rctx);
int SearchResForDevice(ReserveContext& rctx);
void ReleaseReserveMessages(JobControlRecord* jcr);

#define LockReservations() _lockReservations(__FILE__, __LINE__)
#define UnlockReservations() _unLockReservations()
#define LockVolumes() _lockVolumes(__FILE__, __LINE__)
#define UnlockVolumes() _unLockVolumes()
#define LockReadVolumes() _lockReadVolumes(__FILE__, __LINE__)
#define UnlockReadVolumes() _unLockReadVolumes()

bool use_cmd(JobControlRecord* jcr);

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_RESERVE_H_
