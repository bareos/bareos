/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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

#ifndef BAREOS_STORED_DEVICE_RESOURCE_H_
#define BAREOS_STORED_DEVICE_RESOURCE_H_

#include "stored/dev.h"
#include "stored/io_direction.h"
#include "lib/bareos_resource.h"

#include <string>
#include <memory>

namespace storagedaemon {

class Device;
class AutochangerResource;

class DeviceResource : public BareosResource {
 public:
  char* media_type;            /**< User assigned media type */
  char* archive_device_string; /**< Archive device name */
  char* device_options;        /**< Device specific option string */
  char* diag_device_name;      /**< Diagnostic device name */
  char* changer_name;          /**< Changer device name */
  char* changer_command;       /**< Changer command  -- external program */
  char* alert_command;         /**< Alert command -- external program */
  char* spool_directory;       /**< Spool file directory */
  std::string device_type{DeviceType::B_UNKNOWN_DEV};
  uint32_t label_type{B_BAREOS_LABEL};
  IODirection access_mode{
      IODirection::READ_WRITE}; /**< Allowed access mode(s) for reservation */
  bool autoselect{true};        /**< Automatically select from AutoChanger */
  bool norewindonclose{true};   /**< Don't rewind tape drive on close */
  bool drive_tapealert_enabled{false}; /**< Enable Tape Alert monitoring */
  bool drive_crypto_enabled{false};    /**< Enable hardware crypto */
  bool query_crypto_status{false};     /**< Query device for crypto status */
  bool collectstats{false}; /**< Set if statistics should be collected */
  bool eof_on_error_is_eot{
      false};                    /**< Interpret EOF during read error as EOT */
  drive_number_t drive{0};       /**< Autochanger logical drive number */
  drive_number_t drive_index{0}; /**< Autochanger physical drive index */
  char cap_bits[CAP_BYTES]{0};   /**< Capabilities of this device */
  utime_t max_changer_wait{300}; /**< Changer timeout */
  utime_t max_rewind_wait{300};  /**< Maximum secs to wait for rewind */
  utime_t max_open_wait{300};    /**< Maximum secs to wait for open */
  uint32_t max_open_vols{1};     /**< Maximum simultaneous open volumes */
  uint32_t label_block_size{64512};     /**< block size of the label block*/
  uint32_t min_block_size{0};           /**< Current Minimum block size */
  uint32_t max_block_size{1024 * 1024}; /**< Current Maximum block size */
  uint32_t max_network_buffer_size{0};  /**< Max network buf size */
  uint32_t max_concurrent_jobs{0};   /**< Maximum concurrent jobs this drive */
  uint32_t autodeflate_algorithm{0}; /**< Compression algorithm to use for
                                     compression */
  uint16_t autodeflate_level{6}; /**< Compression level to use for compression
                                 algorithm which uses levels */
  IODirection autodeflate{IODirection::NONE}; /**< auto deflation in this IO
                                                                 direction */
  IODirection autoinflate{IODirection::NONE}; /**< auto inflation in this IO
                                                                 direction */
  utime_t vol_poll_interval{
      300}; /**< Interval between polling volume during mount */
  int64_t max_file_size{1000000000}; /**< Max file size in bytes */
  int64_t volume_capacity{0};        /**< Advisory capacity */
  int64_t max_spool_size{0};         /**< Max spool size for all jobs */
  int64_t max_job_spool_size{0};     /**< Max spool size for any single job */

  char* mount_point;     /**< Mount point for require mount devices */
  char* mount_command;   /**< Mount command */
  char* unmount_command; /**< Unmount command */
  uint32_t count{1};     /**< Total number of multiplied devices */
  DeviceResource* multiplied_device_resource; /**< Copied from this device */

  Device* dev; /* Pointer to physical dev -- set at runtime */
  AutochangerResource* changer_res; /* Pointer to changer res if any */

  DeviceResource() = default;
  virtual ~DeviceResource() = default;
  DeviceResource(const DeviceResource& other);
  DeviceResource& operator=(const DeviceResource& rhs);

  bool PrintConfig(OutputFormatterResource& send,
                   const ConfigurationParser& /* unused */,
                   bool hide_sensitive_data = false,
                   bool verbose = false) override;
  bool Validate() override;

  std::unique_ptr<DeviceResource> CreateCopy(const std::string& copy_name);
};
} /* namespace storagedaemon */

#endif  // BAREOS_STORED_DEVICE_RESOURCE_H_
