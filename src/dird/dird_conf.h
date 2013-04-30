/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Director specific configuration and defines
 *
 *     Kern Sibbald, Feb MM
 *
 */

/* NOTE:  #includes at the end of this file */

/*
 * Resource codes -- they must be sequential for indexing
 */
enum {
   R_DIRECTOR = 1001,
   R_CLIENT,
   R_JOB,
   R_STORAGE,
   R_CATALOG,
   R_SCHEDULE,
   R_FILESET,
   R_POOL,
   R_MSGS,
   R_COUNTER,
   R_CONSOLE,
   R_JOBDEFS,
   R_DEVICE,
   R_FIRST = R_DIRECTOR,
   R_LAST  = R_DEVICE                 /* keep this updated */
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


/* Used for certain KeyWord tables */
struct s_kw {
   const char *name;
   uint32_t token;
};

/* Job Level keyword structure */
struct s_jl {
   const char *level_name;                 /* level keyword */
   int32_t  level;                         /* level */
   int32_t  job_type;                      /* JobType permitting this level */
};

/* Job Type keyword structure */
struct s_jt {
   const char *type_name;
   int32_t job_type;
};

/* Definition of the contents of each Resource */
/* Needed for forward references */
class SCHED;
class CLIENT;
class FILESET;
class POOL;
class RUN;
class DEVICE;
class RUNSCRIPT;

/*
 *   Director Resource
 *
 */
class DIRRES {
public:
   RES   hdr;
   dlist *DIRaddrs;
   dlist *DIRsrc_addr;                /* address to source connections from */
   char *password;                    /* Password for UA access */
   char *query_file;                  /* SQL query file */
   char *working_directory;           /* WorkingDirectory */
   const char *scripts_directory;     /* ScriptsDirectory */
   const char *plugin_directory;      /* Plugin Directory */
   char *pid_directory;               /* PidDirectory */
   char *subsys_directory;            /* SubsysDirectory */
   MSGS *messages;                    /* Daemon message handler */
   uint32_t MaxConcurrentJobs;        /* Max concurrent jobs for whole director */
   uint32_t MaxConsoleConnect;        /* Max concurrent console session */
   utime_t FDConnectTimeout;          /* timeout for connect in seconds */
   utime_t SDConnectTimeout;          /* timeout in seconds */
   utime_t heartbeat_interval;        /* Interval to send heartbeats */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Server Certificate File */
   char *tls_keyfile;                 /* TLS Server Key File */
   char *tls_dhfile;                  /* TLS Diffie-Hellman Parameters */
   alist *tls_allowed_cns;            /* TLS Allowed Clients */
   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
   utime_t stats_retention;           /* Stats retention period in seconds */
   bool tls_authenticate;             /* Authenticated with TLS */
   bool tls_enable;                   /* Enable TLS */
   bool tls_require;                  /* Require TLS */
   bool tls_verify_peer;              /* TLS Verify Client Certificate */
   char *verid;                       /* Custom Id to print in version command */
   /* Methods */
   char *name() const;
};

inline char *DIRRES::name() const { return hdr.name; }

/*
 * Device Resource
 *  This resource is a bit different from the other resources
 *  because it is not defined in the Director 
 *  by DEVICE { ... }, but rather by a "reference" such as
 *  DEVICE = xxx; Then when the Director connects to the
 *  SD, it requests the information about the device.
 */
class DEVICE {
public:
   RES hdr;

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

   /* Methods */
   char *name() const;
};

inline char *DEVICE::name() const { return hdr.name; }

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
 *    Console Resource
 */
class CONRES {
public:
   RES   hdr;
   char *password;                    /* UA server password */
   alist *ACL_lists[Num_ACL];         /* pointers to ACLs */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Server Certificate File */
   char *tls_keyfile;                 /* TLS Server Key File */
   char *tls_dhfile;                  /* TLS Diffie-Hellman Parameters */
   alist *tls_allowed_cns;            /* TLS Allowed Clients */
   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
   bool tls_authenticate;             /* Authenticated with TLS */
   bool tls_enable;                   /* Enable TLS */
   bool tls_require;                  /* Require TLS */
   bool tls_verify_peer;              /* TLS Verify Client Certificate */

