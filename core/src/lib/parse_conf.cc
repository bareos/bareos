/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

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
 * Master Configuration routines.
 *
 * This file contains the common parts of the BAREOS configuration routines.
 *
 * Note, the configuration file parser consists of four parts
 *
 * 1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 * 2. The generic config scanner in lib/parse_conf.c and lib/parse_conf.h.
 *    These files contain the parser code, some utility routines,
 *
 * 3. The generic resource functions in lib/res.c
 *    Which form the common store routines (name, int, string, time,
 *    int64, size, ...).
 *
 * 4. The daemon specific file, which contains the Resource definitions
 *    as well as any specific store routines for the resource records.
 *
 * N.B. This is a two pass parser, so if you malloc() a string in a "store"
 * routine, you must ensure to do it during only one of the two passes, or to
 * free it between.
 *
 * Also, note that the resource record is malloced and saved in SaveResource()
 * during pass 1. Anything that you want saved after pass two (e.g. resource
 * pointers) must explicitly be done in SaveResource. Take a look at the Job
 * resource in src/dird/dird_conf.c to see how it is done.
 *
 * Kern Sibbald, January MM
 */

#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/address_conf.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "lib/parse_conf_state_machine.h"
#include "lib/qualified_resource_name_type_converter.h"
#include "lib/bstringlist.h"
#include "lib/ascii_control_characters.h"
#include "lib/messages_resource.h"
#include "lib/resource_item.h"
#include "lib/berrno.h"
#include "lib/util.h"

#include <algorithm>

#if defined(HAVE_WIN32)
#  include "shlobj.h"
#else
#  define MAX_PATH 1024
#endif

bool PrintMessage(void* sock, const char* fmt, ...)
{
  va_list arg_ptr;

  va_start(arg_ptr, fmt);
  vfprintf(stdout, fmt, arg_ptr);
  va_end(arg_ptr);

  return true;
}

ConfigurationParser::ConfigurationParser()
    : scan_error_(nullptr)
    , scan_warning_(nullptr)
    , init_res_(nullptr)
    , store_res_(nullptr)
    , print_res_(nullptr)
    , err_type_(0)
    , omit_defaults_(false)
    , r_num_(0)
    , r_own_(0)
    , own_resource_(nullptr)
    , resource_definitions_(0)
    /* , res_head_(nullptr) */
    /* , res_head_backup_(nullptr) */
    , SaveResourceCb_(nullptr)
    , DumpResourceCb_(nullptr)
    , FreeResourceCb_(nullptr)
    , use_config_include_dir_(false)
    , ParseConfigReadyCb_(nullptr)
    , parser_first_run_(true)
{
  return;
}

ConfigurationParser::ConfigurationParser(
    const char* cf,
    LEX_ERROR_HANDLER* ScanError,
    LEX_WARNING_HANDLER* scan_warning,
    INIT_RES_HANDLER* init_res,
    STORE_RES_HANDLER* StoreRes,
    PRINT_RES_HANDLER* print_res,
    int32_t err_type,
    int32_t r_num,
    ResourceTable* resource_definitions,
    const char* config_default_filename,
    const char* config_include_dir,
    void (*ParseConfigBeforeCb)(ConfigurationParser&),
    void (*ParseConfigReadyCb)(ConfigurationParser&),
    SaveResourceCb_t SaveResourceCb,
    DumpResourceCb_t DumpResourceCb,
    FreeResourceCb_t FreeResourceCb)
    : ConfigurationParser()
{
  cf_ = cf == nullptr ? "" : cf;
  use_config_include_dir_ = false;
  config_include_naming_format_ = "%s/%s/%s.conf";
  scan_error_ = ScanError;
  scan_warning_ = scan_warning;
  init_res_ = init_res;
  store_res_ = StoreRes;
  print_res_ = print_res;
  err_type_ = err_type;
  r_num_ = r_num;
  resource_definitions_ = resource_definitions;
  res_head_container_.reset(new ResHeadContainer(this));
  config_default_filename_
      = config_default_filename == nullptr ? "" : config_default_filename;
  config_include_dir_ = config_include_dir == nullptr ? "" : config_include_dir;
  ParseConfigBeforeCb_ = ParseConfigBeforeCb;
  ParseConfigReadyCb_ = ParseConfigReadyCb;
  ASSERT(SaveResourceCb);
  ASSERT(DumpResourceCb);
  ASSERT(FreeResourceCb);
  SaveResourceCb_ = SaveResourceCb;
  DumpResourceCb_ = DumpResourceCb;
  FreeResourceCb_ = FreeResourceCb;
}
ConfigurationParser::~ConfigurationParser()
{
#if 0
  if (res_head_) {
    for (int i = 0; i <= r_num_ - 1; i++) {
      if (res_head_[i]) { FreeResourceCb_(res_head_[i], i); }
      res_head_[i] = nullptr;
    }
  }
#endif
}

