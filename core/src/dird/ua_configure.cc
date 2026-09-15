/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2026 Bareos GmbH & Co. KG

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
// Written by Marco van Wieringen, January 2015
/**
 * @file
 * Interactive configuration engine for director.
 */

#include "include/fcntl_def.h"
#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/reload.h"
#include "dird/ua_select.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include <array>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace directordaemon {

static void ConfigureLexErrorHandler(const char*, int, lexer* lc, PoolMem& msg)
{
  UaContext* ua;

  lc->error_counter++;
  if (lc->caller_ctx) {
    ua = (UaContext*)(lc->caller_ctx);
    ua->ErrorMsg("configure error: %s\n", msg.c_str());
  }
}

static void ConfigureLexErrorHandler(const char* file,
                                     int line,
                                     lexer* lc,
                                     const char* msg,
                                     ...)
{
  // This function is an error handler, used by lex.
  PoolMem buf(PM_NAME);
  va_list ap;

  va_start(ap, msg);
  buf.Bvsprintf(msg, ap);
  va_end(ap);

  ConfigureLexErrorHandler(file, line, lc, buf);
}

static inline bool configure_write_resource(const char* filename,
                                            const char*,
                                            const char*,
                                            const char* content,
                                            const bool overwrite = false)
{
  bool result = false;
  size_t len;
  int fd;
  int flags = O_CREAT | O_WRONLY | O_TRUNC;

  if (!overwrite) { flags |= O_EXCL; }

  if ((fd = open(filename, flags, 0640)) >= 0) {
    len = strlen(content);
    if (write(fd, content, len) < 0) {
      result = false;
    } else {
      result = true;
    }
    close(fd);
  }

  return result;
}

static inline const ResourceItem* config_get_res_item(
    UaContext* ua,
    const ResourceTable* res_table,
    const char* key,
    const char* value)
{
  const ResourceItem* item = NULL;
  const char* errorcharmsg = NULL;

  if (res_table) {
    item = my_config->GetResourceItem(res_table->items, key);
    if (!item) {
      ua->ErrorMsg("Resource \"%s\" does not permit directive \"%s\".\n",
                   res_table->name, key);
      return NULL;
    }
  }

  /* Check against dangerous characters ('@', ';').
   * Could be less strict, if this characters are quoted,
   * but it is easier to handle it like this. */
  if (strchr(value, '@')) { errorcharmsg = "'@' (include)"; }
  if (strchr(value, ';')) { errorcharmsg = "';' (end of directive)"; }
  if (errorcharmsg) {
    if (ua) {
      ua->ErrorMsg(
          "Could not add directive \"%s\": character %s is forbidden.\n", key,
          errorcharmsg);
    }
    return NULL;
  }

  return item;
}

static inline bool config_add_directive(UaContext* ua,
                                        const ResourceTable* res_table,
                                        const char* key,
                                        const char* value,
                                        PoolMem& resource,
                                        int indent = 2)
{
  PoolMem temp(PM_MESSAGE);
  const ResourceItem* item = NULL;
  std::string format("%-*s%s = %s\n");

  const std::array<int, 15> quotable_types{
      CFG_TYPE_AUTOPASSWORD,   CFG_TYPE_CLEARPASSWORD, CFG_TYPE_DIR,
      CFG_TYPE_DIR_OR_CMD,     CFG_TYPE_LABEL,         CFG_TYPE_MD5PASSWORD,
      CFG_TYPE_NAME,           CFG_TYPE_RES,           CFG_TYPE_RUNSCRIPT_CMD,
      CFG_TYPE_RUNSCRIPT_WHEN, CFG_TYPE_SHRTRUNSCRIPT, CFG_TYPE_STDSTR,
      CFG_TYPE_STDSTRDIR,      CFG_TYPE_STR,           CFG_TYPE_STRNAME};

  const std::array<int, 3> script_types{
      CFG_TYPE_RUNSCRIPT_CMD, CFG_TYPE_DIR_OR_CMD, CFG_TYPE_SHRTRUNSCRIPT};

  std::string temp_value(value);

  item = config_get_res_item(ua, res_table, key, value);

  if (res_table && (!item)) { return false; }

  if (item) {
    // Use item->name instead of key for uniform formatting.
    key = item->name;
    if (std::find(quotable_types.begin(), quotable_types.end(), item->type)
        != quotable_types.end()) {
      format = "%-*s%s = \"%s\"\n";

      if (std::find(script_types.begin(), script_types.end(), item->type)
          != script_types.end()) {
        temp_value = EscapeString(value);
      }
    }
  }

  temp.bsprintf(format.c_str(), indent, "", key, temp_value.c_str());

  resource.strcat(temp);

  return true;
}

