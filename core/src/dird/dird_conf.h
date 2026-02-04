/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
// Kern Sibbald, Feb MM
/**
 * @file
 * Director specific configuration and defines
 */
#ifndef BAREOS_DIRD_DIRD_CONF_H_
#define BAREOS_DIRD_DIRD_CONF_H_

#include <openssl/md5.h>
#include <memory>
#include <optional>

#include "dird/client_connection_handshake_mode.h"
#include "dird/date_time_mask.h"
#include "lib/alist.h"
#include "lib/messages_resource.h"
#include "lib/resource_item.h"
#include "lib/tls_conf.h"

template <typename T> class dlist;
struct json_t;
class RunScript;
class IPADDR;

namespace directordaemon {

static std::string default_config_filename("bareos-dir.conf");

// Resource codes -- they must be sequential for indexing
enum
{
  R_DIRECTOR = 0,
  R_CLIENT,
  R_JOBDEFS,
  R_JOB,
  R_STORAGE,
  R_CATALOG,
  R_SCHEDULE,
  R_FILESET,
  R_POOL,
  R_MSGS,
  R_COUNTER,
  R_PROFILE,
  R_CONSOLE,
  R_USER,
  R_NUM /* Number of entries */
};

// Job Level keyword structure
struct s_jl {
  const char* level_name; /* level keyword */
  uint32_t level;         /* level */
  int32_t job_type;       /* JobType permitting this level */
};

// Job Type keyword structure
struct s_jt {
  const char* type_name;
  uint32_t job_type;
};

/**
 * Definition of the contents of each Resource
 * Needed for forward references
 */
class ScheduleResource;
class ClientResource;
class FilesetResource;
class PoolResource;
class RunResource;
class DeviceResource;

// Print configuration file schema in json format
bool PrintConfigSchemaJson(PoolMem& buff);

//   Director Resource
class DirectorResource
    : public BareosResource
    , public TlsResource {
 public:
  DirectorResource() = default;
  virtual ~DirectorResource() = default;
  dlist<IPADDR>* DIRaddrs = nullptr;
  dlist<IPADDR>* DIRsrc_addr = nullptr; /* Address to source connections from */
  char* query_file = nullptr;           /* SQL query file */
  char* working_directory = nullptr;    /* WorkingDirectory */
  char* scripts_directory = nullptr;    /* ScriptsDirectory */
  char* plugin_directory = nullptr;     /* Plugin Directory */
  alist<const char*>* plugin_names = nullptr; /* Plugin names to load */
  MessagesResource* messages = nullptr;       /* Daemon message handler */
  uint32_t MaxConcurrentJobs = 0; /* Max concurrent jobs for whole director */
  uint32_t MaxConsoleConnections = 0; /* Max concurrent console connections */
  utime_t FDConnectTimeout = {0};     /* Timeout for connect in seconds */
  utime_t SDConnectTimeout = {0};     /* Timeout for connect in seconds */
  utime_t heartbeat_interval = {0};   /* Interval to send heartbeats */
  utime_t stats_retention = {0}; /* Statistics retention period in seconds */
  bool ndmp_snooping = false;    /* NDMP Protocol specific snooping enabled */
  bool ndmp_fhinfo_set_zero_for_invalid_u_quad
      = false;  // Workaround for Isilon 9.1.0.0 not accepting -1 as value for
                // FhInfo (which is the tape offset)
  bool auditing = false; /* Auditing enabled */
  alist<const char*>* audit_events
      = nullptr;                  /* Specific audit events to enable */
  uint32_t ndmp_loglevel = 0;     /* NDMP Protocol specific loglevel to use */
  uint32_t subscriptions = 0;     /* Number of subscribtions available */
  uint32_t jcr_watchdog_time = 0; /* Absolute time after which a Job gets
                                 terminated  regardless of its progress */
  uint32_t stats_collect_interval
      = 0;               /* Statistics collect interval in seconds */
  char* verid = nullptr; /* Custom Id to print in version command */
  char* secure_erase_cmdline = nullptr; /* Cmdline to execute to perform secure
                                 erase of file */
  char* log_timestamp_format = nullptr; /* Timestamp format to use in generic
                                 logging messages */
  s_password keyencrkey;                /* Key Encryption Key */

  bool enable_ktls{false};
};

// Console ACL positions
enum
{
  Job_ACL = 0,
  Client_ACL,
  Storage_ACL,
  Schedule_ACL,
  Pool_ACL,
  Command_ACL,
  FileSet_ACL,
  Catalog_ACL,
  Where_ACL,
  PluginOptions_ACL,
  Num_ACL /**< keep last */
};

// Profile Resource
class ProfileResource : public BareosResource {
 public:
  ProfileResource() = default;
  virtual ~ProfileResource() = default;

