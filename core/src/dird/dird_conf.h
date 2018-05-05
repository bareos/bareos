/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * Kern Sibbald, Feb MM
 */
/**
 * @file
 * Director specific configuration and defines
 */
#ifndef BAREOS_DIRD_DIRD_CONF_H_
#define BAREOS_DIRD_DIRD_CONF_H_
/* NOTE:  #includes at the end of this file */

#define CONFIG_FILE "bareos-dir.conf" /* default configuration file */

/**
 * Resource codes -- they must be sequential for indexing
 */
enum {
   R_DIRECTOR = 1001,
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
   R_DEVICE,
   R_FIRST = R_DIRECTOR,
   R_LAST = R_DEVICE                  /* keep this updated */
};

/**
 * Some resource attributes
 */
enum {
   R_NAME = 1020,
   R_ADDRESS,
   R_PASSWORD,
   R_TYPE,
   R_BACKUP
};

/**
 * Job Level keyword structure
 */
struct s_jl {
   const char *level_name;                 /* level keyword */
   uint32_t level;                         /* level */
   int32_t job_type;                       /* JobType permitting this level */
};

/**
 * Job Type keyword structure
 */
struct s_jt {
   const char *type_name;
   int32_t job_type;
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
class RunScriptResource;

/*
 * Print configuration file schema in json format
 */
bool PrintConfigSchemaJson(PoolMem &buff);

/*
 *   Director Resource
 */
class DirectorResource: public TlsResource {
public:
   dlist *DIRaddrs;
   dlist *DIRsrc_addr;                /* Address to source connections from */
   char *query_file;                  /* SQL query file */
   char *working_directory;           /* WorkingDirectory */
   char *scripts_directory;           /* ScriptsDirectory */
   char *plugin_directory;            /* Plugin Directory */
   alist *plugin_names;               /* Plugin names to load */
   char *pid_directory;               /* PidDirectory */
   char *subsys_directory;            /* SubsysDirectory */
   alist *backend_directories;        /* Backend Directories */
   MessagesResource *messages;                 /* Daemon message handler */
   uint32_t MaxConcurrentJobs;        /* Max concurrent jobs for whole director */
   uint32_t MaxConnections;           /* Max concurrent connections */
   uint32_t MaxConsoleConnections;    /* Max concurrent console connections */
   utime_t FDConnectTimeout;          /* Timeout for connect in seconds */
   utime_t SDConnectTimeout;          /* Timeout for connect in seconds */
   utime_t heartbeat_interval;        /* Interval to send heartbeats */
   utime_t stats_retention;           /* Statistics retention period in seconds */
   bool optimize_for_size;            /* Optimize daemon for minimum memory size */
   bool optimize_for_speed;           /* Optimize daemon for speed which may need more memory */
   bool nokeepalive;                  /* Don't use SO_KEEPALIVE on sockets */
   bool omit_defaults;                /* Omit config variables with default values when dumping the config */
   bool ndmp_snooping;                /* NDMP Protocol specific snooping enabled */
   bool auditing;                     /* Auditing enabled */
   alist *audit_events;               /* Specific audit events to enable */
   uint32_t ndmp_loglevel;            /* NDMP Protocol specific loglevel to use */
   uint32_t subscriptions;            /* Number of subscribtions available */
   uint32_t subscriptions_used;       /* Number of subscribtions used */
   uint32_t jcr_watchdog_time;        /* Absolute time after which a Job gets terminated regardless of its progress */
   uint32_t stats_collect_interval;   /* Statistics collect interval in seconds */
   char *verid;                       /* Custom Id to print in version command */
   char *secure_erase_cmdline;        /* Cmdline to execute to perform secure erase of file */
   char *log_timestamp_format;        /* Timestamp format to use in generic logging messages */
   s_password keyencrkey;             /* Key Encryption Key */

