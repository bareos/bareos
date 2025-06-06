/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
// Kern Sibbald, December MM
/**
 * @file
 * Includes specific to the Director
 */
#ifndef BAREOS_DIRD_DIRD_H_
#define BAREOS_DIRD_DIRD_H_
#include "dird/dird_conf.h"
#include "include/bareos.h"
#include "lib/connection_pool.h"
#include "lib/runscript.h"
#include "stored/bsr.h"
#include "ndmp/smc.h"

#define DIRECTOR_DAEMON 1

#include "cats/cats.h"
#include "dir_plugins.h"

#include "dird/bsr.h"
#include "include/jcr.h"
#include "jobq.h"
#include "ua.h"
template <typename T> class dlist;

namespace directordaemon {

/* Used in ua_prune.c and ua_purge.c */

struct s_count_ctx {
  int count{};
};

#define MAX_DEL_LIST_LEN 2'000'000

/* Flags for FindNextVolumeForAppend() */
enum : bool
{
  fnv_create_vol = true,
  fnv_no_create_vol = false,
  fnv_prune = true,
  fnv_no_prune = false
};

enum e_enabled_val
{
  VOL_NOT_ENABLED = 0,
  VOL_ENABLED = 1,
  VOL_ARCHIVED = 2
};

enum e_prtmsg
{
  DISPLAY_ERROR,
  NO_DISPLAY
};

enum e_pool_op
{
  POOL_OP_UPDATE,
  POOL_OP_CREATE
};

enum e_move_op
{
  VOLUME_IMPORT,
  VOLUME_EXPORT,
  VOLUME_MOVE
};

enum e_slot_flag
{
  can_import = 0x01,
  can_export = 0x02,
  by_oper = 0x04,
  by_mte = 0x08
};

typedef enum
{
  VOL_LIST_ALL,
  VOL_LIST_PARTIAL
} vol_list_type;

enum class slot_type_t : short
{
  kSlotTypeUnknown,
  kSlotTypeDrive,
  kSlotTypeStorage,
  kSlotTypeImport, /**< Import/export slot */
  kSlotTypePicker  /**< Robotics */
};

enum class slot_status_t : short
{
  kSlotStatusUnknown,
  kSlotStatusEmpty,
  kSlotStatusFull
};

enum s_mapping_type
{
  LOGICAL_TO_PHYSICAL,
  PHYSICAL_TO_LOGICAL
};

// Slot list definition
/* clang-format off */
struct vol_list_t {
  dlink<vol_list_t> link;                                            /**< Link for list */
  slot_number_t element_address = kInvalidSlotNumber;    /**< scsi element address */
  slot_flags_t flags = 0;                                /**< Slot specific flags see e_slot_flag enum */
  slot_type_t slot_type = slot_type_t::kSlotTypeUnknown;
  slot_status_t slot_status = slot_status_t::kSlotStatusUnknown;
  slot_number_t bareos_slot_number = kInvalidSlotNumber; /**< Drive number when
                                                              kSlotTypeDrive or actual slot number */
  slot_number_t currently_loaded_slot_number = kInvalidSlotNumber;  /**< Volume loaded in drive when
                                                                         kSlotTypeDrive */
  char* VolName = nullptr; /**< Actual Volume Name */
};
/* clang-format on */

struct changer_vol_list_t {
  int16_t reference_count{};     /**< Number of references to this vol_list */
  vol_list_type type{};          /**< Type of vol_list see vol_list_type enum */
  utime_t timestamp{};           /**< When was this vol_list created */
  dlist<vol_list_t>* contents{}; /**< Contents of autochanger */
};

// Mapping from logical to physical storage address
struct storage_mapping_t {
  dlink<storage_mapping_t> link{};                      /**< Link for list */
  slot_type_t slot_type{slot_type_t::kSlotTypeUnknown}; /**< See slot_type_* */
  slot_number_t element_address{}; /**< scsi element address */
  slot_number_t Slot{};            /**< Drive number when kSlotTypeDrive
                                        or actual slot number */
};

#if HAVE_NDMP
struct ndmp_deviceinfo_t {
  std::string device;
  std::string model;
  JobId_t JobIdUsingDevice{};
};
#endif

struct RuntimeStorageStatus final {
  RuntimeStorageStatus() = default;
  ~RuntimeStorageStatus()
  {
    if (vol_list) {
      if (vol_list->contents) {
        vol_list_t* vl;

        foreach_dlist (vl, vol_list->contents) {
          if (vl->VolName) { free(vl->VolName); }
        }
        vol_list->contents->destroy();
        delete vol_list->contents;
      }
      free(vol_list);
    }
  }
  RuntimeStorageStatus(const RuntimeStorageStatus&) = delete;
  RuntimeStorageStatus& operator=(const RuntimeStorageStatus&) = delete;
  RuntimeStorageStatus(RuntimeStorageStatus&&) = delete;
  RuntimeStorageStatus&& operator=(RuntimeStorageStatus&&) = delete;

  int32_t NumConcurrentJobs{0};      /**< Number of concurrent jobs running */
  int32_t NumConcurrentReadJobs{0};  /**< Number of jobs reading */
  drive_number_t drives{0};          /**< Number of drives in autochanger */
  slot_number_t slots{0};            /**< Number of slots in autochanger */
  std::mutex changer_lock;           /**< Any access to
                                      the autochanger is controlled by this lock */
  unsigned char smc_ident[32] = {0}; /**< smc ident info = changer name */
  changer_vol_list_t* vol_list{nullptr}; /**< Cached content of autochanger */
  std::mutex ndmp_deviceinfo_lock;       /**< Any access to the list devices is
                                            controlled by this lock */
#if HAVE_NDMP
  struct smc_element_address_assignment storage_mapping{};
  std::list<ndmp_deviceinfo_t> ndmp_deviceinfo;
#endif
};

struct RuntimeClientStatus {
  int32_t NumConcurrentJobs{0}; /**< Number of concurrent jobs running */
};

struct RuntimeJobStatus {
  int32_t NumConcurrentJobs{0}; /**< Number of concurrent jobs running */
};

inline constexpr slot_number_t INDEX_DRIVE_OFFSET = 0;
inline constexpr slot_number_t INDEX_MAX_DRIVES = 100;
inline constexpr slot_number_t INDEX_SLOT_OFFSET = 100;

#define FD_VERSION_1 1
#define FD_VERSION_2 2
#define FD_VERSION_3 3
#define FD_VERSION_4 4
#define FD_VERSION_5 5
#define FD_VERSION_51 51
#define FD_VERSION_52 52
#define FD_VERSION_53 53
#define FD_VERSION_54 54

} /* namespace directordaemon */

#endif  // BAREOS_DIRD_DIRD_H_