  alist<const char*>* ACL_lists[Num_ACL] = {0}; /**< Pointers to ACLs */
};

struct UserAcl {
  BareosResource* corresponding_resource = nullptr;
  alist<const char*>* ACL_lists[Num_ACL] = {0}; /**< Pointers to ACLs */
  alist<ProfileResource*>* profiles
      = nullptr; /**< Pointers to profile resources */
  bool HasAcl()
  {
    for (int i = Job_ACL; i < Num_ACL; ++i) {
      if (ACL_lists[i]) { return true; }
    }
    return false;
  }
};

// Console Resource
class ConsoleResource
    : public BareosResource
    , public TlsResource {
 public:
  ConsoleResource() = default;
  virtual ~ConsoleResource() = default;
  UserAcl user_acl;
  bool use_pam_authentication_ = false; /**< PAM Console */
};

class UserResource : public BareosResource {
 public:
  UserResource() = default;
  virtual ~UserResource() = default;
  UserAcl user_acl;
};

// Catalog Resource
class CatalogResource : public BareosResource {
 public:
  CatalogResource() = default;
  virtual ~CatalogResource() = default;
  virtual bool Validate() override;

  uint32_t db_port = 0;       /**< Port */
  char* db_address = nullptr; /**< Hostname for remote access */
  char* db_socket = nullptr;  /**< Socket for local access */
  s_password db_password;
  char* db_user = nullptr;
  char* db_name = nullptr;
  const char* db_driver = "postgresql"; /**< Select appropriate driver */
  uint32_t mult_db_connections = 0; /**< Set if multiple connections wanted */
  bool disable_batch_insert
      = false;                /**< Set if batch inserts should be disabled */
  bool try_reconnect = true;  /**< Try to reconnect a database connection when
                          it is dropped */
  bool exit_on_fatal = false; /**< Make any fatal error in the connection to the
                         database exit the program */
  uint32_t pooling_min_connections
      = 0; /**< When using sql pooling start with this
          number of connections to the database */
  uint32_t pooling_max_connections = 0; /**< When using sql pooling maximum
                                       number of connections to the database */
  uint32_t pooling_increment_connections = 0; /**< When using sql pooling
                                             increment the pool with this amount
                                             when its to small */
  uint32_t pooling_idle_timeout
      = 0; /**< When using sql pooling set this to the number
          of seconds to keep an idle connection */
  uint32_t pooling_validate_timeout = 0; /**< When using sql pooling set this to
                                        the number of seconds after a idle
                                        connection should be validated */