   DirectorResource() : TlsResource() {}
};

/*
 * Device Resource
 *
 * This resource is a bit different from the other resources
 * because it is not defined in the Director
 * by Device { ... }, but rather by a "reference" such as
 * Device = xxx; Then when the Director connects to the
 * SD, it requests the information about the device.
 */
class DeviceResource : public BareosResource {
public:
   bool found;                        /**< found with SD */
   int32_t num_writers;               /**< number of writers */
   int32_t max_writers;               /**< = 1 for files */
   int32_t reserved;                  /**< number of reserves */
   int32_t num_drives;                /**< for autochanger */
   bool autochanger;                  /**< set if device is autochanger */
   bool open;                         /**< drive open */
   bool append;                       /**< in append mode */
   bool read;                         /**< in read mode */
   bool labeled;                      /**< Volume name valid */
   bool offline;                      /**< not available */
   bool autoselect;                   /**< can be selected via autochanger */
   uint32_t PoolId;
   char ChangerName[MAX_NAME_LENGTH];
   char VolumeName[MAX_NAME_LENGTH];
   char MediaType[MAX_NAME_LENGTH];

   DeviceResource() : BareosResource() {}
};

/**
 * Console ACL positions
 */
enum {
   Job_ACL = 0,
   Client_ACL,
   Storage_ACL,
   Schedule_ACL,
   Run_ACL,
   Pool_ACL,
   Command_ACL,
   FileSet_ACL,
   Catalog_ACL,
   Where_ACL,
   PluginOptions_ACL,
   Num_ACL                            /**< keep last */
};

/**
 * Profile Resource
 */
class ProfileResource : public BareosResource {
public:
   alist *ACL_lists[Num_ACL];         /**< Pointers to ACLs */
};

/**
 * Console Resource
 */
class ConsoleResource : public TlsResource {
public:
   alist *ACL_lists[Num_ACL];         /**< Pointers to ACLs */
   alist *profiles;                   /**< Pointers to profile resources */
};

/**
 * Catalog Resource
 */
class CatalogResource : public BareosResource {
public:
   uint32_t db_port;                  /**< Port */
   char *db_address;                  /**< Hostname for remote access */
   char *db_socket;                   /**< Socket for local access */
   s_password db_password;
   char *db_user;
   char *db_name;
   char *db_driver;                   /**< Select appropriate driver */
   uint32_t mult_db_connections;      /**< Set if multiple connections wanted */
   bool disable_batch_insert;         /**< Set if batch inserts should be disabled */
   bool try_reconnect;                /**< Try to reconnect a database connection when its dropped */
   bool exit_on_fatal;                /**< Make any fatal error in the connection to the database exit the program */
   uint32_t pooling_min_connections;  /**< When using sql pooling start with this number of connections to the database */
   uint32_t pooling_max_connections;  /**< When using sql pooling maximum number of connections to the database */
   uint32_t pooling_increment_connections; /**< When using sql pooling increment the pool with this amount when its to small */
   uint32_t pooling_idle_timeout;     /**< When using sql pooling set this to the number of seconds to keep an idle connection */
   uint32_t pooling_validate_timeout; /**< When using sql pooling set this to the number of seconds after a idle connection should be validated */