static inline bool configure_create_resource_string(
    UaContext* ua,
    int first_parameter,
    const ResourceTable* res_table,
    PoolMem& resourcename,
    PoolMem& resource)
{
  resource.strcat(res_table->name);
  resource.strcat(" {\n");

  /* Is the name of the resource already given as value of the resource type?
   * E.g. configure add client=newclient address=127.0.0.1 ...
   * instead of configure add client name=newclient address=127.0.0.1 ... */
  if (ua->argv[first_parameter - 1]) {
    resourcename.strcat(ua->argv[first_parameter - 1]);
    if (!config_add_directive(ua, res_table, "name", resourcename.c_str(),
                              resource)) {
      return false;
    }
  }

  for (int i = first_parameter; i < ua->argc; i++) {
    if (!ua->argv[i]) {
      ua->ErrorMsg("Missing value for directive \"%s\"\n", ua->argk[i]);
      return false;
    }
    if (Bstrcasecmp(ua->argk[i], "name")) { resourcename.strcat(ua->argv[i]); }
    if (!config_add_directive(ua, res_table, ua->argk[i], ua->argv[i],
                              resource)) {
      return false;
    }
  }
  resource.strcat("}\n");

  if (strlen(resourcename.c_str()) <= 0) {
    ua->ErrorMsg("Resource \"%s\": missing name parameter.\n", res_table->name);
    return false;
  }

  return true;
}

static inline bool ConfigureCreateFdResourceString(UaContext* ua,
                                                   PoolMem& resource,
                                                   const char* clientname)
{
  ClientResource* client;
  s_password* password;
  PoolMem temp(PM_MESSAGE);

  client = ua->GetClientResWithName(clientname);
  if (!client) { return false; }
  password = &client->password_;

  resource.strcat("Director {\n");

  Mmsg(temp, "\"%s\"", me->resource_name_);
  config_add_directive(NULL, NULL, "Name", temp.c_str(), resource);

  switch (password->encoding) {
    case p_encoding_clear:
      Mmsg(temp, "\"%s\"", password->value);
      break;
    case p_encoding_md5:
      Mmsg(temp, "\"[md5]%s\"", password->value);
      break;
    default:
      break;
  }
  config_add_directive(NULL, NULL, "Password", temp.c_str(), resource);

  resource.strcat("}\n");

  return true;
}

/**
 * Create a bareos-fd director resource file
 * that corresponds to our client definition.
 */
static inline bool ConfigureCreateFdResource(UaContext* ua,
                                             const char* clientname)
{
  PoolMem resource(PM_MESSAGE);
  PoolMem filename_tmp(PM_FNAME);
  PoolMem filename(PM_FNAME);
  PoolMem basedir(PM_FNAME);
  PoolMem temp(PM_MESSAGE);
  const char* dirname = NULL;
  const bool error_if_exists = false;
  const bool create_directories = true;
  const bool overwrite = true;

  if (!ConfigureCreateFdResourceString(ua, resource, clientname)) {
    return false;
  }

  /* Get the path where the resource should get stored.
   *
   * Use me rather than GetNextRes(R_DIRECTOR, NULL): GetNextRes() does not
   * take the resource lock, and this runs unlocked (both from
   * ConfigureAddResource() and from "configure export"). me is guaranteed
   * non-null by CheckResources() and is swapped as a plain pointer, so
   * reading it here does not race on the loaded_configuration shared_ptr. */
  basedir.bsprintf("bareos-dir-export/client/%s/bareos-fd.d", clientname);
  dirname = me->resource_name_;
  if (!my_config->GetPathOfNewResource(filename, temp, basedir.c_str(),
                                       "director", dirname, error_if_exists,
                                       create_directories)) {
    ua->ErrorMsg("%s", temp.c_str());
    return false;
  }
  filename_tmp.strcpy(temp);

  // Write resource to file.
  if (!configure_write_resource(filename.c_str(), "filedaemon-export",
                                clientname, resource.c_str(), overwrite)) {
    ua->ErrorMsg("failed to write filedaemon config resource file\n");
    return false;
  }

  ua->send->ObjectStart("export");
  ua->send->ObjectKeyValue("clientname", clientname);
  ua->send->ObjectKeyValue("component", "bareos-fd");
  ua->send->ObjectKeyValue("resource", "director");
  ua->send->ObjectKeyValue("name", dirname);
  ua->send->ObjectKeyValue("filename", filename.c_str(),
                           "Exported resource file \"%s\":\n");
  ua->send->ObjectKeyValue("content", resource.c_str(), "%s");
  ua->send->ObjectEnd("export");

  return true;
}

/**
 * "configure add client" writes a File Daemon export file containing a
 * plaintext copy of the Director's password (see
 * ConfigureCreateFdResource() above) to
 * bareos-dir-export/client/<clientname>/bareos-fd.d/director/<dirname>.conf.
 * Deleting the Client resource must also remove this file: otherwise it is
 * left behind as a stale, credential-bearing artifact (CWE-459). Removal is
 * best-effort and does not fail the delete: the Client resource itself is
 * already gone by the time this is called, so there is nothing left to roll
 * back, and a leftover export file is reported to the user instead.
 */