  /**< Methods */
  char* display(POOLMEM* dst); /**< Get catalog information */
};

// Forward referenced structures
struct RuntimeClientStatus;
struct RuntimeStorageStatus;
struct RuntimeJobStatus;

// Client Resource
class ClientResource
    : public BareosResource
    , public TlsResource {
 public:
  ClientResource() = default;
  virtual ~ClientResource() = default;

  uint32_t Protocol = 0;       /* Protocol to use to connect */
  uint32_t AuthType = 0;       /* Authentication Type to use for protocol */
  uint32_t ndmp_loglevel = 0;  /* NDMP Protocol specific loglevel to use */
  uint32_t ndmp_blocksize = 0; /* NDMP Protocol specific blocksize to use */
  uint32_t FDport = 0;         /* Where File daemon listens */
  uint64_t SoftQuota = 0;      /* Soft Quota permitted in bytes */
  uint64_t HardQuota = 0;      /* Maximum permitted quota in bytes */
  uint64_t GraceTime = 0;      /* Time remaining on gracetime */
  uint64_t QuotaLimit = 0;     /* The total softquota supplied if over grace */
  utime_t SoftQuotaGracePeriod = {0}; /* Grace time for softquota */
  utime_t FileRetention = {0};        /* File retention period in seconds */
  utime_t JobRetention = {0};         /* Job retention period in seconds */
  utime_t heartbeat_interval = {0};   /* Interval to send heartbeats */
  char* address = nullptr;            /* Hostname for remote access to Client */
  char* lanaddress
      = nullptr; /* Hostname for remote access to Client if behind NAT in LAN
                  */
  char* username = nullptr; /* Username to use for authentication if protocol
                               supports it */
  CatalogResource* catalog = nullptr; /* Catalog resource */
  int32_t MaxConcurrentJobs = 0;      /* Maximum concurrent jobs */
  bool passive = false;               /* Passive Client */
  bool conn_from_dir_to_fd = false;   /* Connect to Client */
  bool conn_from_fd_to_dir = false;   /* Allow incoming connections */
  bool enabled = false;               /* Set if client is enabled */
  bool AutoPrune = false;             /* Do automatic pruning? */
  bool StrictQuotas = false;          /* Enable strict quotas? */
  bool QuotaIncludeFailedJobs
      = false; /* Ignore failed jobs when calculating quota */
  bool ndmp_use_lmdb
      = false; /* NDMP Protocol specific use LMDB for the FHDB or not */
  int64_t max_bandwidth = 0;                  /* Limit speed on this client */
  std::shared_ptr<RuntimeClientStatus> rcs{}; /* Runtime Client Status */
  ClientConnectionHandshakeMode connection_successful_handshake_
      = ClientConnectionHandshakeMode::kUndefined;
};

struct Device {
  std::string name;
};

// Store Resource
class StorageResource
    : public BareosResource
    , public TlsResource {
 public:
  StorageResource() = default;
  virtual ~StorageResource() = default;

  uint32_t Protocol = 0;      /* Protocol to use to connect */
  uint32_t AuthType = 0;      /* Authentication Type to use for protocol */
  uint32_t SDport = 0;        /* Port where Directors connect */
  char* address = nullptr;    /* Hostname for remote access to Storage */
  char* lanaddress = nullptr; /* Hostname for remote access to Storage if behind
                       NAT in LAN */
  char* username = nullptr;   /* Username to use for authentication if protocol
                                 supports it */
  char* media_type = nullptr; /**< Media Type provided by this Storage */
  char* ndmp_changer_device = nullptr; /**< If DIR controls storage directly
                                (NDMP_NATIVE) changer device used */
  std::vector<Device> devices;       /**< Alternate devices for this Storage */
  int32_t MaxConcurrentJobs = 0;     /**< Maximum concurrent jobs */
  int32_t MaxConcurrentReadJobs = 0; /**< Maximum concurrent jobs reading */
  bool enabled = false;              /**< Set if device is enabled */
  bool autochanger = false;          /**< Set if autochanger */
  bool collectstats
      = false; /**< Set if statistics should be collected of this SD */
  bool AllowCompress = false; /**< Set if this Storage should allow jobs to
                         enable compression */
  int64_t StorageId = 0;      /**< Set from Storage DB record */
  int64_t max_bandwidth
      = 0; /**< Limit speed on this storage daemon for replication */
  utime_t heartbeat_interval = {0}; /**< Interval to send heartbeats */
  utime_t cache_status_interval = {
      0}; /**< Interval to cache the vol_list in the runtime_storage_status */
  std::shared_ptr<RuntimeStorageStatus> runtime_storage_status{};
  StorageResource* paired_storage = nullptr; /**< Paired storage configuration
                                      item for protocols like NDMP */

  /* Methods */
  inline const char* dev_name() const
  {
    auto& dev = devices[0];
    return dev.name.c_str();
  }
};

/**
 * This is a sort of "unified" store that has both the
 * storage pointer and the text of where the pointer was
 * found.
 */
class UnifiedStorageResource : public BareosResource {
 public:
  StorageResource* store = nullptr;
  POOLMEM* store_source = nullptr;

