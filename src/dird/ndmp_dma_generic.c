/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2015 Planets Communications B.V.
   Copyright (C) 2013-2017 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, May 2015
 */
/**
 * @file
 * Generic NDMP Data Management Application (DMA) routines
 */

#include "bareos.h"
#include "dird.h"

#if HAVE_NDMP

#define SMTAPE_MIN_BLOCKSIZE 4096 /**< 4 Kb */
#define SMTAPE_MAX_BLOCKSIZE 262144 /**< 256 Kb */
#define SMTAPE_BLOCKSIZE_INCREMENTS 4096 /**< 4 Kb */

#include "ndmp/ndmagents.h"
#include "ndmp_dma_priv.h"

/* Imported variables */

/* Forward referenced functions */

/**
 * Per NDMP format specific options.
 */
static ndmp_backup_format_option ndmp_backup_format_options[] = {
   /* format uses_file_history  uses_level restore_prefix_relative needs_namelist */
   { (char *)"dump", true, true, true, true },
   { (char *)"tar", true, false, true, true },
   { (char *)"smtape", false, true, false, true },
   { (char *)"zfs", false, true, false, true },
   { (char *)"vbb", false, true, false, true },
   { (char *)"image", false, true, false, true },
   { NULL, false, false, false }
};

/**
 * find ndmp_backup_format_option for given format
 * @param [in] backup_format string specifying the backup format
 * @return returns a pointer to the ndmp_backup_format_option
 */
ndmp_backup_format_option *ndmp_lookup_backup_format_options(const char *backup_format)
{
   int i = 0;

   while (ndmp_backup_format_options[i].format) {
      if (bstrcasecmp(backup_format, ndmp_backup_format_options[i].format)) {
         break;
      }
      i++;
   }

   if (ndmp_backup_format_options[i].format) {
      return &ndmp_backup_format_options[i];
   }

   return (ndmp_backup_format_option *)NULL;
}

/**
 * Validation functions.
 */
bool ndmp_validate_client(JCR *jcr)
{
   switch (jcr->res.client->Protocol) {
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
      if (jcr->res.client->password.encoding != p_encoding_clear) {
         Jmsg(jcr, M_FATAL, 0,
              _("Client %s, has incompatible password encoding for running NDMP backup.\n"),
              jcr->res.client->name());
         return false;
      }
      break;
   default:
      Jmsg(jcr, M_FATAL, 0,
           _("Client %s, with backup protocol %s  not compatible for running NDMP backup.\n"),
           jcr->res.client->name(), auth_protocol_to_str(jcr->res.client->Protocol));
      return false;
   }

   return true;
}

static inline bool ndmp_validate_storage(JCR *jcr, STORERES *store)
{
   switch (store->Protocol) {
   case APT_NDMPV2:
   case APT_NDMPV3:
   case APT_NDMPV4:
      if (store->password.encoding != p_encoding_clear) {
         Jmsg(jcr, M_FATAL, 0,
              _("Storage %s, has incompatible password encoding for running NDMP backup.\n"),
              store->name());
         return false;
      }
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Storage %s has illegal backup protocol %s for NDMP backup\n"),
           store->name(), auth_protocol_to_str(store->Protocol));
      return false;
   }

   return true;
}

bool ndmp_validate_storage(JCR *jcr)
{
   STORERES *store;

   if (jcr->res.wstorage) {
      foreach_alist(store, jcr->res.wstorage) {
         if (!ndmp_validate_storage(jcr, store)) {
            return false;
         }
      }
   } else {
      foreach_alist(store, jcr->res.rstorage) {
         if (!ndmp_validate_storage(jcr, store)) {
            return false;
         }
      }
   }

   return true;
}

bool ndmp_validate_job(JCR *jcr, struct ndm_job_param *job)
{
   int n_err, i;
   char audit_buffer[256];

   /*
    * Audit the job so we only submit a valid NDMP job.
    */
   n_err = 0;
   i = 0;
   do {
      n_err = ndma_job_audit(job, audit_buffer, i);
      if (n_err) {
         Jmsg(jcr, M_ERROR, 0, _("NDMP Job validation error = %s\n"), audit_buffer);
      }
      i++;
   } while (i < n_err);

   if (n_err) {
      return false;
   }

   return true;
}

/**
 * Fill a ndmagent structure with the correct info. Instead of calling ndmagent_from_str
 * we fill the structure ourself from info provides in a resource.
 */
