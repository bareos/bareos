/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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
#include "stored/autoxflate.h"
#include "stored/device_resource.h"
#include "stored/stored_globals.h"
#include <fmt/format.h>

namespace storagedaemon {

DeviceResource::DeviceResource()
    : BareosResource()
    , media_type(nullptr)
    , archive_device_string(nullptr)
    , device_options(nullptr)
    , diag_device_name(nullptr)
    , changer_name(nullptr)
    , changer_command(nullptr)
    , alert_command(nullptr)
    , spool_directory(nullptr)
    , device_type(DeviceType::B_UNKNOWN_DEV)
    , label_type(B_BAREOS_LABEL)
    , autoselect(true)
    , norewindonclose(true)
    , drive_tapealert_enabled(false)
    , drive_crypto_enabled(false)
    , query_crypto_status(false)
    , collectstats(false)
    , eof_on_error_is_eot(false)
    , drive(0)
    , drive_index(0)
    , cap_bits{0}
    , max_changer_wait(300)
    , max_rewind_wait(300)
    , max_open_wait(300)
    , max_open_vols(1)
    , label_block_size(64512)
    , min_block_size(0)
    , max_block_size(0)
    , max_network_buffer_size(0)
    , max_concurrent_jobs(0)
    , autodeflate_algorithm(0)
    , autodeflate_level(6)
    , autodeflate(AutoXflateMode::IO_DIRECTION_NONE)
    , autoinflate(AutoXflateMode::IO_DIRECTION_NONE)
    , vol_poll_interval(300)
    , max_file_size(1000000000)
    , volume_capacity(0)
    , max_spool_size(0)
    , max_job_spool_size(0)

    , mount_point(nullptr)
    , mount_command(nullptr)
    , unmount_command(nullptr)
    , count(1)
    , multiplied_device_resource(nullptr)

    , dev(nullptr)
    , changer_res(nullptr)

    /* private: */
    , temporarily_swapped_numbered_name(nullptr)
{
  return;
}

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
    , temporarily_swapped_numbered_name(nullptr) /* should not copy */
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
  multiplied_device_resource_base_name
      = other.multiplied_device_resource_base_name;
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
  multiplied_device_resource_base_name
      = rhs.multiplied_device_resource_base_name;
  dev = rhs.dev;
  changer_res = rhs.changer_res;
  temporarily_swapped_numbered_name = rhs.temporarily_swapped_numbered_name;

  return *this;
}


bool DeviceResource::PrintConfig(OutputFormatterResource& send,
                                 const ConfigurationParser&,
                                 bool hide_sensitive_data,
                                 bool verbose)
{
  if (multiplied_device_resource) {
    if (multiplied_device_resource == this) {
      MultipliedDeviceRestoreBaseName();
      BareosResource::PrintConfig(send, *my_config, hide_sensitive_data,
                                  verbose);
      MultipliedDeviceRestoreNumberedName();
    } else {
      /* do not print the multiplied devices */
      return false;
    }
  } else {
    BareosResource::PrintConfig(send, *my_config, hide_sensitive_data, verbose);
  }
  return true;
}


void DeviceResource::MultipliedDeviceRestoreBaseName()
{
  temporarily_swapped_numbered_name = resource_name_;
  resource_name_
      = const_cast<char*>(multiplied_device_resource_base_name.c_str());
}

void DeviceResource::MultipliedDeviceRestoreNumberedName()
{
  /* call MultipliedDeviceRestoreBaseName() before */
  ASSERT(temporarily_swapped_numbered_name);

  resource_name_ = temporarily_swapped_numbered_name;
  temporarily_swapped_numbered_name = nullptr;
}

void DeviceResource::CreateAndAssignSerialNumber(uint16_t number)
{
  if (multiplied_device_resource_base_name.empty()) {
    /* save the original name which is
     * the base name for multiplied devices */
    multiplied_device_resource_base_name = resource_name_;
  }

  std::string tmp_name = multiplied_device_resource_base_name;

  char b[4 + 1];
  ::sprintf(b, "%04d", number < 10000 ? number : 9999);
  tmp_name += b;

  free(resource_name_);
  resource_name_ = strdup(tmp_name.c_str());
}

static void WarnOnNonZeroBlockSize(int max_block_size, std::string_view name)
{
  if (max_block_size > 0) {
    my_config->AddWarning(fmt::format(
        FMT_STRING(
            "Device {:s}: Setting 'Maximum Block Size' is only supported on  "
            "tape devices"),
        name));
  }
}

static void WarnOnZeroMaxConcurrentJobs(int max_concurrent_jobs,
                                        std::string_view name)
{
  if (max_concurrent_jobs == 0) {
    my_config->AddWarning(
        fmt::format(FMT_STRING("Device {:s}: unlimited (0) 'Maximum Concurrent "
                               "Jobs' reduces the restore peformance."),
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
  WarnOnNonZeroBlockSize(resource.max_block_size, resource.resource_name_);
  WarnOnZeroMaxConcurrentJobs(resource.max_concurrent_jobs,
                              resource.resource_name_);
  WarnOnGtOneMaxConcurrentJobs(resource.max_concurrent_jobs,
                               resource.resource_name_);
  return true;
}


bool DeviceResource::Validate()
{
  to_lower(device_type);
  if (device_type == DeviceType::B_TAPE_DEV) {
    return ValidateTapeDevice(*this);
  } else {
    return ValidateGenericDevice(*this);
  }
  return true;
}

} /* namespace storagedaemon  */
