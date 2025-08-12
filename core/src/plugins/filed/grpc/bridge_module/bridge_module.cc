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
#include "plugin.grpc.pb.h"
#include "plugin.pb.h"
#include "bareos.grpc.pb.h"
#include "bareos.pb.h"
#include "filed/fd_plugins.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel_posix.h>
#include <grpcpp/server_posix.h>
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


struct connection_builder {
  static std::optional<std::unique_ptr<bc::Core::Stub>> connect_client(
      int sockfd)
  {
    try {
      return bc::Core::NewStub(grpc::CreateInsecureChannelFromFd("", sockfd));
    } catch (...) {
      return std::nullopt;
    }
  }

  static std::optional<std::unique_ptr<grpc::Server>> connect_server(
      int sockfd,
      const std::vector<std::unique_ptr<grpc::Service>>& services)
  {
    try {
      grpc::ServerBuilder builder;

      for (auto& service : services) { builder.RegisterService(service.get()); }

      auto server = builder.BuildAndStart();

      if (!server) { return std::nullopt; }

      grpc::AddInsecureChannelFromFd(server.get(), sockfd);

      return server;
    } catch (const std::exception& e) {
      // DebugLog(50, FMT_STRING("could not attach socket {} to server:
      // Err={}"),
      //          sockfd, e.what());
      return std::nullopt;
    } catch (...) {
      return std::nullopt;
    }
  }
};

#include <fcntl.h>

bc::Core::Stub* global_stub{nullptr};

static inline bc::Core::Stub* stub() { return global_stub; }

bool Register(std::basic_string_view<bc::EventType> types)
{
  bc::RegisterRequest req;

  for (auto type : types) { req.add_event_types(type); }


  bc::RegisterResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Events_Register(&ctx, req, &resp);

  if (!status.ok()) { return false; }

  return true;
}
bool Unregister(std::basic_string_view<bc::EventType> types)
{
  bc::UnregisterRequest req;

  for (auto type : types) { req.add_event_types(type); }


  bc::UnregisterResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Events_Unregister(&ctx, req, &resp);

  if (!status.ok()) { return false; }

  return true;
}

struct plugin_thread {
  plugin_thread() : plugin_thread(channel::CreateBufferedChannel<callback>(10))
  {
  }

  using callback = std::packaged_task<bRC(PluginContext*)>;

  template <typename F> bRC submit(F f)
  {
    callback task(std::move(f));

    auto fut = task.get_future();

    if (!enqueue_task(std::move(task))) { return bRC_Error; }

    return fut.get();
  }

  void join()
  {
    if (t.joinable()) {
      in.close();
      t.join();
    }
  }

  ~plugin_thread() { join(); }

  PluginContext* ctx() { return &pctx; }

  struct cached_event {
    filedaemon::bEvent event{};
    intptr_t ptr_value;

    filedaemon::bEvent* type() { return &event; }
    void* value() { return reinterpret_cast<void*>(ptr_value); }
  };

  std::vector<cached_event> cached_events{};

  std::vector<cached_event> clear_cached_events()
  {
    std::vector<cached_event> result{};
    std::swap(result, cached_events);
    return result;
  }
  bool cache_event(filedaemon::bEvent* event, void* value, std::size_t size)
  {
    auto& cached = cached_events.emplace_back();
    cached.event = *event;
    if (size != 0) {
      DebugLog(100, FMT_STRING("cannot cache data of size {}"), size);
      return false;
    }
    // size is 0 here, so no actual data was stored, i.e. we only care
    // about the value of the pointer
    cached.ptr_value = reinterpret_cast<intptr_t>(value);
    return true;
  }

  void allow_events(std::uint64_t mask) { registered_events |= mask; }

  void disallow_events(std::uint64_t mask) { registered_events &= ~mask; }

  bool is_event_allowed(filedaemon::bEvent* type)
  {
    return ((1 << type->eventType) & registered_events) != 0;
  }

  std::uint64_t allowed_events() { return registered_events; }

