/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.
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
 * Bareos File Daemon specific configuration
 *
 * Kern Sibbald, Sep MM
 */

#ifndef BAREOS_FILED_FILED_CONF_H_
#define BAREOS_FILED_FILED_CONF_H_ 1

#include "lib/messages_resource.h"
#include "lib/tls_conf.h"

class alist;
class dlist;

namespace filedaemon {

static const std::string default_config_filename("bareos-fd.conf");

/*
 * Resource codes -- they must be sequential for indexing
 */
enum
{
  R_DIRECTOR = 1001,
  R_CLIENT,
  R_MSGS,
  R_STORAGE,
  R_JOB,
  R_FIRST = R_DIRECTOR,
  R_LAST = R_JOB /* keep this updated */
};

/*
 * Some resource attributes
 */
enum
{
  R_NAME = 1020,
  R_ADDRESS,
  R_PASSWORD,
  R_TYPE
};

/* Definition of the contents of each Resource */
class DirectorResource
    : public BareosResource
    , public TlsResource {
 public:
  char* address;            /* Director address or zero */
  uint32_t port;            /* Director port */
  bool conn_from_dir_to_fd; /* Allow incoming connections */
  bool conn_from_fd_to_dir; /* Connect to director */
  bool monitor; /* Have only access to status and .status functions */
  alist*
      allowed_script_dirs; /* Only allow to run scripts in this directories */
  alist* allowed_job_cmds; /* Only allow the following Job commands to be
                              executed */
  uint64_t max_bandwidth_per_job; /* Bandwidth limitation (per director) */

  DirectorResource() = default;
};

class ClientResource
    : public BareosResource
    , public TlsResource {
 public:
  dlist* FDaddrs;
  dlist* FDsrc_addr; /* Address to source connections from */
  char* working_directory;
  char* pid_directory;
  char* subsys_directory;
  char* plugin_directory; /* Plugin directory */
  alist* plugin_names;
  char* scripts_directory;
  MessagesResource* messages; /* Daemon message handler */
  uint32_t MaxConcurrentJobs;
  uint32_t MaxConnections;
  utime_t SDConnectTimeout;         /* Timeout in seconds */
  utime_t heartbeat_interval;       /* Interval to send heartbeats */
  uint32_t max_network_buffer_size; /* Max network buf size */
  uint32_t jcr_watchdog_time; /* Absolute time after which a Job gets terminated
                                 regardless of its progress */
  bool compatible;            /* Support old protocol keywords */
  bool allow_bw_bursting;     /* Allow bursting with bandwidth limiting */
  bool pki_sign; /* Enable Data Integrity Verification via Digital Signatures */
  bool pki_encrypt;             /* Enable Data Encryption */
  char* pki_keypair_file;       /* PKI Key Pair File */
  alist* pki_signing_key_files; /* PKI Signing Key Files */
  alist* pki_master_key_files;  /* PKI Master Key Files */
  crypto_cipher_t pki_cipher;   /* PKI Cipher to use */
  bool nokeepalive;             /* Don't use SO_KEEPALIVE on sockets */
  bool always_use_lmdb;         /* Use LMDB for accurate data */
  uint32_t lmdb_threshold;      /* Switch to using LDMD when number of accurate
                                   entries exceeds treshold. */
  X509_KEYPAIR* pki_keypair;    /* Shared PKI Public/Private Keypair */
  alist* pki_signers;           /* Shared PKI Trusted Signers */
  alist* pki_recipients;        /* Shared PKI Recipients */
  alist*
      allowed_script_dirs; /* Only allow to run scripts in this directories */
  alist* allowed_job_cmds; /* Only allow the following Job commands to be
                              executed */
  char* verid;             /* Custom Id to print in version command */
  char* secure_erase_cmdline; /* Cmdline to execute to perform secure erase of
                                 file */
  char* log_timestamp_format; /* Timestamp format to use in generic logging
                                 messages */
  uint64_t max_bandwidth_per_job; /* Bandwidth limitation (global) */

  ClientResource() = default;
};

/* Define the Union of all the above
 * resource structure definitions.
 */
union UnionOfResources {
  DirectorResource res_dir;
  ClientResource res_client;
  MessagesResource res_msgs;
  CommonResourceHeader hdr;

  UnionOfResources() { new (&hdr) CommonResourceHeader(); }
  ~UnionOfResources() {}
};

ConfigurationParser* InitFdConfig(const char* configfile, int exit_code);
bool PrintConfigSchemaJson(PoolMem& buffer);

} /* namespace filedaemon */
#endif /* BAREOS_FILED_FILED_CONF_H_ */
