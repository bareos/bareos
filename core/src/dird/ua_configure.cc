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
// Shared by ConfigureCreateFdResource() and ConfigureRemoveFdExport().
// clientname must be a single path component: it lands as its own segment
// here, so a '/' in it (e.g. "a/../victim") can alias another client's
// directory without ever escaping config_dir_, and a bare "." or ".."
// isn't neutralized by anything after it the way name in
// config_include_naming_format_ is.
static inline bool GetFdExportBasedir(PoolMem& basedir,
                                      const char* clientname)
{
  if (strchr(clientname, '/') || strcmp(clientname, ".") == 0
      || strcmp(clientname, "..") == 0) {
    return false;
  }
  basedir.bsprintf("bareos-dir-export/client/%s/bareos-fd.d", clientname);
  return true;
}

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

  // Use me, not GetNextRes(R_DIRECTOR, NULL): this runs unlocked and
  // GetNextRes() doesn't take the resource lock.
  if (!GetFdExportBasedir(basedir, clientname)) {
    ua->ErrorMsg(
        T_("Client name \"%s\" cannot be used to build a filedaemon export "
           "path.\n"),
        clientname);
    return false;
  }
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

// Removes the FD export file "configure add client" writes (plaintext
// Director password, CWE-459 if left behind). Best-effort: the Client
// resource is already gone by the time this runs, so there's nothing to
// roll back.
static inline void ConfigureRemoveFdExport(UaContext* ua,
                                           const char* clientname)
{
  PoolMem basedir(PM_FNAME);
  PoolMem path(PM_FNAME);

  // me is set by CheckResources(), which the preceding reload had to pass.
  const char* dirname = me->resource_name_;

  // GetPathOfResource() catches escapes GetFdExportBasedir() itself doesn't.
  if (!GetFdExportBasedir(basedir, clientname)
      || !my_config->GetPathOfResource(path, basedir.c_str(), "director",
                                       dirname, false)) {
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

  // Prune the three now-possibly-empty directory levels above; remove()
  // refuses non-empty ones, so this can't touch anything still in use.
  std::filesystem::path dir = export_file.parent_path();
  for (int level = 0; level < 3; level++) {
    ec.clear();
    if (std::filesystem::remove(dir, ec)) {
      dir = dir.parent_path();
      continue;
    }

    // ec stays clear when the path was already gone -- nothing to report.
    if (ec == std::errc::directory_not_empty) {
      if (level == 0) {
        ua->WarningMsg(
            T_("Filedaemon export directory \"%s\" is not empty. It may still "
               "hold an export written under a different Director resource "
               "name, which contains a copy of the Director password. Please "
               "remove it manually.\n"),
            dir.c_str());
      }
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

  /* Holds the write lock for the check-then-mutate sequence below (the
   * parser calls don't lock themselves), but scoped tightly since a write
   * lock blocks every foreach_res()/GetResWithName() director-wide: console
   * output happens after release, except on error paths, which still write
   * to ua from inside the lock since configure_create_resource_string() and
   * ParseConfigFile() need that. RemoveResource() below only ever frees a
   * resource this call just added, never one visible elsewhere -- see
   * ConfigureDeleteResource() for why a live resource can't just be freed. */
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
 * FindResourceReferences() can't see a Schedule's per-Run Pool/Storage/
 * Messages overrides (CFG_TYPE_RUN, not CFG_TYPE_RES); scan those here so a
 * resource referenced only via a Run override isn't deleted from under a
 * Schedule. Also enumerated in dird_conf.cc's PrintConfigRun() and in
 * ua_status.cc; update all three if RunResource gains an override field.
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

// Indirection so tests (core/src/tests/configure.cc) can substitute a
// reload that only swaps the configuration, skipping DoReloadConfig()'s
// catalog/statistics/scheduler side effects. The director never reassigns it.
static bool (*ConfigureReloadConfig)() = DoReloadConfig;

// Collapses identical (type, name, directive) triples: a resource can be
// reached by both the generic scan and the Run scan, or by several Run lines.
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

// Every loaded resource as (type, name) pairs, so a before/after compare
// around the reload can report collateral removals from a shared file
// instead of losing them silently. Caller must hold the resource lock.
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
 * Deletion moves the resource's file aside and reloads the whole
 * configuration; it never frees the live resource in place. A running job
 * keeps raw pointers into the config graph (e.g. JobControlRecord's
 * ClientResource*) for its whole runtime, so freeing a resource still
 * reachable from a job would leave it dangling. DoReloadConfig() performs
 * the generation swap that keeps old resources alive for jobs still using
 * them (DirectorJcrImpl::used_config_for_job, dird/reload.cc), giving this
 * the same safety properties as "reload". The reference check below is
 * only an early, friendlier error -- the reload itself is what actually
 * validates the deletion, by failing to parse if a concurrently added
 * resource still references the one being removed.
 */
static inline bool ConfigureDeleteResource(UaContext* ua,
                                           const ResourceTable* res_table,
                                           const char* name)
{
  PoolMem path(PM_FNAME);
  PoolMem stashed_path(PM_FNAME);
  std::set<ResourceKey> names_before;

  // Released before DoReloadConfig(): it takes LockJobs() then the resource
  // lock, so holding this across the call would invert that order.
  {
    ResLocker _{my_config};

    BareosResource* res = my_config->GetResWithName(res_table->rcode, name);
    if (!res) {
      ua->ErrorMsg(T_("Resource \"%s\" with name \"%s\" does not exist.\n"),
                   res_table->name, name);
      return false;
    }

    // name is the last, ".conf"-suffixed component of
    // config_include_naming_format_, so a bare ".." in it is harmless, but
    // an embedded '/' (e.g. "a/../victim") can still alias another
    // resource's file -- landing inside config_dir_, so GetPathOfResource()'s
    // containment check below does not see it as an escape.
    if (strchr(name, '/')) {
      ua->ErrorMsg(
          T_("Resource \"%s\" with name \"%s\" cannot be deleted: its name "
             "contains a \"/\".\n"),
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

    names_before = ConfigureResourceNames();
  }

  /* The resource must live in its own file under the config include dir --
   * if not (e.g. defined inline in bareos-dir.conf), there's nothing to
   * remove and the reload would just load it again.
   *
   * Move the file aside rather than delete it: renaming it back on failure
   * is atomic and keeps mode/ownership/timestamps, unlike rewriting the
   * contents. ".deleted" keeps it out of the "*.conf" include glob,
   * mirroring ConfigureAddResource()'s ".tmp". A concurrent "configure add"
   * of the same name can still race into the window this opens, but the
   * reload below then fails to parse and rolls back. */
  stashed_path.bsprintf("%s.deleted", path.c_str());

  // rename() would silently replace a stash kept by an earlier collateral
  // removal (see "also_removed" below); refuse instead of losing it.
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

  // On failure DoReloadConfig() already restored the previous generation;
  // only the file needs moving back to leave disk and memory consistent.
  if (!ConfigureReloadConfig()) {
    // Checked before restoring: DoReloadConfig() also returns false when a
    // reload was already running (its is_reloading guard), which may have
    // enacted this removal already, not just on a parse failure.
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
      // Parse errors go to the Director log via Jmsg(), same as "reload".
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

  // The removed file may have defined more than the requested resource;
  // find what else went missing before the stash -- its only copy -- is
  // unlinked.
  std::vector<ResourceKey> also_removed;
  {
    ResLocker _{my_config};

    std::set<ResourceKey> names_after = ConfigureResourceNames();
    for (const auto& before : names_before) {
      if (before.first == static_cast<int>(res_table->rcode)
          && before.second == name) {
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
    // Kept for recovery rather than unlinked; not rolling back since the
    // new configuration is already live and a second reload could itself fail.
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

  // Also remove any FD export file(s) "configure add client" created --
  // for the requested client and any collaterally removed one.
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
  // Emitted even when empty, so callers can tell "nothing else removed"
  // from "this version doesn't report it".
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

// The ACL for an individual resource of this type, or Num_ACL if none exists
// (dird_conf.h); for those, "configure" command access is all that gates
// this, same as "configure add".
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

  // Accept both "delete client=foo" and "delete client name=foo", matching
  // how "configure add" accepts the name.
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

  // Deleting also needs access to the specific resource, not just the
  // "configure" command (unlike "configure add"); mirrors "configure export".
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
