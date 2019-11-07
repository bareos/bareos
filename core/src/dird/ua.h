/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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
 * Kern Sibbald, August MMI
 */
/**
 * @file
 * Includes specific to the Director User Agent Server
 */

#ifndef BAREOS_DIRD_UA_H_
#define BAREOS_DIRD_UA_H_ 1

#include "include/bareos.h"
#include "lib/bsock.h"
#include "lib/output_formatter.h"

class JobControlRecord;
class BareosDb;
class guid_list;
typedef struct s_tree_root TREE_ROOT;
typedef struct s_tree_node TREE_NODE;

namespace directordaemon {

class CatalogResource;
class ConsoleResource;
class PoolResource;
class StorageResource;
class ClientResource;
class JobResource;
class FilesetResource;
class ScheduleResource;
struct RestoreBootstrapRecord;
struct ua_cmdstruct;
class UnifiedStorageResource;
struct UserAcl;

class UaContext {
 public:
  BareosSocket* UA_sock;
  BareosSocket* sd;
  JobControlRecord* jcr;
  BareosDb* db;
  BareosDb* shared_db;  /**< Shared database connection used by multiple ua's */
  BareosDb* private_db; /**< Private database connection only used by this ua */
  CatalogResource* catalog;
  UserAcl* user_acl;              /**< acl from console or user resource */
  POOLMEM* cmd;                   /**< Return command/name buffer */
  POOLMEM* args;                  /**< Command line arguments */
  POOLMEM* errmsg;                /**< Store error message */
  guid_list* guid;                /**< User and Group Name mapping cache */
  char* argk[MAX_CMD_ARGS];       /**< Argument keywords */
  char* argv[MAX_CMD_ARGS];       /**< Argument values */
  int argc;                       /**< Number of arguments */
  char** prompt;                  /**< List of prompts */
  int max_prompts;                /**< Max size of list */
  int num_prompts;                /**< Current number in list */
  int api;                        /**< For programs want an API */
  bool auto_display_messages;     /**< If set, display messages */
  bool user_notified_msg_pending; /**< Set when user notified */
  bool automount;                 /**< If set, mount after label */
  bool quit;                      /**< If set, quit */
  bool verbose;                   /**< Set for normal UA verbosity */
  bool batch;                     /**< Set for non-interactive mode */
  bool gui;                       /**< Set if talking to GUI program */
  bool runscript;                 /**< Set if we are in runscript */
  uint32_t pint32_val;            /**< Positive integer */
  int32_t int32_val;              /**< Positive/negative */
  int64_t int64_val;              /**< Big int */
  OutputFormatter* send;          /**< object instance to handle output */

 private:
  ua_cmdstruct* cmddef; /**< Definition of the currently executed command */

  bool AclAccessOk(int acl,
                   const char* item,
                   int len,
                   bool audit_event = false);
  int RcodeToAcltype(int rcode);
  void LogAuditEventAclFailure(int acl, const char* item);
  void LogAuditEventAclSuccess(int acl, const char* item);
  void SetCommandDefinition(ua_cmdstruct* cmdstruct) { cmddef = cmdstruct; }

 public:
  UaContext();
  void signal(int sig) { UA_sock->signal(sig); }
  bool execute(ua_cmdstruct* cmd);

  /*
   * ACL check method.
   */
  bool AclAccessOk(int rcode, const char* item, bool audit_event = false);
  bool AclNoRestrictions(int acl);
  bool AclHasRestrictions(int acl) { return !AclNoRestrictions(acl); }

  /*
   * Resource retrieval methods including check on ACL.
   */
  bool IsResAllowed(BareosResource* res);
  BareosResource* GetResWithName(int rcode,
                                 const char* name,
                                 bool audit_event = false,
                                 bool lock = true);
  PoolResource* GetPoolResWithName(const char* name,
                                   bool audit_event = true,
                                   bool lock = true);
  StorageResource* GetStoreResWithName(const char* name,
                                       bool audit_event = true,
                                       bool lock = true);
  StorageResource* GetStoreResWithId(DBId_t id,
                                     bool audit_event = true,
                                     bool lock = true);
  ClientResource* GetClientResWithName(const char* name,
                                       bool audit_event = true,
                                       bool lock = true);
  JobResource* GetJobResWithName(const char* name,
                                 bool audit_event = true,
                                 bool lock = true);
  FilesetResource* GetFileSetResWithName(const char* name,
                                         bool audit_event = true,
                                         bool lock = true);
  CatalogResource* GetCatalogResWithName(const char* name,
                                         bool audit_event = true,
                                         bool lock = true);
  ScheduleResource* GetScheduleResWithName(const char* name,
                                           bool audit_event = true,
                                           bool lock = true);