static inline void ConfigureRemoveFdExport(UaContext* ua,
                                           const char* clientname)
{
  PoolMem basedir(PM_FNAME);
  PoolMem path(PM_FNAME);

  /* me is set and non-null by CheckResources(), which the reload that just
   * succeeded had to pass. */
  const char* dirname = me->resource_name_;

  basedir.bsprintf("bareos-dir-export/client/%s/bareos-fd.d", clientname);
  if (!my_config->GetPathOfResource(path, basedir.c_str(), "director", dirname,
                                    false)) {
    Dmsg1(200,
         "Could not determine filedaemon export file path for client "
         "\"%s\"; skipping export removal.\n",
         clientname);
    return;
  }

  std::error_code ec;
  std::filesystem::path export_file{path.c_str()};
  std::filesystem::remove(export_file, ec);
  if (ec) {
    ua->WarningMsg(T_("failed to remove filedaemon export file \"%s\": %s\n"),
                   path.c_str(), ec.message().c_str());
    return;
  }

  /* Prune the three directory levels that exist only to hold this client's
   * export: .../<clientname>/bareos-fd.d/director, .../bareos-fd.d and
   * .../<clientname>. std::filesystem::remove() refuses to remove a
   * non-empty directory, so this can never take anything that is still in
   * use with it.
   *
   * A "director" directory that is not empty means an export written under
   * a previous Director resource name is still sitting there. That file
   * holds a plaintext copy of a Director password, so point at it rather
   * than leave it behind silently. */
  std::filesystem::path dir = export_file.parent_path();
  for (int level = 0; level < 3; level++) {
    ec.clear();
    if (std::filesystem::remove(dir, ec)) {
      dir = dir.parent_path();
      continue;
    }

    /* remove() reports a non-empty directory as an error; it returns false
     * with ec clear only when the path was not there to begin with, which
     * is nothing to report. */
    if (ec == std::errc::directory_not_empty) {
      if (level == 0) {
        ua->WarningMsg(
            T_("Filedaemon export directory \"%s\" is not empty. It may still "
               "hold an export written under a different Director resource "
               "name, which contains a copy of the Director password. Please "
               "remove it manually.\n"),
            dir.c_str());
      }
      /* Higher up, a non-empty directory just means something else lives
       * there. Stop pruning, but there is nothing to warn about. */
    } else if (ec) {
      ua->WarningMsg(
          T_("failed to remove filedaemon export directory \"%s\": %s\n"),
          dir.c_str(), ec.message().c_str());
    }
    return;
  }
}

/**
 * To add a resource during runtime, the following approach is used:
 *
 * - Create a temporary file which contains the new resource.
 * - Use the existing parsing functions to add the new resource to the
 * configured resources.
 *   - on error:
 *     - remove the resource and the temporary file.
 *   - on success:
 *     - move the new temporary resource file to a place, where it will also be
 * loaded on restart
 *       (<CONFIGDIR>/bareos-dir.d/<resourcetype>/<name_of_the_resource>.conf).
 *
 * This way, the existing parsing functionality is used.
 */
