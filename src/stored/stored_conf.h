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
 * Resource codes -- they must be sequential for indexing
 *
 */

enum {
   R_DIRECTOR = 3001,
   R_STORAGE,
   R_DEVICE,
   R_MSGS,
   R_AUTOCHANGER,
   R_FIRST = R_DIRECTOR,
   R_LAST  = R_AUTOCHANGER            /* keep this updated */
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
   RES   hdr;

   char *password;                    /* Director password */
   char *address;                     /* Director IP address or zero */
   bool monitor;                      /* Have only access to status and .status functions */
   bool tls_authenticate;             /* Authenticate with TLS */
   bool tls_enable;                   /* Enable TLS */
   bool tls_require;                  /* Require TLS */
   bool tls_verify_peer;              /* TLS Verify Client Certificate */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Server Certificate File */
   char *tls_keyfile;                 /* TLS Server Key File */
   char *tls_dhfile;                  /* TLS Diffie-Hellman Parameters */
   alist *tls_allowed_cns;            /* TLS Allowed Clients */

   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
};


/* Storage daemon "global" definitions */
class s_res_store {
public:
   RES   hdr;

   dlist *sdaddrs;
   dlist *sddaddrs;
   char *working_directory;           /* working directory for checkpoints */
   char *pid_directory;
   char *subsys_directory;
   char *plugin_directory;            /* Plugin directory */
   char *scripts_directory;
   uint32_t max_concurrent_jobs;      /* maximum concurrent jobs to run */
   MSGS *messages;                    /* Daemon message handler */
   utime_t heartbeat_interval;        /* Interval to send hb to FD */
   utime_t client_wait;               /* Time to wait for FD to connect */
   bool tls_authenticate;             /* Authenticate with TLS */
   bool tls_enable;                   /* Enable TLS */
   bool tls_require;                  /* Require TLS */
   bool tls_verify_peer;              /* TLS Verify Client Certificate */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Server Certificate File */
   char *tls_keyfile;                 /* TLS Server Key File */
   char *tls_dhfile;                  /* TLS Diffie-Hellman Parameters */
   alist *tls_allowed_cns;            /* TLS Allowed Clients */
   char *verid;                       /* Custom Id to print in version command */
   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
};
typedef class s_res_store STORES;

class AUTOCHANGER {
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
   RES   hdr;

   char *media_type;                  /* User assigned media type */
   char *device_name;                 /* Archive device name */
   char *changer_name;                /* Changer device name */
   char *changer_command;             /* Changer command  -- external program */
   char *alert_command;               /* Alert command -- external program */
   char *spool_directory;             /* Spool file directory */
   uint32_t dev_type;                 /* device type */
   uint32_t label_type;               /* label type */
   bool autoselect;                   /* Automatically select from AutoChanger */
   uint32_t drive_index;              /* Autochanger drive index */
   uint32_t cap_bits;                 /* Capabilities of this device */
   utime_t max_changer_wait;          /* Changer timeout */
   utime_t max_rewind_wait;           /* maximum secs to wait for rewind */
   utime_t max_open_wait;             /* maximum secs to wait for open */
   uint32_t max_open_vols;            /* maximum simultaneous open volumes */
   uint32_t min_block_size;           /* min block size */
   uint32_t max_block_size;           /* max block size */
   uint32_t max_volume_jobs;          /* max jobs to put on one volume */
   uint32_t max_network_buffer_size;  /* max network buf size */
   uint32_t max_concurrent_jobs;      /* maximum concurrent jobs this drive */
   utime_t  vol_poll_interval;        /* interval between polling volume during mount */
   int64_t max_volume_files;          /* max files to put on one volume */
   int64_t max_volume_size;           /* max bytes to put on one volume */
   int64_t max_file_size;             /* max file size in bytes */
   int64_t volume_capacity;           /* advisory capacity */
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
   AUTOCHANGER *changer_res;          /* pointer to changer res if any */
};


union URES {
   DIRRES      res_dir;
   STORES      res_store;
   DEVRES      res_dev;
   MSGS        res_msgs;
   AUTOCHANGER res_changer;
   RES         hdr;
};
