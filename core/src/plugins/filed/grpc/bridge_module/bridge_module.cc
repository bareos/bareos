/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2025 Bareos GmbH & Co. KG

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

#include "events.pb.h"
#include "include/filetypes.h"
#include "plugin.pb.h"
#include "bareos.pb.h"
#include "filed/fd_plugins.h"
#include <dlfcn.h>
#include <clocale>

#undef ASSERT
#define ASSERT(x)                                 \
  do {                                            \
    if (!(x)) {                                   \
      fprintf(stderr, "Failed ASSERT: %s\n", #x); \
      abort();                                    \
    }                                             \
  } while (0)
#include <lib/channel.h>

#include <fmt/format.h>
#include <condition_variable>
#include <cstdarg>
#include <future>
#include <sstream>

#include "plugin_service.h"
#include "bridge_module.h"

#include <fcntl.h>

struct global_values {
  int debug_level = 1000;
  prototools::ProtoBidiStream* client_writer{};
};

global_values globals{};

static inline auto request_writer() { return globals.client_writer; }

bool Register(std::basic_string_view<bc::EventType> types)
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_register_();
  for (auto type : types) { req.add_event_types(type); }

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return false; }
  if (!writer->Read(outer_resp)) { return false; }
  if (!outer_resp.has_register_()) { return false; }

  auto& resp = outer_resp.register_();
  (void)resp;

  return true;
}
bool Unregister(std::basic_string_view<bc::EventType> types)
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_unregister();
  for (auto type : types) { req.add_event_types(type); }

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return false; }
  if (!writer->Read(outer_resp)) { return false; }
  if (!outer_resp.has_unregister()) { return false; }

  auto& resp = outer_resp.unregister();
  (void)resp;

  return true;
}

// TODO: implement these

void AddExclude() { return; }
void AddInclude() { return; }
void AddOptions() { return; }
void AddRegex() { return; }
void AddWild() { return; }
void NewOptions() { return; }
void NewInclude() { return; }
void NewPreInclude() { return; }

//

std::optional<size_t> getInstanceCount()
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_getinstancecount();
  (void)req;

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return false; }
  if (!writer->Read(outer_resp)) { return false; }
  if (!outer_resp.has_getinstancecount()) { return false; }

  auto& resp = outer_resp.getinstancecount();

  return resp.instance_count();
}

std::optional<bool> checkChanges(bco::FileType ft,
                                 std::string_view name,
                                 time_t timestamp,
                                 const struct stat& statp)
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_checkchanges();
  req.set_type(ft);
  req.set_file(name.data(), name.size());
  req.mutable_since_time()->set_seconds(timestamp);
  req.set_stats(&statp, sizeof(statp));

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return false; }
  if (!writer->Read(outer_resp)) { return false; }
  if (!outer_resp.has_checkchanges()) { return false; }

  auto& resp = outer_resp.checkchanges();

  return resp.old();
}
std::optional<bool> AcceptFile(std::string_view name, const struct stat& statp)
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_acceptfile();
  req.set_file(name.data(), name.size());
  req.set_stats(&statp, sizeof(statp));

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return false; }
  if (!writer->Read(outer_resp)) { return false; }
  if (!outer_resp.has_acceptfile()) { return false; }

  auto& resp = outer_resp.acceptfile();

  return resp.skip();
}

bool SetSeen(std::optional<std::string_view> name)
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_setseen();
  if (name) { req.set_file(name->data(), name->size()); }

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return false; }
  if (!writer->Read(outer_resp)) { return false; }
  if (!outer_resp.has_setseen()) { return false; }

  auto& resp = outer_resp.setseen();
  (void)resp;

  return true;
}
bool ClearSeen(std::optional<std::string_view> name)
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_clearseen();
  if (name) { req.set_file(name->data(), name->size()); }

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return false; }
  if (!writer->Read(outer_resp)) { return false; }
  if (!outer_resp.has_clearseen()) { return false; }

  auto& resp = outer_resp.clearseen();
  (void)resp;

  return true;
}
void JobMessage(bc::JMsgType type,
                int line,
                const char* file,
                const char* fun,
                std::string_view msg)
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_jobmessage();
  req.set_type(type);
  req.set_msg(msg.data(), msg.size());
  req.set_line(line);
  req.set_file(file);
  req.set_function(fun);

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return; }
  if (!writer->Read(outer_resp)) { return; }
  if (!outer_resp.has_jobmessage()) { return; }

  auto& resp = outer_resp.jobmessage();
  (void)resp;

  return;
}