   /**< Methods */
   char *display(POOLMEM *dst);       /**< Get catalog information */
   CatalogResource() : BareosResource() { }
};

/**
 * Forward referenced structures
 */
struct runtime_client_status_t;
struct runtime_storage_status_t;
struct runtime_job_status_t;

/**
 * Client Resource
 */
class ClientResource: public TlsResource {
public:
   uint32_t Protocol;                 /* Protocol to use to connect */
   uint32_t AuthType;                 /* Authentication Type to use for protocol */
   uint32_t ndmp_loglevel;            /* NDMP Protocol specific loglevel to use */
   uint32_t ndmp_blocksize;           /* NDMP Protocol specific blocksize to use */
   uint32_t FDport;                   /* Where File daemon listens */
   uint64_t SoftQuota;                /* Soft Quota permitted in bytes */
   uint64_t HardQuota;                /* Maximum permitted quota in bytes */
   uint64_t GraceTime;                /* Time remaining on gracetime */
   uint64_t QuotaLimit;               /* The total softquota supplied if over grace */
   utime_t SoftQuotaGracePeriod;      /* Grace time for softquota */
   utime_t FileRetention;             /* File retention period in seconds */
   utime_t JobRetention;              /* Job retention period in seconds */
   utime_t heartbeat_interval;        /* Interval to send heartbeats */
   char *address;                     /* Hostname for remote access to Client */
   char *lanaddress;                  /* Hostname for remote access to Client if behind NAT in LAN */
   char *username;                    /* Username to use for authentication if protocol supports it */
   CatalogResource *catalog;                   /* Catalog resource */
   int32_t MaxConcurrentJobs;         /* Maximum concurrent jobs */
   bool passive;                      /* Passive Client */
   bool conn_from_dir_to_fd;          /* Connect to Client */
   bool conn_from_fd_to_dir;          /* Allow incoming connections */
   bool enabled;                      /* Set if client is enabled */
   bool AutoPrune;                    /* Do automatic pruning? */
   bool StrictQuotas;                 /* Enable strict quotas? */
   bool QuotaIncludeFailedJobs;       /* Ignore failed jobs when calculating quota */
   bool ndmp_use_lmdb;                /* NDMP Protocol specific use LMDB for the FHDB or not */
   int64_t max_bandwidth;             /* Limit speed on this client */
   runtime_client_status_t *rcs;      /* Runtime Client Status */
   ClientResource() : TlsResource() {}
};

/**
 * Store Resource
 */
class StorageResource : public TlsResource {
public:
   uint32_t Protocol;                 /* Protocol to use to connect */
   uint32_t AuthType;                 /* Authentication Type to use for protocol */
   uint32_t SDport;                   /* Port where Directors connect */
   uint32_t SDDport;                  /* Data port for File daemon */
   char *address;                     /* Hostname for remote access to Storage */
   char *lanaddress;                  /* Hostname for remote access to Storage if behind NAT in LAN */
   char *username;                    /* Username to use for authentication if protocol supports it */
   char *media_type;                  /**< Media Type provided by this Storage */
   char *ndmp_changer_device;         /**< If DIR controls storage directly (NDMP_NATIVE) changer device used */
   alist *device;                     /**< Alternate devices for this Storage */
   int32_t MaxConcurrentJobs;         /**< Maximum concurrent jobs */
   int32_t MaxConcurrentReadJobs;     /**< Maximum concurrent jobs reading */
   bool enabled;                      /**< Set if device is enabled */
   bool autochanger;                  /**< Set if autochanger */
   bool collectstats;                 /**< Set if statistics should be collected of this SD */
   bool AllowCompress;                /**< Set if this Storage should allow jobs to enable compression */
   int64_t StorageId;                 /**< Set from Storage DB record */
   int64_t max_bandwidth;             /**< Limit speed on this storage daemon for replication */
   utime_t heartbeat_interval;        /**< Interval to send heartbeats */
   utime_t cache_status_interval;     /**< Interval to cache the vol_list in the rss */
   runtime_storage_status_t *rss;     /**< Runtime Storage Status */
   StorageResource *paired_storage;          /**< Paired storage configuration item for protocols like NDMP */

   /* Methods */
   char *dev_name() const;

   StorageResource() : TlsResource() {}
};

inline char *StorageResource::dev_name() const
{
   DeviceResource *dev = (DeviceResource *)device->first();
   return dev->name();
}

/**
 * This is a sort of "unified" store that has both the
 * storage pointer and the text of where the pointer was
 * found.
 */
class UnifiedStorageResource {
public:
   StorageResource *store;
   POOLMEM *store_source;