   /* Methods */
   char *name() const;
};

inline char *CONRES::name() const { return hdr.name; }


/*
 *   Catalog Resource
 *
 */
class CAT {
public:
   RES   hdr;

   uint32_t db_port;                  /* Port */
   char *db_address;                  /* host name for remote access */
   char *db_socket;                   /* Socket for local access */
   char *db_password;
   char *db_user;
   char *db_name;
   char *db_driver;                   /* Select appropriate driver */
   uint32_t mult_db_connections;      /* set if multiple connections wanted */
   bool disable_batch_insert;         /* set if batch inserts should be disabled */

   /* Methods */
   char *name() const;
   char *display(POOLMEM *dst);       /* Get catalog information */
};

inline char *CAT::name() const { return hdr.name; }


/*
 *   Client Resource
 *
 */
class CLIENT {
public:
   RES   hdr;

   uint32_t FDport;                   /* Where File daemon listens */
   utime_t FileRetention;             /* file retention period in seconds */
   utime_t JobRetention;              /* job retention period in seconds */
   utime_t heartbeat_interval;        /* Interval to send heartbeats */
   char *address;
   char *password;
   CAT *catalog;                      /* Catalog resource */
   int32_t MaxConcurrentJobs;         /* Maximum concurrent jobs */
   int32_t NumConcurrentJobs;         /* number of concurrent jobs running */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Client Certificate File */
   char *tls_keyfile;                 /* TLS Client Key File */
   alist *tls_allowed_cns;            /* TLS Allowed Clients */
   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
   bool tls_authenticate;             /* Authenticated with TLS */
   bool tls_enable;                   /* Enable TLS */
   bool tls_require;                  /* Require TLS */
   bool AutoPrune;                    /* Do automatic pruning? */

   /* Methods */
   char *name() const;
};

inline char *CLIENT::name() const { return hdr.name; }


/*
 *   Store Resource
 *
 */
class STORE {
public:
   RES   hdr;

   uint32_t SDport;                   /* port where Directors connect */
   uint32_t SDDport;                  /* data port for File daemon */
   char *address;
   char *password;
   char *media_type;
   alist *device;                     /* Alternate devices for this Storage */
   int32_t MaxConcurrentJobs;         /* Maximum concurrent jobs */
   int32_t MaxConcurrentReadJobs;     /* Maximum concurrent jobs reading */
   int32_t NumConcurrentJobs;         /* number of concurrent jobs running */
   int32_t NumConcurrentReadJobs;     /* number of jobs reading */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Client Certificate File */
   char *tls_keyfile;                 /* TLS Client Key File */
   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
   bool tls_authenticate;             /* Authenticated with TLS */
   bool tls_enable;                   /* Enable TLS */
   bool tls_require;                  /* Require TLS */
   bool enabled;                      /* Set if device is enabled */
   bool  autochanger;                 /* set if autochanger */
   bool AllowCompress;                /* set if this Storage should allow jobs to enable compression */
   int64_t StorageId;                 /* Set from Storage DB record */
   utime_t heartbeat_interval;        /* Interval to send heartbeats */
   uint32_t drives;                   /* number of drives in autochanger */

   /* Methods */
   char *dev_name() const;
   char *name() const;
};

inline char *STORE::dev_name() const
{ 
   DEVICE *dev = (DEVICE *)device->first();
   return dev->name();
}

inline char *STORE::name() const { return hdr.name; }

/*
 * This is a sort of "unified" store that has both the
 *  storage pointer and the text of where the pointer was
 *  found.
 */
class USTORE {
public:
   STORE *store;
   POOLMEM *store_source;