 private:
  plugin_thread(channel::channel_pair<callback> p)
      : in{std::move(p.first)}, out{std::move(p.second)}
  {
    pctx.core_private_context = this;
    // only start the thread once the plugin context is setup
    t = std::thread{+[](plugin_thread* pt) { pt->run(); }, this};
  }

  bool enqueue_task(callback&& c) { return in.emplace(std::move(c)); }

  void run()
  {
    if (!WaitForReady()) {
      fprintf(stderr, "WaitForReady failed.\n");
      return;
    }
    DebugLog(100, FMT_STRING("plugin thread starting (ctx={})..."),
             (void*)&pctx);
    for (;;) {
      std::optional cb = out.get();
      if (!cb) { break; }
      (*cb)(&pctx);
    }
    DebugLog(100, FMT_STRING("plugin thread stopping (ctx={})..."),
             (void*)&pctx);
  }

  PluginContext pctx{};
  channel::input<callback> in;
  channel::output<callback> out;
  std::thread t;

  std::uint64_t registered_events = 0;
};


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
  bc::getInstanceCountRequest req;
  bc::getInstanceCountResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_getInstanceCount(&ctx, req, &resp);

  if (!status.ok()) { return std::nullopt; }

  return resp.instance_count();
}

std::optional<bool> checkChanges(bco::FileType ft,
                                 std::string_view name,
                                 time_t timestamp,
                                 const struct stat& statp)
{
  bc::checkChangesRequest req;
  req.set_type(ft);
  req.set_file(name.data(), name.size());
  // if (link_name) { req.set_link_target(link_name->data(), link_name->size());
  // }
  req.mutable_since_time()->set_seconds(timestamp);
  req.set_stats(&statp, sizeof(statp));

  bc::checkChangesResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_checkChanges(&ctx, req, &resp);

  if (!status.ok()) { return std::nullopt; }

  return resp.old();
}
std::optional<bool> AcceptFile(std::string_view name, const struct stat& statp)
{
  bc::AcceptFileRequest req;
  req.set_file(name.data(), name.size());
  req.set_stats(&statp, sizeof(statp));

  bc::AcceptFileResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_AcceptFile(&ctx, req, &resp);

  if (!status.ok()) { return std::nullopt; }

  return resp.skip();
}

bool SetSeen(std::optional<std::string_view> name)
{
  bc::SetSeenRequest req;
  if (name) { req.set_file(name->data(), name->size()); }

  bc::SetSeenResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_SetSeen(&ctx, req, &resp);

  if (!status.ok()) { return false; }

  return true;
}
bool ClearSeen(std::optional<std::string_view> name)
{
  bc::ClearSeenRequest req;
  if (name) { req.set_file(name->data(), name->size()); }

  bc::ClearSeenResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_ClearSeen(&ctx, req, &resp);

  if (!status.ok()) { return false; }

  return true;
}
void JobMessage(bc::JMsgType type,
                int line,
                const char* file,
                const char* fun,
                std::string_view msg)
{
  bc::JobMessageRequest req;
  req.set_type(type);
  req.set_msg(msg.data(), msg.size());
  req.set_line(line);
  req.set_file(file);
  req.set_function(fun);

  bc::JobMessageResponse resp;
  grpc::ClientContext ctx;
  (void)stub()->Bareos_JobMessage(&ctx, req, &resp);
}

void DebugMessage(int level,
                  std::string_view msg,
                  int line,
                  const char* file,
                  const char* fun)
{
  bc::DebugMessageRequest req;
  req.set_level(level);
  req.set_msg(msg.data(), msg.size());
  req.set_line(line);
  req.set_file(file);
  req.set_function(fun);

  bc::DebugMessageResponse resp;
  grpc::ClientContext ctx;
  (void)stub()->Bareos_DebugMessage(&ctx, req, &resp);
}
bool Bareos_SetString(bc::BareosStringVariable var, std::string_view val)
{
  bc::SetStringRequest req;
  req.set_var(var);
  req.set_value(val.data(), val.size());

  bc::SetStringResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_SetString(&ctx, req, &resp);

  if (!status.ok()) { return false; }
  return true;
}
std::optional<std::string> Bareos_GetString(bc::BareosStringVariable var)
{
  bc::GetStringRequest req;
  req.set_var(var);

  bc::GetStringResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_GetString(&ctx, req, &resp);

  if (!status.ok()) { return std::nullopt; }
  return std::move(*resp.release_value());
}