   /* Methods */
   UnifiedStorageResource() { store = NULL; store_source = GetPoolMemory(PM_MESSAGE);
              *store_source = 0; }
   ~UnifiedStorageResource() { destroy(); }
   void SetSource(const char *where);
   void destroy();
};

inline void UnifiedStorageResource::destroy()
{
   if (store_source) {
      FreePoolMemory(store_source);
      store_source = NULL;
   }
}

inline void UnifiedStorageResource::SetSource(const char *where)
{
   if (!store_source) {
      store_source = GetPoolMemory(PM_MESSAGE);
   }
   PmStrcpy(store_source, where);
}

/**
 * Job Resource
 */
class JobResource : public BareosResource {
public:
   uint32_t Protocol;                 /**< Protocol to use to connect */
   uint32_t JobType;                  /**< Job type (backup, verify, restore) */
   uint32_t JobLevel;                 /**< default backup/verify level */
   int32_t Priority;                  /**< Job priority */
   uint32_t RestoreJobId;             /**< What -- JobId to restore */
   int32_t RescheduleTimes;           /**< Number of times to reschedule job */
   uint32_t replace;                  /**< How (overwrite, ..) */
   uint32_t selection_type;

   char *RestoreWhere;                /**< Where on disk to restore -- directory */
   char *RegexWhere;                  /**< RegexWhere option */
   char *strip_prefix;                /**< Remove prefix from filename  */
   char *add_prefix;                  /**< add prefix to filename  */
   char *add_suffix;                  /**< add suffix to filename -- .old */
   char *backup_format;               /**< Format of backup to use for protocols supporting multiple backup formats */
   char *RestoreBootstrap;            /**< Bootstrap file */
   char *WriteBootstrap;              /**< Where to write bootstrap Job updates */
   char *WriteVerifyList;             /**< List of changed files */
   utime_t MaxRunTime;                /**< Max run time in seconds */
   utime_t MaxWaitTime;               /**< Max blocking time in seconds */
   utime_t FullMaxRunTime;            /**< Max Full job run time */
   utime_t DiffMaxRunTime;            /**< Max Differential job run time */
   utime_t IncMaxRunTime;             /**< Max Incremental job run time */
   utime_t MaxStartDelay;             /**< Max start delay in seconds */
   utime_t MaxRunSchedTime;           /**< Max run time in seconds from Scheduled time*/
   utime_t RescheduleInterval;        /**< Reschedule interval */
   utime_t MaxFullInterval;           /**< Maximum time interval between Fulls */
   utime_t MaxVFullInterval;          /**< Maximum time interval between Virtual Fulls */
   utime_t MaxDiffInterval;           /**< Maximum time interval between Diffs */
   utime_t DuplicateJobProximity;     /**< Permitted time between duplicicates */
   utime_t AlwaysIncrementalJobRetention; /**< Timeinterval where incrementals are not consolidated */
   utime_t AlwaysIncrementalMaxFullAge; /**< If Full Backup is older than this age the consolidation job will include also the full */
   int64_t spool_size;                /**< Size of spool file for this job */
   int64_t max_bandwidth;             /**< Speed limit on this job */
   int64_t FileHistSize;              /**< Hint about the size of the expected File history */
   int32_t MaxConcurrentJobs;         /**< Maximum concurrent jobs */
   int32_t MaxConcurrentCopies;       /**< Limit number of concurrent jobs one Copy Job spawns */
   int32_t AlwaysIncrementalKeepNumber;/**< Number of incrementals that are always left and not consolidated */
   int32_t MaxFullConsolidations;     /**< Number of consolidate jobs to be started that will include a full */

