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
/*
 * Director specific configuration and defines
 *
 * Kern Sibbald, Feb MM
 */

/* NOTE:  #includes at the end of this file */

#define CONFIG_FILE "bareos-dir.conf" /* default configuration file */

/*
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

/*
 * Some resource attributes
 */
enum {
   R_NAME = 1020,
   R_ADDRESS,
   R_PASSWORD,
   R_TYPE,
   R_BACKUP
};

/*
 * Job Level keyword structure
 */
struct s_jl {
   const char *level_name;                 /* level keyword */
   uint32_t level;                         /* level */
   int32_t job_type;                       /* JobType permitting this level */
};

/*
 * Job Type keyword structure
 */
struct s_jt {
   const char *type_name;
   int32_t job_type;
};

/*
 * Definition of the contents of each Resource
 * Needed for forward references
 */
class SCHEDRES;
class CLIENTRES;
class FILESETRES;
class POOLRES;
class RUNRES;
class DEVICERES;
class RUNSCRIPTRES;

/*
 * Print configuration file schema in json format
 */
bool print_config_schema_json(POOL_MEM &buff);

/*
 *   Director Resource
 */
class DIRRES: public BRSRES {
public:
   dlist *DIRaddrs;
   dlist *DIRsrc_addr;                /* Address to source connections from */
   s_password password;               /* Password for UA access */
   char *query_file;                  /* SQL query file */
   char *working_directory;           /* WorkingDirectory */
   char *scripts_directory;           /* ScriptsDirectory */
   char *plugin_directory;            /* Plugin Directory */
   alist *plugin_names;               /* Plugin names to load */
   char *pid_directory;               /* PidDirectory */
   char *subsys_directory;            /* SubsysDirectory */
   alist *backend_directories;        /* Backend Directories */
   MSGSRES *messages;                 /* Daemon message handler */
   uint32_t MaxConcurrentJobs;        /* Max concurrent jobs for whole director */
   uint32_t MaxConnections;           /* Max concurrent connections */
   uint32_t MaxConsoleConnections;    /* Max concurrent console connections */
   utime_t FDConnectTimeout;          /* Timeout for connect in seconds */
   utime_t SDConnectTimeout;          /* Timeout for connect in seconds */
   utime_t heartbeat_interval;        /* Interval to send heartbeats */
   utime_t stats_retention;           /* Statistics retention period in seconds */
   tls_t tls;                         /* TLS structure */
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
};

/*
 * Device Resource
 *
 * This resource is a bit different from the other resources
 * because it is not defined in the Director
 * by DEVICE { ... }, but rather by a "reference" such as
 * DEVICE = xxx; Then when the Director connects to the
 * SD, it requests the information about the device.
 */
class DEVICERES : public BRSRES {
public:
   bool found;                        /* found with SD */
   int32_t num_writers;               /* number of writers */
   int32_t max_writers;               /* = 1 for files */
   int32_t reserved;                  /* number of reserves */
   int32_t num_drives;                /* for autochanger */
   bool autochanger;                  /* set if device is autochanger */
   bool open;                         /* drive open */
   bool append;                       /* in append mode */
   bool read;                         /* in read mode */
   bool labeled;                      /* Volume name valid */
   bool offline;                      /* not available */
   bool autoselect;                   /* can be selected via autochanger */
   uint32_t PoolId;
   char ChangerName[MAX_NAME_LENGTH];
   char VolumeName[MAX_NAME_LENGTH];
   char MediaType[MAX_NAME_LENGTH];
};

/*
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
   Num_ACL                            /* keep last */
};

/*
 * Profile Resource
 */
class PROFILERES : public BRSRES {
public:
   alist *ACL_lists[Num_ACL];         /* Pointers to ACLs */
};

/*
 * Console Resource
 */
class CONRES : public BRSRES {
public:
   s_password password;               /* UA server password */
   alist *ACL_lists[Num_ACL];         /* Pointers to ACLs */
   alist *profiles;                   /* Pointers to profile resources */
   tls_t tls;                         /* TLS structure */
};

/*
 * Catalog Resource
 */
