/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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
#ifndef BAREOS_SRC_STORED_VOLUME_CATALOG_INFO_H_
#define BAREOS_SRC_STORED_VOLUME_CATALOG_INFO_H_

#include "include/bareos.h"

namespace storagedaemon {

/* clang-format off */
struct VolumeCatalogInfo {
  uint32_t VolCatJobs{0};          /**< number of jobs on this Volume */
  uint32_t VolCatFiles{0};         /**< Number of files */
  uint32_t VolCatBlocks{0};        /**< Number of blocks */
  uint64_t VolCatBytes{0};         /**< Number of bytes written */
  uint32_t VolCatMounts{0};        /**< Number of mounts this volume */
  uint32_t VolCatErrors{0};        /**< Number of errors this volume */
  uint32_t VolCatWrites{0};        /**< Number of writes this volume */
  uint32_t VolCatReads{0};         /**< Number of reads this volume */
  uint64_t VolCatRBytes{0};        /**< Number of bytes read */
  uint32_t VolCatRecycles{0};      /**< Number of recycles this volume */
  uint32_t EndFile{0};             /**< Last file number */
  uint32_t EndBlock{0};            /**< Last block number */
  int32_t LabelType{0};            /**< Bareos/ANSI/IBM */
  int32_t Slot{0};                 /**< >0=Slot loaded, 0=nothing, -1=unknown */
  uint32_t VolCatMaxJobs{0};       /**< Maximum Jobs to write to volume */
  uint32_t VolCatMaxFiles{0};      /**< Maximum files to write to volume */
  uint64_t VolCatMaxBytes{0};      /**< Max bytes to write to volume */
  uint64_t VolCatCapacityBytes{0}; /**< capacity estimate */
  btime_t VolReadTime{0};          /**< time spent reading */
  btime_t VolWriteTime{0};         /**< time spent writing this Volume */
  int64_t VolMediaId{0};           /**< MediaId */
  utime_t VolFirstWritten{0};      /**< Time of first write */
  utime_t VolLastWritten{0};       /**< Time of last write */
  bool InChanger{false};           /**< Set if vol in current magazine */
  bool is_valid{false};            /**< set if this data is valid */
  char VolCatStatus[20]{0};        /**< Volume status */
  char VolCatName[MAX_NAME_LENGTH]{0}; /**< Desired volume to mount */
  char VolEncrKey[MAX_NAME_LENGTH]{0}; /**< Encryption Key needed to read the media
                                */
  uint32_t VolMinBlocksize{0}; /**< Volume Minimum Blocksize */
  uint32_t VolMaxBlocksize{0}; /**< Volume Maximum Blocksize */
};
/* clang-format on */

}  // namespace storagedaemon
#endif  // BAREOS_SRC_STORED_VOLUME_CATALOG_INFO_H_