static inline bool fill_ndmp_agent_config(JCR *jcr,
                                          struct ndmagent *agent,
                                          uint32_t protocol,
                                          uint32_t authtype,
                                          char *address,
                                          uint32_t port,
                                          char *username,
                                          char *password)
{
   agent->conn_type = NDMCONN_TYPE_REMOTE;
   switch (protocol) {
   case APT_NDMPV2:
      agent->protocol_version = 2;
      break;
   case APT_NDMPV3:
      agent->protocol_version = 3;
      break;
   case APT_NDMPV4:
      agent->protocol_version = 4;
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Illegal protocol %d for NDMP Job\n"), protocol);
      return false;
   }

   switch (authtype) {
   case AT_NONE:
      agent->auth_type = 'n';
      break;
   case AT_CLEAR:
      agent->auth_type = 't';
      break;
   case AT_MD5:
      agent->auth_type = 'm';
      break;
   case AT_VOID:
      agent->auth_type = 'v';
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Illegal authtype %d for NDMP Job\n"), authtype);
      return false;
   }
   /*
    *  sanity checks
    */
   if (!address)  {
      Jmsg(jcr, M_FATAL, 0, _("fill_ndmp_agent_config: address is missing\n"));
      return false;
   }

   if (!username) {
      Jmsg(jcr, M_FATAL, 0, _("fill_ndmp_agent_config: username is missing\n"));
      return false;
   }

   if (!password) {
      Jmsg(jcr, M_FATAL, 0, _("fill_ndmp_agent_config: password is missing\n"));
      return false;
   }

   if (!port)     {
      Jmsg(jcr, M_FATAL, 0, _("fill_ndmp_agent_config: port is missing\n"));
      return false;
   }

   agent->port = port;
   bstrncpy(agent->host, address, sizeof(agent->host));
   bstrncpy(agent->account, username, sizeof(agent->account));
   bstrncpy(agent->password, password, sizeof(agent->password));

   return true;
}

/**
 * Parse a meta-tag and convert it into a ndmp_pval
 */
void ndmp_parse_meta_tag(struct ndm_env_table *env_tab, char *meta_tag)
{
   char *p;
   ndmp9_pval pv;

   /*
    * See if the meta-tag is parseable.
    */
   if ((p = strchr(meta_tag, '=')) == NULL) {
      return;
   }

   /*
    * Split the tag on the '='
    */
   *p = '\0';
   pv.name = meta_tag;
   pv.value = p + 1;
   ndma_update_env_list(env_tab, &pv);

   /*
    * Restore the '='
    */
   *p = '=';
}

/**
 * Calculate the wanted NDMP loglevel from the current debug level and
 * any configure minimum level.
 */
int native_to_ndmp_loglevel(int NdmpLoglevel, int debuglevel, NIS *nis)
{
   unsigned int level;

   nis->LogLevel = NdmpLoglevel;

   /*
    * NDMP loglevels run from 0 - 9 so we take a look at the
    * current debug level and divide it by 100 to get a proper
    * value. If the debuglevel is below the wanted initial level
    * we set the loglevel to the wanted initial level. As the
    * debug logging takes care of logging messages that are
    * unwanted we can set the loglevel higher and still don't
    * get debug messages.
    */
   level = debuglevel / 100;
   if (level < nis->LogLevel) {
      level = nis->LogLevel;
   }

   /*
    * Make sure the level is in the wanted range.
    */
   if (level > 9) {
      level = 9;
   }

   return level;
}

bool ndmp_build_client_job(JCR *jcr,
                           CLIENTRES *client,
                           STORERES *store,
                           int operation,
                           struct ndm_job_param *job)
{
   memset(job, 0, sizeof(struct ndm_job_param));

   job->operation = operation;
   job->bu_type = jcr->backup_format;

   /*
    * For NDMP the backupformat is a prerequite abort the backup job when
    * it is not supplied in the config definition.
    */
   if (!job->bu_type) {
      Jmsg(jcr, M_FATAL, 0, _("No backup type specified in NDMP job\n"));
      goto bail_out;
   }

   /*
    * The data_agent is the client being backuped or restored using NDMP.
    */
   ASSERT(client->password.encoding == p_encoding_clear);
   if (!fill_ndmp_agent_config(jcr, &job->data_agent, client->Protocol,
                               client->AuthType, client->address,
                               client->FDport, client->username,
                               client->password.value)) {
      goto bail_out;
   }

   /*
    * The tape_agent is the storage daemon via the NDMP protocol.
    */
   ASSERT(store->password.encoding == p_encoding_clear);
   if (!fill_ndmp_agent_config(jcr, &job->tape_agent, store->Protocol,
                               store->AuthType, store->address,
                               store->SDport, store->username,
                               store->password.value)) {
      goto bail_out;
   }