void ConfigurationParser::InitializeQualifiedResourceNameTypeConverter(
    const std::map<int, std::string>& map)
{
  qualified_resource_name_type_converter_.reset(
      new QualifiedResourceNameTypeConverter(map));
}

std::string ConfigurationParser::CreateOwnQualifiedNameForNetworkDump() const
{
  std::string qualified_name;

  if (own_resource_ && qualified_resource_name_type_converter_) {
    if (qualified_resource_name_type_converter_->ResourceToString(
            own_resource_->resource_name_, own_resource_->rcode_,
            "::", qualified_name)) {
      return qualified_name;
    }
  }
  return qualified_name;
}

bool ConfigurationParser::ParseConfig()
{
  int errstat;
  PoolMem config_path;

  if (ParseConfigBeforeCb_) ParseConfigBeforeCb_(*this);

  if (parser_first_run_ && (errstat = RwlInit(&res_lock_)) != 0) {
    BErrNo be;
    Jmsg1(nullptr, M_ABORT, 0,
          _("Unable to initialize resource lock. ERR=%s\n"),
          be.bstrerror(errstat));
  }
  parser_first_run_ = false;

  if (!FindConfigPath(config_path)) {
    Jmsg0(nullptr, M_ERROR_TERM, 0, _("Failed to find config filename.\n"));
  }
  used_config_path_ = config_path.c_str();
  Dmsg1(100, "config file = %s\n", used_config_path_.c_str());
  bool success = ParseConfigFile(config_path.c_str(), nullptr, scan_error_,
                                 scan_warning_);
  if (success && ParseConfigReadyCb_) { ParseConfigReadyCb_(*this); }
  return success;
}

void ConfigurationParser::lex_error(const char* cf,
                                    LEX_ERROR_HANDLER* ScanError,
                                    LEX_WARNING_HANDLER* scan_warning) const
{
  // We must create a lex packet to print the error
  LEX* lexical_parser_ = (LEX*)malloc(sizeof(LEX));
  memset(lexical_parser_, 0, sizeof(LEX));

  if (ScanError) {
    lexical_parser_->ScanError = ScanError;
  } else {
    LexSetDefaultErrorHandler(lexical_parser_);
  }

  if (scan_warning) {
    lexical_parser_->scan_warning = scan_warning;
  } else {
    LexSetDefaultWarningHandler(lexical_parser_);
  }

  LexSetErrorHandlerErrorType(lexical_parser_, err_type_);
  BErrNo be;
  scan_err2(lexical_parser_, _("Cannot open config file \"%s\": %s\n"), cf,
            be.bstrerror());
  free(lexical_parser_);
}

bool ConfigurationParser::ParseConfigFile(const char* config_file_name,
                                          void* caller_ctx,
                                          LEX_ERROR_HANDLER* scan_error,
                                          LEX_WARNING_HANDLER* scan_warning)
{
  ConfigParserStateMachine state_machine(config_file_name, caller_ctx,
                                         scan_error, scan_warning, *this);

  Dmsg1(900, "Enter ParseConfigFile(%s)\n", config_file_name);

  do {
    if (!state_machine.InitParserPass()) { return false; }

    if (!state_machine.ParseAllTokens()) {
      scan_err0(state_machine.lexical_parser_, _("ParseAllTokens failed."));
      return false;
    }

    switch (state_machine.GetParseError()) {
      case ConfigParserStateMachine::ParserError::kResourceIncomplete:
        scan_err0(state_machine.lexical_parser_,
                  _("End of conf file reached with unclosed resource."));
        return false;
      case ConfigParserStateMachine::ParserError::kParserError:
        scan_err0(state_machine.lexical_parser_, _("Parser Error occurred."));
        return false;
      case ConfigParserStateMachine::ParserError::kNoError:
        break;
    }

  } while (!state_machine.Finished());

  state_machine.DumpResourcesAfterSecondPass();

  Dmsg0(900, "Leave ParseConfigFile()\n");
  return true;
}