  /*
   * Audit event methods.
   */
  bool AuditEventWanted(bool audit_event_enabled);
  void LogAuditEventCmdline();

  /*
   * The below are in ua_output.c
   */
  void SendRawMsg(const char* msg);
  void SendMsg(const char* fmt, ...);
  void ErrorMsg(const char* fmt, ...);
  void WarningMsg(const char* fmt, ...);
  void InfoMsg(const char* fmt, ...);
  void SendCmdUsage(const char* fmt, ...);
};

/*
 * Context for InsertTreeHandler()
 */
struct TreeContext {
  TREE_ROOT* root = nullptr;       /**< Root */
  TREE_NODE* node = nullptr;       /**< Current node */
  TREE_NODE* avail_node = nullptr; /**< Unused node last insert */
  int cnt = 0;                     /**< Count for user feedback */
  bool all = false;                /**< If set mark all as default */
  UaContext* ua = nullptr;
  uint32_t FileEstimate = 0; /**< Estimate of number of files */
  uint32_t FileCount = 0;    /**< Current count of files */
  uint32_t LastCount = 0;    /**< Last count of files */
  uint32_t DeltaCount = 0;   /**< Trigger for printing */

  TreeContext() = default;
  ~TreeContext() = default;
};

struct NameList {
  char** name = nullptr; /**< List of names */
  int num_ids = 0;       /**< Ids stored */
  int max_ids = 0;       /**< Size of array */
  int num_del = 0;       /**< Number deleted */
  int tot_ids = 0;       /**< Total to process */

  NameList() = default;
  ~NameList() = default;
};

/*
 * Context for restore job.
 */
struct RestoreContext {
  utime_t JobTDate = {0};
  uint32_t TotalFiles = 0;
  JobId_t JobId = 0;
  char* backup_format = nullptr;
  char* ClientName = nullptr;        /**< Backup client */
  char* RestoreClientName = nullptr; /**< Restore client */
  char last_jobid[20]{0};
  POOLMEM* JobIds = nullptr;     /**< User entered string of JobIds */
  POOLMEM* BaseJobIds = nullptr; /**< Base jobids */
  StorageResource* store = nullptr;
  JobResource* restore_job = nullptr;
  PoolResource* pool = nullptr;
  int restore_jobs = 0;
  uint32_t selected_files = 0;
  char* comment = nullptr;
  char* where = nullptr;
  char* RegexWhere = nullptr;
  char* replace = nullptr;
  char* plugin_options = nullptr;
  std::unique_ptr<RestoreBootstrapRecord> bsr;
  POOLMEM* fname = nullptr; /**< Filename only */
  POOLMEM* path = nullptr;  /**< Path only */
  POOLMEM* query = nullptr;
  int fnl = 0; /**< Filename length */
  int pnl = 0; /**< Path length */
  bool found = false;
  bool all = false; /**< Mark all as default */
  NameList name_list;

  RestoreContext() = default;
  ~RestoreContext() = default;
};

/*
 * Context for run job.
 */
class RunContext {
 public:
  char* backup_format = nullptr;
  char* bootstrap = nullptr;
  char* catalog_name = nullptr;
  char* client_name = nullptr;
  char* comment = nullptr;
  char* fileset_name = nullptr;
  char* jid = nullptr;
  char* job_name = nullptr;
  char* level_name = nullptr;
  char* next_pool_name = nullptr;
  char* plugin_options = nullptr;
  char* pool_name = nullptr;
  char* previous_job_name = nullptr;
  char* regexwhere = nullptr;
  char* restore_client_name = nullptr;
  char* since = nullptr;
  char* StoreName = nullptr;
  char* verify_job_name = nullptr;
  char* when = nullptr;
  char* where = nullptr;
  const char* replace = nullptr;
  const char* verify_list = nullptr;
  JobResource* job = nullptr;
  JobResource* verify_job = nullptr;
  JobResource* previous_job = nullptr;
  UnifiedStorageResource* store = nullptr;
  ClientResource* client = nullptr;
  FilesetResource* fileset = nullptr;
  PoolResource* pool = nullptr;
  PoolResource* next_pool = nullptr;
  CatalogResource* catalog = nullptr;
  int Priority = 0;
  int files = 0;
  bool level_override = false;
  bool pool_override = false;
  bool spool_data = false;
  bool accurate = false;
  bool ignoreduplicatecheck = false;
  bool cloned = false;
  bool mod = false;
  bool spool_data_set = false;
  bool nextpool_set = false;
  bool accurate_set = false;
  bool ignoreduplicatecheck_set = false;

  RunContext();
  ~RunContext();
};

} /* namespace directordaemon */
#endif /* BAREOS_DIRD_UA_H_ */