  UnifiedStorageResource()
  {
    store = nullptr;
    store_source = GetPoolMemory(PM_MESSAGE);
    *store_source = 0;
  }
  ~UnifiedStorageResource() { destroy(); }
  void destroy();
};

inline void UnifiedStorageResource::destroy()
{
  if (store_source) {
    FreePoolMemory(store_source);
    store_source = nullptr;
  }
}

// Job Resource
/* clang-format off */
class JobResource : public BareosResource {
 public:
  JobResource() = default;
  virtual ~JobResource() = default;

  uint32_t Protocol = 0;       /**< Protocol to use to connect */
  uint32_t JobType = 0;        /**< Job type (backup, verify, restore) */
  uint32_t JobLevel = 0;       /**< default backup/verify level */
  int32_t Priority = 0;        /**< Job priority */
  uint32_t RestoreJobId = 0;   /**< What -- JobId to restore */
  int32_t RescheduleTimes = 0; /**< Number of times to reschedule job */
  uint32_t replace = 0;        /**< How (overwrite, ..) */
  uint32_t selection_type = 0;

  char* RestoreWhere = nullptr;  /**< Where on disk to restore -- directory */
  char* RegexWhere = nullptr;    /**< RegexWhere option */
  char* strip_prefix = nullptr;  /**< Remove prefix from filename  */
  char* add_prefix = nullptr;    /**< add prefix to filename  */
  char* add_suffix = nullptr;    /**< add suffix to filename -- .old */
  char* backup_format = nullptr; /**< Format of backup to use for protocols supporting multiple backup formats */
  char* RestoreBootstrap = nullptr; /**< Bootstrap file */
  char* WriteBootstrap = nullptr;   /**< Where to write bootstrap Job updates */
  char* WriteVerifyList = nullptr;  /**< List of changed files */
  utime_t MaxRunTime = {0};         /**< Max run time in seconds */
  utime_t MaxWaitTime = {0};        /**< Max blocking time in seconds */
  utime_t FullMaxRunTime = {0};     /**< Max Full job run time */
  utime_t DiffMaxRunTime = {0};     /**< Max Differential job run time */
  utime_t IncMaxRunTime = {0};      /**< Max Incremental job run time */
  utime_t MaxStartDelay = {0};      /**< Max start delay in seconds */
  utime_t MaxRunSchedTime = {0}; /**< Max run time in seconds from Scheduled time*/
  utime_t RescheduleInterval = {0}; /**< Reschedule interval */
  utime_t MaxFullInterval = {0};    /**< Maximum time interval between Fulls */
  utime_t MaxVFullInterval = {0}; /**< Maximum time interval between Virtual Fulls */
  utime_t MaxDiffInterval = {0}; /**< Maximum time interval between Diffs */
  utime_t DuplicateJobProximity = {0}; /**< Permitted time between duplicicates */
  utime_t AlwaysIncrementalJobRetention = {0}; /**< Timeinterval where incrementals are not consolidated */
  utime_t AlwaysIncrementalMaxFullAge = {0};   /**< If Full Backup is older than this age
                                                *   the consolidation job will include also the full */
  utime_t RunOnIncomingConnectInterval = {0};
  int64_t spool_size = 0;    /**< Size of spool file for this job */
  int64_t max_bandwidth = 0; /**< Speed limit on this job */
  int64_t FileHistSize = 0; /**< Hint about the size of the expected File history */
  int32_t MaxConcurrentJobs = 0;   /**< Maximum concurrent jobs */
  int32_t MaxConcurrentCopies = 0; /**< Limit number of concurrent jobs one Copy Job spawns */
  int32_t AlwaysIncrementalKeepNumber = 0; /**< Number of incrementals that are always left and not consolidated */
  int32_t MaxFullConsolidations = 0;       /**< Number of consolidate jobs to be started that will include a full */

