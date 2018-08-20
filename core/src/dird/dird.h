/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, December MM
 */
/**
 * @file
 * Includes specific to the Director
 */
#ifndef BAREOS_DIRD_DIRD_H_
#define BAREOS_DIRD_DIRD_H_
#include "lib/connection_pool.h"
#include "lib/runscript.h"
#include "lib/breg.h"
#include "stored/bsr.h"
#include "dird_conf.h"

#define DIRECTOR_DAEMON 1

#include "dir_plugins.h"
#include "cats/cats.h"

#include "jcr.h"
#include "dird/bsr.h"
#include "ua.h"
#include "jobq.h"

namespace directordaemon {

/* Used in ua_prune.c and ua_purge.c */

struct s_count_ctx {
   int count;
};

#define MAX_DEL_LIST_LEN 2000000

struct del_ctx {
   JobId_t *JobId;                    /**< array of JobIds */
   char *PurgedFiles;                 /**< Array of PurgedFile flags */
   int num_ids;                       /**< ids stored */
   int max_ids;                       /**< size of array */
   int num_del;                       /**< number deleted */
   int tot_ids;                       /**< total to process */
};

/* Flags for FindNextVolumeForAppend() */
enum {
  fnv_create_vol = true,
  fnv_no_create_vol = false,
  fnv_prune = true,
  fnv_no_prune = false
};

enum e_enabled_val {
   VOL_NOT_ENABLED = 0,
   VOL_ENABLED = 1,
   VOL_ARCHIVED = 2
};

enum e_prtmsg {
   DISPLAY_ERROR,
   NO_DISPLAY
};

enum e_pool_op {
   POOL_OP_UPDATE,
   POOL_OP_CREATE
};

enum e_move_op {
   VOLUME_IMPORT,
   VOLUME_EXPORT,
   VOLUME_MOVE
};

enum e_slot_flag {
   can_import = 0x01,
   can_export = 0x02,
   by_oper = 0x04,
   by_mte = 0x08
};

typedef enum {
   VOL_LIST_ALL,
   VOL_LIST_PARTIAL
} vol_list_type;

typedef enum {
   slot_type_unknown,             /**< Unknown slot type */
   slot_type_drive,               /**< Drive slot */
   slot_type_normal,              /**< Normal slot */
   slot_type_import,              /**< Import/export slot */
   slot_type_picker               /**< Robotics */
} slot_type;

typedef enum {
   slot_content_unknown,          /**< Slot content is unknown */
   slot_content_empty,            /**< Slot is empty */
   slot_content_full              /**< Slot is full */
} slot_content;

enum s_mapping_type {
   LOGICAL_TO_PHYSICAL,
   PHYSICAL_TO_LOGICAL
};

/*
 * Slot list definition
 */
struct vol_list_t {
   dlink link;                    /**< Link for list */
   slot_number_t Index;           /**< Unique index */
   slot_flags_t Flags;            /**< Slot specific flags see e_slot_flag enum */
   slot_type Type;                /**< See slot_type_* */
   slot_content Content;          /**< See slot_content_* */
   slot_number_t Slot;            /**< Drive number when slot_type_drive or actual slot number */
   slot_number_t Loaded;          /**< Volume loaded in drive when slot_type_drive */
   char *VolName;                 /**< Actual Volume Name */
};

struct changer_vol_list_t {
   int16_t reference_count;       /**< Number of references to this vol_list */
   vol_list_type type;            /**< Type of vol_list see vol_list_type enum */
   utime_t timestamp;             /**< When was this vol_list created */
   dlist *contents;               /**< Contents of autochanger */
};

/*
 * Mapping from logical to physical storage address
 */
struct storage_mapping_t {
   dlink link;                   /**< Link for list */
   slot_type Type;               /**< See slot_type_* */
   slot_number_t Index;          /**< Unique index */
   slot_number_t Slot;           /**< Drive number when slot_type_drive or actual slot number */
};

struct runtime_storage_status_t {
   int32_t NumConcurrentJobs;     /**< Number of concurrent jobs running */
   int32_t NumConcurrentReadJobs; /**< Number of jobs reading */
   drive_number_t drives;         /**< Number of drives in autochanger */
   slot_number_t slots;           /**< Number of slots in autochanger */
   dlist *storage_mappings;       /**< Mappings from logical to physical storage address */
   changer_vol_list_t *vol_list;  /**< Cached content of autochanger */
   pthread_mutex_t changer_lock;  /**< Any access to the autochanger is controlled by this lock */
};

struct runtime_client_status_t {
   int32_t NumConcurrentJobs;     /**< Number of concurrent jobs running */
};

struct runtime_job_status_t {
   int32_t NumConcurrentJobs;     /**< Number of concurrent jobs running */
};

#define INDEX_DRIVE_OFFSET 0
#define INDEX_MAX_DRIVES 100
#define INDEX_SLOT_OFFSET 100

#define FD_VERSION_1 1
#define FD_VERSION_2 2
#define FD_VERSION_3 3
#define FD_VERSION_4 4
#define FD_VERSION_5 5
#define FD_VERSION_51 51
#define FD_VERSION_52 52
#define FD_VERSION_53 53
#define FD_VERSION_54 54

bool DoReloadConfig();

} /* namespace directordaemon */

#endif // BAREOS_DIRD_DIRD_H_