bool ConfigurationParser::AppendToResourcesChain(BareosResource* new_resource,
                                                 int rcode)
{
  int rindex = rcode;

  if (!new_resource->resource_name_) {
    Emsg1(M_ERROR, 0,
          _("Name item is required in %s resource, but not found.\n"),
          resource_definitions_[rindex].name);
    return false;
  }

  if (!res_head_container_->res_head_[rindex]) {
    res_head_container_->res_head_[rindex] = new_resource;
    Dmsg3(900, "Inserting first %s res: %s index=%d\n", ResToStr(rcode),
          new_resource->resource_name_, rindex);
  } else {  // append
    BareosResource* last = nullptr;
    BareosResource* current = res_head_container_->res_head_[rindex];
    do {
      if (bstrcmp(current->resource_name_, new_resource->resource_name_)) {
        Emsg2(M_ERROR, 0,
              _("Attempt to define second %s resource named \"%s\" is not "
                "permitted.\n"),
              resource_definitions_[rindex].name, new_resource->resource_name_);
        return false;
      }
      last = current;
      current = last->next_;
    } while (current);
    last->next_ = new_resource;
    Dmsg3(900, _("Inserting %s res: %s index=%d\n"), ResToStr(rcode),
          new_resource->resource_name_, rindex);
  }
  return true;
}

int ConfigurationParser::GetResourceTableIndex(const char* resource_type_name)
{
  for (int i = 0; resource_definitions_[i].name; i++) {
    if (Bstrcasecmp(resource_definitions_[i].name, resource_type_name)) {
      return i;
    }
  }

  return -1;
}

int ConfigurationParser::GetResourceCode(const char* resource_type_name)
{
  for (int i = 0; resource_definitions_[i].name; i++) {
    if (Bstrcasecmp(resource_definitions_[i].name, resource_type_name)) {
      return resource_definitions_[i].rcode;
    }
  }

  return 0;
}

ResourceTable* ConfigurationParser::GetResourceTable(
    const char* resource_type_name)
{
  int res_table_index = GetResourceTableIndex(resource_type_name);
  return &resource_definitions_[res_table_index];
}

int ConfigurationParser::GetResourceItemIndex(ResourceItem* resource_items_,
                                              const char* item)
{
  int result = -1;
  int i;

  for (i = 0; resource_items_[i].name; i++) {
    if (Bstrcasecmp(resource_items_[i].name, item)) {
      result = i;
      break;
    }
  }

  return result;
}

ResourceItem* ConfigurationParser::GetResourceItem(
    ResourceItem* resource_items_,
    const char* item)
{
  ResourceItem* result = nullptr;
  int i = -1;

  if (resource_items_) {
    i = GetResourceItemIndex(resource_items_, item);
    if (i >= 0) { result = &resource_items_[i]; }
  }

  return result;
}

const char* ConfigurationParser::GetDefaultConfigDir()
{
#if defined(HAVE_WIN32)
  HRESULT hr;
  static char szConfigDir[MAX_PATH + 1] = {0};

  if (!p_SHGetFolderPath) {
    bstrncpy(szConfigDir, DEFAULT_CONFIGDIR, sizeof(szConfigDir));
    return szConfigDir;
  }

  if (szConfigDir[0] == '\0') {
    hr = p_SHGetFolderPath(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0,
                           szConfigDir);

    if (SUCCEEDED(hr)) {
      bstrncat(szConfigDir, "\\Bareos", sizeof(szConfigDir));
    } else {
      bstrncpy(szConfigDir, DEFAULT_CONFIGDIR, sizeof(szConfigDir));
    }
  }

  return szConfigDir;
#else
  return CONFDIR;
#endif
}

bool ConfigurationParser::GetConfigFile(PoolMem& full_path,
                                        const char* config_dir,
                                        const char* config_filename)
{
  bool found = false;

  if (!PathIsDirectory(config_dir)) { return false; }

  if (config_filename) {
    full_path.strcpy(config_dir);
    if (PathAppend(full_path, config_filename)) {
      if (PathExists(full_path)) {
        config_dir_ = config_dir;
        found = true;
      }
    }
  }

  return found;
}

bool ConfigurationParser::GetConfigIncludePath(PoolMem& full_path,
                                               const char* config_dir)
{
  bool found = false;

  if (!config_include_dir_.empty()) {
    /*
     * Set full_path to the initial part of the include path,
     * so it can be used as result, even on errors.
     * On success, full_path will be overwritten with the full path.
     */
    full_path.strcpy(config_dir);
    PathAppend(full_path, config_include_dir_.c_str());
    if (PathIsDirectory(full_path)) {
      config_dir_ = config_dir;
      // Set full_path to wildcard path.
      if (GetPathOfResource(full_path, nullptr, nullptr, nullptr, true)) {
        use_config_include_dir_ = true;
        found = true;
      }
    }
  }

  return found;
}