   if (bstrcasecmp(jcr->backup_format, "smtape")) {
      /*
       * SMTAPE only wants certain blocksizes.
       */
      if (jcr->res.client->ndmp_blocksize < SMTAPE_MIN_BLOCKSIZE ||
          jcr->res.client->ndmp_blocksize > SMTAPE_MAX_BLOCKSIZE) {
         Jmsg(jcr, M_FATAL, 0,
              _("For SMTAPE NDMP jobs the NDMP blocksize needs to be between %d and %d, but is set to %d\n"),
              SMTAPE_MIN_BLOCKSIZE, SMTAPE_MAX_BLOCKSIZE, jcr->res.client->ndmp_blocksize);
         goto bail_out;
      }

      if ((jcr->res.client->ndmp_blocksize % SMTAPE_BLOCKSIZE_INCREMENTS) != 0) {
         Jmsg(jcr, M_FATAL, 0,
              _("For SMTAPE NDMP jobs the NDMP blocksize needs to be in increments of %d bytes, but is set to %d\n"),
              SMTAPE_BLOCKSIZE_INCREMENTS, jcr->res.client->ndmp_blocksize);
         goto bail_out;
      }

      job->record_size = jcr->res.client->ndmp_blocksize;
   } else {
      job->record_size = jcr->res.client->ndmp_blocksize;
   }

   return true;

bail_out:
   return false;
}




bool ndmp_build_storage_job(JCR *jcr,
                            STORERES *store,
                            bool init_tape,
                            bool init_robot,
                            int operation,
                            struct ndm_job_param *job)
{
   memset(job, 0, sizeof(struct ndm_job_param));

   job->operation = operation;
   job->bu_type = jcr->backup_format;

   if (!fill_ndmp_agent_config(jcr, &job->data_agent, store->Protocol,
                               store->AuthType, store->address,
                               store->SDport, store->username,
                               store->password.value)) {
      goto bail_out;
   }


  if (init_tape) {
     /*
      * Setup the TAPE agent of the NDMP job.
      */
     ASSERT(store->password.encoding == p_encoding_clear);
     if (!fill_ndmp_agent_config(jcr, &job->tape_agent,
                                 store->Protocol, store->AuthType,
                                 store->address, store->SDport,
                                 store->username, store->password.value)) {
        goto bail_out;
     }
  }

  if (init_robot) {
     /*
      * Setup the ROBOT agent of the NDMP job.
      */
     if (!fill_ndmp_agent_config(jcr, &job->robot_agent,
                                 store->Protocol, store->AuthType,
                                 store->address, store->SDport,
                                 store->username, store->password.value)) {
        goto bail_out;
     }
  }



   return true;

bail_out:
   return false;
}

bool ndmp_build_client_and_storage_job(JCR *jcr,
                            STORERES *store,
                            CLIENTRES *client,
                            bool init_tape,
                            bool init_robot,
                            int operation,
                            struct ndm_job_param *job)
{
   /*
    * setup storage job
    * i.e. setup tape_agent and robot_agent
    */
   ndmp_build_storage_job(jcr, store, init_tape, init_robot,
                            operation, job);

   /*
    * now configure client job
    * i.e. setup data_agent
    */

   //job->operation = operation;
   //job->bu_type = jcr->backup_format;

   if (!fill_ndmp_agent_config(jcr, &job->data_agent, client->Protocol,
                               client->AuthType, client->address,
                               client->FDport, client->username,
                               client->password.value)) {
      goto bail_out;
   }

   return true;

bail_out:
   return false;
}

/**
 * Interface function which glues the logging infra of the NDMP lib with the daemon.
 */
extern "C" void ndmp_loghandler(struct ndmlog *log, char *tag, int level, char *msg)
{
   int internal_level;
   NIS *nis;

   /*
    * We don't want any trailing newline in log messages.
    */
   strip_trailing_newline(msg);

   /*
    * Make sure if the logging system was setup properly.
    */
   nis = (NIS *)log->ctx;
   if (!nis) {
      return;
   }

   /*
    * If the log level of this message is under our logging treshold we
    * log it as part of the Job.
    */
   if (level <= (int)nis->LogLevel) {
      if (nis->jcr) {
         /*
          * Look at the tag field to see what is logged.
          */
         if (bstrncmp(tag + 1, "LM", 2)) {
            /*
             * *LM* messages. E.g. log message NDMP protocol msgs.
             * First character of the tag is the agent sending the
             * message e.g. 'D' == Data Agent
             *              'T' == Tape Agent
             *              'R' == Robot Agent
             *              'C' == Control Agent (DMA)
             *
             * Last character is the type of message e.g.
             * 'n' - normal message
             * 'd' - debug message
             * 'e' - error message
             * 'w' - warning message
             * '?' - unknown message level
             */
            switch (*(tag + 3)) {
            case 'n':
               Jmsg(nis->jcr, M_INFO, 0, "%s\n", msg);
               break;
            case 'e':
               Jmsg(nis->jcr, M_ERROR, 0, "%s\n", msg);
               break;
            case 'w':
               Jmsg(nis->jcr, M_WARNING, 0, "%s\n", msg);
               break;
            case '?':
               Jmsg(nis->jcr, M_INFO, 0, "%s\n", msg);
               break;
            default:
               break;
            }
         } else {
            Jmsg(nis->jcr, M_INFO, 0, "%s\n", msg);
         }
      }
   }

   /*
    * Print any debug message we convert the NDMP level back to an internal
    * level and let the normal debug logging handle if it needs to be printed
    * or not.
    */
   internal_level = level * 100;
   Dmsg3(internal_level, "NDMP: [%s] [%d] %s\n", tag, level, msg);
}

