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

#include "lib/parse_conf.h"
#include "stored/device_resource.h"
#include "stored/stored_globals.h"
#include <fmt/format.h>

namespace storagedaemon {

DeviceResource::DeviceResource(const DeviceResource& other)
    : BareosResource(other)
    , media_type(nullptr)
    , archive_device_string(nullptr)
    , device_options(nullptr)
    , diag_device_name(nullptr)
    , changer_name(nullptr)
    , changer_command(nullptr)
    , alert_command(nullptr)
    , spool_directory(nullptr)
    , mount_point(nullptr)
    , mount_command(nullptr)
    , unmount_command(nullptr)
{
  if (other.media_type) { media_type = strdup(other.media_type); }
  if (other.archive_device_string) {
    archive_device_string = strdup(other.archive_device_string);
  }
  if (other.device_options) { device_options = strdup(other.device_options); }
  if (other.diag_device_name) {
    diag_device_name = strdup(other.diag_device_name);
  }
  if (other.changer_name) { changer_name = strdup(other.changer_name); }
  if (other.changer_command) {
    changer_command = strdup(other.changer_command);
  }
  if (other.alert_command) { alert_command = strdup(other.alert_command); }
  if (other.spool_directory) {
    spool_directory = strdup(other.spool_directory);
  }
  device_type = other.device_type;
  label_type = other.label_type;
  access_mode = other.access_mode;
  autoselect = other.autoselect;
  norewindonclose = other.norewindonclose;
  drive_tapealert_enabled = other.drive_tapealert_enabled;
  drive_crypto_enabled = other.drive_crypto_enabled;
  query_crypto_status = other.query_crypto_status;
  collectstats = other.collectstats;
  eof_on_error_is_eot = other.eof_on_error_is_eot;
  drive = other.drive;
  drive_index = other.drive_index;
  memcpy(cap_bits, other.cap_bits, CAP_BYTES);
  max_changer_wait = other.max_changer_wait;
  max_rewind_wait = other.max_rewind_wait;
  max_open_wait = other.max_open_wait;
  max_open_vols = other.max_open_vols;
  label_block_size = other.label_block_size;
  min_block_size = other.min_block_size;
  max_block_size = other.max_block_size;
  max_network_buffer_size = other.max_network_buffer_size;
  max_concurrent_jobs = other.max_concurrent_jobs;
  autodeflate_algorithm = other.autodeflate_algorithm;
  autodeflate_level = other.autodeflate_level;
  autodeflate = other.autodeflate;
  autoinflate = other.autoinflate;
  vol_poll_interval = other.vol_poll_interval;
  max_file_size = other.max_file_size;
  volume_capacity = other.volume_capacity;
  max_spool_size = other.max_spool_size;
  max_job_spool_size = other.max_job_spool_size;

  if (other.mount_point) { mount_point = strdup(other.mount_point); }
  if (other.mount_command) { mount_command = strdup(other.mount_command); }
  if (other.unmount_command) {
    unmount_command = strdup(other.unmount_command);
  }
  count = other.count;
  multiplied_device_resource = other.multiplied_device_resource;
  dev = other.dev;
  changer_res = other.changer_res;
}

DeviceResource& DeviceResource::operator=(const DeviceResource& rhs)
{
  BareosResource::operator=(rhs);
  media_type = rhs.media_type;
  archive_device_string = rhs.archive_device_string;
  device_options = rhs.device_options;
  diag_device_name = rhs.diag_device_name;
  changer_name = rhs.changer_name;
  changer_command = rhs.changer_command;
  alert_command = rhs.alert_command;
  spool_directory = rhs.spool_directory;
  device_type = rhs.device_type;
  label_type = rhs.label_type;
  access_mode = rhs.access_mode;
  autoselect = rhs.autoselect;
  norewindonclose = rhs.norewindonclose;
  drive_tapealert_enabled = rhs.drive_tapealert_enabled;
  drive_crypto_enabled = rhs.drive_crypto_enabled;
  query_crypto_status = rhs.query_crypto_status;
  collectstats = rhs.collectstats;
  eof_on_error_is_eot = rhs.eof_on_error_is_eot;
  drive = rhs.drive;
  drive_index = rhs.drive_index;
  memcpy(cap_bits, rhs.cap_bits, CAP_BYTES);
  max_changer_wait = rhs.max_changer_wait;
  max_rewind_wait = rhs.max_rewind_wait;
  max_open_wait = rhs.max_open_wait;
  max_open_vols = rhs.max_open_vols;
  label_block_size = rhs.label_block_size;
  min_block_size = rhs.min_block_size;
  max_block_size = rhs.max_block_size;
  max_network_buffer_size = rhs.max_network_buffer_size;
  max_concurrent_jobs = rhs.max_concurrent_jobs;
  autodeflate_algorithm = rhs.autodeflate_algorithm;
  autodeflate_level = rhs.autodeflate_level;
  autodeflate = rhs.autodeflate;
  autoinflate = rhs.autoinflate;
  vol_poll_interval = rhs.vol_poll_interval;
  max_file_size = rhs.max_file_size;
  volume_capacity = rhs.volume_capacity;
  max_spool_size = rhs.max_spool_size;
  max_job_spool_size = rhs.max_job_spool_size;

  mount_point = rhs.mount_point;
  mount_command = rhs.mount_command;
  unmount_command = rhs.unmount_command;
  count = rhs.count;
  multiplied_device_resource = rhs.multiplied_device_resource;
  dev = rhs.dev;
  changer_res = rhs.changer_res;

  return *this;
}


bool DeviceResource::PrintConfig(OutputFormatterResource& send,
                                 const ConfigurationParser&,
                                 bool hide_sensitive_data,
                                 bool verbose)
{
  if (multiplied_device_resource) { return false; }
  BareosResource::PrintConfig(send, *my_config, hide_sensitive_data, verbose);
  return true;
}

std::unique_ptr<DeviceResource> DeviceResource::CreateCopy(
    const std::string& copy_name)
{
  auto device = std::make_unique<DeviceResource>(*this);
  if (device->resource_name_) { free(device->resource_name_); }
  device->resource_name_ = strdup(copy_name.c_str());
  device->multiplied_device_resource = this;
  device->count = 0;
  return device;
}

static void WarnOnSetMaxBlockSize(const DeviceResource& resource)
{
  if (resource.IsMemberPresent("MaximumBlockSize")) {
    my_config->AddWarning(fmt::format(
        FMT_STRING(
            "Device {:s}: Setting 'Maximum Block Size' is only supported on "
            "tape devices"),
        resource.resource_name_));
  }
}

static void WarnOnZeroMaxConcurrentJobs(int max_concurrent_jobs,
                                        std::string_view name)
{
  if (max_concurrent_jobs == 0) {
    my_config->AddWarning(
        fmt::format(FMT_STRING("Device {:s}: unlimited (0) 'Maximum Concurrent "
                               "Jobs' reduces the restore performance."),
                    name));
  }
}

static void WarnOnGtOneMaxConcurrentJobs(int max_concurrent_jobs,
                                         std::string_view name)
{
  if (max_concurrent_jobs > 1) {
    my_config->AddWarning(fmt::format(
        FMT_STRING(
            "Device {:s}: setting 'Maximum Concurrent Jobs' on "
            "device that are not of type tape to a value higher than 1 is not "
            "recommended as it will reduce the restore performance."),
        name));
  }
}

static bool ValidateTapeDevice(const DeviceResource& resource)
{
  ASSERT(resource.device_type == DeviceType::B_TAPE_DEV);

  WarnOnZeroMaxConcurrentJobs(resource.max_concurrent_jobs,
                              resource.resource_name_);

  return true;
}

static bool ValidateGenericDevice(const DeviceResource& resource)
{
  WarnOnSetMaxBlockSize(resource);
  WarnOnZeroMaxConcurrentJobs(resource.max_concurrent_jobs,
                              resource.resource_name_);
  WarnOnGtOneMaxConcurrentJobs(resource.max_concurrent_jobs,
                               resource.resource_name_);
  return true;
}


bool DeviceResource::Validate()
{
  if (IsMemberPresent("AutoDeflate")
      && !IsMemberPresent("AutoDeflateAlgorithm")) {
    Jmsg(nullptr, M_ERROR, 0,
         T_("Device %s: If 'AutoDeflate' is set, then 'AutoDeflateAlgorithm' "
            "also has to be set.\n"),
         resource_name_);

    return false;
  }

  to_lower(device_type);
  if (device_type == DeviceType::B_TAPE_DEV) {
    return ValidateTapeDevice(*this);
  }

  return ValidateGenericDevice(*this);
}

} /* namespace storagedaemon  */