/*
 * Returns false on error
 *         true  on OK, with full_path set to where config file should be
 */
bool ConfigurationParser::FindConfigPath(PoolMem& full_path)
{
  bool found = false;
  PoolMem config_dir;
  PoolMem config_path_file;

  if (cf_.empty()) {
    // No path is given, so use the defaults.
    found = GetConfigFile(full_path, GetDefaultConfigDir(),
                          config_default_filename_.c_str());
    if (!found) {
      config_path_file.strcpy(full_path);
      found = GetConfigIncludePath(full_path, GetDefaultConfigDir());
    }
    if (!found) {
      Jmsg2(nullptr, M_ERROR, 0,
            _("Failed to read config file at the default locations "
              "\"%s\" (config file path) and \"%s\" (config include "
              "directory).\n"),
            config_path_file.c_str(), full_path.c_str());
    }
  } else if (PathExists(cf_.c_str())) {
    // Path is given and exists.
    if (PathIsDirectory(cf_.c_str())) {
      found = GetConfigFile(full_path, cf_.c_str(),
                            config_default_filename_.c_str());
      if (!found) {
        config_path_file.strcpy(full_path);
        found = GetConfigIncludePath(full_path, cf_.c_str());
      }
      if (!found) {
        Jmsg3(nullptr, M_ERROR, 0,
              _("Failed to find configuration files under directory \"%s\". "
                "Did look for \"%s\" (config file path) and \"%s\" (config "
                "include directory).\n"),
              cf_.c_str(), config_path_file.c_str(), full_path.c_str());
      }
    } else {
      full_path.strcpy(cf_.c_str());
      PathGetDirectory(config_dir, full_path);
      config_dir_ = config_dir.c_str();
      found = true;
    }
  } else if (config_default_filename_.empty()) {
    /*
     * Compatibility with older versions.
     * If config_default_filename_ is not set,
     * cf_ may contain what is expected in config_default_filename_.
     */
    found = GetConfigFile(full_path, GetDefaultConfigDir(), cf_.c_str());
    if (!found) {
      Jmsg2(nullptr, M_ERROR, 0,
            _("Failed to find configuration files at \"%s\" and \"%s\".\n"),
            cf_.c_str(), full_path.c_str());
    }
  } else {
    Jmsg1(nullptr, M_ERROR, 0, _("Failed to read config file \"%s\"\n"),
          cf_.c_str());
  }

  return found;
}

// remove our shared pointer to previous configuration so that it will be freed
// when last job also owning finishes
void ConfigurationParser::ReleasePreviousResourceTable()
{
  Dmsg1(10, "ConfigurationParser::ReleasePreviousResourceTable: %p\n",
        res_head_container_backup_->res_head_);
  res_head_container_backup_ = nullptr;
}

// swap the previously saved res_head_previous_ with res_head_
// and release the res_head_previous_
void ConfigurationParser::RestoreResourceTable()
{
  std::swap(res_head_container_, res_head_container_backup_);
  Dmsg1(10, "ConfigurationParser::RestoreResourceTable: %p -> %p\n",
        res_head_container_backup_->res_head_, res_head_container_->res_head_);
  Dmsg1(10, "ConfigurationParser::RestoreResourceTable: release %p\n",
        res_head_container_backup_->res_head_);
  res_head_container_backup_ = nullptr;
}

// copy the current resource table to res_head_backup_
// and create a new empty res_head_container_
void ConfigurationParser::BackupResourceTable()
{
  std::swap(res_head_container_, res_head_container_backup_);
  res_head_container_.reset(new ResHeadContainer(this));
}

bool ConfigurationParser::RemoveResource(int rcode, const char* name)
{
  int rindex = rcode;
  BareosResource* last;

  /*
   * Remove resource from list.
   *
   * Note: this is intended for removing a resource that has just been added,
   * but proven to be incorrect (added by console command "configure add").
   * For a general approach, a check if this resource is referenced by other
   * resource_definitions must be added. If it is referenced, don't remove it.
   */
  last = nullptr;
  for (BareosResource* res = res_head_container_->res_head_[rindex]; res;
       res = res->next_) {
    if (bstrcmp(res->resource_name_, name)) {
      if (!last) {
        Dmsg2(900,
              _("removing resource %s, name=%s (first resource in list)\n"),
              ResToStr(rcode), name);
        res_head_container_->res_head_[rindex] = res->next_;
      } else {
        Dmsg2(900, _("removing resource %s, name=%s\n"), ResToStr(rcode), name);
        last->next_ = res->next_;
      }
      res->next_ = nullptr;
      FreeResourceCb_(res, rcode);
      return true;
    }
    last = res;
  }

  // Resource with this name not found
  return false;
}