   MessagesResource *messages;                 /**< How and where to send messages */
   ScheduleResource *schedule;                /**< When -- Automatic schedule */
   ClientResource *client;                 /**< Who to backup */
   FilesetResource *fileset;               /**< What to backup -- Fileset */
   CatalogResource *catalog;                   /**< Which Catalog to use */
   alist *storage;                    /**< Where is device -- list of Storage to be used */
   PoolResource *pool;                     /**< Where is media -- Media Pool */
   PoolResource *full_pool;                /**< Pool for Full backups */
   PoolResource *vfull_pool;               /**< Pool for Virtual Full backups */
   PoolResource *inc_pool;                 /**< Pool for Incremental backups */
   PoolResource *diff_pool;                /**< Pool for Differental backups */
   PoolResource *next_pool;                /**< Next Pool for Copy/Migration Jobs and Virtual backups */
   char *selection_pattern;
   JobResource *verify_job;                /**< Job name to verify */
   JobResource *jobdefs;                   /**< Job defaults */
   alist *run_cmds;                   /**< Run commands */
   alist *RunScripts;                 /**< Run {client} program {after|before} Job */
   alist *FdPluginOptions;            /**< Generic FD plugin options used by this Job */
   alist *SdPluginOptions;            /**< Generic SD plugin options used by this Job */
   alist *DirPluginOptions;           /**< Generic DIR plugin options used by this Job */
   alist *base;                       /**< Base jobs */

   bool allow_mixed_priority;         /**< Allow jobs with higher priority concurrently with this */
   bool where_use_regexp;             /**< true if RestoreWhere is a BareosRegex */
   bool RescheduleOnError;            /**< Set to reschedule on error */
   bool RescheduleIncompleteJobs;     /**< Set to reschedule incomplete Jobs */
   bool PrefixLinks;                  /**< Prefix soft links with Where path */
   bool PruneJobs;                    /**< Force pruning of Jobs */
   bool PruneFiles;                   /**< Force pruning of Files */
   bool PruneVolumes;                 /**< Force pruning of Volumes */
   bool SpoolAttributes;              /**< Set to spool attributes in SD */
   bool spool_data;                   /**< Set to spool data in SD */
   bool rerun_failed_levels;          /**< Upgrade to rerun failed levels */
   bool PreferMountedVolumes;         /**< Prefer vols mounted rather than new one */
   bool write_part_after_job;         /**< Set to write part after job in SD */
   bool enabled;                      /**< Set if job enabled */
   bool accurate;                     /**< Set if it is an accurate backup job */
   bool AllowDuplicateJobs;           /**< Allow duplicate jobs */
   bool AllowHigherDuplicates;        /**< Permit Higher Level */
   bool CancelLowerLevelDuplicates;   /**< Cancel lower level backup jobs */
   bool CancelQueuedDuplicates;       /**< Cancel queued jobs */
   bool CancelRunningDuplicates;      /**< Cancel Running jobs */
   bool PurgeMigrateJob;              /**< Purges source job on completion */
   bool IgnoreDuplicateJobChecking;   /**< Ignore Duplicate Job Checking */
   bool SaveFileHist;                 /**< Ability to disable File history saving for certain protocols */
   bool AlwaysIncremental;            /**< Always incremental with regular consolidation */

   runtime_job_status_t *rjs;         /**< Runtime Job Status */

   /* Methods */
   bool validate();