bool Bareos_SetInt(bc::BareosIntVariable var, int val)
{
  bc::SetIntRequest req;
  req.set_var(var);
  req.set_value(val);

  bc::SetIntResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_SetInt(&ctx, req, &resp);

  if (!status.ok()) { return false; }
  return true;
}
std::optional<int> Bareos_GetInt(bc::BareosIntVariable var)
{
  bc::GetIntRequest req;
  req.set_var(var);

  bc::GetIntResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_GetInt(&ctx, req, &resp);

  if (!status.ok()) { return std::nullopt; }
  return resp.value();
}

bool Bareos_SetFlag(bc::BareosFlagVariable var, bool val)
{
  bc::SetFlagRequest req;
  req.set_var(var);
  req.set_value(val);

  bc::SetFlagResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_SetFlag(&ctx, req, &resp);

  if (!status.ok()) { return false; }
  return true;
}
std::optional<bool> Bareos_GetFlag(bc::BareosFlagVariable var)
{
  bc::GetFlagRequest req;
  req.set_var(var);

  bc::GetFlagResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_GetFlag(&ctx, req, &resp);

  if (!status.ok()) { return std::nullopt; }
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

  auto* thrd = reinterpret_cast<plugin_thread*>(ctx->core_private_context);
  thrd->allow_events(event_mask);
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

  auto* thrd = reinterpret_cast<plugin_thread*>(ctx->core_private_context);
  thrd->disallow_events(event_mask);

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
  std::optional res = checkChanges(*ft, sp->fname, sp->save_time, sp->statp);
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
  auto* thrd = reinterpret_cast<plugin_thread*>(ctx->core_private_context);
  return thrd->submit(std::move(f));
}

std::optional<std::string_view> inferior_setup(PluginContext* ctx,
                                               std::string_view cmd);