class CATRES : public BRSRES {
public:
   uint32_t db_port;                  /* Port */
   char *db_address;                  /* Hostname for remote access */
   char *db_socket;                   /* Socket for local access */
   s_password db_password;
   char *db_user;
   char *db_name;
   char *db_driver;                   /* Select appropriate driver */
   uint32_t mult_db_connections;      /* Set if multiple connections wanted */
   bool disable_batch_insert;         /* Set if batch inserts should be disabled */
   bool try_reconnect;                /* Try to reconnect a database connection when its dropped */
   bool exit_on_fatal;                /* Make any fatal error in the connection to the database exit the program */
   uint32_t pooling_min_connections;  /* When using sql pooling start with this number of connections to the database */
   uint32_t pooling_max_connections;  /* When using sql pooling maximum number of connections to the database */
   uint32_t pooling_increment_connections; /* When using sql pooling increment the pool with this amount when its to small */
   uint32_t pooling_idle_timeout;     /* When using sql pooling set this to the number of seconds to keep an idle connection */
   uint32_t pooling_validate_timeout; /* When using sql pooling set this to the number of seconds after a idle connection should be validated */

   /* Methods */
   char *display(POOLMEM *dst);       /* Get catalog information */
};

/*
 * Forward referenced structures
 */
struct runtime_client_status_t;
struct runtime_storage_status_t;
struct runtime_job_status_t;

/*
 * Client Resource
 */
class CLIENTRES: public BRSRES {
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
   char *username;                    /* Username to use for authentication if protocol supports it */
   s_password password;
   CATRES *catalog;                   /* Catalog resource */
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
   tls_t tls;                         /* TLS structure */
};

/*
 * Store Resource
 */
class STORERES : public BRSRES {
public:
   uint32_t Protocol;                 /* Protocol to use to connect */
   uint32_t AuthType;                 /* Authentication Type to use for protocol */
   uint32_t SDport;                   /* Port where Directors connect */
   uint32_t SDDport;                  /* Data port for File daemon */
   char *address;                     /* Hostname for remote access to Storage */
   char *username;                    /* Username to use for authentication if protocol supports it */
   s_password password;
   char *media_type;                  /* Media Type provided by this Storage */
   char *changer_device;              /* If DIR controls storage directly changer device used */
   alist *tape_devices;               /* If DIR controls storage directly tape devices in storage */
   alist *device;                     /* Alternate devices for this Storage */
   int32_t MaxConcurrentJobs;         /* Maximum concurrent jobs */
   int32_t MaxConcurrentReadJobs;     /* Maximum concurrent jobs reading */
   bool enabled;                      /* Set if device is enabled */
   bool autochanger;                  /* Set if autochanger */
   bool collectstats;                 /* Set if statistics should be collected of this SD */
   bool AllowCompress;                /* Set if this Storage should allow jobs to enable compression */
   int64_t StorageId;                 /* Set from Storage DB record */
   int64_t max_bandwidth;             /* Limit speed on this storage daemon for replication */
   utime_t heartbeat_interval;        /* Interval to send heartbeats */
   utime_t cache_status_interval;     /* Interval to cache the vol_list in the rss */
   runtime_storage_status_t *rss;     /* Runtime Storage Status */
   STORERES *paired_storage;          /* Paired storage configuration item for protocols like NDMP */
   tls_t tls;                         /* TLS structure */

   /* Methods */
   char *dev_name() const;
};

inline char *STORERES::dev_name() const
{
   DEVICERES *dev = (DEVICERES *)device->first();
   return dev->name();
}

/*
 * This is a sort of "unified" store that has both the
 * storage pointer and the text of where the pointer was
 * found.
 */
class USTORERES {
public:
   STORERES *store;
   POOLMEM *store_source;

   /* Methods */
   USTORERES() { store = NULL; store_source = get_pool_memory(PM_MESSAGE);
              *store_source = 0; };
   ~USTORERES() { destroy(); }
   void set_source(const char *where);
   void destroy();
};

inline void USTORERES::destroy()
{
   if (store_source) {
      free_pool_memory(store_source);
      store_source = NULL;
   }
}

inline void USTORERES::set_source(const char *where)
{
   if (!store_source) {
      store_source = get_pool_memory(PM_MESSAGE);
   }
   pm_strcpy(store_source, where);
}

