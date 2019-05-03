/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_DEVICE_RESOURCE_H_
#define BAREOS_STORED_DEVICE_RESOURCE_H_

#include "stored/dev.h"
#include "lib/bareos_resource.h"

namespace storagedaemon {

class Device;
class AutochangerResource;

class DeviceResource : public BareosResource {
 public:
  char* media_type;             /**< User assigned media type */
  char* device_name;            /**< Archive device name */
  char* device_options;         /**< Device specific option string */
  char* diag_device_name;       /**< Diagnostic device name */
  char* changer_name;           /**< Changer device name */
  char* changer_command;        /**< Changer command  -- external program */
  char* alert_command;          /**< Alert command -- external program */
  char* spool_directory;        /**< Spool file directory */
  uint32_t dev_type;            /**< device type */
  uint32_t label_type;          /**< label type */
  bool autoselect;              /**< Automatically select from AutoChanger */
  bool norewindonclose;         /**< Don't rewind tape drive on close */
  bool drive_tapealert_enabled; /**< Enable Tape Alert monitoring */
  bool drive_crypto_enabled;    /**< Enable hardware crypto */
  bool query_crypto_status;     /**< Query device for crypto status */
  bool collectstats;            /**< Set if statistics should be collected */
  bool eof_on_error_is_eot;     /**< Interpret EOF during read error as EOT */
  drive_number_t drive;         /**< Autochanger logical drive number */
  drive_number_t drive_index;   /**< Autochanger physical drive index */
  char cap_bits[CAP_BYTES];     /**< Capabilities of this device */
  utime_t max_changer_wait;     /**< Changer timeout */
  utime_t max_rewind_wait;      /**< Maximum secs to wait for rewind */
  utime_t max_open_wait;        /**< Maximum secs to wait for open */
  uint32_t max_open_vols;       /**< Maximum simultaneous open volumes */
  uint32_t label_block_size;    /**< block size of the label block*/
  uint32_t min_block_size;      /**< Current Minimum block size */
  uint32_t max_block_size;      /**< Current Maximum block size */
  uint32_t max_network_buffer_size; /**< Max network buf size */
  uint32_t max_concurrent_jobs;     /**< Maximum concurrent jobs this drive */
  uint32_t autodeflate_algorithm;   /**< Compression algorithm to use for
                                       compression */
  uint16_t autodeflate_level; /**< Compression level to use for compression
                                 algorithm which uses levels */
  uint16_t autodeflate; /**< Perform auto deflation in this IO direction */
  uint16_t autoinflate; /**< Perform auto inflation in this IO direction */
  utime_t
      vol_poll_interval;   /**< Interval between polling volume during mount */
  int64_t max_volume_size; /**< Max bytes to put on one volume */
  int64_t max_file_size;   /**< Max file size in bytes */
  int64_t volume_capacity; /**< Advisory capacity */
  int64_t max_spool_size;  /**< Max spool size for all jobs */
  int64_t max_job_spool_size; /**< Max spool size for any single job */

  int64_t max_part_size;    /**< Max part size */
  char* mount_point;        /**< Mount point for require mount devices */
  char* mount_command;      /**< Mount command */
  char* unmount_command;    /**< Unmount command */
  char* write_part_command; /**< Write part command */
  char* free_space_command; /**< Free space command */
  uint32_t count;           /**< Total number of multiplied devices */
  DeviceResource* multiplied_device_resource; /**< Copied from this device */

  Device* dev; /* Pointer to physical dev -- set at runtime */
  AutochangerResource* changer_res; /* Pointer to changer res if any */

  DeviceResource();
  virtual ~DeviceResource() = default;
  DeviceResource(const DeviceResource& other);
  DeviceResource& operator=(const DeviceResource& rhs);
  bool PrintConfig(PoolMem& buf,
                   const ConfigurationParser& /* unused */,
                   bool hide_sensitive_data = false,
                   bool verbose = false) override;
  void CreateAndAssignSerialNumber(uint16_t number);
  void MultipliedDeviceRestoreBaseName();
  void MultipliedDeviceRestoreNumberedName();

 private:
  std::string multiplied_device_resource_base_name; /** < base name without
                                                     appended numbers */
  char* temporarily_swapped_numbered_name;
};
} /* namespace storagedaemon */

#endif /* BAREOS_STORED_DEVICE_RESOURCE_H_ */