static inline bool ConfigureAddResource(UaContext* ua,
                                        int first_parameter,
                                        const ResourceTable* res_table)
{
  PoolMem resource(PM_MESSAGE);
  PoolMem name(PM_MESSAGE);
  PoolMem filename_tmp(PM_FNAME);
  PoolMem filename(PM_FNAME);
  PoolMem temp(PM_FNAME);
  JobResource* res = NULL;

  /* Scoped deliberately. The lock is needed for the check-then-mutate
   * sequence below, since ParseConfigFile() -> SaveResource() ->
   * AppendToResourcesChain() and RemoveResource() do not lock the resource
   * tree themselves. The underlying rwlock is re-entrant for the locking
   * thread, so nesting with the locking the called functions (e.g.
   * GetResWithName()) do themselves is safe.
   *
   * It must not be held any longer than that, though: b_LockRes() takes a
   * *write* lock, so for as long as it is held every foreach_res() and
   * GetResWithName() in every other thread blocks. Writing the export file
   * and the command's output to the console socket below therefore happens
   * unlocked -- a slow or wedged console must not be able to stall config
   * access director-wide.
   *
   * The failure paths inside the scope do write to the console, and cannot
   * be moved out of it without restructuring how the parser reports errors:
   * configure_create_resource_string() writes to ua itself, and
   * ParseConfigFile() below is handed ua so that lexer errors reach the
   * console at all. What is bounded here is the bulk of the writing and the
   * successful path; an error ends the command right after the message.
   *
   * Note that the RemoveResource() calls below only ever free a resource
   * that this function just added and that failed validation, i.e. one that
   * was never visible to anything else. Freeing a resource that has been
   * part of the running configuration is not safe and is not done here; see
   * ConfigureDeleteResource() for why deletion goes through a reload
   * instead. */
  {
    ResLocker _{my_config};

    if (!configure_create_resource_string(ua, first_parameter, res_table, name,
                                          resource)) {
      return false;
    }

    if (my_config->GetResWithName(res_table->rcode, name.c_str())) {
      ua->ErrorMsg("Resource \"%s\" with name \"%s\" already exists.\n",
                   res_table->name, name.c_str());
      return false;
    }

    if (!my_config->GetPathOfNewResource(filename, temp, NULL, res_table->name,
                                         name.c_str(), true)) {
      ua->ErrorMsg("%s", temp.c_str());
      return false;
    } else {
      filename_tmp.strcpy(temp);
    }

    if (!configure_write_resource(filename_tmp.c_str(), res_table->name,
                                  name.c_str(), resource.c_str())) {
      ua->ErrorMsg("failed to write config resource file\n");
      return false;
    }

    my_config->err_type_ = M_ERROR;
    if (!my_config->ParseConfigFile(filename_tmp.c_str(), ua,
                                    ConfigureLexErrorHandler, NULL)) {
      unlink(filename_tmp.c_str());
      my_config->RemoveResource(res_table->rcode, name.c_str());
      return false;
    }

    /* ParseConfigFile has already done some validation.
     * However, it skipped at least some checks for R_JOB
     * (reason: a job can get values from jobdefs,
     * and the value propagation happens after reading the full configuration)
     * therefore we explicitly check the new resource here. */
    if ((res_table->rcode == R_JOB) || (res_table->rcode == R_JOBDEFS)) {
      res = (JobResource*)my_config->GetResWithName(res_table->rcode,
                                                    name.c_str());
      PropagateJobdefs(res_table->rcode, res);
      if (!ValidateResource(res_table->rcode, res_table->items,
                            (BareosResource*)res)) {
        ua->ErrorMsg("failed to create config resource \"%s\"\n", name.c_str());
        unlink(filename_tmp.c_str());
        my_config->RemoveResource(res_table->rcode, name.c_str());
        return false;
      }
    }

    // new config resource is working fine. Rename file to its permanent name.
    if (rename(filename_tmp.c_str(), filename.c_str()) != 0) {
      ua->ErrorMsg("failed to create config file \"%s\"\n", filename.c_str());
      unlink(filename_tmp.c_str());
      my_config->RemoveResource(res_table->rcode, name.c_str());
      return false;
    }
  }

  // When adding a client, also create the File Daemon client resource file.
  if (res_table->rcode == R_CLIENT) {
    ConfigureCreateFdResource(ua, name.c_str());
  }

  ua->send->ObjectStart("add");
  ua->send->ObjectKeyValue("resource", res_table->name);
  ua->send->ObjectKeyValue("name", name.c_str());
  ua->send->ObjectKeyValue("filename", filename.c_str(),
                           "Created resource config file \"%s\":\n");
  ua->send->ObjectKeyValue("content", resource.c_str(), "%s");
  ua->send->ObjectEnd("add");

  return true;
}

static inline bool ConfigureAdd(UaContext* ua, int resource_type_parameter)
{
  bool result = false;
  const ResourceTable* res_table
      = my_config->GetResourceTable(ua->argk[resource_type_parameter]);
  if (!res_table) {
    ua->ErrorMsg(T_("invalid resource type %s.\n"),
                 ua->argk[resource_type_parameter]);
    return false;
  }

  if (res_table->rcode == R_DIRECTOR) {
    ua->ErrorMsg(T_("Only one Director resource allowed.\n"));
    return false;
  }

  ua->send->ObjectStart("configure");
  result = ConfigureAddResource(ua, resource_type_parameter + 1, res_table);
  ua->send->ObjectEnd("configure");

  return result;
}

/**
 * FindResourceReferences() only scans directives of type CFG_TYPE_RES and
 * CFG_TYPE_ALIST_RES, which covers plain resource-pointer directives. A
 * Schedule's "Run" directive is of type CFG_TYPE_RUN instead: it can carry
 * its own Pool/Storage/Messages overrides via a chain of RunResource
 * records that live only in the dird-specific ScheduleResource, so the
 * generic (daemon-agnostic) FindResourceReferences() in lib/parse_conf.cc
 * cannot inspect them. Scan those overrides here instead, at the dird
 * level where RunResource is visible, and append any matches so a
 * resource still referenced only via a Run override is not deleted out
 * from under a Schedule.
 */
static inline void FindRunResourceReferences(
    int rcode,
    const BareosResource* target,
    std::vector<ResourceReference>* references)
{
  ScheduleResource* sched = nullptr;
  foreach_res (sched, R_SCHEDULE) {
    for (RunResource* run = sched->run; run; run = run->next) {
      auto check = [&](BareosResource* referenced, const char* directive) {
        if (referenced == target) {
          references->push_back({R_SCHEDULE, sched->resource_name_, directive});
        }
      };
      switch (rcode) {
        case R_POOL:
          check(run->pool, "Pool");
          check(run->full_pool, "FullBackupPool");
          check(run->vfull_pool, "VirtualFullBackupPool");
          check(run->inc_pool, "IncrementalBackupPool");
          check(run->diff_pool, "DifferentialBackupPool");
          check(run->next_pool, "NextPool");
          break;
        case R_STORAGE:
          check(run->storage, "Storage");
          break;
        case R_MSGS:
          check(run->msgs, "Messages");
          break;
        default:
          break;
      }
    }
  }
}