  MessagesResource* messages = nullptr; /**< How and where to send messages */
  ScheduleResource* schedule = nullptr; /**< When -- Automatic schedule */
  ClientResource* client = nullptr;     /**< Who to backup */
  FilesetResource* fileset = nullptr;   /**< What to backup -- Fileset */
  CatalogResource* catalog = nullptr;   /**< Which Catalog to use */
  alist<StorageResource*>* storage = nullptr; /**< Where is device -- list of Storage to be used */
  PoolResource* pool = nullptr;       /**< Where is media -- Media Pool */
  PoolResource* full_pool = nullptr;  /**< Pool for Full backups */
  PoolResource* vfull_pool = nullptr; /**< Pool for Virtual Full backups */
  PoolResource* inc_pool = nullptr;   /**< Pool for Incremental backups */
  PoolResource* diff_pool = nullptr;  /**< Pool for Differental backups */
  PoolResource* next_pool = nullptr; /**< Next Pool for Copy/Migration Jobs and Virtual backups */
  char* selection_pattern = nullptr;
  JobResource* verify_job = nullptr; /**< Job name to verify */
  JobResource* jobdefs = nullptr;    /**< Job defaults */
  alist<const char*>* run_cmds = nullptr;         /**< Run commands */
  alist<RunScript*>* RunScripts = nullptr; /**< Run {client} program {after|before} Job */
  alist<const char*>* FdPluginOptions = nullptr; /**< Generic FD plugin options used by this Job */
  alist<const char*>* SdPluginOptions = nullptr; /**< Generic SD plugin options used by this Job */
  alist<const char*>* DirPluginOptions = nullptr;           /**< Generic DIR plugin options used by this Job */
  alist<JobResource*>* base = nullptr; /**< Base jobs */

  bool allow_mixed_priority = false; /**< Allow jobs with higher priority concurrently with this */
  bool where_use_regexp = false;  /**< true if RestoreWhere is a BareosRegex */
  bool RescheduleOnError = false; /**< Set to reschedule on error */
  bool RescheduleIncompleteJobs = false; /**< Set to reschedule incomplete Jobs */
  bool PrefixLinks = false;         /**< Prefix soft links with Where path */
  bool PruneJobs = false;           /**< Force pruning of Jobs */
  bool PruneFiles = false;          /**< Force pruning of Files */
  bool PruneVolumes = false;        /**< Force pruning of Volumes */
  bool SpoolAttributes = false;     /**< Set to spool attributes in SD */
  bool spool_data = false;          /**< Set to spool data in SD */
  bool rerun_failed_levels = false; /**< Upgrade to rerun failed levels */
  bool PreferMountedVolumes = false; /**< Prefer vols mounted rather than new one */
  bool enabled = false;              /**< Set if job enabled */
  bool accurate = false;             /**< Set if it is an accurate backup job */
  bool AllowDuplicateJobs = false;   /**< Allow duplicate jobs */
  bool AllowHigherDuplicates = false; /**< Permit Higher Level */
  bool CancelLowerLevelDuplicates = false; /**< Cancel lower level backup jobs */
  bool CancelQueuedDuplicates = false;  /**< Cancel queued jobs */
  bool CancelRunningDuplicates = false; /**< Cancel Running jobs */
  bool PurgeMigrateJob = false;         /**< Purges source job on completion */
  bool IgnoreDuplicateJobChecking = false; /**< Ignore Duplicate Job Checking */
  bool SaveFileHist = false; /**< Ability to disable File history saving for certain protocols */
  bool AlwaysIncremental = false; /**< Always incremental with regular consolidation */

  std::shared_ptr<RuntimeJobStatus> rjs; /**< Runtime Job Status */

  /* Methods */
  virtual bool Validate() override;
};
/* clang-format on */

#undef MAX_FOPTS
#define MAX_FOPTS 40

// File options structure
struct FileOptions {
  FileOptions() = default;
  virtual ~FileOptions() = default;