   /* Methods */
   USTORE() { store = NULL; store_source = get_pool_memory(PM_MESSAGE); 
              *store_source = 0; };
   ~USTORE() { destroy(); }   
   void set_source(const char *where);
   void destroy();
};

inline void USTORE::destroy()
{
   if (store_source) {
      free_pool_memory(store_source);
      store_source = NULL;
   }
}


inline void USTORE::set_source(const char *where)
{
   if (!store_source) {
      store_source = get_pool_memory(PM_MESSAGE);
   }
   pm_strcpy(store_source, where);
}


/*
 *   Job Resource
 */
class JOB {
public:
   RES   hdr;

   uint32_t   JobType;                /* job type (backup, verify, restore */
   uint32_t   JobLevel;               /* default backup/verify level */
   int32_t   Priority;                /* Job priority */
   uint32_t   RestoreJobId;           /* What -- JobId to restore */
   int32_t   RescheduleTimes;         /* Number of times to reschedule job */
   uint32_t   replace;                /* How (overwrite, ..) */
   uint32_t   selection_type;

   char *RestoreWhere;                /* Where on disk to restore -- directory */
   char *RegexWhere;                  /* RegexWhere option */
   char *strip_prefix;                /* remove prefix from filename  */
   char *add_prefix;                  /* add prefix to filename  */
   char *add_suffix;                  /* add suffix to filename -- .old */
   char *RestoreBootstrap;            /* Bootstrap file */
   char *PluginOptions;               /* Options to pass to plugin */
   union {
      char *WriteBootstrap;           /* Where to write bootstrap Job updates */
      char *WriteVerifyList;          /* List of changed files */
   };
   utime_t MaxRunTime;                /* max run time in seconds */
   utime_t MaxWaitTime;               /* max blocking time in seconds */
   utime_t FullMaxRunTime;            /* Max Full job run time */
   utime_t DiffMaxRunTime;            /* Max Differential job run time */
   utime_t IncMaxRunTime;             /* Max Incremental job run time */
   utime_t MaxStartDelay;             /* max start delay in seconds */
   utime_t MaxRunSchedTime;           /* max run time in seconds from Scheduled time*/
   utime_t RescheduleInterval;        /* Reschedule interval */
   utime_t MaxFullInterval;           /* Maximum time interval between Fulls */
   utime_t MaxDiffInterval;           /* Maximum time interval between Diffs */
   utime_t DuplicateJobProximity;     /* Permitted time between duplicicates */
   int64_t spool_size;                /* Size of spool file for this job */
   int32_t MaxConcurrentJobs;         /* Maximum concurrent jobs */
   int32_t NumConcurrentJobs;         /* number of concurrent jobs running */
   bool allow_mixed_priority;         /* Allow jobs with higher priority concurrently with this */

   MSGS      *messages;               /* How and where to send messages */
   SCHED     *schedule;               /* When -- Automatic schedule */
   CLIENT    *client;                 /* Who to backup */
   FILESET   *fileset;                /* What to backup -- Fileset */
   alist     *storage;                /* Where is device -- list of Storage to be used */
   POOL      *pool;                   /* Where is media -- Media Pool */
   POOL      *full_pool;              /* Pool for Full backups */
   POOL      *inc_pool;               /* Pool for Incremental backups */
   POOL      *diff_pool;              /* Pool for Differental backups */
   char      *selection_pattern;
   union {
      JOB    *verify_job;             /* Job name to verify */
   };
   JOB       *jobdefs;                /* Job defaults */
   alist     *run_cmds;               /* Run commands */
   alist *RunScripts;                 /* Run {client} program {after|before} Job */

   bool where_use_regexp;             /* true if RestoreWhere is a BREGEXP */
   bool RescheduleOnError;            /* Set to reschedule on error */
   bool RescheduleIncompleteJobs;     /* Set to reschedule incomplete Jobs */
   bool PrefixLinks;                  /* prefix soft links with Where path */
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