void DebugMessage(int level,
                  std::string_view msg,
                  int line,
                  const char* file,
                  const char* fun)
{
  if (level > globals.debug_level) { return; }

  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_debugmessage();
  req.set_level(level);
  req.set_msg(msg.data(), msg.size());
  req.set_line(line);
  req.set_file(file);
  req.set_function(fun);

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->WriteBuffered(outer_req)) { return; }
}
bool Bareos_SetString(bc::BareosStringVariable var, std::string_view val)
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_setstring();
  req.set_var(var);
  req.set_value(val.data(), val.size());

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return false; }
  if (!writer->Read(outer_resp)) { return false; }
  if (!outer_resp.has_setstring()) { return false; }

  auto& resp = outer_resp.setstring();
  (void)resp;

  return true;
}
std::optional<std::string> Bareos_GetString(bc::BareosStringVariable var)
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_getstring();
  req.set_var(var);

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return std::nullopt; }
  if (!writer->Read(outer_resp)) { return std::nullopt; }
  if (!outer_resp.has_getstring()) { return std::nullopt; }

  auto& resp = *outer_resp.mutable_getstring();
  return std::move(*resp.release_value());
}

bool Bareos_SetInt(bc::BareosIntVariable var, int val)
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_setint();
  req.set_var(var);
  req.set_value(val);

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return false; }
  if (!writer->Read(outer_resp)) { return false; }
  if (!outer_resp.has_setint()) { return false; }

  auto& resp = outer_resp.setint();
  (void)resp;

  return true;
}
std::optional<int> Bareos_GetInt(bc::BareosIntVariable var)
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_getint();
  req.set_var(var);

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return std::nullopt; }
  if (!writer->Read(outer_resp)) { return std::nullopt; }
  if (!outer_resp.has_getint()) { return std::nullopt; }

  auto& resp = outer_resp.getint();
  return resp.value();
}

bool Bareos_SetFlag(bc::BareosFlagVariable var, bool val)
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_setflag();
  req.set_var(var);
  req.set_value(val);

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return false; }
  if (!writer->Read(outer_resp)) { return false; }
  if (!outer_resp.has_setflag()) { return false; }

  auto& resp = outer_resp.setflag();
  (void)resp;

  return true;
}
std::optional<bool> Bareos_GetFlag(bc::BareosFlagVariable var)
{
  bc::CoreRequest outer_req;
  auto& req = *outer_req.mutable_getflag();
  req.set_var(var);

  bc::CoreResponse outer_resp;

  auto* writer = request_writer();
  if (!writer->Write(outer_req)) { return std::nullopt; }
  if (!writer->Read(outer_resp)) { return std::nullopt; }
  if (!outer_resp.has_getflag()) { return std::nullopt; }

  auto& resp = outer_resp.getflag();
  return resp.value();
}

static Plugin plugin_data;
static std::string plugin_path;

static filedaemon::PluginApiDefinition core_api;
static filedaemon::CoreFunctions core_funs;
static filedaemon::PluginFunctions* plugin_funs;

