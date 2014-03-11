/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Resource codes -- they must be sequential for indexing
 */

enum {
   R_DIRECTOR = 3001,
   R_NDMP,
   R_STORAGE,
   R_DEVICE,
   R_MSGS,
   R_AUTOCHANGER,
   R_FIRST = R_DIRECTOR,
   R_LAST = R_AUTOCHANGER             /* keep this updated */
};

enum {
   R_NAME = 3020,
   R_ADDRESS,
   R_PASSWORD,
   R_TYPE,
   R_BACKUP
};

/* Definition of the contents of each Resource */
class DIRRES {
public:
   RES hdr;

   char *password;                    /* Director password */
   char *address;                     /* Director IP address or zero */
   bool monitor;                      /* Have only access to status and .status functions */
   bool tls_authenticate;             /* Authenticate with TLS */
   bool tls_enable;                   /* Enable TLS */
   bool tls_require;                  /* Require TLS */
   bool tls_verify_peer;              /* TLS Verify Peer Certificate */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_crlfile;                 /* TLS CA Certificate Revocation List File */
   char *tls_certfile;                /* TLS Server Certificate File */
   char *tls_keyfile;                 /* TLS Server Key File */
   char *tls_dhfile;                  /* TLS Diffie-Hellman Parameters */
   alist *tls_allowed_cns;            /* TLS Allowed Clients */
   uint64_t max_bandwidth_per_job;    /* Bandwidth limitation (per director) */
   char *keyencrkey;                  /* Key Encryption Key */

   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
};

class NDMPRES {
public:
   RES hdr;

   uint32_t AuthType;                 /* Authentication Type to use */
   uint32_t LogLevel;                 /* Log level to use for logging NDMP protocol msgs */
   char *username;                    /* NDMP username */
   char *password;                    /* NDMP password */
};

/* Storage daemon "global" definitions */
class STORES {
public:
   RES hdr;

   dlist *SDaddrs;
   dlist *SDsrc_addr;                 /* Address to source connections from */
   dlist *NDMPaddrs;
   char *working_directory;           /* Working directory for checkpoints */
   char *pid_directory;
   char *subsys_directory;
   char *plugin_directory;            /* Plugin directory */
   char *plugin_names;
   char *scripts_directory;
   uint32_t max_concurrent_jobs;      /* Maximum concurrent jobs to run */
   uint32_t ndmploglevel;             /* Initial NDMP log level */
   uint32_t jcr_watchdog_time;        /* Absolute time after which a Job gets terminated regardless of its progress */
   MSGSRES *messages;                 /* Daemon message handler */
   utime_t SDConnectTimeout;          /* Timeout in seconds */
   utime_t FDConnectTimeout;          /* Timeout in seconds */
   utime_t heartbeat_interval;        /* Interval to send hb to FD */
   utime_t client_wait;               /* Time to wait for FD to connect */
   uint32_t max_network_buffer_size;  /* Max network buf size */
   bool compatible;                   /* Write compatible format */
   bool allow_bw_bursting;            /* Allow bursting with bandwidth limiting */
   bool ndmp_enable;                  /* Enable NDMP protocol listener */
   bool ndmp_snooping;                /* Enable NDMP protocol snooping */
   bool tls_authenticate;             /* Authenticate with TLS */
   bool tls_enable;                   /* Enable TLS */
   bool tls_require;                  /* Require TLS */
   bool tls_verify_peer;              /* TLS Verify Peer Certificate */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_crlfile;                 /* TLS CA Certificate Revocation List File */
   char *tls_certfile;                /* TLS Server Certificate File */
   char *tls_keyfile;                 /* TLS Server Key File */
   char *tls_dhfile;                  /* TLS Diffie-Hellman Parameters */
   alist *tls_allowed_cns;            /* TLS Allowed Clients */
   char *verid;                       /* Custom Id to print in version command */
   uint64_t max_bandwidth_per_job;    /* Bandwidth limitation (global) */

   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
};

class AUTOCHANGERRES {
public:
   RES hdr;

   alist *device;                     /* List of DEVRES device pointers */
   char *changer_name;                /* Changer device name */
   char *changer_command;             /* Changer command  -- external program */
   brwlock_t changer_lock;            /* One changer operation at a time */
};

/* Device specific definitions */
class DEVRES {
public:
   RES hdr;

   char *media_type;                  /* User assigned media type */
   char *device_name;                 /* Archive device name */
   char *changer_name;                /* Changer device name */
   char *changer_command;             /* Changer command  -- external program */
   char *alert_command;               /* Alert command -- external program */
   char *spool_directory;             /* Spool file directory */
   uint32_t dev_type;                 /* device type */
   uint32_t label_type;               /* label type */
   bool autoselect;                   /* Automatically select from AutoChanger */
   bool norewindonclose;              /* Don't rewind tape drive on close */
   bool drive_crypto_enabled;         /* Enable hardware crypto */
   bool query_crypto_status;          /* Query device for crypto status */
   uint32_t drive_index;              /* Autochanger drive index */
   uint32_t cap_bits;                 /* Capabilities of this device */
   utime_t max_changer_wait;          /* Changer timeout */
   utime_t max_rewind_wait;           /* Maximum secs to wait for rewind */
   utime_t max_open_wait;             /* Maximum secs to wait for open */
   uint32_t max_open_vols;            /* Maximum simultaneous open volumes */
   uint32_t min_block_size;           /* Min block size */
   uint32_t max_block_size;           /* Max block size */
   uint32_t max_volume_jobs;          /* Max jobs to put on one volume */
   uint32_t max_network_buffer_size;  /* Max network buf size */
   uint32_t max_concurrent_jobs;      /* Maximum concurrent jobs this drive */
   utime_t vol_poll_interval;         /* Interval between polling volume during mount */
   int64_t max_volume_files;          /* Max files to put on one volume */
   int64_t max_volume_size;           /* Max bytes to put on one volume */
   int64_t max_file_size;             /* Max file size in bytes */
   int64_t volume_capacity;           /* Advisory capacity */
   int64_t max_spool_size;            /* Max spool size for all jobs */
   int64_t max_job_spool_size;        /* Max spool size for any single job */

   int64_t max_part_size;             /* Max part size */
   char *mount_point;                 /* Mount point for require mount devices */
   char *mount_command;               /* Mount command */
   char *unmount_command;             /* Unmount command */
   char *write_part_command;          /* Write part command */
   char *free_space_command;          /* Free space command */

   /* The following are set at runtime */
   DEVICE *dev;                       /* Pointer to phyical dev -- set at runtime */
   AUTOCHANGERRES *changer_res;       /* Pointer to changer res if any */
};

union URES {
   DIRRES res_dir;
   NDMPRES res_ndmp;
   STORES res_store;
   DEVRES res_dev;
   MSGSRES res_msgs;
   AUTOCHANGERRES res_changer;
   RES hdr;
};

/* Used for certain KeyWord tables */
struct s_kw {
   const char *name;
   uint32_t token;
};