namespace filedaemon {
bRC Wrapper_newPlugin(PluginContext* outer_ctx)
{
  return plugin_run(outer_ctx, [](PluginContext*) {
    // we do not do anything here
    auto events = std::array{
        bc::EventType::Event_JobStart,
        bc::EventType::Event_JobEnd,
        bc::EventType::Event_PluginCommand,
        bc::EventType::Event_BackupCommand,
        bc::EventType::Event_StartBackupJob,
        bc::EventType::Event_StartRestoreJob,
        bc::EventType::Event_RestoreCommand,
    };

    if (!Register({events.data(), events.size()})) {
      JobLog(bc::JMSG_ERROR, "could not register my events");
      return bRC_Term;
    }
    return bRC_OK;
  });
}
bRC Wrapper_freePlugin(PluginContext* outer_ctx)
{
  return plugin_run(outer_ctx, [](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    return plugin_funs->freePlugin(ctx);
  });
}
bRC Wrapper_getPluginValue(PluginContext* outer_ctx, pVariable var, void* value)
{
  return plugin_run(outer_ctx, [var, value](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    return plugin_funs->getPluginValue(ctx, var, value);
  });
}
bRC Wrapper_setPluginValue(PluginContext* outer_ctx, pVariable var, void* value)
{
  return plugin_run(outer_ctx, [var, value](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    return plugin_funs->setPluginValue(ctx, var, value);
  });
}
static const char* event_type_to_str(int type)
{
  switch (type) {
    case bEventJobStart:
      return "JobStart";
    case bEventJobEnd:
      return "JobEnd";
    case bEventStartBackupJob:
      return "StartBackupJob";
    case bEventEndBackupJob:
      return "EndBackupJob";
    case bEventStartRestoreJob:
      return "StartRestoreJob";
    case bEventEndRestoreJob:
      return "EndRestoreJob";
    case bEventStartVerifyJob:
      return "StartVerifyJob";
    case bEventEndVerifyJob:
      return "EndVerifyJob";
    case bEventBackupCommand:
      return "BackupCommand";
    case bEventRestoreCommand:
      return "RestoreCommand";
    case bEventEstimateCommand:
      return "EstimateCommand";
    case bEventLevel:
      return "Level";
    case bEventSince:
      return "Since";
    case bEventCancelCommand:
      return "CancelCommand";
    case bEventRestoreObject:
      return "RestoreObject";
    case bEventEndFileSet:
      return "EndFileSet";
    case bEventPluginCommand:
      return "PluginCommand";
    case bEventOptionPlugin:
      return "OptionPlugin";
    case bEventHandleBackupFile:
      return "HandleBackupFile";
    case bEventNewPluginOptions:
      return "NewPluginOptions";
    case bEventVssInitializeForBackup:
      return "VssInitializeForBackup";
    case bEventVssInitializeForRestore:
      return "VssInitializeForRestore";
    case bEventVssSetBackupState:
      return "VssSetBackupState";
    case bEventVssPrepareForBackup:
      return "VssPrepareForBackup";
    case bEventVssBackupAddComponents:
      return "VssBackupAddComponents";
    case bEventVssPrepareSnapshot:
      return "VssPrepareSnapshot";
    case bEventVssCreateSnapshots:
      return "VssCreateSnapshots";
    case bEventVssRestoreLoadComponentMetadata:
      return "VssRestoreLoadComponentMetadata";
    case bEventVssRestoreSetComponentsSelected:
      return "VssRestoreSetComponentsSelected";
    case bEventVssCloseRestore:
      return "VssCloseRestore";
    case bEventVssBackupComplete:
      return "VssBackupComplete";
    default: {
      return "<unknown type>";
    }
  }
}

bRC Wrapper_handlePluginEvent(PluginContext* outer_ctx,
                              bEvent* event,
                              void* value,
                              std::size_t size)
{
  return plugin_run(outer_ctx, [event, value, size](PluginContext* ctx) {
    DebugLog(100, FMT_STRING("handling event of type \"{}\" ({})"),
             event_type_to_str(event->eventType), event->eventType);


    switch (event->eventType) {
      case bEventRestoreObject: {
        auto* rop = reinterpret_cast<restore_object_pkt*>(value);
        std::string_view cmd{rop->plugin_name};
        DebugLog(100, FMT_STRING("got cmd string \"{}\""), cmd);
        std::optional pstring = inferior_setup(ctx, cmd);
        if (!pstring) { return bRC_Error; }

        DebugLog(100, FMT_STRING("using cmd string \"{}\" for the plugin"),
                 pstring.value());

        auto* old = rop->plugin_name;
        rop->plugin_name = const_cast<char*>(pstring->data());
        auto res = plugin_funs->handlePluginEvent(ctx, event, (void*)rop);
        rop->plugin_name = old;
        return res;
      } break;
      case bEventPluginCommand:
        [[fallthrough]];
      case bEventEstimateCommand:
        [[fallthrough]];
      case bEventBackupCommand:
        [[fallthrough]];
      case bEventRestoreCommand:
        [[fallthrough]];
      case bEventNewPluginOptions: {
        std::string cmd{(char*)value};
        DebugLog(100, FMT_STRING("got cmd string \"{}\""), cmd);
        std::optional pstring = inferior_setup(ctx, cmd);
        if (!pstring) { return bRC_Error; }

        DebugLog(100, FMT_STRING("using cmd string \"{}\" for the plugin"),
                 pstring.value());

        auto* thrd
            = reinterpret_cast<plugin_thread*>(ctx->core_private_context);
        auto cached = thrd->clear_cached_events();

        DebugLog(100, FMT_STRING("inserting {} cached events ({:x})"),
                 cached.size(), thrd->allowed_events());

        bool cached_err = false;
        for (auto& cached_event : cached) {
          if (!thrd->is_event_allowed(cached_event.type())) {
            DebugLog(100, FMT_STRING("skipping cached {}-event ({})"),
                     cached_event.type()->eventType, cached_event.value());
            continue;
          }


          DebugLog(100, FMT_STRING("inserting cached {}-event ({})"),
                   cached_event.type()->eventType, cached_event.value());
          if (plugin_funs->handlePluginEvent(ctx, cached_event.type(),
                                             cached_event.value())
              == bRC_Error) {
            DebugLog(100, FMT_STRING(" ... lead to failure"));
            cached_err = true;
          }
        }

        DebugLog(100, FMT_STRING("plugin funs = {}"), (void*)plugin_funs);
        DebugLog(100, FMT_STRING("handle plugin events = {}"),
                 (void*)plugin_funs->handlePluginEvent);
        auto result = plugin_funs->handlePluginEvent(ctx, event,
                                                     (void*)pstring->data());
        if (cached_err) { return bRC_Error; }

        return result;

      } break;
    }

    auto* thrd = reinterpret_cast<plugin_thread*>(ctx->core_private_context);
    if (!plugin_funs) {
      DebugLog(100,
               FMT_STRING("inferior not setup yet, so we cache event {} ({})"),
               event->eventType, (void*)thrd);

      if (!thrd->cache_event(event, value, size)) {
        DebugLog(100, FMT_STRING("could not cache event {}"), event->eventType);
        return bRC_Error;
      }
      DebugLog(100, FMT_STRING("cached {} ({})"), event->eventType,
               (void*)thrd);

      return bRC_OK;
    }

    if (!thrd->is_event_allowed(event)) {
      DebugLog(100, FMT_STRING("skipping cached {}-event ({})"),
               event->eventType, value);
      return bRC_OK;
    }
    return plugin_funs->handlePluginEvent(ctx, event, value);
  });
}
bRC Wrapper_startBackupFile(PluginContext* outer_ctx, save_pkt* sp)
{
  return plugin_run(outer_ctx, [sp](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    return plugin_funs->startBackupFile(ctx, sp);
  });
}
bRC Wrapper_endBackupFile(PluginContext* outer_ctx)
{
  return plugin_run(outer_ctx, [](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    return plugin_funs->endBackupFile(ctx);
    return bRC_Error;
  });
}
bRC Wrapper_startRestoreFile(PluginContext* outer_ctx, const char* cmd)
{
  return plugin_run(outer_ctx, [cmd](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    return plugin_funs->startRestoreFile(ctx, cmd);
  });
}
bRC Wrapper_endRestoreFile(PluginContext* outer_ctx)
{
  return plugin_run(outer_ctx, [](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    return plugin_funs->endRestoreFile(ctx);
  });
}
bRC Wrapper_pluginIO(PluginContext* outer_ctx, io_pkt* io)
{
  return plugin_run(outer_ctx, [io](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    return plugin_funs->pluginIO(ctx, io);
  });
}
bRC Wrapper_createFile(PluginContext* outer_ctx, restore_pkt* rp)
{
  return plugin_run(outer_ctx, [rp](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    return plugin_funs->createFile(ctx, rp);
  });
}
bRC Wrapper_setFileAttributes(PluginContext* outer_ctx, restore_pkt* rp)
{
  return plugin_run(outer_ctx, [rp](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    return plugin_funs->setFileAttributes(ctx, rp);
  });
}
bRC Wrapper_checkFile(PluginContext* outer_ctx, char* fname)
{
  return plugin_run(outer_ctx, [fname](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    // this isnt really right, but there is no right answer here
    if (!plugin_funs->checkFile) { return bRC_Error; }
    return plugin_funs->checkFile(ctx, fname);
  });
}
bRC Wrapper_getAcl(PluginContext* outer_ctx, acl_pkt* ap)
{
  return plugin_run(outer_ctx, [ap](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    // this isnt really right, but there is no right answer here
    if (!plugin_funs->getAcl) {
      ap->content_length = 0;
      return bRC_OK;
    }
    return plugin_funs->getAcl(ctx, ap);
  });
}
bRC Wrapper_setAcl(PluginContext* outer_ctx, acl_pkt* ap)
{
  return plugin_run(outer_ctx, [ap](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    // this isnt really right, but there is no right answer here
    if (!plugin_funs->setAcl) { return bRC_Error; }
    return plugin_funs->setAcl(ctx, ap);
  });
}
bRC Wrapper_getXattr(PluginContext* outer_ctx, xattr_pkt* xp)
{
  return plugin_run(outer_ctx, [xp](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    // this isnt really right, but there is no right answer here
    if (!plugin_funs->getXattr) {
      xp->name_length = 0;
      return bRC_OK;
    }
    return plugin_funs->getXattr(ctx, xp);
  });
}
bRC Wrapper_setXattr(PluginContext* outer_ctx, xattr_pkt* xp)
{
  return plugin_run(outer_ctx, [xp](PluginContext* ctx) {
    if (!plugin_funs) { return bRC_Error; }
    // this isnt really right, but there is no right answer here
    if (!plugin_funs->setXattr) { return bRC_Error; }
    return plugin_funs->setXattr(ctx, xp);
  });
}
}  // namespace filedaemon