  // accurate + verify + other fileset options
  char opts[3 * MAX_FOPTS] = {0}; /**< Options string */
  alist<const char*> regex;       /**< Regex string(s) */
  alist<const char*> regexdir;    /**< Regex string(s) for directories */
  alist<const char*> regexfile;   /**< Regex string(s) for files */
  alist<const char*> wild;        /**< Wild card strings */
  alist<const char*> wilddir;     /**< Wild card strings for directories */
  alist<const char*> wildfile;    /**< Wild card strings for files */
  alist<const char*> wildbase;  /**< Wild card strings for files without '/' */
  alist<const char*> base;      /**< List of base names */
  alist<const char*> fstype;    /**< File system type limitation */
  alist<const char*> Drivetype; /**< Drive type limitation */
  alist<const char*> meta;      /**< Backup meta information */
  char* reader = nullptr;       /**< Reader program */
  char* writer = nullptr;       /**< Writer program */
  char* plugin = nullptr;       /**< Plugin program */
};

// This is either an include item or an exclude item
class IncludeExcludeItem {
 public:
  IncludeExcludeItem() = default;
  virtual ~IncludeExcludeItem() = default;

  FileOptions* current_opts = nullptr;
  std::vector<FileOptions*> file_options_list;
  alist<const char*> name_list;   /**< Filename list -- holds char * */
  alist<const char*> plugin_list; /**< Filename list for plugins */
  alist<const char*> ignoredir;   /**< Ignoredir string */
};

// FileSet Resource
class FilesetResource : public BareosResource {
 public:
  FilesetResource() = default;
  virtual ~FilesetResource() = default;

  bool new_include = false; /**< Set if new include used */
  std::vector<IncludeExcludeItem*> include_items;
  std::vector<IncludeExcludeItem*> exclude_items;
  bool have_MD5 = false;          /**< Set if MD5 initialized */
  MD5_CTX md5c = {};              /**< MD5 of include/exclude */
  char MD5[30]{0};                /**< Base 64 representation of MD5 */
  bool ignore_fs_changes = false; /**< Don't force Full if FS changed */
  bool enable_vss = false;        /**< Enable Volume Shadow Copy */

  /* Methods */
  bool PrintConfig(OutputFormatterResource& send,
                   const ConfigurationParser&,
                   bool hide_sensitive_data = false,
                   bool verbose = false) override;
  void PrintConfigIncludeExcludeOptions(OutputFormatterResource& send,
                                        FileOptions* fo,
                                        bool verbose);
  std::string GetOptionValue(const char** option);
};

// Schedule Resource
class ScheduleResource : public BareosResource {
 public:
  ScheduleResource() = default;
  virtual ~ScheduleResource() = default;

  RunResource* run = nullptr;
  bool enabled = false; /* Set if schedule is enabled */
};

// Counter Resource
class CounterResource : public BareosResource {
 public:
  CounterResource() = default;
  virtual ~CounterResource() = default;

  int32_t MinValue = 0;                   /* Minimum value */
  int32_t MaxValue = 0;                   /* Maximum value */
  int32_t CurrentValue = 0;               /* Current value */
  CounterResource* WrapCounter = nullptr; /* Wrap counter name */
  CatalogResource* Catalog = nullptr;     /* Where to store */
  bool created;                           /* Created in DB */
};

// Pool Resource
class PoolResource : public BareosResource {
 public:
  PoolResource() = default;
  virtual ~PoolResource() = default;