/*
 * Job Resource
 */
class JOBRES : public BRSRES {
public:
   uint32_t Protocol;                 /* Protocol to use to connect */
   uint32_t JobType;                  /* Job type (backup, verify, restore) */
   uint32_t JobLevel;                 /* default backup/verify level */
   int32_t Priority;                  /* Job priority */
   uint32_t RestoreJobId;             /* What -- JobId to restore */
   int32_t RescheduleTimes;           /* Number of times to reschedule job */
   uint32_t replace;                  /* How (overwrite, ..) */
   uint32_t selection_type;

   char *RestoreWhere;                /* Where on disk to restore -- directory */
   char *RegexWhere;                  /* RegexWhere option */
   char *strip_prefix;                /* Remove prefix from filename  */
   char *add_prefix;                  /* add prefix to filename  */
   char *add_suffix;                  /* add suffix to filename -- .old */
   char *backup_format;               /* Format of backup to use for protocols supporting multiple backup formats */
   char *RestoreBootstrap;            /* Bootstrap file */
   char *WriteBootstrap;              /* Where to write bootstrap Job updates */
   char *WriteVerifyList;             /* List of changed files */
   utime_t MaxRunTime;                /* Max run time in seconds */
   utime_t MaxWaitTime;               /* Max blocking time in seconds */
   utime_t FullMaxRunTime;            /* Max Full job run time */
   utime_t DiffMaxRunTime;            /* Max Differential job run time */
   utime_t IncMaxRunTime;             /* Max Incremental job run time */
   utime_t MaxStartDelay;             /* Max start delay in seconds */
   utime_t MaxRunSchedTime;           /* Max run time in seconds from Scheduled time*/
   utime_t RescheduleInterval;        /* Reschedule interval */
   utime_t MaxFullInterval;           /* Maximum time interval between Fulls */
   utime_t MaxVFullInterval;          /* Maximum time interval between Virtual Fulls */
   utime_t MaxDiffInterval;           /* Maximum time interval between Diffs */
   utime_t DuplicateJobProximity;     /* Permitted time between duplicicates */
   utime_t AlwaysIncrementalJobRetention; /* Timeinterval where incrementals are not consolidated */
   utime_t AlwaysIncrementalMaxFullAge; /* If Full Backup is older than this age the consolidation job will include also the full */
   int64_t spool_size;                /* Size of spool file for this job */
   int64_t max_bandwidth;             /* Speed limit on this job */
   int64_t FileHistSize;              /* Hint about the size of the expected File history */
   int32_t MaxConcurrentJobs;         /* Maximum concurrent jobs */
   int32_t MaxConcurrentCopies;       /* Limit number of concurrent jobs one Copy Job spawns */
   int32_t AlwaysIncrementalKeepNumber;/* Number of incrementals that are always left and not consolidated */

   MSGSRES *messages;                 /* How and where to send messages */
   SCHEDRES *schedule;                /* When -- Automatic schedule */
   CLIENTRES *client;                 /* Who to backup */
   FILESETRES *fileset;               /* What to backup -- Fileset */
   CATRES *catalog;                   /* Which Catalog to use */
   alist *storage;                    /* Where is device -- list of Storage to be used */
   POOLRES *pool;                     /* Where is media -- Media Pool */
   POOLRES *full_pool;                /* Pool for Full backups */
   POOLRES *vfull_pool;               /* Pool for Virtual Full backups */
   POOLRES *inc_pool;                 /* Pool for Incremental backups */
   POOLRES *diff_pool;                /* Pool for Differental backups */
   POOLRES *next_pool;                /* Next Pool for Copy/Migration Jobs and Virtual backups */
   char *selection_pattern;
   JOBRES *verify_job;                /* Job name to verify */
   JOBRES *jobdefs;                   /* Job defaults */
   alist *run_cmds;                   /* Run commands */
   alist *RunScripts;                 /* Run {client} program {after|before} Job */
   alist *FdPluginOptions;            /* Generic FD plugin options used by this Job */
   alist *SdPluginOptions;            /* Generic SD plugin options used by this Job */
   alist *DirPluginOptions;           /* Generic DIR plugin options used by this Job */
   alist *base;                       /* Base jobs */

