/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2025 Bareos GmbH & Co. KG

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
#include <atomic>

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
  JobResource* job;

  ResLocker _{my_config};

  const std::string& configfile_name = my_config->get_base_config_path();

  job = (JobResource*)my_config->GetNextRes(R_JOB, nullptr);
  me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
  my_config->own_resource_ = me;
  if (!me) {
    Jmsg(nullptr, M_FATAL, 0,
         T_("No Director resource defined in %s\n"
            "Without that I don't know who I am :-(\n"),
         configfile_name.c_str());
    return false;
  } else {
    my_config->omit_defaults_ = true;
    SetWorkingDirectory(me->working_directory);

    // See if message resource is specified.
    if (!me->messages) {
      me->messages = (MessagesResource*)my_config->GetNextRes(R_MSGS, nullptr);
      if (!me->messages) {
        Jmsg(nullptr, M_FATAL, 0, T_("No Messages resource defined in %s\n"),
             configfile_name.c_str());
        return false;
      }
    }

    if (my_config->GetNextRes(R_DIRECTOR, (BareosResource*)me) != nullptr) {
      Jmsg(nullptr, M_FATAL, 0,
           T_("Only one Director resource permitted in %s\n"),
           configfile_name.c_str());
      return false;
    }
  }

  if (!job) {
    Jmsg(nullptr, M_FATAL, 0, T_("No Job records defined in %s\n"),
         configfile_name.c_str());
    return false;
  }

  if (!PopulateDefs()) { return false; }

  // Loop over Jobs
  foreach_res (job, R_JOB) {
    if (job->MaxFullConsolidations && job->JobType != JT_CONSOLIDATE) {
      Jmsg(nullptr, M_FATAL, 0,
           T_("MaxFullConsolidations configured in job %s which is not of job "
              "type \"consolidate\" in file %s\n"),
           job->resource_name_, configfile_name.c_str());
      return false;
    }

    if (job->JobType != JT_BACKUP
        && (job->AlwaysIncremental || job->AlwaysIncrementalJobRetention
            || job->AlwaysIncrementalKeepNumber
            || job->AlwaysIncrementalMaxFullAge)) {
      Jmsg(nullptr, M_FATAL, 0,
           T_("AlwaysIncremental configured in job %s which is not of job type "
              "\"backup\" in file %s\n"),
           job->resource_name_, configfile_name.c_str());
      return false;
    }
  }

  StorageResource *store, *nstore;
  foreach_res (store, R_STORAGE) {
    /* If we collect statistics on this SD make sure any other entry pointing to
     * the same SD does not collect statistics otherwise we collect the same
     * data multiple times. */
    if (store->collectstats) {
      nstore = store;
      while ((nstore = (StorageResource*)my_config->GetNextRes(
                  R_STORAGE, (BareosResource*)nstore))) {
        if (IsSameStorageDaemon(store, nstore) && nstore->collectstats) {
          nstore->collectstats = false;
          Dmsg1(200,
                T_("Disabling collectstats for storage \"%s\""
                   " as other storage already collects from this SD.\n"),
                nstore->resource_name_);
        }
      }
    }
  }

  ConsoleResource* console;
  BStringList consoles_with_auth_problems;
  foreach_res (console, R_CONSOLE) {
    if (console->use_pam_authentication_
        && (console->user_acl.HasAcl() || console->user_acl.profiles)) {
      consoles_with_auth_problems.emplace_back(console->resource_name_);
    }
  }
  if (!consoles_with_auth_problems.empty()) {
    Jmsg(nullptr, M_FATAL, 0,
         "Combining `Use Pam Authentication` with ACL commands or `Profile` in "
         "Console(s) `%s` is not allowed\n",
         consoles_with_auth_problems.Join(',').c_str());
    return false;
  }

  CloseMsg(nullptr);              /* close temp message handler */
  InitMsg(nullptr, me->messages); /* open daemon message handler */
  if (me->secure_erase_cmdline) {
    SetSecureEraseCmdline(me->secure_erase_cmdline);
  }
  if (me->log_timestamp_format) {
    SetLogTimestampFormat(me->log_timestamp_format);
  }

  return true;
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
           T_("Could not setup sql pooling for Catalog \"%s\", database "
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
  // ATOMIC_FLAG_INIT is needed until we switch to C++20;
  // but it can be kept even after that.
  static std::atomic_flag is_reloading = ATOMIC_FLAG_INIT;
  bool reloaded = false;


  if (is_reloading.test_and_set()) {
    /* Note: don't use Jmsg here, as it could produce a race condition
     * on multiple parallel reloads */
    Qmsg(nullptr, M_ERROR, 0, T_("Already reloading. Request ignored.\n"));
    return false;
  }

  StopStatisticsThread();

  LockJobs();
  ResLocker _{my_config};

  DbSqlPoolFlush();

  auto backup_container = my_config->BackupResourcesContainer();
  Dmsg0(100, "Reloading config file\n");


  my_config->err_type_ = M_ERROR;
  my_config->ClearWarnings();
  bool ok = my_config->ParseConfig();

  // parse config successful
  if (ok && CheckResources() && CheckCatalog(UPDATE_CATALOG)
      && InitializeSqlPooling()) {
    Scheduler::GetMainScheduler().ClearQueue();
    reloaded = true;

    // make sure that jobs that keep the last configuration alive
    // also keep alive this config in case they (accidentally) access it.
    backup_container->SetNext(my_config->GetResourcesContainer());

    Dmsg0(10, "Director's configuration file reread successfully.\n");
  } else {  // parse config failed
    Jmsg(nullptr, M_ERROR, 0, T_("Please correct the configuration in %s\n"),
         my_config->get_base_config_path().c_str());
    Jmsg(nullptr, M_ERROR, 0, T_("Resetting to previous configuration.\n"));
    my_config->RestoreResourcesContainer(std::move(backup_container));
    // me is changed above by CheckResources()
    me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
    assert(me);
    my_config->own_resource_ = me;
  }
  SetWorkingDirectory(me->working_directory);
  StartStatisticsThread();
  UnlockJobs();

  is_reloading.clear();
  return reloaded;
}


}  // namespace directordaemon
