/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2007 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2018 Bareos GmbH & Co. KG

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
/*
 * Kern Sibbald, February MMVI
 */
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
#define BAREOS_STORED_RESERVE_H_ 1

namespace storagedaemon {

class DirectorStorage {
public:
   alist *device;
   bool append;
   char name[MAX_NAME_LENGTH];
   char media_type[MAX_NAME_LENGTH];
   char pool_name[MAX_NAME_LENGTH];
   char pool_type[MAX_NAME_LENGTH];
};

/* Reserve context */
class ReserveContext {
public:
   JobControlRecord *jcr;
   char *device_name;
   DirectorStorage *store;
   DeviceResource *device;
   Device *low_use_drive;             /**< Low use drive candidate */
   int num_writers;                   /**< for selecting low use drive */
   bool try_low_use_drive;            /**< see if low use drive available */
   bool any_drive;                    /**< Accept any drive if set */
   bool PreferMountedVols;            /**< Prefer volumes already mounted */
   bool exact_match;                  /**< Want exact volume */
   bool have_volume;                  /**< Have DIR suggested vol name */
   bool suitable_device;              /**< at least one device is suitable */
   bool autochanger_only;             /**< look at autochangers only */
   bool notify_dir;                   /**< Notify DIR about device */
   bool append;                       /**< set if append device */
   char VolumeName[MAX_NAME_LENGTH];  /**< Vol name suggested by DIR */
};


void InitReservationsLock();
void TermReservationsLock();
void _lockReservations(const char *file = "**Unknown**", int line = 0);
void _unLockReservations();
void _lockVolumes(const char *file = "**Unknown**", int line = 0);
void _unLockVolumes();
void _lockReadVolumes(const char *file = "**Unknown**", int line = 0);
void _unLockReadVolumes();
void UnreserveDevice(DeviceControlRecord *dcr);
bool FindSuitableDeviceForJob(JobControlRecord *jcr, ReserveContext &rctx);
int SearchResForDevice(ReserveContext &rctx);
void ReleaseReserveMessages(JobControlRecord *jcr);

#ifdef SD_DEBUG_LOCK
extern int reservations_lock_count;
extern int vol_list_lock_count;
extern int read_vol_list_lock_count;

#define LockReservations() \
         do { Dmsg3(sd_debuglevel, "LockReservations at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    reservations_lock_count); \
              _lockReservations(__FILE__, __LINE__); \
              Dmsg0(sd_debuglevel, "LockReservations: got lock\n"); \
         } while (0)
#define UnlockReservations() \
         do { Dmsg3(sd_debuglevel, "UnlockReservations at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    reservations_lock_count); \
              _unLockReservations(); } while (0)
#define LockVolumes() \
         do { Dmsg3(sd_debuglevel, "LockVolumes at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    vol_list_lock_count); \
              _lockVolumes(__FILE__, __LINE__); \
              Dmsg0(sd_debuglevel, "LockVolumes: got lock\n"); \
         } while (0)
#define UnlockVolumes() \
         do { Dmsg3(sd_debuglevel, "UnlockVolumes at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    vol_list_lock_count); \
              _unLockVolumes(); } while (0)
#define LockReadVolumes() \
         do { Dmsg3(sd_debuglevel, "LockReadVolumes at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    read_vol_list_lock_count); \
              _lockReadVolumes(__FILE__, __LINE__); \
              Dmsg0(sd_debuglevel, "LockReadVolumes: got lock\n"); \
         } while (0)
#define UnlockReadVolumes() \
         do { Dmsg3(sd_debuglevel, "UnlockReadVolumes at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    read_vol_list_lock_count); \
              _unLockReadVolumes(); } while (0)
#else
#define LockReservations() _lockReservations(__FILE__, __LINE__)
#define UnlockReservations() _unLockReservations()
#define LockVolumes() _lockVolumes(__FILE__, __LINE__)
#define UnlockVolumes() _unLockVolumes()
#define LockReadVolumes() _lockReadVolumes(__FILE__, __LINE__)
#define UnlockReadVolumes() _unLockReadVolumes()
#endif

bool use_cmd(JobControlRecord *jcr);

} /* namespace storagedaemon */

#endif /* BAREOS_STORED_RESERVE_H_ */
