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


DLL_IMP_EXP void init_reservations_lock();
DLL_IMP_EXP void term_reservations_lock();
DLL_IMP_EXP void _lock_reservations(const char *file = "**Unknown**", int line = 0);
DLL_IMP_EXP void _unlock_reservations();
DLL_IMP_EXP void _lock_volumes(const char *file = "**Unknown**", int line = 0);
DLL_IMP_EXP void _unlock_volumes();
DLL_IMP_EXP void _lock_read_volumes(const char *file = "**Unknown**", int line = 0);
DLL_IMP_EXP void _unlock_read_volumes();
DLL_IMP_EXP void unreserve_device(DeviceControlRecord *dcr);
DLL_IMP_EXP bool find_suitable_device_for_job(JobControlRecord *jcr, ReserveContext &rctx);
DLL_IMP_EXP int search_res_for_device(ReserveContext &rctx);
DLL_IMP_EXP void release_reserve_messages(JobControlRecord *jcr);

#ifdef SD_DEBUG_LOCK
DLL_IMP_EXP extern int reservations_lock_count;
DLL_IMP_EXP extern int vol_list_lock_count;
DLL_IMP_EXP extern int read_vol_list_lock_count;

#define lock_reservations() \
         do { Dmsg3(sd_debuglevel, "lock_reservations at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    reservations_lock_count); \
              _lock_reservations(__FILE__, __LINE__); \
              Dmsg0(sd_debuglevel, "lock_reservations: got lock\n"); \
         } while (0)
#define unlock_reservations() \
         do { Dmsg3(sd_debuglevel, "unlock_reservations at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    reservations_lock_count); \
              _unlock_reservations(); } while (0)
#define lock_volumes() \
         do { Dmsg3(sd_debuglevel, "lock_volumes at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    vol_list_lock_count); \
              _lock_volumes(__FILE__, __LINE__); \
              Dmsg0(sd_debuglevel, "lock_volumes: got lock\n"); \
         } while (0)
#define unlock_volumes() \
         do { Dmsg3(sd_debuglevel, "unlock_volumes at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    vol_list_lock_count); \
              _unlock_volumes(); } while (0)
#define lock_read_volumes() \
         do { Dmsg3(sd_debuglevel, "lock_read_volumes at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    read_vol_list_lock_count); \
              _lock_read_volumes(__FILE__, __LINE__); \
              Dmsg0(sd_debuglevel, "lock_read_volumes: got lock\n"); \
         } while (0)
#define unlock_read_volumes() \
         do { Dmsg3(sd_debuglevel, "unlock_read_volumes at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    read_vol_list_lock_count); \
              _unlock_read_volumes(); } while (0)
#else
#define lock_reservations() _lock_reservations(__FILE__, __LINE__)
#define unlock_reservations() _unlock_reservations()
#define lock_volumes() _lock_volumes(__FILE__, __LINE__)
#define unlock_volumes() _unlock_volumes()
#define lock_read_volumes() _lock_read_volumes(__FILE__, __LINE__)
#define unlock_read_volumes() _unlock_read_volumes()
#endif

