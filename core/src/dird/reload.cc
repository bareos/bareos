/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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

#include "dird/reload.h"

#include <cassert>

namespace directordaemon {

/**
 * Make a quick check to see that we have all the
 * resources needed.
 *
 *  **** FIXME **** this routine could be a lot more
 *   intelligent and comprehensive.
 */
bool CheckResources()
{
  bool OK = true;
  JobResource* job;
  const std::string& configfile = my_config->get_base_config_path();

  LockRes(my_config);

  job = (JobResource*)my_config->GetNextRes(R_JOB, nullptr);
  me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
  my_config->own_resource_ = me;
  if (!me) {
    Jmsg(nullptr, M_FATAL, 0,
         _("No Director resource defined in %s\n"
           "Without that I don't know who I am :-(\n"),
         configfile.c_str());
    OK = false;
    goto bail_out;
  } else {
    // Sanity check.
    if (me->MaxConsoleConnections > me->MaxConnections) {
      me->MaxConnections = me->MaxConsoleConnections + 10;
    }

    my_config->omit_defaults_ = true;
    SetWorkingDirectory(me->working_directory);

    // See if message resource is specified.
    if (!me->messages) {
      me->messages = (MessagesResource*)my_config->GetNextRes(R_MSGS, nullptr);
      if (!me->messages) {
        Jmsg(nullptr, M_FATAL, 0, _("No Messages resource defined in %s\n"),
             configfile.c_str());
        OK = false;
        goto bail_out;
      }
    }

    // When the user didn't force us we optimize for size.
    if (!me->optimize_for_size && !me->optimize_for_speed) {
      me->optimize_for_size = true;
    } else if (me->optimize_for_size && me->optimize_for_speed) {
      Jmsg(nullptr, M_FATAL, 0,
           _("Cannot optimize for speed and size define only one in %s\n"),
           configfile.c_str());
      OK = false;
      goto bail_out;
    }

    if (my_config->GetNextRes(R_DIRECTOR, (BareosResource*)me) != nullptr) {
      Jmsg(nullptr, M_FATAL, 0,
           _("Only one Director resource permitted in %s\n"),
           configfile.c_str());
      OK = false;
      goto bail_out;
    }

    if (me->IsTlsConfigured()) {
      if (!have_tls) {
        Jmsg(nullptr, M_FATAL, 0,
             _("TLS required but not compiled into BAREOS.\n"));
        OK = false;
        goto bail_out;
      }
    }
  }

  if (!job) {
    Jmsg(nullptr, M_FATAL, 0, _("No Job records defined in %s\n"),
         configfile.c_str());
    OK = false;
    goto bail_out;
  }

  if (!PopulateDefs()) {
    OK = false;
    goto bail_out;
  }

  // Loop over Jobs
  foreach_res (job, R_JOB) {
    if (job->MaxFullConsolidations && job->JobType != JT_CONSOLIDATE) {
      Jmsg(nullptr, M_FATAL, 0,
           _("MaxFullConsolidations configured in job %s which is not of job "
             "type \"consolidate\" in file %s\n"),
           job->resource_name_, configfile.c_str());
      OK = false;
      goto bail_out;
    }

    if (job->JobType != JT_BACKUP
        && (job->AlwaysIncremental || job->AlwaysIncrementalJobRetention
            || job->AlwaysIncrementalKeepNumber
            || job->AlwaysIncrementalMaxFullAge)) {
      Jmsg(nullptr, M_FATAL, 0,
           _("AlwaysIncremental configured in job %s which is not of job type "
             "\"backup\" in file %s\n"),
           job->resource_name_, configfile.c_str());
      OK = false;
      goto bail_out;
    }
  }

  ConsoleResource* cons;
  foreach_res (cons, R_CONSOLE) {
    if (cons->IsTlsConfigured()) {
      if (!have_tls) {
        Jmsg(nullptr, M_FATAL, 0,
             _("TLS required but not configured in BAREOS.\n"));
        OK = false;
        goto bail_out;
      }
    }
  }

  ClientResource* client;
  foreach_res (client, R_CLIENT) {
    if (client->IsTlsConfigured()) {
      if (!have_tls) {
        Jmsg(nullptr, M_FATAL, 0, _("TLS required but not configured.\n"));
        OK = false;
        goto bail_out;
      }
    }
  }

  StorageResource *store, *nstore;
  foreach_res (store, R_STORAGE) {
    if (store->IsTlsConfigured()) {
      if (!have_tls) {
        Jmsg(nullptr, M_FATAL, 0, _("TLS required but not configured.\n"));
        OK = false;
        goto bail_out;
      }
    }

    /*
     * If we collect statistics on this SD make sure any other entry pointing to
     * the same SD does not collect statistics otherwise we collect the same
     * data multiple times.
     */
    if (store->collectstats) {
      nstore = store;
      while ((nstore = (StorageResource*)my_config->GetNextRes(
                  R_STORAGE, (BareosResource*)nstore))) {
        if (IsSameStorageDaemon(store, nstore) && nstore->collectstats) {
          nstore->collectstats = false;
          Dmsg1(200,
                _("Disabling collectstats for storage \"%s\""
                  " as other storage already collects from this SD.\n"),
                nstore->resource_name_);
        }
      }
    }
  }

  if (OK) {
    CloseMsg(nullptr);              /* close temp message handler */
    InitMsg(nullptr, me->messages); /* open daemon message handler */
    if (me->secure_erase_cmdline) {
      SetSecureEraseCmdline(me->secure_erase_cmdline);
    }
    if (me->log_timestamp_format) {
      SetLogTimestampFormat(me->log_timestamp_format);
    }
  }

bail_out:
  UnlockRes(my_config);
  return OK;
}

// Initialize the sql pooling.
bool InitializeSqlPooling(void)
{
  bool retval = true;
  CatalogResource* catalog;

  foreach_res (catalog, R_CATALOG) {
    if (!db_sql_pool_initialize(
            catalog->db_driver, catalog->db_name, catalog->db_user,
            catalog->db_password.value, catalog->db_address, catalog->db_port,
            catalog->db_socket, catalog->disable_batch_insert,
            catalog->try_reconnect, catalog->exit_on_fatal,
            catalog->pooling_min_connections, catalog->pooling_max_connections,
            catalog->pooling_increment_connections,
            catalog->pooling_idle_timeout, catalog->pooling_validate_timeout)) {
      Jmsg(nullptr, M_FATAL, 0,
           _("Could not setup sql pooling for Catalog \"%s\", database "
             "\"%s\".\n"),
           catalog->resource_name_, catalog->db_name);
      retval = false;
      goto bail_out;
    }
  }

bail_out:
  return retval;
}

bool DoReloadConfig()
{
  static bool is_reloading = false;
  bool reloaded = false;


  if (is_reloading) {
    /*
     * Note: don't use Jmsg here, as it could produce a race condition
     * on multiple parallel reloads
     */
    Qmsg(nullptr, M_ERROR, 0, _("Already reloading. Request ignored.\n"));
    return false;
  }
  is_reloading = true;

  StopStatisticsThread();

  LockJobs();
  LockRes(my_config);

  DbSqlPoolFlush();

  auto backup_table = my_config->BackupResourceTable();
  Dmsg0(100, "Reloading config file\n");


  my_config->err_type_ = M_ERROR;
  my_config->ClearWarnings();
  bool ok = my_config->ParseConfig();

  // parse config successful
  if (ok && CheckResources() && CheckCatalog(UPDATE_CATALOG)
      && InitializeSqlPooling()) {
    Scheduler::GetMainScheduler().ClearQueue();

    reloaded = true;

    SetWorkingDirectory(me->working_directory);
    Dmsg0(10, "Director's configuration file reread successfully.\n");
    Dmsg0(10, "Releasing previous configuration resource table.\n");

    StartStatisticsThread();

  } else {  // parse config failed
    Jmsg(nullptr, M_ERROR, 0, _("Please correct the configuration in %s\n"),
         my_config->get_base_config_path().c_str());

    Jmsg(nullptr, M_ERROR, 0, _("Resetting to previous configuration.\n"));
    my_config->RestoreResourceTable(std::move(backup_table));
    // me is changed above by CheckResources()
    me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
    assert(me);
    my_config->own_resource_ = me;

    StartStatisticsThread();
  }

  UnlockRes(my_config);
  UnlockJobs();
  is_reloading = false;
  return reloaded;
}


}  // namespace directordaemon