bool ConfigurationParser::DumpResources(bool sendit(void* sock,
                                                    const char* fmt,
                                                    ...),
                                        void* sock,
                                        const std::string& res_type_name,
                                        const std::string& res_name,
                                        bool hide_sensitive_data)
{
  bool result = false;
  if (res_type_name.empty()) {
    DumpResources(sendit, sock, hide_sensitive_data);
    result = true;
  } else {
    int res_type = GetResourceCode(res_type_name.c_str());
    if (res_type > 0) {
      BareosResource* res = nullptr;
      if (res_name.empty()) {
        // No name, dump all resources of specified type
        res = GetNextRes(res_type, nullptr);
      } else {
        // Dump a single resource with specified name
        res = GetResWithName(res_type, res_name.c_str());
        res_type = -res_type;
      }
      if (res) { result = true; }
      DumpResourceCb_(res_type, res, sendit, sock, hide_sensitive_data, false);
    }
  }
  return result;
}

void ConfigurationParser::DumpResources(bool sendit(void* sock,
                                                    const char* fmt,
                                                    ...),
                                        void* sock,
                                        bool hide_sensitive_data)
{
  for (int i = 0; i <= r_num_ - 1; i++) {
    if (res_head_container_->res_head_[i]) {
      DumpResourceCb_(i, res_head_container_->res_head_[i], sendit, sock,
                      hide_sensitive_data, false);
    }
  }
}

bool ConfigurationParser::GetPathOfResource(PoolMem& path,
                                            const char* component,
                                            const char* resourcetype,
                                            const char* name,
                                            bool set_wildcards)
{
  PoolMem rel_path(PM_FNAME);
  PoolMem directory(PM_FNAME);
  PoolMem resourcetype_lowercase(resourcetype);
  resourcetype_lowercase.toLower();

  if (!component) {
    if (!config_include_dir_.empty()) {
      component = config_include_dir_.c_str();
    } else {
      return false;
    }
  }

  if (resourcetype_lowercase.strlen() <= 0) {
    if (set_wildcards) {
      resourcetype_lowercase.strcpy("*");
    } else {
      return false;
    }
  }

  if (!name) {
    if (set_wildcards) {
      name = "*";
    } else {
      return false;
    }
  }

  path.strcpy(config_dir_.c_str());
  rel_path.bsprintf(config_include_naming_format_.c_str(), component,
                    resourcetype_lowercase.c_str(), name);
  PathAppend(path, rel_path);

  return true;
}

bool ConfigurationParser::GetPathOfNewResource(PoolMem& path,
                                               PoolMem& extramsg,
                                               const char* component,
                                               const char* resourcetype,
                                               const char* name,
                                               bool error_if_exists,
                                               bool create_directories)
{
  PoolMem rel_path(PM_FNAME);
  PoolMem directory(PM_FNAME);
  PoolMem resourcetype_lowercase(resourcetype);
  resourcetype_lowercase.toLower();

  if (!GetPathOfResource(path, component, resourcetype, name, false)) {
    return false;
  }

  PathGetDirectory(directory, path);

  if (create_directories) { PathCreate(directory); }

  if (!PathExists(directory)) {
    extramsg.bsprintf("Resource config directory \"%s\" does not exist.\n",
                      directory.c_str());
    return false;
  }

  /*
   * Store name for temporary file in extramsg.
   * Can be used, if result is true.
   * Otherwise it contains an error message.
   */
  extramsg.bsprintf("%s.tmp", path.c_str());

  if (!error_if_exists) { return true; }

  // File should not exists, as it is going to be created.
  if (PathExists(path)) {
    extramsg.bsprintf("Resource config file \"%s\" already exists.\n",
                      path.c_str());
    return false;
  }

  if (PathExists(extramsg)) {
    extramsg.bsprintf(
        "Temporary resource config file \"%s.tmp\" already exists.\n",
        path.c_str());
    return false;
  }

  return true;
}

void ConfigurationParser::AddWarning(const std::string& warning)
{
  warnings_ << warning;
}

void ConfigurationParser::ClearWarnings() { warnings_.clear(); }

bool ConfigurationParser::HasWarnings() const { return !warnings_.empty(); }

const BStringList& ConfigurationParser::GetWarnings() const
{
  return warnings_;
}
