/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

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

/**
 * @file bareos storage daemon configuration definitions
 *
 */
#ifndef BAREOS_STORED_STORED_CONF_H_
#define BAREOS_STORED_STORED_CONF_H_

#include "stored/dev.h"
#include "stored/autochanger_resource.h"
#include "stored/device_resource.h"
#include "lib/messages_resource.h"
#include "lib/tls_conf.h"

template <typename T> class alist;
template <typename T> class dlist;

class IPADDR;

namespace storagedaemon {

static const std::string default_config_filename("bareos-sd.conf");

// Resource codes -- they must be sequential for indexing
enum
{
  R_DIRECTOR = 0,
  R_NDMP,
  R_STORAGE,
  R_DEVICE,
  R_MSGS,
  R_AUTOCHANGER,
  R_JOB,           // needed for Job name conversion
  R_NUM,           // number of entires
  R_CLIENT = 0xff  // dummy for bsock printing
};

enum
{
  R_NAME = 0,
  R_ADDRESS,
  R_PASSWORD,
  R_TYPE,
  R_BACKUP
};

/* Definition of the contents of each Resource */
class DirectorResource
    : public BareosResource
    , public TlsResource {
 public:
  char* address = nullptr; /**< Director IP address or zero */
  bool monitor = false; /**< Have only access to status and .status functions */
  uint64_t max_bandwidth_per_job = 0; /**< Per director bandwidth limitation  */
  s_password keyencrkey;              /**< Key Encryption Key */

  DirectorResource() = default;
  virtual ~DirectorResource() = default;
};

class NdmpResource : public BareosResource {
 public:
  uint32_t AuthType = 0; /**< Authentication Type to use */
  uint32_t LogLevel = 0; /**< Log level to use for logging NDMP protocol msgs */
  char* username = nullptr; /**< NDMP username */
  s_password password;      /**< NDMP password */

  NdmpResource() = default;
  virtual ~NdmpResource() = default;
};

/* Storage daemon "global" definitions */
class StorageResource
    : public BareosResource
    , public TlsResource {
 public:
  dlist<IPADDR>* SDaddrs = nullptr;
  dlist<IPADDR>* SDsrc_addr
      = nullptr; /**< Address to source connections from */
  dlist<IPADDR>* NDMPaddrs = nullptr;
  char* working_directory = nullptr; /**< Working directory for checkpoints */
  char* pid_directory = nullptr;
  char* plugin_directory = nullptr; /**< Plugin directory */
  alist<const char*>* plugin_names = nullptr;
  char* scripts_directory = nullptr;
  std::vector<std::string> backend_directories;
  uint32_t MaxConcurrentJobs = 0;      /**< Maximum concurrent jobs to run */
  uint32_t MaxConnections = 0;         /**< Maximum connections to allow */
  uint32_t ndmploglevel = 0;           /**< Initial NDMP log level */
  uint32_t jcr_watchdog_time = 0;      /**< Absolute time after which a Job gets
                                      terminated regardless of its progress */
  uint32_t stats_collect_interval = 0; /**<  in seconds */
  MessagesResource* messages = nullptr; /**< Daemon message handler */
  utime_t SDConnectTimeout = {0};       /**< Timeout in seconds */
  utime_t FDConnectTimeout = {0};       /**< Timeout in seconds */
  utime_t heartbeat_interval = {0};     /**< Interval to send hb to FD */
  utime_t client_wait = {0};            /**< Time to wait for FD to connect */
  uint32_t max_network_buffer_size = 0; /**< Max network buf size */
  bool autoxflateonreplication
      = false;             /**< Perform autoxflation when replicating data
                            */
  bool compatible = false; /**< Write compatible format */
  bool allow_bw_bursting = false; /**< Allow bursting with bandwidth limiting */
  bool ndmp_enable = false;       /**< Enable NDMP protocol listener */
  bool ndmp_snooping = false;     /**< Enable NDMP protocol snooping */
  bool collect_dev_stats = false; /**< Collect Device Statistics */
  bool collect_job_stats = false; /**< Collect Job Statistics */
  bool device_reserve_by_mediatype = false; /**< Allow device reservation based
                                       on a matching mediatype */
  bool filedevice_concurrent_read = false;  /**< Allow filedevices to be read
                                       concurrently */
  char* verid = nullptr; /**< Custom Id to print in version command */
  char* secure_erase_cmdline = nullptr; /**< Cmdline to execute to perform
                                 secure erase of file */
  char* log_timestamp_format = nullptr; /**< Timestamp format to use in generic
                                 logging messages */
  uint64_t max_bandwidth_per_job = 0;   /**< Bandwidth limitation (global) */

  StorageResource() = default;
  virtual ~StorageResource() = default;
};

ConfigurationParser* InitSdConfig(const char* configfile, int exit_code);
bool ParseSdConfig(const char* configfile, int exit_code);
bool PrintConfigSchemaJson(PoolMem& buffer);

} /* namespace storagedaemon */
#endif  // BAREOS_STORED_STORED_CONF_H_