namespace filedaemon {
std::optional<bco::FileType> to_grpc(int32_t num)
{
  switch (num) {
    case FT_LNKSAVED:
      return bco::FileType::HardlinkCopy;
    case FT_REGE:
      return bco::FileType::RegularFile;
    case FT_REG:
      return bco::FileType::RegularFile;
    case FT_LNK:
      return bco::FileType::SoftLink;
    case FT_DIREND:
      return bco::FileType::Directory;
    case FT_SPEC:
      return bco::FileType::SpecialFile;
    case FT_ISARCH:
      return bco::FileType::RegularFile;
    case FT_RAW:
      return bco::FileType::BlockDevice;
    case FT_FIFO:
      return bco::FileType::Fifo;
    case FT_REPARSE:
      return bco::FileType::ReparsePoint;
    case FT_DELETED:
      return bco::FileType::Deleted;
    case FT_JUNCTION:
      return bco::FileType::Junction;
    case FT_RESTORE_FIRST:
      return bco::FileType::RestoreObject;
    default:
      return std::nullopt;
  }
}
std::optional<bc::EventType> to_grpc(uint32_t num)
{
  switch (num) {
    case bEventJobStart:
      return bc::Event_JobStart;
    case bEventJobEnd:
      return bc::Event_JobEnd;
    case bEventStartBackupJob:
      return bc::Event_StartBackupJob;
    case bEventEndBackupJob:
      return bc::Event_EndBackupJob;
    case bEventStartRestoreJob:
      return bc::Event_StartRestoreJob;
    case bEventEndRestoreJob:
      return bc::Event_EndRestoreJob;
    case bEventStartVerifyJob:
      return bc::Event_StartVerifyJob;
    case bEventEndVerifyJob:
      return bc::Event_EndVerifyJob;
    case bEventBackupCommand:
      return bc::Event_BackupCommand;
    case bEventRestoreCommand:
      return bc::Event_RestoreCommand;
    case bEventEstimateCommand:
      return bc::Event_EstimateCommand;
    case bEventLevel:
      return bc::Event_Level;
    case bEventSince:
      return bc::Event_Since;
    case bEventCancelCommand:
      return bc::Event_CancelCommand;
    case bEventRestoreObject:
      return bc::Event_RestoreObject;
    case bEventEndFileSet:
      return bc::Event_EndFileSet;
    case bEventPluginCommand:
      return bc::Event_PluginCommand;
    case bEventOptionPlugin:
      return bc::Event_OptionPlugin;
    case bEventHandleBackupFile:
      return bc::Event_HandleBackupFile;
    case bEventNewPluginOptions:
      return bc::Event_NewPluginOptions;
    case bEventVssInitializeForBackup:
      return bc::Event_VssInitializeForBackup;
    case bEventVssInitializeForRestore:
      return bc::Event_VssInitializeForRestore;
    case bEventVssSetBackupState:
      return bc::Event_VssSetBackupState;
    case bEventVssPrepareForBackup:
      return bc::Event_VssPrepareForBackup;
    case bEventVssBackupAddComponents:
      return bc::Event_VssBackupAddComponents;
    case bEventVssPrepareSnapshot:
      return bc::Event_VssPrepareSnapshot;
    case bEventVssCreateSnapshots:
      return bc::Event_VssCreateSnapshots;
    case bEventVssRestoreLoadComponentMetadata:
      return bc::Event_VssRestoreLoadComponentMetadata;
    case bEventVssRestoreSetComponentsSelected:
      return bc::Event_VssRestoreSetComponentsSelected;
    case bEventVssCloseRestore:
      return bc::Event_VssCloseRestore;
    case bEventVssBackupComplete:
      return bc::Event_VssBackupComplete;
  }
  return std::nullopt;
}


bRC Wrapper_registerBareosEvents(PluginContext* ctx, int nr_events, ...)
{
  std::vector<bc::EventType> events;

  std::uint64_t event_mask = 0;

  va_list args;
  va_start(args, nr_events);
  for (int i = 0; i < nr_events; ++i) {
    auto event_num = va_arg(args, uint32_t);
    std::optional event = to_grpc(event_num);
    if (event) {
      events.push_back(*event);
      event_mask |= 1 << event_num;
    } else {
      JobLog(bc::JMSG_ERROR, FMT_STRING("bad event nr {}"), event_num);
      return bRC_Error;
    }
  }
  va_end(args);
  if (!Register({events.data(), events.size()})) { return bRC_Error; }

  auto* thrd = reinterpret_cast<PluginThread*>(ctx->core_private_context);
  allow_events(thrd, event_mask);
  return bRC_OK;
}
bRC Wrapper_unregisterBareosEvents(PluginContext* ctx, int nr_events, ...)
{
  std::vector<bc::EventType> events;
  std::uint64_t event_mask = 0;

  va_list args;
  va_start(args, nr_events);
  for (int i = 0; i < nr_events; ++i) {
    auto event_num = va_arg(args, uint32_t);
    std::optional event = to_grpc(event_num);
    if (event) {
      event_mask |= 1 << event_num;
      events.push_back(*event);
    } else {
      JobLog(bc::JMSG_ERROR, FMT_STRING("bad event nr {}"), event_num);
      return bRC_Error;
    }
  }
  va_end(args);
  if (!Unregister({events.data(), events.size()})) { return bRC_Error; }

  auto* thrd = reinterpret_cast<PluginThread*>(ctx->core_private_context);
  disallow_events(thrd, event_mask);

  return bRC_OK;
}
bRC Wrapper_getInstanceCount(PluginContext*, int* ret)
{
  auto cnt = getInstanceCount();
  if (cnt) {
    *ret = *cnt;
    return bRC_OK;
  }
  return bRC_Error;
}
bRC Wrapper_getBareosValue(PluginContext*, bVariable var, void* value)
{
  switch (var) {
    case bVarFDName: {
      auto res = Bareos_GetString(bc::BV_FDName);
      if (!res) { return bRC_Error; }
      *(char**)value = strdup(res->c_str());
    } break;
    case bVarClient: {
      auto res = Bareos_GetString(bc::BV_ClientName);
      if (!res) { return bRC_Error; }
      *(char**)value = strdup(res->c_str());
    } break;
    case bVarJobName: {
      auto res = Bareos_GetString(bc::BV_JobName);
      if (!res) { return bRC_Error; }
      *(char**)value = strdup(res->c_str());
    } break;
    case bVarWorkingDir: {
      auto res = Bareos_GetString(bc::BV_WorkingDir);
      if (!res) { return bRC_Error; }
      *(char**)value = strdup(res->c_str());
    } break;
    case bVarWhere: {
      auto res = Bareos_GetString(bc::BV_Where);
      if (!res) { return bRC_Error; }
      *(char**)value = strdup(res->c_str());
    } break;
    case bVarRegexWhere: {
      auto res = Bareos_GetString(bc::BV_RegexWhere);
      if (!res) { return bRC_Error; }
      *(char**)value = strdup(res->c_str());
    } break;
    case bVarExePath: {
      auto res = Bareos_GetString(bc::BV_ExePath);
      if (!res) { return bRC_Error; }
      *(char**)value = strdup(res->c_str());
    } break;
    case bVarVersion: {
      auto res = Bareos_GetString(bc::BV_BareosVersion);
      if (!res) { return bRC_Error; }
      *(char**)value = strdup(res->c_str());
    } break;
    case bVarPrevJobName: {
      auto res = Bareos_GetString(bc::BV_PreviousJobName);
      if (!res) { return bRC_Error; }
      *(char**)value = strdup(res->c_str());
    } break;
    case bVarUsedConfig: {
      auto res = Bareos_GetString(bc::BV_UsedConfig);
      if (!res) { return bRC_Error; }
      *(char**)value = strdup(res->c_str());
    } break;
    case bVarPluginPath: {
      auto res = Bareos_GetString(bc::BV_PluginPath);
      if (!res) { return bRC_Error; }
      *(char**)value = strdup(res->c_str());
    } break;
    case bVarJobId: {
      auto res = Bareos_GetInt(bc::BV_JobId);
      if (!res) { return bRC_Error; }
      *(int*)value = *res;
    } break;
    case bVarLevel: {
      auto res = Bareos_GetInt(bc::BV_JobLevel);
      if (!res) { return bRC_Error; }
      *(int*)value = *res;
    } break;
    case bVarType: {
      auto res = Bareos_GetInt(bc::BV_JobType);
      if (!res) { return bRC_Error; }
      *(int*)value = *res;
    } break;
    case bVarJobStatus: {
      auto res = Bareos_GetInt(bc::BV_JobStatus);
      if (!res) { return bRC_Error; }
      *(int*)value = *res;
    } break;
    case bVarSinceTime: {
      auto res = Bareos_GetInt(bc::BV_SinceTime);
      if (!res) { return bRC_Error; }
      *(int*)value = *res;
    } break;
    case bVarAccurate: {
      auto res = Bareos_GetInt(bc::BV_Accurate);
      if (!res) { return bRC_Error; }
      *(int*)value = *res;
    } break;
    case bVarPrefixLinks: {
      auto res = Bareos_GetInt(bc::BV_PrefixLinks);
      if (!res) { return bRC_Error; }
      *(int*)value = *res;
    } break;
    case bVarFileSeen: {
      auto res = Bareos_GetFlag(bc::BV_FileSeen);
      if (!res) { return bRC_Error; }
      *(bool*)value = *res;
    } break;
    case bVarCheckChanges: {
      auto res = Bareos_GetFlag(bc::BV_CheckChanges);
      if (!res) { return bRC_Error; }
      *(bool*)value = *res;
    } break;
    default: {
      JobLog(bc::JMSG_ERROR, FMT_STRING("bad bVariable {}"), int(var));
      return bRC_Error;
    } break;
  }
  return bRC_OK;
}
bRC Wrapper_setBareosValue(PluginContext*, bVariable var, const void* value)
{
  switch (var) {
    case bVarFDName: {
      auto res = Bareos_SetString(bc::BV_FDName, (char*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarClient: {
      auto res = Bareos_SetString(bc::BV_ClientName, (char*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarJobName: {
      auto res = Bareos_SetString(bc::BV_JobName, (char*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarWorkingDir: {
      auto res = Bareos_SetString(bc::BV_WorkingDir, (char*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarWhere: {
      auto res = Bareos_SetString(bc::BV_Where, (char*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarRegexWhere: {
      auto res = Bareos_SetString(bc::BV_RegexWhere, (char*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarExePath: {
      auto res = Bareos_SetString(bc::BV_ExePath, (char*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarVersion: {
      auto res = Bareos_SetString(bc::BV_BareosVersion, (char*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarPrevJobName: {
      auto res = Bareos_SetString(bc::BV_PreviousJobName, (char*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarUsedConfig: {
      auto res = Bareos_SetString(bc::BV_UsedConfig, (char*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarPluginPath: {
      auto res = Bareos_SetString(bc::BV_PluginPath, (char*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarJobId: {
      auto res = Bareos_SetInt(bc::BV_JobId, *(int*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarLevel: {
      auto res = Bareos_SetInt(bc::BV_JobLevel, *(int*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarType: {
      auto res = Bareos_SetInt(bc::BV_JobType, *(int*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarJobStatus: {
      auto res = Bareos_SetInt(bc::BV_JobStatus, *(int*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarSinceTime: {
      auto res = Bareos_SetInt(bc::BV_SinceTime, *(int*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarAccurate: {
      auto res = Bareos_SetInt(bc::BV_Accurate, *(int*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarPrefixLinks: {
      auto res = Bareos_SetInt(bc::BV_PrefixLinks, *(int*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarFileSeen: {
      auto res = Bareos_SetFlag(bc::BV_FileSeen, *(bool*)value);
      if (!res) { return bRC_Error; }
    } break;
    case bVarCheckChanges: {
      auto res = Bareos_SetFlag(bc::BV_CheckChanges, *(bool*)value);
      if (!res) { return bRC_Error; }
    } break;
    default: {
      JobLog(bc::JMSG_ERROR, FMT_STRING("bad bVariable {}"), int(var));
      return bRC_Error;
    } break;
  }
  return bRC_OK;
}
bRC Wrapper_JobMessage(PluginContext*,
                       const char* file,
                       int line,
                       int type,
                       utime_t mtime,
                       const char* fmt,
                       ...)
{
  // todo: jobmsg should contain mtime!
  (void)mtime;
  auto gtype = [&]() -> std::optional<bc::JMsgType> {
    switch (type) {
      case M_ABORT:
        return bc::JMSG_ABORT;
      case M_DEBUG:
        return bc::JMSG_DEBUG;
      case M_FATAL:
        return bc::JMSG_FATAL;
      case M_ERROR:
        return bc::JMSG_ERROR;
      case M_WARNING:
        return bc::JMSG_WARNING;
      case M_INFO:
        return bc::JMSG_INFO;
      case M_SAVED:
        return bc::JMSG_SAVED;
      case M_NOTSAVED:
        return bc::JMSG_NOTSAVED;
      case M_SKIPPED:
        return bc::JMSG_SKIPPED;
      case M_MOUNT:
        return bc::JMSG_MOUNT;
      case M_ERROR_TERM:
        return bc::JMSG_ERROR_TERM;
      case M_TERM:
        return bc::JMSG_TERM;
      case M_RESTORED:
        return bc::JMSG_RESTORED;
      case M_SECURITY:
        return bc::JMSG_SECURITY;
      case M_ALERT:
        return bc::JMSG_ALERT;
      case M_VOLMGMT:
        return bc::JMSG_VOLMGMT;
      case M_AUDIT:
        return bc::JMSG_AUDIT;
      case M_CONFIG_ERROR:
        return bc::JMSG_CONFIG_ERROR;
      default:
        return std::nullopt;
    }
  }();

  va_list args, copy;
  va_start(args, fmt);
  va_copy(copy, args);
  auto len = vsnprintf(nullptr, 0, fmt, copy);
  if (len < 0) {
    JobLog(bc::JMSG_ERROR, FMT_STRING("error while formatting message '{}'"),
           fmt);
    va_end(copy);
    va_end(args);
    return bRC_Error;
  }
  va_end(copy);
  std::string message;
  message.resize(len);
  auto len2 = vsnprintf(message.data(), message.size(), fmt, args);
  if (len2 != len) {
    JobLog(bc::JMSG_ERROR,
           FMT_STRING("error while formatting message '{}' (first and second "
                      "parse lengths do not match up)"),
           fmt);
    va_end(args);
    return bRC_Error;
  }
  va_end(args);
  if (!gtype) {
    JobLog(bc::JMSG_ERROR,
           FMT_STRING("unknown job message type {} (message = '{}')"), type,
           message);
    return bRC_Error;
  }
  ::JobMessage(*gtype, line, file, "<unknown fun>", message.c_str());
  return bRC_OK;
}
bRC Wrapper_DebugMessage(PluginContext*,
                         const char* file,
                         int line,
                         int level,
                         const char* fmt,
                         ...)
{
  va_list args, copy;
  va_start(args, fmt);
  va_copy(copy, args);
  auto len = vsnprintf(nullptr, 0, fmt, copy);
  if (len < 0) {
    JobLog(bc::JMSG_ERROR, FMT_STRING("error while formatting message '{}'"),
           fmt);
    va_end(copy);
    va_end(args);
    return bRC_Error;
  }
  va_end(copy);
  std::string message;
  message.resize(len);
  auto len2 = vsnprintf(message.data(), message.size(), fmt, args);
  if (len2 != len) {
    JobLog(bc::JMSG_ERROR,
           FMT_STRING("error while formatting message '{}' (first and second "
                      "parse lengths do not match up)"),
           fmt);
    va_end(args);
    return bRC_Error;
  }
  va_end(args);
  ::DebugMessage(level, message.c_str(), line, file, "<unknown fun>");
  return bRC_OK;
}
bRC Wrapper_checkChanges(PluginContext*, save_pkt* sp)
{
  std::optional ft = to_grpc(sp->type);
  if (!ft) {
    JobLog(bc::JMSG_ERROR, FMT_STRING("could not convert {} to filetype"),
           sp->type);
    return bRC_Error;
  }

  const char* name = sp->fname;

  if (S_ISDIR(sp->statp.st_mode)) {
    if (!sp->link) {
      DebugLog(100, "directory '{}' has no link field set", sp->fname);
      return bRC_Error;
    }

    name = sp->link;
  }

  std::optional res = checkChanges(*ft, name, sp->save_time, sp->statp);
  if (!res) { return bRC_Error; }
  // TODO: check changes can also change the delte seq
  sp->accurate_found = *res;

  if (sp->accurate_found) { return bRC_Seen; }
  return bRC_OK;
}
bRC Wrapper_AcceptFile(PluginContext*, save_pkt* sp) /* Need fname and statp */
{
  std::optional res = AcceptFile(sp->fname, sp->statp);
  if (!res) { return bRC_Error; }

  if (*res) {
    return bRC_OK;
  } else {
    return bRC_Seen;
  }
}
bRC Wrapper_SetSeenBitmap(PluginContext*, bool all, char* fname)
{
  if (all) {
    if (SetSeen(std::nullopt)) { return bRC_OK; }
  } else {
    if (SetSeen(fname)) { return bRC_OK; }
  }
  return bRC_Error;
}
bRC Wrapper_ClearSeenBitmap(PluginContext*, bool all, char* fname)
{
  if (all) {
    if (ClearSeen(std::nullopt)) { return bRC_OK; }
  } else {
    if (ClearSeen(fname)) { return bRC_OK; }
  }
  return bRC_Error;
}
};  // namespace filedaemon

bool setup()
{
  if (auto opt = Bareos_GetString(bc::BV_PluginPath); opt) {
    plugin_path = opt.value();
  } else {
    return false;
  }
  if (plugin_path.size() == 0) {
    fprintf(stderr, "plugin_path is empty\n");
    return false;
  }
  if (plugin_path.back() != '/') { plugin_path += '/'; }

  core_api = {
      .size = sizeof(core_api),
      .version = 1,
  };

  core_funs = {
      .size = sizeof(core_funs),
      .version = 1,
      .registerBareosEvents = &filedaemon::Wrapper_registerBareosEvents,
      .unregisterBareosEvents = &filedaemon::Wrapper_unregisterBareosEvents,
      .getInstanceCount = &filedaemon::Wrapper_getInstanceCount,
      .getBareosValue = &filedaemon::Wrapper_getBareosValue,
      .setBareosValue = &filedaemon::Wrapper_setBareosValue,
      .JobMessage = &filedaemon::Wrapper_JobMessage,
      .DebugMessage = &filedaemon::Wrapper_DebugMessage,
      .bareosMalloc = nullptr,
      .bareosFree = nullptr,
      .AddExclude = nullptr,
      .AddInclude = nullptr,
      .AddOptions = nullptr,
      .AddRegex = nullptr,
      .AddWild = nullptr,
      .NewOptions = nullptr,
      .NewInclude = nullptr,
      .NewPreInclude = nullptr,
      .checkChanges = &filedaemon::Wrapper_checkChanges,
      .AcceptFile = &filedaemon::Wrapper_AcceptFile,
      .SetSeenBitmap = &filedaemon::Wrapper_SetSeenBitmap,
      .ClearSeenBitmap = &filedaemon::Wrapper_ClearSeenBitmap,
  };

  return true;
}

template <typename F> bRC plugin_run(PluginContext* ctx, F&& f)
{
  bRC result = f(ctx);

  return result;
}

std::optional<std::string_view> inferior_setup(PluginContext* ctx,
                                               std::string_view cmd);

static std::string join(const std::vector<std::string>& x)
{
  std::stringstream output;

  output << "[";

  bool first = true;
  for (auto& m : x) {
    if (first) {
      first = false;
    } else {
      output << ", ";
    }
    output << "\"" << m << "\"";
  }

  output << "]";

  return output.str();
}

std::optional<std::string_view> inferior_setup(PluginContext* ctx,
                                               std::string_view cmd)
{
  if (!ctx->plugin) {
    auto ires = cmd.find_first_of(":");
    if (ires == cmd.npos) {
      JobLog(bc::JMSG_ERROR,
             FMT_STRING("bad command string (no inferior name): {}"), cmd);
      return std::nullopt;
    }
    auto plugin_name = cmd.substr(0, ires);

    static constexpr std::string_view python_prefix = "python";

    // we want to treat "python", "python2", "python3", etc. as the same
    bool have_python = false;
    if (plugin_name >= python_prefix
        && plugin_name.size() <= python_prefix.size() + 1) {
      DebugLog(100, FMT_STRING("detected {} as python"), plugin_name);
      have_python = true;
    }

    JobLog(bc::JMSG_INFO, FMT_STRING("selected inferior: {}"), plugin_name);

    ctx->plugin = &plugin_data;

    std::vector<std::string> candidates;

    if (have_python) {
      // prefer python3 over python, as python-fd uses python2 if both
      // python3-fd and python-fd are installable
      candidates = std::vector<std::string>{
          plugin_path + "python3-fd.so", plugin_path + "python-fd.so",
          plugin_path + "python2-fd.so",  // python2 should be the last resort
      };
    } else {
      auto inferior_path = plugin_path;
      inferior_path += plugin_name;
      inferior_path += "-fd.so";
      candidates.emplace_back(std::move(inferior_path));
    }

    auto candidates_string = join(candidates);
    DebugLog(100, FMT_STRING("loading inferior from candidates: {}"),
             candidates_string);

    void* inferior_lib = nullptr;

    for (auto& candidate : candidates) {
      DebugLog(100, FMT_STRING("loading inferior from path '{}'"), candidate);

      inferior_lib = dlopen(candidate.c_str(), RTLD_NOW | RTLD_GLOBAL);

      if (inferior_lib) {
        DebugLog(100, FMT_STRING("loaded {} at {}"), candidate, inferior_lib);
        break;
      }
    }

    if (!inferior_lib) {
      JobLog(bc::JMSG_ERROR, FMT_STRING("could not load inferior. Tried {}"),
             candidates_string);
      return std::nullopt;
    }

    void* load_plugin = dlsym(inferior_lib, "loadPlugin");

    if (!load_plugin) {
      JobLog(bc::JMSG_ERROR, FMT_STRING("could not find loadPlugin: Err={}"),
             std::string_view{strerror(errno)});
      return std::nullopt;
    }

    DebugLog(100, FMT_STRING("found loadPlugin at {}"), load_plugin);

    using load_plugin_type
        = bRC(filedaemon::PluginApiDefinition*,
              filedaemon::CoreFunctions * bareos_core_functions,
              PluginInformation * *plugin_information,
              filedaemon::PluginFunctions * *plugin_functions);

    load_plugin_type* func = reinterpret_cast<load_plugin_type*>(load_plugin);

    PluginInformation* plugin_info{};

    auto load_res = (*func)(&core_api, &core_funs, &plugin_info, &plugin_funs);

    if (load_res != bRC_OK) {
      JobLog(bc::JMSG_ERROR, FMT_STRING("bad response to loadPlugin"));
      return std::nullopt;
    }

    JobLog(bc::JMSG_INFO,
           FMT_STRING("loaded inferior: {}\n"
                      "Version: {} ({})\n"
                      "Author: {}\n"
                      "License: {}\n"),
           plugin_info->plugin_description, plugin_info->plugin_version,
           plugin_info->plugin_date, plugin_info->plugin_author,
           plugin_info->plugin_license);

    plugin_data.plugin_information = plugin_info;
    plugin_data.plugin_handle = inferior_lib;
    plugin_data.plugin_functions = plugin_funs;

    auto new_res = plugin_funs->newPlugin(ctx);
    if (new_res != bRC_OK) {
      JobLog(bc::JMSG_ERROR, FMT_STRING("bad response to newPlugin"));
      return std::nullopt;
    }

    JobLog(bc::JMSG_INFO, FMT_STRING("loaded inferior successfully"));
  }

  return cmd;
}

std::condition_variable ready_cond;
std::mutex ready_mut;
std::optional<bool> ready_var;

bool WaitForReady()
{
  std::unique_lock lock{ready_mut};

  ready_cond.wait(lock, []() { return ready_var.has_value(); });

  return ready_var.value();
}

void SetReady(bool result)
{
  std::unique_lock lock{ready_mut};
  ready_var = result;
  ready_cond.notify_all();
}

bool handle_setup(prototools::ProtoBidiStream* core_events)
{
  bp::PluginRequest req;
  if (!core_events->Read(req)) {
    DebugLog(50, FMT_STRING("could not read next message"));
    return false;
  }

  if (!req.has_setup()) {
    DebugLog(50, FMT_STRING("bad communication: expected setup message"));
    return false;
  }

  globals.debug_level = req.setup().initial_debug_level();

  bp::PluginResponse resp;
  (void)resp.mutable_setup();

  if (!core_events->Write(resp)) {
    DebugLog(50, FMT_STRING("could not send setup response"));
    return false;
  }

  return setup();
}

void HandleConnection(int server_sock, int client_sock, int io_sock)
{
  prototools::ProtoBidiStream core_events{server_sock};
  prototools::ProtoBidiStream plugin_requests{client_sock};
  prototools::ProtoBidiStream data_transfer{io_sock};
  globals.client_writer = &plugin_requests;

  DebugLog(100, FMT_STRING("setting up ..."));

  bool done = false;
  if (!handle_setup(&core_events)) {
    DebugLog(50, FMT_STRING("setup failed"));
    SetReady(false);
  } else {
    Inferior inferior{&core_funs, &data_transfer, plugin_path};

    DebugLog(100,
             FMT_STRING("... successfully.\nwaiting for server to finish ..."));
    SetReady(true);

    google::protobuf::Arena arena;

    {
      while (!done) {
        auto* req = google::protobuf::Arena::Create<bp::PluginRequest>(&arena);
        if (!core_events.Read(*req)) { break; }

        bool pass_event_to_inferior = true;

        if (req->has_setup()) {
          JobLog(bc::JMSG_FATAL, FMT_STRING("core send second setup event"));
          done = true;
          pass_event_to_inferior = false;
        } else if (req->has_handle_plugin_event()) {
          auto& plugin_event = req->handle_plugin_event().to_handle();

          if (plugin_event.has_set_debug_level()) {
            globals.debug_level = plugin_event.set_debug_level().debug_level();
            pass_event_to_inferior = false;
          } else if (plugin_event.has_job_end()) {
            done = true;
          }
        }

        if (pass_event_to_inferior) {
          auto* resp
              = google::protobuf::Arena::Create<bp::PluginResponse>(&arena);

          if (!inferior.handle_core_event(req, resp)) { ASSERT(0); }

          if (!core_events.Write(*resp)) { ASSERT(0); }
        }
        arena.Reset();
      }
    }


    DebugLog(100, FMT_STRING("grpc server finished: closing connections"));
  }
  // destroy the server before killing the plugin thread
  DebugLog(100, FMT_STRING("shutdown - shutting down the grpc server"));
  // finally destroy the plugin thread
  // DebugLog(100, FMT_STRING("shutdown - stopping plugin thread"));
  // plugin.join();
  DebugLog(100, FMT_STRING("shutdown - finished"));
  if (plugin_data.plugin_handle) { dlclose(plugin_data.plugin_handle); }
}

int main(int argc, char* argv[], char* envp[])
{
  // every (C) program is started in the C locale,
  // so the following call ensures that we respect the users locale setting.
  // This is necessary to ensure UTF-8 support as well.
  setlocale(LC_ALL, "");

  (void)envp;
  (void)argv;
  if (argc != 1) {
    fprintf(
        stderr,
        "Usage: call this program with no arguments but the sockets set up\n");
    return 5;
  }

  // todo: add these in a header somewhere ?
  int server = 3;
  int client = 4;
  int io = 5;

  if (fcntl(server, F_GETFD) < 0) {
    fprintf(stderr, "bad server file descriptor given: %d\n", server);
    return 3;
  }

  if (fcntl(client, F_GETFD) < 0) {
    fprintf(stderr, "bad client file descriptor given: %d\n", client);
    return 3;
  }

  if (fcntl(io, F_GETFD) < 0) {
    fprintf(stderr, "bad io file descriptor given: %d\n", io);
    return 3;
  }

  HandleConnection(server, client, io);
}