static const PluginFunctions funcs = {
    .newPlugin = filedaemon::Wrapper_newPlugin,
    .freePlugin = filedaemon::Wrapper_freePlugin,
    .getPluginValue = filedaemon::Wrapper_getPluginValue,
    .setPluginValue = filedaemon::Wrapper_setPluginValue,
    .handlePluginEvent = filedaemon::Wrapper_handlePluginEvent,
    .startBackupFile = filedaemon::Wrapper_startBackupFile,
    .endBackupFile = filedaemon::Wrapper_endBackupFile,
    .startRestoreFile = filedaemon::Wrapper_startRestoreFile,
    .endRestoreFile = filedaemon::Wrapper_endRestoreFile,
    .pluginIO = filedaemon::Wrapper_pluginIO,
    .createFile = filedaemon::Wrapper_createFile,
    .setFileAttributes = filedaemon::Wrapper_setFileAttributes,
    .checkFile = filedaemon::Wrapper_checkFile,
    .getAcl = filedaemon::Wrapper_getAcl,
    .setAcl = filedaemon::Wrapper_setAcl,
    .getXattr = filedaemon::Wrapper_getXattr,
    .setXattr = filedaemon::Wrapper_setXattr,
};

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
             strerror(errno));
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

void HandleConnection(int server_sock, int client_sock, int io_sock)
{
  std::promise<void> shutdown_signal;

  auto barrier = shutdown_signal.get_future();

  std::optional client = connection_builder::connect_client(client_sock);

  if (!client) {
    fprintf(stderr, "could not create client grpc connection ...\n");
    exit(1);
  }

  global_stub = client->get();

  plugin_thread plugin{};

  std::vector<std::unique_ptr<grpc::Service>> services{};
  services.push_back(std::make_unique<PluginService>(
      plugin.ctx(), io_sock, funcs, std::move(shutdown_signal)));

  std::optional server
      = connection_builder::connect_server(server_sock, services);

  if (!server) {
    fprintf(stderr, "Could not establish server grpc connection ...\n");
    exit(1);
  }

  DebugLog(100, FMT_STRING("setting up ..."));
  if (!setup()) {
    DebugLog(50, FMT_STRING("setup failed"));
    SetReady(false);
  } else {
    DebugLog(100,
             FMT_STRING("... successfully.\nwaiting for server to finish ..."));
    SetReady(true);

    barrier.wait();  // wait for the server to order a shutdown

    DebugLog(100, FMT_STRING("grpc server finished: closing connections"));
  }
  // destroy the server before killing the plugin thread
  DebugLog(100, FMT_STRING("shutdown - shutting down the grpc server"));
  server->get()->Shutdown();
  // destroy the services (as they have a reference to the plugin_thread)
  DebugLog(100, FMT_STRING("shutdown - stopping the server services"));
  services.clear();
  // finally destroy the plugin thread
  DebugLog(100, FMT_STRING("shutdown - stopping plugin thread"));
  plugin.join();
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