/**
 * ConfigureDeleteResource() enacts a deletion by reloading the whole
 * configuration; see the comment there for why that is the only safe way to
 * do it. DoReloadConfig() does more than swap the configuration though: it
 * also flushes and re-checks the catalog, restarts the statistics thread and
 * clears the scheduler queue, none of which a unit test can provide. The
 * call therefore goes through this pointer, which the tests in
 * core/src/tests/configure.cc replace with a reload that performs the
 * configuration swap only. The director itself never reassigns it.
 */
static bool (*ConfigureReloadConfig)() = DoReloadConfig;

/**
 * A Schedule with several "Run" lines that all override the same Pool
 * reports that Pool once per line, and a resource can in principle be
 * reached by both the generic and the Run scan. Collapse identical
 * (resource type, resource name, directive) triples so the user is told
 * once per place they actually have to go and edit.
 */
static inline void ConfigureDedupReferences(
    std::vector<ResourceReference>* references)
{
  auto key = [](const ResourceReference& ref) {
    return std::tie(ref.rcode, ref.resource_name, ref.directive_name);
  };
  auto less = [&key](const ResourceReference& a, const ResourceReference& b) {
    return key(a) < key(b);
  };
  auto equal = [&key](const ResourceReference& a, const ResourceReference& b) {
    return key(a) == key(b);
  };

  std::sort(references->begin(), references->end(), less);
  references->erase(std::unique(references->begin(), references->end(), equal),
                    references->end());
}

static inline void ConfigureSendReferences(
    UaContext* ua,
    const std::vector<ResourceReference>& references)
{
  ua->send->ArrayStart("references");
  for (const auto& ref : references) {
    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("resource", my_config->ResToStr(ref.rcode));
    ua->send->ObjectKeyValue("name", ref.resource_name.c_str());
    ua->send->ObjectKeyValue("directive", ref.directive_name.c_str());
    ua->send->ObjectEnd();
  }
  ua->send->ArrayEnd("references");
}

using ResourceKey = std::pair<int, std::string>;

/**
 * Every resource in the running configuration, as (resource type, name)
 * pairs.
 *
 * Deleting works on whole configuration files, but nothing requires a
 * configuration file to define only the one resource its name suggests --
 * that is a convention "configure add" follows, not a rule the parser
 * enforces on hand-written files. Comparing this before and after the
 * reload is what turns a resource that was removed as collateral from a
 * silent loss into a reported one.
 *
 * The caller must hold the resource lock: GetNextRes() does not take it.
 */
static std::set<ResourceKey> ConfigureResourceNames()
{
  std::set<ResourceKey> names;

  for (int rcode = 0; rcode < R_NUM; rcode++) {
    BareosResource* res = nullptr;
    while ((res = my_config->GetNextRes(rcode, res)) != nullptr) {
      if (res->resource_name_) { names.emplace(rcode, res->resource_name_); }
    }
  }

  return names;
}

static inline void ConfigureSendAlsoRemoved(
    UaContext* ua,
    const std::vector<ResourceKey>& also_removed)
{
  ua->send->ArrayStart("also_removed");
  for (const auto& [rcode, resource_name] : also_removed) {
    ua->send->ObjectStart();
    ua->send->ObjectKeyValue("resource", my_config->ResToStr(rcode));
    ua->send->ObjectKeyValue("name", resource_name.c_str());
    ua->send->ObjectEnd();
  }
  ua->send->ArrayEnd("also_removed");
}

/**
 * To delete a resource during runtime, the following approach is used:
 *
 * - Look up the resource and refuse to proceed if any other loaded
 *   resource still references it. The caller must remove or update the
 *   referencing resource(s) first.
 * - Move the resource's on-disk config file out of the way.
 * - Reload the whole configuration from disk.
 *
 * The resource is deliberately *not* removed from the live configuration
 * in place. The invariant that forbids it: raw resource pointers outlive
 * the config graph. A job started with "run job=<job> client=<client>"
 * keeps a raw ClientResource* in its JobControlRecord for its whole
 * runtime even when no other resource references that Client, so freeing
 * it would leave the running job with a dangling pointer. Deletion must
 * therefore go through a configuration generation swap, never a free.
 *
 * DoReloadConfig() is exactly that swap, and jobs pin the generation they
 * were started with in DirectorJcrImpl::used_config_for_job, so a deleted
 * resource stays valid for as long as some job can still reach it. See
 * dird/reload.cc for how that is arranged; "configure delete" ends up with
 * the same safety properties as the "reload" command, which is the only
 * reason it is safe at all.
 *
 * Because the reload re-reads everything from disk, it is also the real
 * validator: if a resource added concurrently references the one being
 * deleted, the parse fails and the previous configuration is restored. The
 * reference check below is therefore an early, friendly error rather than
 * the thing that makes this correct.
 */