   alist *base;                       /* Base jobs */   

   /* Methods */
   char *name() const;
};

inline char *JOB::name() const { return hdr.name; }

#undef  MAX_FOPTS
#define MAX_FOPTS 40

/* File options structure */
struct FOPTS {
   char opts[MAX_FOPTS];              /* options string */
   alist regex;                       /* regex string(s) */
   alist regexdir;                    /* regex string(s) for directories */
   alist regexfile;                   /* regex string(s) for files */
   alist wild;                        /* wild card strings */
   alist wilddir;                     /* wild card strings for directories */
   alist wildfile;                    /* wild card strings for files */
   alist wildbase;                    /* wild card strings for files without '/' */
   alist base;                        /* list of base names */
   alist fstype;                      /* file system type limitation */
   alist drivetype;                   /* drive type limitation */
   char *reader;                      /* reader program */
   char *writer;                      /* writer program */
   char *plugin;                      /* plugin program */
};


/* This is either an include item or an exclude item */
struct INCEXE {
   FOPTS *current_opts;               /* points to current options structure */
   FOPTS **opts_list;                 /* options list */
   int32_t num_opts;                  /* number of options items */
   alist name_list;                   /* filename list -- holds char * */
   alist plugin_list;                 /* filename list for plugins */
   char *ignoredir;                   /* ignoredir string */
};

/*
 *   FileSet Resource
 *
 */
class FILESET {
public:
   RES   hdr;

   bool new_include;                  /* Set if new include used */
   INCEXE **include_items;            /* array of incexe structures */
   int32_t num_includes;              /* number in array */
   INCEXE **exclude_items;
   int32_t num_excludes;
   bool have_MD5;                     /* set if MD5 initialized */
   struct MD5Context md5c;            /* MD5 of include/exclude */
   char MD5[30];                      /* base 64 representation of MD5 */
   bool ignore_fs_changes;            /* Don't force Full if FS changed */
   bool enable_vss;                   /* Enable Volume Shadow Copy */

   /* Methods */
   char *name() const;
};

inline char *FILESET::name() const { return hdr.name; }

/*
 *   Schedule Resource
 *
 */
class SCHED {
public:
   RES   hdr;

   RUN *run;
};

/*
 *   Counter Resource
 */
class COUNTER {
public:
   RES   hdr;

   int32_t  MinValue;                 /* Minimum value */
   int32_t  MaxValue;                 /* Maximum value */
   int32_t  CurrentValue;             /* Current value */
   COUNTER *WrapCounter;              /* Wrap counter name */
   CAT     *Catalog;                  /* Where to store */
   bool     created;                  /* Created in DB */
   /* Methods */
   char *name() const;
};

inline char *COUNTER::name() const { return hdr.name; }

/*
 *   Pool Resource
 *
 */
class POOL {
public:
   RES   hdr;

