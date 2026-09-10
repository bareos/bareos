/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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
// Kern Sibbald, August MMI
/**
 * @file
 * Includes specific to the Director User Agent Server
 */

#ifndef BAREOS_DIRD_UA_H_
#define BAREOS_DIRD_UA_H_

#include "include/bareos.h"
#include "lib/bsock.h"
#include "dird/dird_conf.h"

class JobControlRecord;
class BareosDb;
class guid_list;
class OutputFormatter;
typedef struct s_tree_root TREE_ROOT;
struct tree_node;

namespace directordaemon {

std::string NormalizeAuditMessageText(const char* text);

struct ua_cmdstruct;
struct RestoreBootstrapRecord;

struct UserAcl {
  std::string name{};

  std::vector<std::string> acl_lists[Num_ACL];

  template <typename Resource,
            AclConfig Resource::* Accessor = &Resource::user_acl>
  static std::unique_ptr<UserAcl> from_config(Resource* res)
  {
    auto result = std::make_unique<UserAcl>(res->resource_name_);

    AclConfig* cfg = &(res->*Accessor);
    // a valid resource must _always_ have this set
    // otherwise they would become "root" when used!
    ASSERT(cfg);

    for (int acl = 0; acl < Num_ACL; ++acl) {
      auto& list = result->acl_lists[acl];

      // acl of a resource have higher priority than acls from a profile

      for (auto* entry : cfg->ACL_lists[acl]) { list.push_back(entry); }

      for (auto* profile : cfg->profiles) {
        for (auto* entry : profile->ACL_lists[acl]) { list.push_back(entry); }
      }
    }

    return result;
  }
};

class UaContext {
 public:
  UaContext(JobControlRecord* jcr);
  ~UaContext();
  UaContext(const UaContext&) = delete;
  UaContext& operator=(const UaContext&) = delete;
  UaContext(UaContext&&) = delete;
  UaContext& operator=(UaContext&&) = delete;


 public:
  BareosSocket* UA_sock{nullptr};
  BareosSocket* sd{nullptr};
  JobControlRecord* jcr{nullptr};
  BareosDb* db{nullptr};
  BareosDb* shared_db{
      nullptr}; /**< Shared database connection used by multiple ua's */
  BareosDb* private_db{
      nullptr}; /**< Private database connection only used by this ua */
  CatalogResource* catalog{nullptr};
  std::unique_ptr<UserAcl> user_acl{
      nullptr};                       /**< acl from console or user resource */
  POOLMEM* cmd;                       /**< Return command/name buffer */
  POOLMEM* args;                      /**< Command line arguments */
  std::string errmsg{};               /**< Store error message */
  guid_list* guid{nullptr};           /**< User and Group Name mapping cache */
  char* argk[MAX_CMD_ARGS] = {};      /**< Argument keywords */
  char* argv[MAX_CMD_ARGS] = {};      /**< Argument values */
  int argc{0};                        /**< Number of arguments */
  std::string prompt_header{};        /**< Name of current prompt */
  std::vector<std::string> prompts{}; /**< List of prompts */
  int api{0};                         /**< For programs want an API */
  bool auto_display_messages{false};  /**< If set, display messages */
  bool user_notified_msg_pending{false}; /**< Set when user notified */
  bool automount{true};                  /**< If set, mount after label */
  bool quit{false};                      /**< If set, quit */
  bool verbose{true};                    /**< Set for normal UA verbosity */
  bool batch{false};                     /**< Set for non-interactive mode */
  bool gui{false};                       /**< Set if talking to GUI program */
  bool runscript{false};                 /**< Set if we are in runscript */
  uint32_t pint32_val{};                 /**< Positive integer */
  int32_t int32_val{};                   /**< Positive/negative */
  int64_t int64_val{};                   /**< Big int */
  std::unique_ptr<OutputFormatter>
      send; /**< object instance to handle output */

 private:
  ua_cmdstruct* cmddef{
      nullptr}; /**< Definition of the currently executed command */
  bool console_is_connected{true};  // is this ua connected to a console (and
                                    // not a fake for a running job)

  bool AclAccessOk(int acl,
                   const char* item,
                   int len,
                   bool audit_event = false);
  int RcodeToAcltype(int rcode);
  void LogAuditEventAclFailure(int acl, const char* item);
  void LogAuditEventAclSuccess(int acl, const char* item);
  void SetCommandDefinition(ua_cmdstruct* cmdstruct) { cmddef = cmdstruct; }

 public:
  void signal(int sig) { UA_sock->signal(sig); }
  bool execute(ua_cmdstruct* cmd);

  // ACL check method.
  bool AclAccessOk(int rcode, const char* item, bool audit_event = false);
  bool AclNoRestrictions(int acl);
  bool AclHasRestrictions(int acl) { return !AclNoRestrictions(acl); }

  // Resource retrieval methods including check on ACL.
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

  // Audit event methods.
  bool AuditEventWanted(bool audit_event_enabled);
  void LogAuditEventCmdline();
  void LogAuditEventInfoMsg(const char* fmt, ...) PRINTF_LIKE(2, 3);

  // The below are in ua_output.c
  void SendRawMsg(const char* msg);
  void SendMsg(const char* fmt, ...) PRINTF_LIKE(2, 3);
  void ErrorMsg(const char* fmt, ...) PRINTF_LIKE(2, 3);
  void WarningMsg(const char* fmt, ...) PRINTF_LIKE(2, 3);
  void InfoMsg(const char* fmt, ...) PRINTF_LIKE(2, 3);
  void SendCmdUsage(const char* msg);

  void vSendMsg(int signal,
                const char* messagetype,
                const char* fmt,
                va_list arg_ptr);
};

// Context for InsertTreeHandler()
struct TreeContext {
  TREE_ROOT* root = nullptr;       /**< Root */
  tree_node* node = nullptr;       /**< Current node */
  tree_node* avail_node = nullptr; /**< Unused node last insert */
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

// Context for restore job.
struct RestoreContext {
  enum class JobTypeFilter
  {
    Backup,
    Archive,
  };
  JobTypeFilter job_filter = JobTypeFilter::Backup;

  utime_t JobTDate = {0};
  uint32_t TotalFiles = 0;
  JobId_t JobId = 0;
  char* backup_format = nullptr;
  char* ClientName = nullptr;        /**< Backup client */
  char* RestoreClientName = nullptr; /**< Restore client */
  char last_jobid[20]{0};
  POOLMEM* JobIds = nullptr; /**< User entered string of JobIds */
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
  int fnl = 0;        /**< Filename length */
  int pnl = 0;        /**< Path length */
  bool found = false; /**< the last handler found at least one result */
  bool all = false;   /**< Mark all as default */

  RestoreContext() = default;
  ~RestoreContext() = default;
  static char FilterIdentifier(JobTypeFilter filter);
};

// Context for run job.
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
  JobResource* job = nullptr;
  JobResource* verify_job = nullptr;
  JobResource* previous_job = nullptr;
  JobResource* consolidate_job = nullptr;
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
#endif  // BAREOS_DIRD_UA_H_