   JobResource() : BareosResource() {}
};

#undef  MAX_FOPTS
#define MAX_FOPTS 40

/**
 * File options structure
 */
struct FileOptions {
   char opts[MAX_FOPTS];              /**< Options string */
   alist regex;                       /**< Regex string(s) */
   alist regexdir;                    /**< Regex string(s) for directories */
   alist regexfile;                   /**< Regex string(s) for files */
   alist wild;                        /**< Wild card strings */
   alist wilddir;                     /**< Wild card strings for directories */
   alist wildfile;                    /**< Wild card strings for files */
   alist wildbase;                    /**< Wild card strings for files without '/' */
   alist base;                        /**< List of base names */
   alist fstype;                      /**< File system type limitation */
   alist Drivetype;                   /**< Drive type limitation */
   alist meta;                        /**< Backup meta information */
   char *reader;                      /**< Reader program */
   char *writer;                      /**< Writer program */
   char *plugin;                      /**< Plugin program */
};

/**
 * This is either an include item or an exclude item
 */
struct IncludeExcludeItem {
   FileOptions *current_opts;               /**< Points to current options structure */
   FileOptions **opts_list;                 /**< Options list */
   int32_t num_opts;                  /**< Number of options items */
   alist name_list;                   /**< Filename list -- holds char * */
   alist plugin_list;                 /**< Filename list for plugins */
   alist ignoredir;                   /**< Ignoredir string */
};

/**
 * FileSet Resource
 */
class FilesetResource : public BareosResource {
public:
   bool new_include;                  /**< Set if new include used */
   IncludeExcludeItem **include_items;            /**< Array of incexe structures */
   int32_t num_includes;              /**< Number in array */
   IncludeExcludeItem **exclude_items;
   int32_t num_excludes;
   bool have_MD5;                     /**< Set if MD5 initialized */
   MD5_CTX md5c;                      /**< MD5 of include/exclude */
   char MD5[30];                      /**< Base 64 representation of MD5 */
   bool ignore_fs_changes;            /**< Don't force Full if FS changed */
   bool enable_vss;                   /**< Enable Volume Shadow Copy */

   /* Methods */
   bool PrintConfig(PoolMem& buf, bool hide_sensitive_data = false, bool verbose = false);

   FilesetResource() : BareosResource() {}
};

/**
 * Schedule Resource
 */
class ScheduleResource: public BareosResource {
public:
   RunResource *run;
   bool enabled;                      /* Set if schedule is enabled */

   ScheduleResource() : BareosResource() {}
};

/**
 * Counter Resource
 */
class CounterResource: public BareosResource {
public:
   int32_t MinValue;                  /* Minimum value */
   int32_t MaxValue;                  /* Maximum value */
   int32_t CurrentValue ;             /* Current value */
   CounterResource *WrapCounter;           /* Wrap counter name */
   CatalogResource *Catalog;                   /* Where to store */
   bool created;                      /* Created in DB */

   CounterResource() : BareosResource() {}
};

/**
 * Pool Resource
 */
class PoolResource: public BareosResource {
public:
   char *pool_type;                   /* Pool type */
   char *label_format;                /* Label format string */
   char *cleaning_prefix;             /* Cleaning label prefix */
   int32_t LabelType;                 /* Bareos/ANSI/IBM label type */
   uint32_t max_volumes;              /* Max number of volumes */
   utime_t VolRetention;              /* Volume retention period in seconds */
   utime_t VolUseDuration;            /* Duration volume can be used */
   uint32_t MaxVolJobs;               /* Maximum jobs on the Volume */
   uint32_t MaxVolFiles;              /* Maximum files on the Volume */
   uint64_t MaxVolBytes;              /* Maximum bytes on the Volume */
   utime_t MigrationTime;             /* Time to migrate to next pool */
   uint64_t MigrationHighBytes;       /* When migration starts */
   uint64_t MigrationLowBytes;        /* When migration stops */
   PoolResource *NextPool;                 /* Next pool for migration */
   alist *storage;                    /* Where is device -- list of Storage to be used */
   bool use_catalog;                  /* Maintain catalog for media */
   bool catalog_files;                /* Maintain file entries in catalog */
   bool use_volume_once;              /* Write on volume only once */
   bool purge_oldest_volume;          /* Purge oldest volume */
   bool recycle_oldest_volume;        /* Attempt to recycle oldest volume */
   bool recycle_current_volume;       /* Attempt recycle of current volume */
   bool AutoPrune;                    /* Default for pool auto prune */
   bool Recycle;                      /* Default for media recycle yes/no */
   uint32_t action_on_purge;          /* Action on purge, e.g. truncate the disk volume */
   PoolResource *RecyclePool;              /* RecyclePool destination when media is purged */
   PoolResource *ScratchPool;              /* ScratchPool source when requesting media */
   CatalogResource *catalog;                   /* Catalog to be used */
   utime_t FileRetention;             /* File retention period in seconds */
   utime_t JobRetention;              /* Job retention period in seconds */
   uint32_t MinBlocksize;             /* Minimum Blocksize */
   uint32_t MaxBlocksize;             /* Maximum Blocksize */

