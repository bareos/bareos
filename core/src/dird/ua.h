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

#define MAX_ID_LIST_LEN 2000000

struct ua_cmdstruct {
   const char *key; /**< Command */
   bool (*func)(UaContext *ua, const char *cmd); /**< Handler */
   const char *help; /**< Main purpose */
   const char *usage; /**< All arguments to build usage */
   const bool use_in_rs; /**< Can use it in Console RunScript */
   const bool audit_event; /**< Log an audit event when this Command is executed */
};

class UaContext {
public:
   /*
    * Members
    */
   BareosSocket *UA_sock;
   BareosSocket *sd;
   JobControlRecord *jcr;
   BareosDb *db;
   BareosDb *shared_db;                   /**< Shared database connection used by multiple ua's */
   BareosDb *private_db;                  /**< Private database connection only used by this ua */
   CatalogResource *catalog;
   ConsoleResource *cons;                      /**< Console resource */
   POOLMEM *cmd;                      /**< Return command/name buffer */
   POOLMEM *args;                     /**< Command line arguments */
   POOLMEM *errmsg;                   /**< Store error message */
   guid_list *guid;                   /**< User and Group Name mapping cache */
   char *argk[MAX_CMD_ARGS];          /**< Argument keywords */
   char *argv[MAX_CMD_ARGS];          /**< Argument values */
   int argc;                          /**< Number of arguments */
   char **prompt;                     /**< List of prompts */
   int max_prompts;                   /**< Max size of list */
   int num_prompts;                   /**< Current number in list */
   int api;                           /**< For programs want an API */
   bool auto_display_messages;        /**< If set, display messages */
   bool user_notified_msg_pending;    /**< Set when user notified */
   bool automount;                    /**< If set, mount after label */
   bool quit;                         /**< If set, quit */
   bool verbose;                      /**< Set for normal UA verbosity */
   bool batch;                        /**< Set for non-interactive mode */
   bool gui;                          /**< Set if talking to GUI program */
   bool runscript;                    /**< Set if we are in runscript */
   uint32_t pint32_val;               /**< Positive integer */
   int32_t int32_val;                 /**< Positive/negative */
   int64_t int64_val;                 /**< Big int */
   OutputFormatter *send;            /**< object instance to handle output */

private:
   /*
    * Members
    */
   ua_cmdstruct *cmddef;              /**< Definition of the currently executed command */

   /*
    * Methods
    */
   bool AclAccessOk(int acl, const char *item, int len, bool audit_event = false);
   int RcodeToAcltype(int rcode);
   void LogAuditEventAclFailure(int acl, const char *item);
   void LogAuditEventAclSuccess(int acl, const char *item);
   void SetCommandDefinition(ua_cmdstruct *cmdstruct) { cmddef = cmdstruct; }

public:
   /*
    * Methods
    */
   void signal(int sig) { UA_sock->signal(sig); }
   bool execute(ua_cmdstruct *cmd);

   /*
    * ACL check method.
    */
   bool AclAccessOk(int rcode, const char *item, bool audit_event = false);
   bool AclNoRestrictions(int acl);
   bool AclHasRestrictions(int acl) { return !AclNoRestrictions(acl); }

   /*
    * Resource retrieval methods including check on ACL.
    */
   bool IsResAllowed(CommonResourceHeader *res);
   CommonResourceHeader *GetResWithName(int rcode, const char *name, bool audit_event = false, bool lock = true);
   PoolResource *GetPoolResWithName(const char *name, bool audit_event = true, bool lock = true);
   StorageResource *GetStoreResWithName(const char *name, bool audit_event = true, bool lock = true);
   StorageResource *GetStoreResWithId(DBId_t id, bool audit_event = true, bool lock = true);
   ClientResource *GetClientResWithName(const char *name, bool audit_event = true, bool lock = true);
   JobResource *GetJobResWithName(const char *name, bool audit_event = true, bool lock = true);
   FilesetResource *GetFileSetResWithName(const char *name, bool audit_event = true, bool lock = true);
   CatalogResource *GetCatalogResWithName(const char *name, bool audit_event = true, bool lock = true);
   ScheduleResource *GetScheduleResWithName(const char *name, bool audit_event = true, bool lock = true);