   bool allow_mixed_priority;         /* Allow jobs with higher priority concurrently with this */
   bool where_use_regexp;             /* true if RestoreWhere is a BREGEXP */
   bool RescheduleOnError;            /* Set to reschedule on error */
   bool RescheduleIncompleteJobs;     /* Set to reschedule incomplete Jobs */
   bool PrefixLinks;                  /* Prefix soft links with Where path */
   bool PruneJobs;                    /* Force pruning of Jobs */
   bool PruneFiles;                   /* Force pruning of Files */
   bool PruneVolumes;                 /* Force pruning of Volumes */
   bool SpoolAttributes;              /* Set to spool attributes in SD */
   bool spool_data;                   /* Set to spool data in SD */
   bool rerun_failed_levels;          /* Upgrade to rerun failed levels */
   bool PreferMountedVolumes;         /* Prefer vols mounted rather than new one */
   bool write_part_after_job;         /* Set to write part after job in SD */
   bool enabled;                      /* Set if job enabled */
   bool accurate;                     /* Set if it is an accurate backup job */
   bool AllowDuplicateJobs;           /* Allow duplicate jobs */
   bool AllowHigherDuplicates;        /* Permit Higher Level */
   bool CancelLowerLevelDuplicates;   /* Cancel lower level backup jobs */
   bool CancelQueuedDuplicates;       /* Cancel queued jobs */
   bool CancelRunningDuplicates;      /* Cancel Running jobs */
   bool PurgeMigrateJob;              /* Purges source job on completion */
   bool IgnoreDuplicateJobChecking;   /* Ignore Duplicate Job Checking */
   bool SaveFileHist;                 /* Ability to disable File history saving for certain protocols */
   bool AlwaysIncremental;            /* Always incremental with regular consolidation */

   runtime_job_status_t *rjs;         /* Runtime Job Status */

   /* Methods */
};

#undef  MAX_FOPTS
#define MAX_FOPTS 40

/*
 * File options structure
 */
struct FOPTS {
   char opts[MAX_FOPTS];              /* Options string */
   alist regex;                       /* Regex string(s) */
   alist regexdir;                    /* Regex string(s) for directories */
   alist regexfile;                   /* Regex string(s) for files */
   alist wild;                        /* Wild card strings */
   alist wilddir;                     /* Wild card strings for directories */
   alist wildfile;                    /* Wild card strings for files */
   alist wildbase;                    /* Wild card strings for files without '/' */
   alist base;                        /* List of base names */
   alist fstype;                      /* File system type limitation */
   alist drivetype;                   /* Drive type limitation */
   alist meta;                        /* Backup meta information */
   char *reader;                      /* Reader program */
   char *writer;                      /* Writer program */
   char *plugin;                      /* Plugin program */
};

/*
 * This is either an include item or an exclude item
 */
struct INCEXE {
   FOPTS *current_opts;               /* Points to current options structure */
   FOPTS **opts_list;                 /* Options list */
   int32_t num_opts;                  /* Number of options items */
   alist name_list;                   /* Filename list -- holds char * */
   alist plugin_list;                 /* Filename list for plugins */
   alist ignoredir;                   /* Ignoredir string */
};

/*
 * FileSet Resource
 */
class FILESETRES : public BRSRES {
public:
   bool new_include;                  /* Set if new include used */
   INCEXE **include_items;            /* Array of incexe structures */
   int32_t num_includes;              /* Number in array */
   INCEXE **exclude_items;
   int32_t num_excludes;
   bool have_MD5;                     /* Set if MD5 initialized */
   MD5_CTX md5c;                      /* MD5 of include/exclude */
   char MD5[30];                      /* Base 64 representation of MD5 */
   bool ignore_fs_changes;            /* Don't force Full if FS changed */
   bool enable_vss;                   /* Enable Volume Shadow Copy */

   /* Methods */
   bool print_config(POOL_MEM& buff, bool hide_sensitive_data);
};

/*
 * Schedule Resource
 */
class SCHEDRES: public BRSRES {
public:
   RUNRES *run;
   bool enabled;                      /* Set if schedule is enabled */
};

/*
 * Counter Resource
 */