   PoolResource() : BareosResource() {}
};

/**
 * Run structure contained in Schedule Resource
 */
class RunResource: public BareosResource {
public:
   RunResource *next;                      /**< points to next run record */
   uint32_t level;                    /**< level override */
   int32_t Priority;                  /**< priority override */
   uint32_t job_type;
   utime_t MaxRunSchedTime;           /**< max run time in sec from Sched time */
   bool MaxRunSchedTime_set;          /**< MaxRunSchedTime given */
   bool spool_data;                   /**< Data spooling override */
   bool spool_data_set;               /**< Data spooling override given */
   bool accurate;                     /**< accurate */
   bool accurate_set;                 /**< accurate given */

   PoolResource *pool;                     /**< Pool override */
   PoolResource *full_pool;                /**< Full Pool override */
   PoolResource *vfull_pool;               /**< Virtual Full Pool override */
   PoolResource *inc_pool;                 /**< Incr Pool override */
   PoolResource *diff_pool;                /**< Diff Pool override */
   PoolResource *next_pool;                /**< Next Pool override */
   StorageResource *storage;                 /**< Storage override */
   MessagesResource *msgs;                     /**< Messages override */
   char *since;
   uint32_t level_no;
   uint32_t minute;                   /* minute to run job */
   time_t last_run;                   /* last time run */
   time_t next_run;                   /* next time to run */
   char hour[NbytesForBits(24 + 1)];  /* bit set for each hour */
   char mday[NbytesForBits(31 + 1)];  /* bit set for each day of month */
   char month[NbytesForBits(12 + 1)]; /* bit set for each month */
   char wday[NbytesForBits(7 + 1)];   /* bit set for each day of the week */
   char wom[NbytesForBits(5 + 1)];    /* week of month */
   char woy[NbytesForBits(54 + 1)];   /* week of year */
   bool last_set;                       /* last week of month */

   RunResource() : BareosResource() {}
};

/**
 * Define the Union of all the above
 * resource structure definitions.
 */
union UnionOfResources {
   DirectorResource res_dir;
   ConsoleResource res_con;
   ProfileResource res_profile;
   ClientResource res_client;
   StorageResource res_store;
   CatalogResource res_cat;
   JobResource res_job;
   FilesetResource res_fs;
   ScheduleResource res_sch;
   PoolResource res_pool;
   MessagesResource res_msgs;
   CounterResource res_counter;
   DeviceResource res_dev;
   CommonResourceHeader hdr;

   UnionOfResources() {
      new (&hdr) CommonResourceHeader();
      Dmsg1(900, "hdr:        %p \n", &hdr);
      Dmsg1(900, "res_dir.hdr %p\n", &res_dir.hdr);
      Dmsg1(900, "res_con.hdr %p\n", &res_con.hdr);
   }
   ~UnionOfResources() {}
};

void InitDirConfig(ConfigurationParser *config, const char *configfile, int exit_code);
bool PropagateJobdefs(int res_type, JobResource *res);
bool ValidateResource(int type, ResourceItem *items, BareosResource *res);

bool print_datatype_schema_json(PoolMem &buffer, int level, const int type,
                                ResourceItem items[], const bool last = false);
#ifdef HAVE_JANSSON
json_t *json_datatype(const int type, ResourceItem items[]);
#endif
const char *auth_protocol_to_str(uint32_t auth_protocol);
const char *level_to_str(int level);
extern "C" char *job_code_callback_director(JobControlRecord *jcr, const char*);
const char *get_configure_usage_string();
void DestroyConfigureUsageString();
bool PopulateDefs();

#endif // BAREOS_DIRD_DIRD_CONF_H_