static inline bool ConfigureDeleteResource(UaContext* ua,
                                           const ResourceTable* res_table,
                                           const char* name)
{
  PoolMem path(PM_FNAME);
  PoolMem stashed_path(PM_FNAME);
  std::string target_name;
  std::set<ResourceKey> names_before;

  /* Scoped deliberately: the lock must be released before DoReloadConfig()
   * is called below. DoReloadConfig() acquires LockJobs() and only then the
   * resource lock, so holding the resource lock across the call would
   * invert the lock order. Nothing here needs to stay locked afterwards:
   * the reload re-reads and re-validates the whole configuration from disk
   * and rolls back on failure, so it, not this check, is what makes the
   * delete safe. */
  {
    ResLocker _{my_config};

    BareosResource* res = my_config->GetResWithName(res_table->rcode, name);
    if (!res) {
      ua->ErrorMsg(T_("Resource \"%s\" with name \"%s\" does not exist.\n"),
                   res_table->name, name);
      return false;
    }

    std::vector<ResourceReference> references
        = my_config->FindResourceReferences(res_table->rcode, res);
    FindRunResourceReferences(res_table->rcode, res, &references);
    ConfigureDedupReferences(&references);
    if (!references.empty()) {
      ua->send->ObjectStart("delete");
      ua->send->ObjectKeyValue("resource", res_table->name);
      ua->send->ObjectKeyValue("name", name);
      ConfigureSendReferences(ua, references);
      ua->send->ObjectEnd("delete");
      ua->ErrorMsg(
          T_("Resource \"%s\" with name \"%s\" is still referenced by %zu "
             "other resource(s). Remove or update those first.\n"),
          res_table->name, name, references.size());
      return false;
    }

    if (!my_config->GetPathOfResource(path, NULL, res_table->name, name,
                                      false)) {
      ua->ErrorMsg(
          T_("failed to determine config resource file path for \"%s\"\n"),
          name);
      return false;
    }

    /* Key the snapshot below off the resource's own name rather than the
     * command line. GetResWithName() matches exactly, so the two are the
     * same string today; taking it from the resource is what keeps them the
     * same if that ever stops being true. */
    target_name = res->resource_name_;
    names_before = ConfigureResourceNames();
  }

  /* Removal is enacted by reloading the configuration without this file, so
   * the resource must actually live in its own file below the config
   * include dir. If it does not -- e.g. it is defined inline in
   * bareos-dir.conf -- there is nothing to remove and the reload would
   * simply load it again. Say so instead of reporting a delete that did not
   * happen.
   *
   * Move the file aside rather than delete it outright. The reload can
   * fail, and renaming it back is atomic, cannot itself fail for lack of
   * space or permissions, and preserves the file's mode, ownership and
   * timestamps -- none of which is true of reading the contents into memory
   * and writing them out again. Capturing the contents and removing the
   * file is also a single operation this way, where reading and unlinking
   * would leave a window in which a concurrent "configure add" of the same
   * name could create the file between the two and have it unlinked with
   * contents that were never read, and therefore cannot be restored.
   *
   * Note that the rename frees the path just as an unlink would, so the
   * window between it and the rollback below is not closed by any of this:
   * a concurrent "configure add" that gets far enough to see the name as
   * absent can write the path in that window, and the rollback then
   * overwrites what it wrote. That needs the reload to fail because another
   * one was already in flight and removed the resource, since a reload that
   * fails to parse restores a configuration that still holds the name, and
   * ConfigureAddResource() refuses a name it can still see.
   *
   * The ".deleted" suffix keeps the stashed file out of the config include
   * dir's "*.conf" glob, so the reload below does not pick it up again.
   * This mirrors the ".tmp" suffix ConfigureAddResource() uses. */
  stashed_path.bsprintf("%s.deleted", path.c_str());

  /* A stash left behind by an earlier delete (see the "also_removed" handling
   * below) is the only remaining copy of whatever else that removed file
   * defined. rename() below would silently replace it -- POSIX rename()
   * overwrites an existing destination -- losing that copy for good. This
   * can only happen if a "configure add" recreated the same path in between,
   * since GetResWithName() above already confirms a resource is currently
   * loaded at this name. Refuse rather than clobber; the stash must be
   * recovered or removed manually first. */
  if (std::filesystem::exists(stashed_path.c_str())) {
    ua->ErrorMsg(
        T_("A stashed copy of a previously removed configuration file "
           "already exists at \"%s\". It was kept because removing it had "
           "also removed other resources from the configuration. Recover or "
           "remove it manually before deleting \"%s\" again.\n"),
        stashed_path.c_str(), path.c_str());
    return false;
  }

  if (rename(path.c_str(), stashed_path.c_str()) != 0) {
    if (errno == ENOENT) {
      ua->ErrorMsg(
          T_("Resource \"%s\" with name \"%s\" is not defined in its own "
             "configuration file \"%s\" and can therefore not be deleted.\n"),
          res_table->name, name, path.c_str());
    } else {
      ua->ErrorMsg(T_("failed to remove config resource file \"%s\": %s\n"),
                   path.c_str(), strerror(errno));
    }
    return false;
  }

  /* The reload builds a new configuration generation from disk and swaps it
   * in; running jobs keep the old generation, and with it this resource,
   * alive until they finish. On failure DoReloadConfig() has already
   * restored the previous generation, so only the file has to be moved back
   * to leave on-disk and in-memory config consistent again. */
  if (!ConfigureReloadConfig()) {
    /* Check before restoring the file, so that what is reported below is
     * what actually happened. A failed reload usually means the remaining
     * configuration did not parse and DoReloadConfig() put the previous
     * generation back, but it also returns false when a reload was already
     * running (see the is_reloading guard in DoReloadConfig()) -- and that
     * other reload may well have enacted the removal in the meantime. */
    bool still_loaded
        = my_config->GetResWithName(res_table->rcode, name) != nullptr;

    if (rename(stashed_path.c_str(), path.c_str()) != 0) {
      ua->ErrorMsg(
          T_("Could not restore config resource file \"%s\" from \"%s\" after "
             "the reload failed: %s. The resource will be missing after a "
             "restart.\n"),
          path.c_str(), stashed_path.c_str(), strerror(errno));
    }

    if (still_loaded) {
      /* The reload's own diagnostics (the parse errors, naming file and
       * line) go to the Director's Messages resource via Jmsg(), not to this
       * console -- same as for the "reload" command. Say where to find
       * them rather than leave the user with just "it failed". */
      ua->ErrorMsg(
          T_("Reloading the configuration without resource \"%s\" with name "
             "\"%s\" failed, the deletion was rolled back. See the Director "
             "log for the errors found while parsing. Please correct the "
             "configuration and try again.\n"),
          res_table->name, name);
    } else {
      ua->ErrorMsg(
          T_("Reloading the configuration without resource \"%s\" with name "
             "\"%s\" failed, and its config resource file was restored, but "
             "the resource is no longer part of the running configuration -- "
             "a concurrent reload removed it. Run \"reload\" to bring the "
             "running configuration back in sync with the configuration on "
             "disk.\n"),
          res_table->name, name);
    }
    return false;
  }

  /* The new configuration is live and does not contain the resource
   * anymore. Whatever else the removed file defined is gone from it too,
   * though, and the reference check above only looked at the resource that
   * was asked for. Find out what else went missing before the stashed file
   * -- the only remaining copy of those definitions -- is unlinked. */
  std::vector<ResourceKey> also_removed;
  {
    ResLocker _{my_config};

    std::set<ResourceKey> names_after = ConfigureResourceNames();
    for (const auto& before : names_before) {
      if (before.first == static_cast<int>(res_table->rcode)
          && before.second == target_name) {
        continue;
      }
      if (!names_after.count(before)) { also_removed.push_back(before); }
    }
  }

  if (also_removed.empty()) {
    if (unlink(stashed_path.c_str()) != 0) {
      ua->WarningMsg(T_("failed to remove config resource file \"%s\": %s\n"),
                     stashed_path.c_str(), strerror(errno));
    }
  } else {
    /* Keep the stashed file instead of unlinking it, so that what it
     * defined can be recovered. Not rolling the delete back: the new
     * configuration is already live, and a second reload to undo it could
     * fail on its own or race with another one. */
    ua->WarningMsg(
        T_("Removing config resource file \"%s\" also removed the following "
           "resource(s) from the configuration:\n"),
        path.c_str());
    for (const auto& [rcode, resource_name] : also_removed) {
      ua->WarningMsg(T_("  %s \"%s\"\n"), my_config->ResToStr(rcode),
                     resource_name.c_str());
    }
    ua->WarningMsg(
        T_("They were most likely defined in the same file, which has "
           "therefore been kept as \"%s\". Move the definitions you want to "
           "keep back into the configuration and reload, then remove that "
           "file. Note that the reload also enacts unrelated changes made to "
           "the configuration on disk since the last one, so a resource "
           "removed there shows up here as well.\n"),
        stashed_path.c_str());
  }

  if (my_config->GetResWithName(res_table->rcode, name)) {
    ua->WarningMsg(
        T_("Removed config resource file \"%s\", but resource \"%s\" with name "
           "\"%s\" is still defined elsewhere in the configuration.\n"),
        path.c_str(), res_table->name, name);
  }

  /* Also remove the credential-bearing File Daemon export files created by
   * "configure add client", if any. A client that went with the file needs
   * this just as much as the one that was asked for: its export holds a
   * plaintext copy of the Director password and there is no longer a client
   * it belongs to. For a client that never had an export this does nothing
   * and says nothing -- removing a path that is not there is not an error.
   */
  if (res_table->rcode == R_CLIENT) { ConfigureRemoveFdExport(ua, name); }
  for (const auto& [rcode, resource_name] : also_removed) {
    if (rcode == R_CLIENT) {
      ConfigureRemoveFdExport(ua, resource_name.c_str());
    }
  }

  ua->send->ObjectStart("delete");
  ua->send->ObjectKeyValue("resource", res_table->name);
  ua->send->ObjectKeyValue("name", name);
  ua->send->ObjectKeyValue("filename", path.c_str(),
                           "Removed resource config file \"%s\".\n");
  /* Emitted even when empty, so that a caller reading the output does not
   * have to tell "nothing else was removed" from "this version does not
   * report it". */
  ConfigureSendAlsoRemoved(ua, also_removed);
  ua->send->ObjectEnd("delete");

  return true;
}