   /*
    * Audit event methods.
    */
   bool AuditEventWanted(bool audit_event_enabled);
   void LogAuditEventCmdline();

   /*
    * The below are in ua_output.c
    */
   void SendMsg(const char *fmt, ...);
   void ErrorMsg(const char *fmt, ...);
   void WarningMsg(const char *fmt, ...);
   void InfoMsg(const char *fmt, ...);
   void SendCmdUsage(const char *fmt, ...);
};

/*
 * Context for InsertTreeHandler()
 */
struct TreeContext {
   TREE_ROOT *root;                   /**< Root */
   TREE_NODE *node;                   /**< Current node */
   TREE_NODE *avail_node;             /**< Unused node last insert */
   int cnt;                           /**< Count for user feedback */
   bool all;                          /**< If set mark all as default */
   UaContext *ua;
   uint32_t FileEstimate;             /**< Estimate of number of files */
   uint32_t FileCount;                /**< Current count of files */
   uint32_t LastCount;                /**< Last count of files */
   uint32_t DeltaCount;               /**< Trigger for printing */
};

struct NameList {
   char **name;                       /**< List of names */
   int num_ids;                       /**< Ids stored */
   int max_ids;                       /**< Size of array */
   int num_del;                       /**< Number deleted */
   int tot_ids;                       /**< Total to process */
};

/*
 * Context for restore job.
 */
struct RestoreContext {
   utime_t JobTDate;
   uint32_t TotalFiles;
   JobId_t JobId;
   char *backup_format;
   char *ClientName;                  /**< Backup client */
   char *RestoreClientName;           /**< Restore client */
   char last_jobid[20];
   POOLMEM *JobIds;                   /**< User entered string of JobIds */
   POOLMEM *BaseJobIds;               /**< Base jobids */
   StorageResource *store;
   JobResource *restore_job;
   PoolResource *pool;
   int restore_jobs;
   uint32_t selected_files;
   char *comment;
   char *where;
   char *RegexWhere;
   char *replace;
   char *plugin_options;
   RestoreBootstrapRecord *bsr;
   POOLMEM *fname;                    /**< Filename only */
   POOLMEM *path;                     /**< Path only */
   POOLMEM *query;
   int fnl;                           /**< Filename length */
   int pnl;                           /**< Path length */
   bool found;
   bool all;                          /**< Mark all as default */
   NameList name_list;
};

/*
 * Context for run job.
 */
class RunContext {
public:
   char *backup_format;
   char *bootstrap;
   char *catalog_name;
   char *client_name;
   char *comment;
   char *fileset_name;
   char *jid;
   char *job_name;
   char *level_name;
   char *next_pool_name;
   char *plugin_options;
   char *pool_name;
   char *previous_job_name;
   char *regexwhere;
   char *restore_client_name;
   char *since;
   char *StoreName;
   char *verify_job_name;
   char *when;
   char *where;
   const char *replace;
   const char *verify_list;
   JobResource *job;
   JobResource *verify_job;
   JobResource *previous_job;
   UnifiedStorageResource *store;
   ClientResource *client;
   FilesetResource *fileset;
   PoolResource *pool;
   PoolResource *next_pool;
   CatalogResource *catalog;
   int Priority;
   int files;
   bool level_override;
   bool pool_override;
   bool spool_data;
   bool accurate;
   bool ignoreduplicatecheck;
   bool cloned;
   bool mod;
   bool spool_data_set;
   bool nextpool_set;
   bool accurate_set;
   bool ignoreduplicatecheck_set;

   /*
    * Methods
    */
   RunContext() { memset(this, 0, sizeof(RunContext));
               store = new UnifiedStorageResource; }
   ~RunContext() { delete store; }
};
#endif
