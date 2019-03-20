/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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

/**
 * @file bareos storage daemon configuration definitions
 *
 */
#ifndef BAREOS_STORED_STORED_CONF_H_
#define BAREOS_STORED_STORED_CONF_H_ 1

#include "stored/dev.h"
#include "stored/autochanger_resource.h"
#include "stored/device_resource.h"
#include "lib/messages_resource.h"
#include "lib/tls_conf.h"

class alist;
class dlist;

namespace storagedaemon {

static const std::string default_config_filename("bareos-sd.conf");

/*
 * Resource codes -- they must be sequential for indexing
 */
enum
{
  R_DIRECTOR = 3001,
  R_NDMP,
  R_STORAGE,
  R_DEVICE,
  R_MSGS,
  R_AUTOCHANGER,
  R_JOB, /* needed for Job name conversion */
  R_FIRST = R_DIRECTOR,
  R_LAST = R_JOB /* keep this updated */
};

enum
{
  R_NAME = 3020,
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
  char* address; /**< Director IP address or zero */
  bool monitor;  /**< Have only access to status and .status functions */
  uint64_t max_bandwidth_per_job; /**< Bandwidth limitation (per director) */
  s_password keyencrkey;          /**< Key Encryption Key */
};

class NdmpResource {
 public:
  CommonResourceHeader hdr;

  uint32_t AuthType;   /**< Authentication Type to use */
  uint32_t LogLevel;   /**< Log level to use for logging NDMP protocol msgs */
  char* username;      /**< NDMP username */
  s_password password; /**< NDMP password */
};

/* Storage daemon "global" definitions */
class StorageResource
    : public BareosResource
    , public TlsResource {
 public:
  dlist* SDaddrs;
  dlist* SDsrc_addr; /**< Address to source connections from */
  dlist* NDMPaddrs;
  char* working_directory; /**< Working directory for checkpoints */
  char* pid_directory;
  char* subsys_directory;
  char* plugin_directory; /**< Plugin directory */
  alist* plugin_names;
  char* scripts_directory;
  alist* backend_directories; /**< Backend Directories */
  uint32_t MaxConcurrentJobs; /**< Maximum concurrent jobs to run */
  uint32_t MaxConnections;    /**< Maximum connections to allow */
  uint32_t ndmploglevel;      /**< Initial NDMP log level */
  uint32_t jcr_watchdog_time; /**< Absolute time after which a Job gets
                                 terminated regardless of its progress */
  uint32_t
      stats_collect_interval; /**< Statistics collect interval in seconds */
  MessagesResource* messages; /**< Daemon message handler */
  utime_t SDConnectTimeout;   /**< Timeout in seconds */
  utime_t FDConnectTimeout;   /**< Timeout in seconds */
  utime_t heartbeat_interval; /**< Interval to send hb to FD */
  utime_t client_wait;        /**< Time to wait for FD to connect */
  uint32_t max_network_buffer_size; /**< Max network buf size */
  bool autoxflateonreplication; /**< Perform autoxflation when replicating data
                                 */
  bool compatible;              /**< Write compatible format */
  bool allow_bw_bursting;       /**< Allow bursting with bandwidth limiting */
  bool ndmp_enable;             /**< Enable NDMP protocol listener */
  bool ndmp_snooping;           /**< Enable NDMP protocol snooping */
  bool nokeepalive;             /**< Don't use SO_KEEPALIVE on sockets */
  bool collect_dev_stats;       /**< Collect Device Statistics */
  bool collect_job_stats;       /**< Collect Job Statistics */
  bool device_reserve_by_mediatype; /**< Allow device reservation based on a
                                       matching mediatype */
  bool filedevice_concurrent_read;  /**< Allow filedevices to be read
                                       concurrently */
  char* verid;                /**< Custom Id to print in version command */
  char* secure_erase_cmdline; /**< Cmdline to execute to perform secure erase of
                                 file */
  char* log_timestamp_format; /**< Timestamp format to use in generic logging
                                 messages */
  uint64_t max_bandwidth_per_job; /**< Bandwidth limitation (global) */
};

union UnionOfResources {
  DirectorResource res_dir;
  NdmpResource res_ndmp;
  StorageResource res_store;
  DeviceResource res_dev;
  MessagesResource res_msgs;
  AutochangerResource res_changer;
  CommonResourceHeader hdr;
};

ConfigurationParser* InitSdConfig(const char* configfile, int exit_code);
bool PrintConfigSchemaJson(PoolMem& buffer);

} /* namespace storagedaemon */
#endif /* BAREOS_STORED_STORED_CONF_H_ */