static inline void ConfigureDeleteUsage(UaContext* ua)
{
  ua->ErrorMsg(
      T_("usage: configure delete <resourcetype> name=<name>\n"
         "       configure delete <resourcetype>=<name>\n"));
}

/**
 * The ACL that governs access to an individual resource of this type, or
 * Num_ACL if there is none. Only a subset of the resource types has a
 * matching ACL (see the ACL enum in dird_conf.h); for the rest, access to
 * the "configure" command itself is all that gates this, which is the same
 * as for "configure add".
 */
static inline int ConfigureAclForResource(int rcode)
{
  switch (rcode) {
    case R_JOB:
      return Job_ACL;
    case R_CLIENT:
      return Client_ACL;
    case R_STORAGE:
      return Storage_ACL;
    case R_SCHEDULE:
      return Schedule_ACL;
    case R_POOL:
      return Pool_ACL;
    case R_FILESET:
      return FileSet_ACL;
    case R_CATALOG:
      return Catalog_ACL;
    default:
      return Num_ACL;
  }
}

static inline bool ConfigureDelete(UaContext* ua, int resource_type_parameter)
{
  bool result = false;

  const ResourceTable* res_table
      = my_config->GetResourceTable(ua->argk[resource_type_parameter]);
  if (!res_table) {
    ua->ErrorMsg(T_("invalid resource type %s.\n"),
                 ua->argk[resource_type_parameter]);
    return false;
  }

  if (res_table->rcode == R_DIRECTOR) {
    ua->ErrorMsg(
        T_("Only one Director resource allowed. It cannot be "
           "deleted.\n"));
    return false;
  }

  /* Take the name from a "name=" argument, or, failing that, from the value
   * of the resource type itself. "configure add" accepts both spellings --
   * "configure add client=foo address=..." as well as "configure add client
   * name=foo address=..." -- and there is no reason for deleting to be
   * stricter about how the same resource is named. */
  const char* name = nullptr;
  int name_index = FindArgWithValue(ua, NT_("name"));
  if (name_index >= 0) {
    name = ua->argv[name_index];
  } else {
    name = ua->argv[resource_type_parameter];
  }
  if (!name) {
    ConfigureDeleteUsage(ua);
    return false;
  }

  /* Deleting a resource is destructive, so unlike "configure add" it is not
   * enough for the console to have access to the "configure" command: it
   * also has to have access to this particular resource. This mirrors what
   * "configure export" does via ua->GetClientResWithName(). */
  int acl = ConfigureAclForResource(res_table->rcode);
  if (acl != Num_ACL && !ua->AclAccessOk(acl, name, true)) {
    ua->ErrorMsg(T_("No access to %s \"%s\".\n"), res_table->name, name);
    return false;
  }

  ua->send->ObjectStart("configure");
  result = ConfigureDeleteResource(ua, res_table, name);
  ua->send->ObjectEnd("configure");

  return result;
}