/**
 * Interface function which glues the logging infra of the NDMP lib with the user context.
 */
extern "C" void ndmp_client_status_handler(struct ndmlog *log, char *tag, int lev, char *msg)
{
   NIS *nis;

   /*
    * Make sure if the logging system was setup properly.
    */
   nis = (NIS *)log->ctx;
   if (!nis) {
      return;
   }

   nis->ua->send_msg("%s\n", msg);
}

/**
 * Generic function to query the NDMP server using the NDM_JOB_OP_QUERY_AGENTS
 * operation. Callback is the above ndmp_client_status_handler which prints
 * the data to the user context.
 */
void ndmp_do_query(UAContext *ua, ndm_job_param *ndmp_job, int NdmpLoglevel)
{
   NIS *nis;
   struct ndm_session ndmp_sess;

   /*
    * Initialize a new NDMP session
    */
   memset(&ndmp_sess, 0, sizeof(ndmp_sess));
   ndmp_sess.conn_snooping = (me->ndmp_snooping) ? 1 : 0;
   ndmp_sess.control_agent_enabled = 1;

   ndmp_sess.param = (struct ndm_session_param *)malloc(sizeof(struct ndm_session_param));
   memset(ndmp_sess.param, 0, sizeof(struct ndm_session_param));
   ndmp_sess.param->log.deliver = ndmp_client_status_handler;
   nis = (NIS *)malloc(sizeof(NIS));
   memset(nis, 0, sizeof(NIS));
   ndmp_sess.param->log_level = native_to_ndmp_loglevel(NdmpLoglevel, debug_level, nis);
   nis->ua = ua;
   ndmp_sess.param->log.ctx = nis;
   ndmp_sess.param->log_tag = bstrdup("DIR-NDMP");

   /*
    * Initialize the session structure.
    */
   if (ndma_session_initialize(&ndmp_sess)) {
      goto bail_out;
   }

   /*
    * Copy the actual job to perform.
    */
   memcpy(&ndmp_sess.control_acb->job, ndmp_job, sizeof(struct ndm_job_param));
   if (!ndmp_validate_job(ua->jcr, &ndmp_sess.control_acb->job)) {
      goto cleanup;
   }

   /*
    * Commission the session for a run.
    */
   if (ndma_session_commission(&ndmp_sess)) {
      goto cleanup;
   }

   /*
    * Setup the DMA.
    */
   if (ndmca_connect_control_agent(&ndmp_sess)) {
      goto cleanup;
   }

   ndmp_sess.conn_open = 1;
   ndmp_sess.conn_authorized = 1;

   /*
    * Let the DMA perform its magic.
    */
   if (ndmca_control_agent(&ndmp_sess) != 0) {
      goto cleanup;
   }

cleanup:
   /*
    * Destroy the session.
    */
   ndma_session_destroy(&ndmp_sess);

bail_out:

   /*
    * Free the param block.
    */
   free(ndmp_sess.param->log_tag);
   free(ndmp_sess.param);
   free(nis);
   ndmp_sess.param = NULL;
}

/**
 * Output the status of a NDMP client. Query the DATA agent of a
 * native NDMP server to give some info.
 */
void do_ndmp_client_status(UAContext *ua, CLIENTRES *client, char *cmd)
{
   struct ndm_job_param ndmp_job;

   memset(&ndmp_job, 0, sizeof(struct ndm_job_param));
   ndmp_job.operation = NDM_JOB_OP_QUERY_AGENTS;

   /*
    * Query the DATA agent of the NDMP server.
    */
   ASSERT(client->password.encoding == p_encoding_clear);
   if (!fill_ndmp_agent_config(ua->jcr, &ndmp_job.data_agent,
                               client->Protocol, client->AuthType,
                               client->address, client->FDport,
                               client->username, client->password.value)) {
      return;
   }

   ndmp_do_query(ua, &ndmp_job,
                 (client->ndmp_loglevel > me->ndmp_loglevel) ? client->ndmp_loglevel :
                                                               me->ndmp_loglevel);
}
#else
void do_ndmp_client_status(UAContext *ua, CLIENTRES *client, char *cmd)
{
   Jmsg(ua->jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
}
#endif /* HAVE_NDMP */