   char *pool_type;                   /* Pool type */
   char *label_format;                /* Label format string */
   char *cleaning_prefix;             /* Cleaning label prefix */
   int32_t LabelType;                 /* Bacula/ANSI/IBM label type */
   uint32_t max_volumes;              /* max number of volumes */
   utime_t VolRetention;              /* volume retention period in seconds */
   utime_t VolUseDuration;            /* duration volume can be used */
   uint32_t MaxVolJobs;               /* Maximum jobs on the Volume */
   uint32_t MaxVolFiles;              /* Maximum files on the Volume */
   uint64_t MaxVolBytes;              /* Maximum bytes on the Volume */
   utime_t MigrationTime;             /* Time to migrate to next pool */
   uint64_t MigrationHighBytes;       /* When migration starts */
   uint64_t MigrationLowBytes;        /* When migration stops */
   POOL  *NextPool;                   /* Next pool for migration */
   alist *storage;                    /* Where is device -- list of Storage to be used */
   bool  use_catalog;                 /* maintain catalog for media */
   bool  catalog_files;               /* maintain file entries in catalog */
   bool  use_volume_once;             /* write on volume only once */
   bool  purge_oldest_volume;         /* purge oldest volume */
   bool  recycle_oldest_volume;       /* attempt to recycle oldest volume */
   bool  recycle_current_volume;      /* attempt recycle of current volume */
   bool  AutoPrune;                   /* default for pool auto prune */
   bool  Recycle;                     /* default for media recycle yes/no */
   uint32_t action_on_purge;          /* action on purge, e.g. truncate the disk volume */
   POOL  *RecyclePool;                /* RecyclePool destination when media is purged */
   POOL  *ScratchPool;                /* ScratchPool source when requesting media */
   alist *CopyPool;                   /* List of copy pools */
   CAT *catalog;                      /* Catalog to be used */
   utime_t FileRetention;             /* file retention period in seconds */
   utime_t JobRetention;              /* job retention period in seconds */

   /* Methods */
   char *name() const;
};

inline char *POOL::name() const { return hdr.name; }




/* Define the Union of all the above
 * resource structure definitions.
 */
union URES {
   DIRRES     res_dir;
   CONRES     res_con;
   CLIENT     res_client;
   STORE      res_store;
   CAT        res_cat;
   JOB        res_job;
   FILESET    res_fs;
   SCHED      res_sch;
   POOL       res_pool;
   MSGS       res_msgs;
   COUNTER    res_counter;
   DEVICE     res_dev;
   RES        hdr;
   RUNSCRIPT  res_runscript;
};



/* Run structure contained in Schedule Resource */
class RUN {
public:
   RUN *next;                         /* points to next run record */
   uint32_t level;                    /* level override */
   int32_t Priority;                  /* priority override */
   uint32_t job_type;
   utime_t MaxRunSchedTime;           /* max run time in sec from Sched time */
   bool MaxRunSchedTime_set;          /* MaxRunSchedTime given */
   bool spool_data;                   /* Data spooling override */
   bool spool_data_set;               /* Data spooling override given */
   bool accurate;                     /* accurate */
   bool accurate_set;                 /* accurate given */
   bool write_part_after_job;         /* Write part after job override */
   bool write_part_after_job_set;     /* Write part after job override given */
   
   POOL *pool;                        /* Pool override */
   POOL *full_pool;                   /* Pool override */
   POOL *inc_pool;                    /* Pool override */
   POOL *diff_pool;                   /* Pool override */
   STORE *storage;                    /* Storage override */
   MSGS *msgs;                        /* Messages override */
   char *since;
   uint32_t level_no;
   uint32_t minute;                   /* minute to run job */
   time_t last_run;                   /* last time run */
   time_t next_run;                   /* next time to run */
   char hour[nbytes_for_bits(24)];    /* bit set for each hour */
   char mday[nbytes_for_bits(31)];    /* bit set for each day of month */
   char month[nbytes_for_bits(12)];   /* bit set for each month */
   char wday[nbytes_for_bits(7)];     /* bit set for each day of the week */
   char wom[nbytes_for_bits(5)];      /* week of month */
   char woy[nbytes_for_bits(54)];     /* week of year */
};

#define GetPoolResWithName(x) ((POOL *)GetResWithName(R_POOL, (x)))
#define GetStoreResWithName(x) ((STORE *)GetResWithName(R_STORAGE, (x)))
#define GetClientResWithName(x) ((CLIENT *)GetResWithName(R_CLIENT, (x)))
#define GetJobResWithName(x) ((JOB *)GetResWithName(R_JOB, (x)))
#define GetFileSetResWithName(x) ((FILESET *)GetResWithName(R_FILESET, (x)))
#define GetCatalogResWithName(x) ((CAT *)GetResWithName(R_CATALOG, (x)))
