/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.
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
 * Bareos File Daemon specific configuration
 *
 * Kern Sibbald, Sep MM
 */

/*
 * Resource codes -- they must be sequential for indexing
 */
enum {
   R_DIRECTOR = 1001,
   R_CLIENT,
   R_MSGS,
   R_FIRST = R_DIRECTOR,
   R_LAST = R_MSGS                    /* keep this updated */
};

/*
 * Some resource attributes
 */
enum {
   R_NAME = 1020,
   R_ADDRESS,
   R_PASSWORD,
   R_TYPE
};

/* Used for certain KeyWord tables */
struct s_kw {
   const char *name;
   uint32_t token;
};

/* Definition of the contents of each Resource */
struct DIRRES {
   RES hdr;

   char *password;                    /* Director password */
   char *address;                     /* Director address or zero */
   bool monitor;                      /* Have only access to status and .status functions */
   bool tls_authenticate;             /* Authenticate with TSL */
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
   alist *allowed_script_dirs;        /* Only allow to run scripts in this directories */
   alist *allowed_job_cmds;           /* Only allow the following Job commands to be executed */
   uint64_t max_bandwidth_per_job;    /* Bandwidth limitation (per director) */
   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
};

struct CLIENTRES {
   RES hdr;

   dlist *FDaddrs;
   dlist *FDsrc_addr;                 /* Address to source connections from */
   char *working_directory;
   char *pid_directory;
   char *subsys_directory;
   char *plugin_directory;            /* Plugin directory */
   char *plugin_names;
   char *scripts_directory;
   MSGSRES *messages;                 /* Daemon message handler */
   uint32_t MaxConcurrentJobs;
   utime_t SDConnectTimeout;          /* Timeout in seconds */
   utime_t heartbeat_interval;        /* Interval to send heartbeats */
   uint32_t max_network_buffer_size;  /* Max network buf size */
   bool compatible;                   /* Support old protocol keywords */
   bool allow_bw_bursting;            /* Allow bursting with bandwidth limiting */
   bool pki_sign;                     /* Enable Data Integrity Verification via Digital Signatures */
   bool pki_encrypt;                  /* Enable Data Encryption */
   char *pki_keypair_file;            /* PKI Key Pair File */
   alist *pki_signing_key_files;      /* PKI Signing Key Files */
   alist *pki_master_key_files;       /* PKI Master Key Files */
   crypto_cipher_t pki_cipher;        /* PKI Cipher to use */
   bool tls_authenticate;             /* Authenticate with TLS */
   bool tls_enable;                   /* Enable TLS */
   bool tls_require;                  /* Require TLS */
   bool tls_verify_peer;              /* TLS Verify Peer Certificate */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_crlfile;                 /* TLS CA Certificate Revocation List File */
   char *tls_certfile;                /* TLS Client Certificate File */
   char *tls_keyfile;                 /* TLS Client Key File */
   X509_KEYPAIR *pki_keypair;         /* Shared PKI Public/Private Keypair */
   alist *pki_signers;                /* Shared PKI Trusted Signers */
   alist *pki_recipients;             /* Shared PKI Recipients */
   alist *allowed_script_dirs;        /* Only allow to run scripts in this directories */
   alist *allowed_job_cmds;           /* Only allow the following Job commands to be executed */
   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
   char *verid;                       /* Custom Id to print in version command */
   uint64_t max_bandwidth_per_job;    /* Bandwidth limitation (global) */
};

/* Define the Union of all the above
 * resource structure definitions.
 */
union URES {
   DIRRES res_dir;
   CLIENTRES res_client;
   MSGSRES res_msgs;
   RES hdr;
};