static inline void ConfigureExportUsage(UaContext* ua)
{
  ua->ErrorMsg(T_("usage: configure export client=<clientname>\n"));
}

static inline bool ConfigureExport(UaContext* ua)
{
  bool result = false;
  int i;

  i = FindArgWithValue(ua, NT_("client"));
  if (i < 0) {
    ConfigureExportUsage(ua);
    return false;
  }

  if (!ua->GetClientResWithName(ua->argv[i])) {
    ConfigureExportUsage(ua);
    return false;
  }

  ua->send->ObjectStart("configure");
  result = ConfigureCreateFdResource(ua, ua->argv[i]);
  ua->send->ObjectEnd("configure");

  return result;
}

bool ConfigureCmd(UaContext* ua, const char*)
{
  bool result = false;

  if (!(my_config->IsUsingConfigIncludeDir())) {
    ua->WarningMsg(
        T_("It seems that the configuration is not adapted to the include "
           "directory structure. "
           "This means, that the configure command may not work as expected. "
           "Your configuration changes may not survive a reload/restart. "));
  }

  if (ua->argc < 3) {
    ua->ErrorMsg(
        T_("usage:\n"
           "  configure add <resourcetype> <key1>=<value1> ...\n"
           "  configure delete <resourcetype> name=<name>\n"
           "  configure delete <resourcetype>=<name>\n"
           "  configure export client=<clientname>\n"));
    return false;
  }

  if (Bstrcasecmp(ua->argk[1], NT_("add"))) {
    result = ConfigureAdd(ua, 2);
  } else if (Bstrcasecmp(ua->argk[1], NT_("delete"))) {
    result = ConfigureDelete(ua, 2);
  } else if (Bstrcasecmp(ua->argk[1], NT_("export"))) {
    result = ConfigureExport(ua);
  } else {
    ua->ErrorMsg(T_("invalid subcommand %s.\n"), ua->argk[1]);
    return false;
  }

  return result;
}
} /* namespace directordaemon */