class COUNTERRES: public BRSRES {
public:
   int32_t MinValue;                  /* Minimum value */
   int32_t MaxValue;                  /* Maximum value */
   int32_t CurrentValue ;             /* Current value */
   COUNTERRES *WrapCounter;           /* Wrap counter name */
   CATRES *Catalog;                   /* Where to store */
   bool created;                      /* Created in DB */
};

/*
 * Pool Resource
 */
class POOLRES: public BRSRES {
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
   POOLRES *NextPool;                 /* Next pool for migration */
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
   POOLRES *RecyclePool;              /* RecyclePool destination when media is purged */
   POOLRES *ScratchPool;              /* ScratchPool source when requesting media */
   CATRES *catalog;                   /* Catalog to be used */
   utime_t FileRetention;             /* File retention period in seconds */
   utime_t JobRetention;              /* Job retention period in seconds */
   uint32_t MinBlocksize;             /* Minimum Blocksize */
   uint32_t MaxBlocksize;             /* Maximum Blocksize */
};

/*
 * Run structure contained in Schedule Resource
 */
class RUNRES: public BRSRES {
public:
   RUNRES *next;                      /* points to next run record */
   uint32_t level;                    /* level override */
   int32_t Priority;                  /* priority override */
   uint32_t job_type;
   utime_t MaxRunSchedTime;           /* max run time in sec from Sched time */
   bool MaxRunSchedTime_set;          /* MaxRunSchedTime given */
   bool spool_data;                   /* Data spooling override */
   bool spool_data_set;               /* Data spooling override given */
   bool accurate;                     /* accurate */
   bool accurate_set;                 /* accurate given */

   POOLRES *pool;                     /* Pool override */
   POOLRES *full_pool;                /* Full Pool override */
   POOLRES *vfull_pool;               /* Virtual Full Pool override */
   POOLRES *inc_pool;                 /* Incr Pool override */
   POOLRES *diff_pool;                /* Diff Pool override */
   POOLRES *next_pool;                /* Next Pool override */
   STORERES *storage;                 /* Storage override */
   MSGSRES *msgs;                     /* Messages override */
   char *since;
   uint32_t level_no;
   uint32_t minute;                   /* minute to run job */
   time_t last_run;                   /* last time run */
   time_t next_run;                   /* next time to run */
   char hour[nbytes_for_bits(24 + 1)];  /* bit set for each hour */
   char mday[nbytes_for_bits(31 + 1)];  /* bit set for each day of month */
   char month[nbytes_for_bits(12 + 1)]; /* bit set for each month */
   char wday[nbytes_for_bits(7 + 1)];   /* bit set for each day of the week */
   char wom[nbytes_for_bits(5 + 1)];    /* week of month */
   char woy[nbytes_for_bits(54 + 1)];   /* week of year */
   bool last_set;                       /* last week of month */
};

/*
 * Define the Union of all the above
 * resource structure definitions.
 */
union URES {
   DIRRES res_dir;
   CONRES res_con;
   PROFILERES res_profile;
   CLIENTRES res_client;
   STORERES res_store;
   CATRES res_cat;
   JOBRES res_job;
   FILESETRES res_fs;
   SCHEDRES res_sch;
   POOLRES res_pool;
   MSGSRES res_msgs;
   COUNTERRES res_counter;
   DEVICERES res_dev;
   RES hdr;
};

void init_dir_config(CONFIG *config, const char *configfile, int exit_code);

#define GetPoolResWithName(x) ((POOLRES *)GetResWithName(R_POOL, (x)))
#define GetStoreResWithName(x) ((STORERES *)GetResWithName(R_STORAGE, (x)))
#define GetClientResWithName(x) ((CLIENTRES *)GetResWithName(R_CLIENT, (x)))
#define GetJobResWithName(x) ((JOBRES *)GetResWithName(R_JOB, (x)))
#define GetFileSetResWithName(x) ((FILESETRES *)GetResWithName(R_FILESET, (x)))
#define GetCatalogResWithName(x) ((CATRES *)GetResWithName(R_CATALOG, (x)))
#define GetScheduleResWithName(x) ((SCHEDRES *)GetResWithName(R_SCHEDULE, (x)))
