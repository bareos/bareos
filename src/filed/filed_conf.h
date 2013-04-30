/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.

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
 * Bacula File Daemon specific configuration
 *
 *     Kern Sibbald, Sep MM
 *
 *   Version $Id$
 */

/*
 * Resource codes -- they must be sequential for indexing
 */
#define R_FIRST                       1001

#define R_DIRECTOR                    1001
#define R_CLIENT                      1002
#define R_MSGS                        1003

#define R_LAST                        R_MSGS

/*
 * Some resource attributes
 */
#define R_NAME                        1020
#define R_ADDRESS                     1021
#define R_PASSWORD                    1022
#define R_TYPE                        1023


/* Definition of the contents of each Resource */
struct DIRRES {
   RES   hdr;
   char *password;                    /* Director password */
   char *address;                     /* Director address or zero */
   bool monitor;                      /* Have only access to status and .status functions */
   bool tls_authenticate;             /* Authenticate with TSL */
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

struct CLIENT {
   RES   hdr;
   dlist *FDaddrs;
   dlist *FDsrc_addr;                 /* address to source connections from */
   char *working_directory;
   char *pid_directory;
   char *subsys_directory;
   char *plugin_directory;            /* Plugin directory */
   char *scripts_directory;
   MSGS *messages;                    /* daemon message handler */
   uint32_t MaxConcurrentJobs;
   utime_t SDConnectTimeout;          /* timeout in seconds */
   utime_t heartbeat_interval;        /* Interval to send heartbeats */
   uint32_t max_network_buffer_size;  /* max network buf size */
   bool pki_sign;                     /* Enable Data Integrity Verification via Digital Signatures */
   bool pki_encrypt;                  /* Enable Data Encryption */
   char *pki_keypair_file;            /* PKI Key Pair File */
   alist *pki_signing_key_files;      /* PKI Signing Key Files */
   alist *pki_master_key_files;       /* PKI Master Key Files */
   bool tls_authenticate;             /* Authenticate with TLS */
   bool tls_enable;                   /* Enable TLS */
   bool tls_require;                  /* Require TLS */
   char *tls_ca_certfile;             /* TLS CA Certificate File */
   char *tls_ca_certdir;              /* TLS CA Certificate Directory */
   char *tls_certfile;                /* TLS Client Certificate File */
   char *tls_keyfile;                 /* TLS Client Key File */

   X509_KEYPAIR *pki_keypair;         /* Shared PKI Public/Private Keypair */
   alist *pki_signers;                /* Shared PKI Trusted Signers */
   alist *pki_recipients;             /* Shared PKI Recipients */
   TLS_CONTEXT *tls_ctx;              /* Shared TLS Context */
   char *verid;                       /* Custom Id to print in version command */
};



/* Define the Union of all the above
 * resource structure definitions.
 */
union URES {
   DIRRES res_dir;
   CLIENT res_client;
   MSGS   res_msgs;
   RES    hdr;
};