  char* pool_type = nullptr;        /* Pool type */
  char* label_format = nullptr;     /* Label format string */
  char* cleaning_prefix = nullptr;  /* Cleaning label prefix */
  int32_t LabelType = 0;            /* Bareos/ANSI/IBM label type */
  uint32_t max_volumes = 0;         /* Max number of volumes */
  utime_t VolRetention = {0};       /* Volume retention period in seconds */
  utime_t VolUseDuration = {0};     /* Duration volume can be used */
  uint32_t MaxVolJobs = 0;          /* Maximum jobs on the Volume */
  uint32_t MaxVolFiles = 0;         /* Maximum files on the Volume */
  uint64_t MaxVolBytes = 0;         /* Maximum bytes on the Volume */
  utime_t MigrationTime = {0};      /* Time to migrate to next pool */
  uint64_t MigrationHighBytes = 0;  /* When migration starts */
  uint64_t MigrationLowBytes = 0;   /* When migration stops */
  PoolResource* NextPool = nullptr; /* Next pool for migration */
  alist<StorageResource*>* storage
      = nullptr;            /* Where is device -- list of Storage to be used */
  bool use_catalog = false; /* Maintain catalog for media */
  bool catalog_files = false;          /* Maintain file entries in catalog */
  bool purge_oldest_volume = false;    /* Purge oldest volume */
  bool recycle_oldest_volume = false;  /* Attempt to recycle oldest volume */
  bool recycle_current_volume = false; /* Attempt recycle of current volume */
  bool AutoPrune = false;              /* Default for pool auto prune */
  bool Recycle = false;                /* Default for media recycle yes/no */
  uint32_t action_on_purge
      = 0; /* Action on purge, e.g. truncate the disk volume */
  PoolResource* RecyclePool
      = nullptr; /* RecyclePool destination when media is purged */
  PoolResource* ScratchPool
      = nullptr; /* ScratchPool source when requesting media */
  CatalogResource* catalog = nullptr; /* Catalog to be used */
  utime_t FileRetention = {0};        /* File retention period in seconds */
  utime_t JobRetention = {0};         /* Job retention period in seconds */
  uint32_t MinBlocksize = 0;          /* Minimum Blocksize */
  uint32_t MaxBlocksize = 0;          /* Maximum Blocksize */
};

// Run structure contained in Schedule Resource
class RunResource : public BareosResource {
 public:
  RunResource() = default;
  virtual ~RunResource() = default;

  RunResource* next = nullptr; /**< points to next run record */
  uint32_t level = 0;          /**< level override */
  int32_t Priority = 0;        /**< priority override */
  uint32_t job_type = 0;
  utime_t MaxRunSchedTime = {0};    /**< max run time in sec from Sched time */
  bool MaxRunSchedTime_set = false; /**< MaxRunSchedTime given */
  bool spool_data = false;          /**< Data spooling override */
  bool spool_data_set = false;      /**< Data spooling override given */
  bool accurate = false;            /**< accurate */
  bool accurate_set = false;        /**< accurate given */

  PoolResource* pool = nullptr;       /**< Pool override */
  PoolResource* full_pool = nullptr;  /**< Full Pool override */
  PoolResource* vfull_pool = nullptr; /**< Virtual Full Pool override */
  PoolResource* inc_pool = nullptr;   /**< Incr Pool override */
  PoolResource* diff_pool = nullptr;  /**< Diff Pool override */
  PoolResource* next_pool = nullptr;  /**< Next Pool override */
  StorageResource* storage = nullptr; /**< Storage override */
  MessagesResource* msgs = nullptr;   /**< Messages override */
  uint32_t minute = 0;                /* minute to run job */
  time_t scheduled_last = {0};
  DateTimeMask date_time_mask;

  /* Methods */
  std::optional<time_t> NextScheduleTime(time_t start, uint32_t ndays) const;
};

ConfigurationParser* InitDirConfig(const char* configfile, int exit_code);
bool PropagateJobdefs(int res_type, JobResource* res);
bool ValidateResource(int type, const ResourceItem* items, BareosResource* res);

bool print_datatype_schema_json(PoolMem& buffer,
                                int level,
                                const int type,
                                const ResourceItem items[],
                                const bool last = false);
json_t* json_datatype(const int type, const ResourceItem items[]);
const char* AuthenticationProtocolTypeToString(uint32_t auth_protocol);
const char* JobLevelToString(int level);
std::optional<std::string> job_code_callback_director(JobControlRecord* jcr,
                                                      const char*);
const char* GetUsageStringForConsoleConfigureCommand();
void DestroyConfigureUsageString();
bool PopulateDefs();
std::vector<JobResource*> GetAllJobResourcesByClientName(std::string name);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_DIRD_CONF_H_
