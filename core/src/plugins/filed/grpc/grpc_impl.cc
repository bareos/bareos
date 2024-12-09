/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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

#include "common.pb.h"
#include "events.pb.h"
#include "plugin.grpc.pb.h"
#include "plugin.pb.h"
#include "bareos.grpc.pb.h"
#include "bareos.pb.h"
#include "include/baconfig.h"
#include "filed/fd_plugins.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_posix.h>
#include <grpcpp/create_channel_posix.h>

#include "plugins/filed/grpc/grpc_impl.h"
#include <fcntl.h>
#include <thread>
#include <grpcpp/impl/codegen/channel_interface.h>
#include <grpcpp/security/server_credentials.h>
#include <sys/poll.h>
#include <sys/wait.h>

#include <aio.h>

#include <chrono>

#include "bareos_api.h"

#include "include/filetypes.h"

namespace {
namespace bp = bareos::plugin;

namespace bc = bareos::core;
namespace bco = bareos::common;

std::string_view strip_prefix(std::string_view v)
{
  auto pos = v.find_first_of(":");
  if (pos == v.npos) { return v; }
  auto spos = v.find_first_of(":", pos + 1);

  if (spos == v.npos) { return v; }
  auto res = v.substr(spos + 1);

  DebugLog(100, FMT_STRING("{} => {}"), v, res);

  return res;
}

bool SetNonBlocking(PluginContext* ctx, OSFile& f)
{
  int flags = fcntl(f.get(), F_GETFL);
  if (flags == -1) {
    DebugLog(ctx, 50, FMT_STRING("could not get flags for socket {}: Err={}"),
             f.get(), strerror(errno));
    return false;
  }

  if (fcntl(f.get(), F_SETFL, flags | O_NONBLOCK) < 0) {
    DebugLog(
        ctx, 50,
        FMT_STRING("could not add non blocking flags to socket {}: Err={}"),
        f.get(), strerror(errno));
    return false;
  }

  return true;
}

struct flush_fd {
  int fd;
  const char* name;
  enum {
    OUTPUT_DEBUG,
    OUTPUT_JOB_ERROR,
  } output;

  // this buffer is used for line buffering
  // ie we read into this buffer until we hit an \n,
  // then we output that line, and continue
  std::array<char, 4096> buffer{};
  size_t buffer_free{buffer.size()};

  void reset_buffer()
  {
    buffer_free = buffer.size();
  }

  void remove_printed_bytes(size_t byte_count)
  {
    ASSERT(byte_count <= read_size());
    // this is the size of the unprinted leftovers in the buffer
    auto leftover_size = read_size() - byte_count;

    // we printed something, so move everything back to create space
    memmove(buffer.data() + byte_count,
            buffer.data(), leftover_size);

    buffer_free += byte_count;
  }

  void advance_write_head(size_t byte_count)
  {
    ASSERT(buffer_free >= byte_count);
    buffer_free -= byte_count;
  }

  char* write_start()
  {
    return buffer.data() + (buffer.size() - buffer_free);
  }

  size_t write_size() const noexcept
  {
    return buffer_free;
  }

  const char* read_start() const noexcept
  {
    return buffer.data();
  }

  size_t read_size() const noexcept
  {
    return buffer.size() - buffer_free;
  }
};

void do_std_io(std::atomic<bool>* quit,
               PluginContext* ctx,
               OSFile out,
               OSFile err)
{
  // on windows we need overlapped (async) io here
  // as pipes by themselves are not awaitable, so we need to use
  // CreateEvent on the overlapped structure to get an awaitable event
  // after issuing ReadFileEx

  DebugLog(ctx, 100, FMT_STRING("io thread - starting"));

  SetNonBlocking(ctx, out);
  SetNonBlocking(ctx, err);

  std::array fds = {
    flush_fd{out.get(), "stdout", flush_fd::OUTPUT_DEBUG},
    flush_fd{err.get(), "stderr", flush_fd::OUTPUT_JOB_ERROR},
  };

  size_t fd_count = fds.size();

  // we should be alive until the child dies, so we only use "quit" as a hint
  // to stop processing stuff.
  // as such we wait up to 5 seconds after we noticed the quit, before we
  // actually quit.  This enables us to print the output of the child
  // that it created just before it died

  std::optional<std::chrono::steady_clock::time_point> end_time{};
  while (!end_time || end_time.value() >= std::chrono::steady_clock::now()) {

    if (!end_time && quit->load()) {
      auto current_time = std::chrono::steady_clock::now();
      end_time = current_time + std::chrono::seconds(5);
      DebugLog(ctx, 100,
               FMT_STRING("io thread received end."
                          " Current time = {}, End time = {}"),
               current_time.time_since_epoch().count(),
               end_time->time_since_epoch().count());
    }

    std::array pfds = {
      pollfd{fds[0].fd, POLLIN, 0},
      pollfd{fds[1].fd, POLLIN, 0}
    };

    auto num_fired = poll(pfds.data(), fd_count, 500);
    if (num_fired < 0) {
      DebugLog(ctx, 50,
               FMT_STRING("io thread poll returned {}: Err={}"),
               num_fired, strerror(errno));
      break;
    }

    if (num_fired == 0) {
      DebugLog(ctx, 500,
               FMT_STRING("io thread - no news"));
      continue;
    }

    size_t fds_dead = 0;

    for (size_t i = 0; i < fd_count; ++i) {
      DebugLog(ctx, 200, FMT_STRING("io thread - {} events: {:#x}"),
               fds[i].name, pfds[i].revents);
      if ((pfds[i].revents & POLLIN) == POLLIN) {
        // we received data
        DebugLog(ctx, 200, FMT_STRING("io thread - {} (fd: {}) has data"),
                 fds[i].name, fds[i].fd);

        ssize_t bytes_read = read(fds[i].fd, fds[i].write_start(),
                                  fds[i].write_size());

        if (bytes_read < 0) {
          // ?? how is this possible ??
          DebugLog(ctx, 50, FMT_STRING("reading from {} returned {}: Err={}"),
                   fds[i].fd, bytes_read, strerror(errno));
        } else if (bytes_read > 0) {
          fds[i].advance_write_head(bytes_read);
          // we read new data so print any newly discovered lines
          // as we know that this buffer did not contain any complete line
          // previously, we just need to look at the newly read characters

          // we search in [search_start, search_end) for newlines
          auto* search_end = fds[i].read_start() + fds[i].read_size();
          // just look at the last `bytes_read` bytes
          auto* search_start = search_end - bytes_read;

          // if we find \n, then we print starting at print_start
          auto* print_start = fds[i].read_start();

          for (;;) {
            auto* line_end = std::find(search_start, search_end, '\n');
            if (line_end == search_end) { break; }
            std::string_view line{print_start,
                                  static_cast<size_t>(line_end - print_start)};

            switch (fds[i].output) {
            case flush_fd::OUTPUT_DEBUG: {
              DebugLog(ctx, 100, FMT_STRING("{}: {}"),
                       fds[i].name, line);
            } break;
            case flush_fd::OUTPUT_JOB_ERROR: {
              JobLog(ctx, M_ERROR, FMT_STRING("{}: {}"), fds[i].name, line);

            } break;
            }
            // if we found a newline then we print it as debug message

            // skip the newline itself
            search_start = print_start = line_end + 1;
          }

          if (print_start != fds[i].read_start()) {
            // if we printed anything, we have to fixup our buffer
            // and remove all data that was already printed

            fds[i].remove_printed_bytes(print_start - fds[i].read_start());
          } else if (fds[i].write_size() == 0) {
            // if our buffer is full, we need to flush it completely
            // to make room for more data

            std::string_view content{fds[i].read_start(),
                                     fds[i].read_size()};

            switch (fds[i].output) {
            case flush_fd::OUTPUT_DEBUG: {
              DebugLog(ctx, 100, FMT_STRING("{} (full): {}"),
                       fds[i].name, content);
            } break;
            case flush_fd::OUTPUT_JOB_ERROR: {
              JobLog(ctx, M_ERROR, FMT_STRING("{} (full): {}"),
                     fds[i].name, content);

            } break;
            }

            fds[i].reset_buffer();
          }
        }
      } else if (((pfds[i].revents & POLLHUP) == POLLHUP)
                 || (pfds[i].revents & POLLERR) == POLLERR) {
        // both pollhub and pollerr indicate that the connection broke.
        // so if there is no more data in that fd, we know that there
        // also will be no more new data.  As such we can consider this
        // connection to be done
        fds_dead += 1;
      }
    }

    if (fds_dead == fd_count) {
      DebugLog(ctx, 100, FMT_STRING("all fds are dead, so we quit"));
      break;
    }
  }

  // if there is still data left in the buffers, we dump them now
  // this can happen if the data was not properly suffixed by \n
  for (auto& buf : fds) {
    if (buf.read_size() > 0) {
      auto content = std::string_view{
        buf.read_start(),
        buf.read_size()
      };
      switch (buf.output) {
      case flush_fd::OUTPUT_DEBUG: {
        DebugLog(ctx, 100, FMT_STRING("{} (dump): {}"),
                 buf.name, content);
      } break;
      case flush_fd::OUTPUT_JOB_ERROR: {
        JobLog(ctx, M_ERROR, FMT_STRING("{} (dump): {}"),
               buf.name, content);

      } break;
      }
    }
  }
}


class BareosCore : public bc::Core::Service {
 public:
  BareosCore(PluginContext* ctx) : core{ctx} {}

  static std::optional<filedaemon::bEventType> from_grpc(bc::EventType type)
  {
    switch (type) {
      case bc::Event_JobStart:
        return filedaemon::bEventJobStart;
      case bc::Event_JobEnd:
        return filedaemon::bEventJobEnd;
      case bc::Event_StartBackupJob:
        return filedaemon::bEventStartBackupJob;
      case bc::Event_EndBackupJob:
        return filedaemon::bEventEndBackupJob;
      case bc::Event_StartRestoreJob:
        return filedaemon::bEventStartRestoreJob;
      case bc::Event_EndRestoreJob:
        return filedaemon::bEventEndRestoreJob;
      case bc::Event_StartVerifyJob:
        return filedaemon::bEventStartVerifyJob;
      case bc::Event_EndVerifyJob:
        return filedaemon::bEventEndVerifyJob;
      case bc::Event_BackupCommand:
        return filedaemon::bEventBackupCommand;
      case bc::Event_RestoreCommand:
        return filedaemon::bEventRestoreCommand;
      case bc::Event_EstimateCommand:
        return filedaemon::bEventEstimateCommand;
      case bc::Event_Level:
        return filedaemon::bEventLevel;
      case bc::Event_Since:
        return filedaemon::bEventSince;
      case bc::Event_CancelCommand:
        return filedaemon::bEventCancelCommand;
      case bc::Event_RestoreObject:
        return filedaemon::bEventRestoreObject;
      case bc::Event_EndFileSet:
        return filedaemon::bEventEndFileSet;
      case bc::Event_PluginCommand:
        return filedaemon::bEventPluginCommand;
      case bc::Event_OptionPlugin:
        return filedaemon::bEventOptionPlugin;
      case bc::Event_HandleBackupFile:
        return filedaemon::bEventHandleBackupFile;
      case bc::Event_NewPluginOptions:
        return filedaemon::bEventNewPluginOptions;
      case bc::Event_VssInitializeForBackup:
        return filedaemon::bEventVssInitializeForBackup;
      case bc::Event_VssInitializeForRestore:
        return filedaemon::bEventVssInitializeForRestore;
      case bc::Event_VssSetBackupState:
        return filedaemon::bEventVssSetBackupState;
      case bc::Event_VssPrepareForBackup:
        return filedaemon::bEventVssPrepareForBackup;
      case bc::Event_VssBackupAddComponents:
        return filedaemon::bEventVssBackupAddComponents;
      case bc::Event_VssPrepareSnapshot:
        return filedaemon::bEventVssPrepareSnapshot;
      case bc::Event_VssCreateSnapshots:
        return filedaemon::bEventVssCreateSnapshots;
      case bc::Event_VssRestoreLoadComponentMetadata:
        return filedaemon::bEventVssRestoreLoadComponentMetadata;
      case bc::Event_VssRestoreSetComponentsSelected:
        return filedaemon::bEventVssRestoreSetComponentsSelected;
      case bc::Event_VssCloseRestore:
        return filedaemon::bEventVssCloseRestore;
      case bc::Event_VssBackupComplete:
        return filedaemon::bEventVssBackupComplete;
      default:
        return std::nullopt;
    }
  }

  static std::optional<int> from_grpc(bco::FileType type)
  {
    switch (type) {
      case bco::RegularFile:
        return FT_REG;
      case bco::Directory:
        return FT_DIREND;
      case bco::SoftLink:
        return FT_LNK;
      case bco::SpecialFile:
        return FT_SPEC;
      case bco::BlockDevice:
        return FT_RAW;
      case bco::Fifo:
        return FT_FIFO;
      case bco::ReparsePoint:
        return FT_REPARSE;
      case bco::Junction:
        return FT_JUNCTION;
      case bco::Deleted:
        return FT_DELETED;
      case bco::HardlinkCopy:
        return FT_LNKSAVED;
      default:
        return std::nullopt;
    }
  }

  static std::optional<filedaemon::bVariable> from_grpc(
      bc::BareosStringVariable var)
  {
    switch (var) {
      case bc::BV_FDName:
        return filedaemon::bVarFDName;
      case bc::BV_ClientName:
        return filedaemon::bVarClient;
      case bc::BV_JobName:
        return filedaemon::bVarJobName;
      case bc::BV_WorkingDir:
        return filedaemon::bVarWorkingDir;
      case bc::BV_Where:
        return filedaemon::bVarWhere;
      case bc::BV_RegexWhere:
        return filedaemon::bVarRegexWhere;
      case bc::BV_ExePath:
        return filedaemon::bVarExePath;
      case bc::BV_BareosVersion:
        return filedaemon::bVarVersion;
      case bc::BV_PreviousJobName:
        return filedaemon::bVarPrevJobName;
      case bc::BV_UsedConfig:
        return filedaemon::bVarUsedConfig;
      case bc::BV_PluginPath:
        return filedaemon::bVarPluginPath;
      default:
        return std::nullopt;
    }
  }

  static std::optional<filedaemon::bVariable> from_grpc(
      bc::BareosIntVariable var)
  {
    switch (var) {
      case bc::BV_JobId:
        return filedaemon::bVarJobId;
      case bc::BV_JobLevel:
        return filedaemon::bVarLevel;
      case bc::BV_JobType:
        return filedaemon::bVarType;
      case bc::BV_JobStatus:
        return filedaemon::bVarJobStatus;
      case bc::BV_SinceTime:
        return filedaemon::bVarSinceTime;
      case bc::BV_Accurate:
        return filedaemon::bVarAccurate;
      case bc::BV_PrefixLinks:
        return filedaemon::bVarPrefixLinks;
      default:
        return std::nullopt;
    }
  }

  static std::optional<filedaemon::bVariable> from_grpc(
      bc::BareosFlagVariable var)
  {
    switch (var) {
      case bc::BV_FileSeen:
        return filedaemon::bVarFileSeen;
      case bc::BV_CheckChanges:
        return filedaemon::bVarCheckChanges;
      default:
        return std::nullopt;
    }
  }


 private:
  grpc::Status Events_Register(grpc::ServerContext*,
                               const bc::RegisterRequest* req,
                               bc::RegisterResponse*) override
  {
    for (auto event : req->event_types()) {
      if (!bc::EventType_IsValid(event)) {
        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            fmt::format(FMT_STRING("event {} is not a valid bareos event"),
                        event));
      }
    }

    for (auto event : req->event_types()) {
      // for some reason event is an int ??
      std::optional bareos_type = from_grpc(static_cast<bc::EventType>(event));
      if (!bareos_type) {
        return grpc::Status(
            grpc::StatusCode::INTERNAL,
            fmt::format(
                FMT_STRING("could not convert valid event {} to bareos event"),
                event));
      }

      RegisterBareosEvent(core, *bareos_type);
    }

    return grpc::Status::OK;
  }

  grpc::Status Events_Unregister(grpc::ServerContext*,
                                 const bc::UnregisterRequest* req,
                                 bc::UnregisterResponse*) override
  {
    for (auto event : req->event_types()) {
      if (!bc::EventType_IsValid(event)) {
        return grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            fmt::format(FMT_STRING("event {} is not a valid bareos event"),
                        event));
      }
    }

    for (auto event : req->event_types()) {
      // for some reason event is an int ??
      std::optional bareos_type = from_grpc(static_cast<bc::EventType>(event));
      if (!bareos_type) {
        return grpc::Status(
            grpc::StatusCode::INTERNAL,
            fmt::format(
                FMT_STRING("could not convert valid event {} to bareos event"),
                event));
      }

      UnregisterBareosEvent(core, *bareos_type);
    }

    return grpc::Status::OK;
  }

  // grpc::Status Fileset_AddExclude(grpc::ServerContext*, const
  // bc::AddExcludeRequest* request, bc::AddExcludeResponse* response) override
  // {
  // }
  // grpc::Status Fileset_AddInclude(grpc::ServerContext*, const
  // bc::AddIncludeRequest* request, bc::AddIncludeResponse* response) override
  // {
  // }
  // grpc::Status Fileset_AddOptions(grpc::ServerContext*, const
  // bc::AddOptionsRequest* request, bc::AddOptionsResponse* response) override
  // {
  // }
  // grpc::Status Fileset_AddRegex(grpc::ServerContext*, const
  // bc::AddRegexRequest* request, bc::AddRegexResponse* response) override {
  // }
  // grpc::Status Fileset_AddWild(grpc::ServerContext*, const
  // bc::AddWildRequest* request, bc::AddWildResponse* response) override {
  // }
  // grpc::Status Fileset_NewOptions(grpc::ServerContext*, const
  // bc::NewOptionsRequest* request, bc::NewOptionsResponse* response) override
  // {
  // }
  // grpc::Status Fileset_NewInclude(grpc::ServerContext*, const
  // bc::NewIncludeRequest* request, bc::NewIncludeResponse* response) override
  // {
  // }
  // grpc::Status Fileset_NewPreInclude(grpc::ServerContext*, const
  // bc::NewPreIncludeRequest* request, bc::NewPreIncludeResponse* response)
  // override {
  // }
  grpc::Status Bareos_getInstanceCount(
      grpc::ServerContext*,
      const bc::getInstanceCountRequest*,
      bc::getInstanceCountResponse* response) override
  {
    // there is only one instance per process.  Its also pretty easy to give a
    // real answer here (i guess).
    response->set_instance_count(1);
    return grpc::Status::OK;
  }

  grpc::Status Bareos_checkChanges(grpc::ServerContext*,
                                   const bc::checkChangesRequest* request,
                                   bc::checkChangesResponse* response) override
  {
    auto type = request->type();

    auto bareos_type = from_grpc(type);

    if (!bareos_type) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("could not parse {} as bareos type"),
                      int(type)));
    }

    auto& stats = request->stats();
    struct stat statp;

    if (stats.size() != sizeof(statp)) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(
              FMT_STRING(
                  "stats is not a valid stats object: size mismatch {} != {}"),
              stats.size(), sizeof(statp)));
    }

    memcpy(&statp, stats.data(), stats.size());

    auto result
        = checkChanges(core, request->file(), *bareos_type, statp,
                       static_cast<time_t>(request->since_time().seconds()));

    response->set_old(!result);

    return grpc::Status::OK;
  }

  grpc::Status Bareos_SetString(grpc::ServerContext*,
                                const bc::SetStringRequest* request,
                                bc::SetStringResponse*) override
  {
    auto var = request->var();

    std::optional bareos_var = from_grpc(var);

    if (!bareos_var) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("unknown string variable {}"), int(var)));
    }

    if (!SetBareosValue(core, *bareos_var,
                        const_cast<char*>(request->value().c_str()))) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("set not supported for {}"), int(var)));
    }

    return grpc::Status::OK;
  }

  grpc::Status Bareos_GetString(grpc::ServerContext*,
                                const bc::GetStringRequest* request,
                                bc::GetStringResponse* response) override
  {
    auto var = request->var();

    std::optional bareos_var = from_grpc(var);

    if (!bareos_var) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("unknown string variable {}"), int(var)));
    }

    const char* str = nullptr;

    if (!GetBareosValue(core, *bareos_var, &str)) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("get not supported for {}"), int(var)));
    }

    if (str == nullptr) {
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          "nullptr returned by core");
    }

    response->set_value(str);

    return grpc::Status::OK;
  }

  grpc::Status Bareos_SetInt(grpc::ServerContext*,
                             const bc::SetIntRequest* request,
                             bc::SetIntResponse*) override
  {
    auto var = request->var();

    std::optional bareos_var = from_grpc(var);

    if (!bareos_var) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("unknown string variable {}"), int(var)));
    }

    int val = request->value();
    if (!SetBareosValue(core, *bareos_var, &val)) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("set not supported for {}"), int(var)));
    }

    return grpc::Status::OK;
  }

  grpc::Status Bareos_GetInt(grpc::ServerContext*,
                             const bc::GetIntRequest* request,
                             bc::GetIntResponse* response) override
  {
    auto var = request->var();

    std::optional bareos_var = from_grpc(var);

    if (!bareos_var) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("unknown string variable {}"), int(var)));
    }

    int value{0};

    if (!GetBareosValue(core, *bareos_var, &value)) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("get not supported for {}"), int(var)));
    }

    response->set_value(value);

    return grpc::Status::OK;
  }

  grpc::Status Bareos_SetFlag(grpc::ServerContext*,
                              const bc::SetFlagRequest* request,
                              bc::SetFlagResponse*) override
  {
    auto var = request->var();

    std::optional bareos_var = from_grpc(var);

    if (!bareos_var) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("unknown string variable {}"), int(var)));
    }

    bool val = request->value();
    if (!SetBareosValue(core, *bareos_var, &val)) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("set not supported for {}"), int(var)));
    }

    return grpc::Status::OK;
  }

  grpc::Status Bareos_GetFlag(grpc::ServerContext*,
                              const bc::GetFlagRequest* request,
                              bc::GetFlagResponse* response) override
  {
    auto var = request->var();

    std::optional bareos_var = from_grpc(var);

    if (!bareos_var) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("unknown string variable {}"), int(var)));
    }

    bool value{false};

    if (!GetBareosValue(core, *bareos_var, &value)) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("get not supported for {}"), int(var)));
    }

    response->set_value(value);

    return grpc::Status::OK;
  }

  grpc::Status Bareos_AcceptFile(grpc::ServerContext*,
                                 const bc::AcceptFileRequest* request,
                                 bc::AcceptFileResponse* response) override
  {
    auto& stats = request->stats();
    struct stat statp;

    if (stats.size() != sizeof(statp)) {
      return grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          fmt::format(
              FMT_STRING(
                  "stats is not a valid stats object: size mismatch {} != {}"),
              stats.size(), sizeof(statp)));
    }

    memcpy(&statp, stats.data(), stats.size());

    auto result = AcceptFile(core, request->file(), statp);

    response->set_skip(!result);

    return grpc::Status::OK;
  }
  grpc::Status Bareos_SetSeen(grpc::ServerContext*,
                              const bc::SetSeenRequest* request,
                              bc::SetSeenResponse*) override
  {
    auto result = [&] {
      if (request->has_file()) {
        return SetSeenBitmap(core, false, request->file().c_str());
      } else {
        return SetSeenBitmap(core, true, nullptr);
      }
    }();

    if (result == bRC_Error) {
      return grpc::Status(grpc::StatusCode::INTERNAL, "something went wrong!");
    }
    return grpc::Status::OK;
  }
  grpc::Status Bareos_ClearSeen(grpc::ServerContext*,
                                const bc::ClearSeenRequest* request,
                                bc::ClearSeenResponse*) override
  {
    auto result = [&] {
      if (request->has_file()) {
        return ClearSeenBitmap(core, false, request->file().c_str());
      } else {
        return ClearSeenBitmap(core, true, nullptr);
      }
    }();

    if (result == bRC_Error) {
      return grpc::Status(grpc::StatusCode::INTERNAL, "something went wrong!");
    }
    return grpc::Status::OK;
  }

  grpc::Status Bareos_JobMessage(grpc::ServerContext*,
                                 const bc::JobMessageRequest* req,
                                 bc::JobMessageResponse*) override
  {
    JobLog(core, Type{req->type(), req->file().c_str(), (int)req->line()},
           FMT_STRING("{}"), req->msg());

    return grpc::Status::OK;
  }

  grpc::Status Bareos_DebugMessage(grpc::ServerContext*,
                                   const bc::DebugMessageRequest* req,
                                   bc::DebugMessageResponse*) override
  {
    DebugLog(core,
             Severity{(int)req->level(), req->file().c_str(), (int)req->line()},
             FMT_STRING("{}"), req->msg());

    return grpc::Status::OK;
  }

 private:
  PluginContext* core{nullptr};
};

class PluginClient {
 public:
  PluginClient(std::shared_ptr<grpc::ChannelInterface> channel,
               PluginContext* ctx)
      : stub_(bp::Plugin::NewStub(channel)), core{ctx}
  {
  }

  bRC Setup()
  {
    bp::SetupRequest req;
    bp::SetupResponse resp;
    grpc::ClientContext ctx;

    auto status = stub_->Setup(&ctx, req, &resp);

    if (!status.ok()) { return bRC_Error; }

    return bRC_OK;
  }

  bRC handlePluginEvent(filedaemon::bEventType type, void* data)
  {
    bp::handlePluginEventRequest req;
    auto* event = req.mutable_to_handle();

    switch (type) {
      case filedaemon::bEventJobStart: {
        auto* inner = event->mutable_job_start();
        inner->set_data((char*)data);
      } break;
      case filedaemon::bEventJobEnd: {
        auto* inner = event->mutable_job_end();
        (void)inner;
      } break;
      case filedaemon::bEventStartBackupJob: {
        auto* inner = event->mutable_start_backup_job();
        (void)inner;
      } break;
      case filedaemon::bEventEndBackupJob: {
        auto* inner = event->mutable_end_backup_job();
        (void)inner;
      } break;
      case filedaemon::bEventStartRestoreJob: {
        auto* inner = event->mutable_start_restore_job();
        (void)inner;
      } break;
      case filedaemon::bEventEndRestoreJob: {
        auto* inner = event->mutable_end_restore_job();
        (void)inner;
      } break;
      case filedaemon::bEventStartVerifyJob: {
        auto* inner = event->mutable_start_verify_job();
        (void)inner;
      } break;
      case filedaemon::bEventEndVerifyJob: {
        auto* inner = event->mutable_end_verify_job();
        (void)inner;
      } break;
      case filedaemon::bEventBackupCommand: {
        auto* inner = event->mutable_backup_command();
        inner->set_data((char*)data);
      } break;
      case filedaemon::bEventRestoreCommand: {
        auto* inner = event->mutable_restore_command();
        inner->set_data((char*)data);
      } break;
      case filedaemon::bEventEstimateCommand: {
        auto* inner = event->mutable_estimate_command();
        inner->set_data((char*)data);
      } break;
      case filedaemon::bEventLevel: {
        auto* inner = event->mutable_level();
        inner->set_level((intptr_t)data);
      } break;
      case filedaemon::bEventSince: {
        auto* inner = event->mutable_since();
        time_t since_time = (intptr_t)data;
        inner->mutable_since()->set_seconds(since_time);
      } break;
      case filedaemon::bEventCancelCommand: {
        auto* inner = event->mutable_cancel_command();
        (void)inner;
      } break;
      case filedaemon::bEventRestoreObject: {
        if (data == nullptr) {
          auto* inner = event->mutable_end_restore_object();
          (void)inner;
        } else {
          auto* rop = reinterpret_cast<filedaemon::restore_object_pkt*>(data);

          auto* inner = event->mutable_restore_object();
          auto* grop = inner->mutable_rop();
          grop->set_jobid(rop->JobId);
          // TODO: we need to remove grpc: from this!
          grop->set_used_cmd_string(rop->plugin_name);
          auto* sent = grop->mutable_sent();
          sent->set_index(rop->object_index);
          sent->set_data(rop->object, rop->object_len);
          sent->set_name(rop->object_name);
        }
      } break;
      case filedaemon::bEventEndFileSet: {
        auto* inner = event->mutable_end_fileset();
        (void)inner;
      } break;
      case filedaemon::bEventPluginCommand: {
        auto* inner = event->mutable_plugin_command();
        inner->set_data((char*)data);
      } break;
      case filedaemon::bEventOptionPlugin: {
        auto* inner = event->mutable_option_plugin();
        (void)inner;
      } break;
      case filedaemon::bEventHandleBackupFile: {
        auto* inner = event->mutable_handle_backup_file();
        // TODO
        (void)inner;
        DebugLog(50, "handle backup file not supported yet");
        return bRC_Error;
      } break;
      case filedaemon::bEventNewPluginOptions: {
        auto* inner = event->mutable_new_plugin_options();
        inner->set_data((char*)data);
      } break;
      case filedaemon::bEventVssInitializeForBackup: {
        auto* inner = event->mutable_vss_init_backup();
        (void)inner;
      } break;
      case filedaemon::bEventVssInitializeForRestore: {
        auto* inner = event->mutable_vss_init_restore();
        (void)inner;
      } break;
      case filedaemon::bEventVssSetBackupState: {
        auto* inner = event->mutable_vss_set_backup_state();
        (void)inner;
      } break;
      case filedaemon::bEventVssPrepareForBackup: {
        auto* inner = event->mutable_vss_prepare_snapshot();
        (void)inner;
      } break;
      case filedaemon::bEventVssBackupAddComponents: {
        auto* inner = event->mutable_vss_backup_add_components();
        (void)inner;
      } break;
      case filedaemon::bEventVssPrepareSnapshot: {
        auto* inner = event->mutable_vss_prepare_snapshot();
        (void)inner;
      } break;
      case filedaemon::bEventVssCreateSnapshots: {
        auto* inner = event->mutable_vss_create_snapshot();
        (void)inner;
      } break;
      case filedaemon::bEventVssRestoreLoadComponentMetadata: {
        auto* inner = event->mutable_vss_restore_load_companents_metadata();
        (void)inner;
      } break;
      case filedaemon::bEventVssRestoreSetComponentsSelected: {
        auto* inner = event->mutable_vss_restore_set_components_selected();
        (void)inner;
      } break;
      case filedaemon::bEventVssCloseRestore: {
        auto* inner = event->mutable_vss_close_restore();
        (void)inner;
      } break;
      case filedaemon::bEventVssBackupComplete: {
        auto* inner = event->mutable_vss_backup_complete();
        (void)inner;
      } break;
    }

    bp::handlePluginEventResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->handlePluginEvent(&ctx, req, &resp);


    if (!status.ok()) {
      DebugLog(50, FMT_STRING("rpc did not succeed for event {} ({}): Err={}"),
               int(type), int(status.error_code()), status.error_message());
      return bRC_Error;
    }

    auto res = resp.res();

    DebugLog(100, FMT_STRING("plugin handled event {} with res = {} ({})"),
             int(type), bp::ReturnCode_Name(res), int(res));

    switch (res) {
      case bareos::plugin::RC_OK:
        return bRC_OK;
      case bareos::plugin::RC_Stop:
        return bRC_Stop;
      case bareos::plugin::RC_More:
        return bRC_More;
      case bareos::plugin::RC_Term:
        return bRC_Term;
      case bareos::plugin::RC_Seen:
        return bRC_Seen;
      case bareos::plugin::RC_Core:
        return bRC_Core;
      case bareos::plugin::RC_Skip:
        return bRC_Skip;
      case bareos::plugin::RC_Cancel:
        return bRC_Cancel;

      case bareos::plugin::RC_Error:
        [[fallthrough]];
      default:
        return bRC_Error;
    }
  }

  bRC startBackupFile(filedaemon::save_pkt* pkt)
  {
    bp::startBackupFileRequest req;
    req.set_no_read(pkt->no_read);
    req.set_portable(pkt->portable);

    auto inner = strip_prefix(pkt->cmd);

    req.set_cmd(inner.data(), inner.size());
    req.set_flags(pkt->flags, sizeof(pkt->flags));

    bp::startBackupFileResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->startBackupFile(&ctx, req, &resp);

    if (!status.ok()) { return bRC_Error; }

    switch (resp.result()) {
      case bareos::plugin::SBF_OK: {
        if (resp.has_file()) {
          auto& file = resp.file();

          DebugLog(100, FMT_STRING("received a file"));

          if (file.stats().size() != sizeof(pkt->statp)) {
            DebugLog(50, FMT_STRING("stats has the wrong size {} != {}"),
                     file.stats().size(), sizeof(pkt->statp));
            return bRC_Error;
          }
          std::optional ft = BareosCore::from_grpc(file.ft());
          if (!ft) {
            DebugLog(50, FMT_STRING("could not convert filetype {} ({})"),
                     bco::FileType_Name(file.ft()), int(file.ft()));
            return bRC_Error;
          }
          if (pkt->fname) {
            free(pkt->fname);
            pkt->fname = nullptr;
          }
          if (pkt->link) {
            free(pkt->link);
            pkt->link = nullptr;
          }

          switch (file.ft()) {
            case bco::SpecialFile:
              [[fallthrough]];
            case bco::BlockDevice:
              [[fallthrough]];
            case bco::Fifo:
              [[fallthrough]];
            case bco::ReparsePoint:
              [[fallthrough]];
            case bco::Junction:
              [[fallthrough]];
            case bco::Deleted:
              [[fallthrough]];
            case bco::HardlinkCopy:
              [[fallthrough]];
            case bco::RegularFile: {
              pkt->fname = strdup(file.file().c_str());
              pkt->link = nullptr;
            } break;
            case bco::Directory: {
              pkt->fname = strdup(file.file().c_str());
              // todo: fix this
              auto path = file.file();
              path += "/";
              pkt->link = strdup(path.c_str());
            } break;
            case bco::SoftLink: {
              pkt->fname = strdup(file.file().c_str());
              pkt->link = strdup(file.link().c_str());
            } break;
            default: {
              DebugLog(50, FMT_STRING("bad filetype {} ({})"),
                       bco::FileType_Name(file.ft()), int(file.ft()));
              return bRC_Error;
            } break;
          }
          memcpy(&pkt->statp, file.stats().data(), file.stats().size());
          pkt->type = *ft;
          pkt->no_read = file.no_read();
          pkt->portable = file.portable();

          if (file.has_delta_seq() && file.delta_seq() != 0) {
            SetBit(FO_DELTA, pkt->flags);
            pkt->delta_seq = file.delta_seq();
          } else {
            ClearBit(FO_DELTA, pkt->flags);
            pkt->delta_seq = 0;
          }
          if (file.offset_backup()) {
            SetBit(FO_OFFSETS, pkt->flags);
          } else {
            ClearBit(FO_OFFSETS, pkt->flags);
          }
          if (file.sparse_backup()) {
            SetBit(FO_SPARSE, pkt->flags);
          } else {
            ClearBit(FO_SPARSE, pkt->flags);
          }
          // THIS is unused, so we always clear it to be sure
          ClearBit(FO_PORTABLE_DATA, pkt->flags);
        } else if (resp.has_object()) {
          DebugLog(100, FMT_STRING("received an object"));
          auto& object = resp.object();
          pkt->type = FT_RESTORE_FIRST;
          if (pkt->object) {
            free(pkt->object);
            pkt->object = nullptr;
          }
          if (pkt->object_name) {
            free(pkt->object_name);
            pkt->object_name = nullptr;
          }

          // TODO: this is not cleaned up properly.  The only way to do this
          //       is to use a job-local variable.  Maybe this can be put
          //       into the plugin context ...
          pkt->object = reinterpret_cast<char*>(malloc(object.data().size()));
          memcpy(pkt->object, object.data().c_str(), object.data().size());
          pkt->object_len = object.data().size();
          // TODO: this as well
          pkt->object_name = strdup(object.name().c_str());
          pkt->index = object.index();
        } else if (resp.has_error()) {
          auto& err = resp.error();

          switch (err.error()) {
            case bco::InvalidFileSystem: {
              pkt->type = FT_INVALIDFS;
            } break;
            case bco::InvalidDriveType: {
              pkt->type = FT_INVALIDDT;
            } break;
            case bco::CouldNotOpenDirectory: {
              pkt->type = FT_NOOPEN;
            } break;
            case bco::CouldNotChangeFilesystem: {
              pkt->type = FT_NOFSCHG;
            } break;
            case bco::RecursionDisabled: {
              pkt->type = FT_NORECURSE;
            } break;
            case bco::CouldNotStat: {
              pkt->type = FT_NOSTAT;
            } break;
            case bco::CouldNotFollowLink: {
              pkt->type = FT_NOFOLLOW;
            } break;
            case bco::CouldNotAccessFile: {
              pkt->type = FT_NOACCESS;
            } break;
            default: {
              DebugLog(50, FMT_STRING("bad type {} ({})"),
                       bco::FileErrorType_Name(err.error()), int(err.error()));
              return bRC_Error;
            }
          }

          pkt->fname = strdup(err.file().c_str());
        } else {
          DebugLog(100, FMT_STRING("received nothing"));
          return bRC_Error;
        }

        return bRC_OK;
      }
      case bareos::plugin::SBF_Stop:
        return bRC_Stop;
      case bareos::plugin::SBF_Skip:
        return bRC_Skip;
      default:
        return bRC_Error;
    }
  }

  bRC endBackupFile()
  {
    bp::endBackupFileRequest req;

    bp::endBackupFileResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->endBackupFile(&ctx, req, &resp);

    if (!status.ok()) { return bRC_Error; }

    switch (resp.result()) {
      case bareos::plugin::EBF_Done:
        return bRC_OK;
      case bareos::plugin::EBF_More:
        return bRC_More;
      default:
        return bRC_Error;
    }
  }

  bRC startRestoreFile(std::string_view cmd)
  {
    bp::startRestoreFileRequest req;

    auto inner = strip_prefix(cmd);

    req.set_command(inner.data(), inner.size());

    bp::startRestoreFileResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->startRestoreFile(&ctx, req, &resp);

    if (!status.ok()) { return bRC_Error; }

    return bRC_OK;
  }

  bRC endRestoreFile()
  {
    bp::endRestoreFileRequest req;

    bp::endRestoreFileResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->endRestoreFile(&ctx, req, &resp);

    if (!status.ok()) { return bRC_Error; }

    return bRC_OK;
  }


  bRC FileOpen(std::string_view name,
               int32_t flags,
               int32_t mode,
               bool* io_in_core)
  {
    bp::fileOpenRequest req;
    req.set_mode(mode);
    req.set_flags(flags);
    req.set_file(name.data(), name.size());

    bp::fileOpenResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->FileOpen(&ctx, req, &resp);

    if (!status.ok()) { return bRC_Error; }

    DebugLog(100, FMT_STRING("FileOpen {{io_in_core = {}}}"),
             resp.io_in_core());

    *io_in_core = resp.io_in_core();

    return bRC_OK;
  }

  bRC FileSeek(int whence, int64_t offset)
  {
    bp::SeekStart start;
    switch (whence) {
      case SEEK_SET: {
        start = bp::SS_StartOfFile;
      } break;
      case SEEK_CUR: {
        start = bp::SS_CurrentPos;
      } break;
      case SEEK_END: {
        start = bp::SS_EndOfFile;
      } break;
      default: {
        return bRC_Error;
      }
    }

    bp::fileSeekRequest req;
    req.set_whence(start);
    req.set_offset(offset);

    bp::fileSeekResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->FileSeek(&ctx, req, &resp);

    if (!status.ok()) { return bRC_Error; }

    return bRC_OK;
  }

  struct read_iter {
    bp::fileReadRequest req;
    grpc::ClientContext ctx;
    PluginContext* pctx;
    std::unique_ptr<grpc::ClientReader<bp::fileReadResponse>> reader;

    read_iter(bp::Plugin::Stub* stub, size_t size, PluginContext* plugin_ctx)
        : pctx{plugin_ctx}
    {
      req.set_num_bytes(size);
      reader = stub->FileRead(&ctx, req);
    }

    bool next(size_t* size)
    {
      bp::fileReadResponse resp;
      if (!reader->Read(&resp)) { return false; }
      *size = resp.size();
      return true;
    }

    bRC result()
    {
      auto status = reader->Finish();
      if (!status.ok()) {
        ::DebugLog(pctx, 50, FMT_STRING("file read error {}: {}"),
                   int(status.error_code()), status.error_message());
        return bRC_Error;
      }
      return bRC_OK;
    }
  };

  read_iter FileRead(size_t size) { return read_iter{stub_.get(), size, core}; }

  bRC FileWrite(size_t size, size_t* num_bytes_written)
  {
    bp::fileWriteRequest req;
    req.set_bytes_written(size);

    bp::fileWriteResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->FileWrite(&ctx, req, &resp);

    if (!status.ok()) {
      DebugLog(50, FMT_STRING("file write error {}: {}"),
               int(status.error_code()), status.error_message());
      return bRC_Error;
    }

    *num_bytes_written = resp.bytes_written();

    // ASSERT(num_bytes_written <= size);

    return bRC_OK;
  }

  bRC FileClose()
  {
    bp::fileCloseRequest req;

    bp::fileCloseResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->FileClose(&ctx, req, &resp);

    if (!status.ok()) { return bRC_Error; }

    return bRC_OK;
  }

  std::optional<bareos::common::ReplaceType> grpc_replace_type(int replace)
  {
    switch (replace) {
      case REPLACE_IFOLDER: {
        return bareos::common::ReplaceIfOlder;
      }
      case REPLACE_IFNEWER: {
        return bareos::common::ReplaceIfNewer;
      }
      case REPLACE_ALWAYS: {
        return bareos::common::ReplaceAlways;
      }
      case REPLACE_NEVER: {
        return bareos::common::ReplaceNever;
      }
    }

    return std::nullopt;
  }

  std::optional<bareos::common::FileType> grpc_file_type(int ft)
  {
    switch (ft) {
      case FT_LNKSAVED: {
        return bco::HardlinkCopy;
      } break;
      case FT_REGE: {
        return bco::RegularFile;
      } break;
      case FT_REG: {
        return bco::RegularFile;
      } break;
      case FT_LNK: {
        return bco::SoftLink;
      } break;
      case FT_DIREND: {
        return bco::Directory;
      } break;
      case FT_SPEC: {
        return bco::SpecialFile;
      } break;
      case FT_RAW: {
        return bco::BlockDevice;
      } break;
      case FT_REPARSE: {
        return bco::ReparsePoint;
      } break;
      case FT_JUNCTION: {
        return bco::Junction;
      } break;
      case FT_FIFO: {
        return bco::Fifo;
      } break;
      case FT_DELETED: {
        return bco::Deleted;
      } break;
      case FT_ISARCH: {
        return bco::RegularFile;
      } break;
    }

    return std::nullopt;
  }

  bRC createFile(filedaemon::restore_pkt* pkt)
  {
    bp::createFileRequest req;

    auto replace_type = grpc_replace_type(pkt->replace);
    auto file_type = grpc_file_type(pkt->type);

    if (!replace_type) {
      DebugLog(50, FMT_STRING("got a bad replace value {}"), pkt->replace);
      return bRC_Error;
    }

    if (!file_type) {
      DebugLog(50, FMT_STRING("got a bad file type {}"), pkt->type);
      return bRC_Error;
    }

    req.set_ft(file_type.value());
    req.set_stats((char*)&pkt->statp, sizeof(pkt->statp));
    req.set_output_name(pkt->ofname);
    req.set_soft_link_to(pkt->olname);
    req.set_replace(*replace_type);
    req.set_delta_seq(pkt->delta_seq);

    bp::createFileResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->createFile(&ctx, req, &resp);

    if (!status.ok()) { return bRC_Error; }

    switch (resp.status()) {
      case bareos::plugin::CF_Created: {
        pkt->create_status = CF_CREATED;
      } break;
      case bareos::plugin::CF_Extract: {
        pkt->create_status = CF_EXTRACT;
      } break;
      case bareos::plugin::CF_Skip: {
        pkt->create_status = CF_SKIP;
      } break;
      case bareos::plugin::CF_Core: {
        pkt->create_status = CF_CORE;
      } break;
      case bareos::plugin::CF_Error: {
        pkt->create_status = CF_ERROR;
      } break;
      default:
        return bRC_Term;
    }

    return bRC_OK;
  }
  bRC setFileAttributes(std::string_view name,
                        const struct stat& statp,
                        std::string_view extended_attributes,
                        std::string_view where,
                        bool* do_in_core)
  {
    bp::setFileAttributesRequest req;
    req.set_file(name.data(), name.size());
    req.set_stats((char*)&statp, sizeof(statp));
    req.set_extended_attributes(extended_attributes.data(),
                                extended_attributes.size());
    req.set_where(where.data(), where.size());

    bp::setFileAttributesResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->setFileAttributes(&ctx, req, &resp);

    if (!status.ok()) { return bRC_Error; }

    *do_in_core = resp.set_attributes_in_core();

    return bRC_OK;
  }
  bRC checkFile(std::string_view name)
  {
    bp::checkFileRequest req;
    req.set_file(name.data(), name.size());
    bp::checkFileResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->checkFile(&ctx, req, &resp);

    if (!status.ok()) { return bRC_Error; }

    if (resp.seen()) { return bRC_Seen; }

    return bRC_OK;
  }

  bRC setAcl(std::string_view file, std::string_view content)
  {
    bp::setAclRequest req;
    req.set_file(file.data(), file.size());
    req.mutable_content()->set_data(content.data(), content.size());

    bp::setAclResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->setAcl(&ctx, req, &resp);

    if (!status.ok()) { return bRC_Error; }

    return bRC_OK;
  }

  bRC getAcl(std::string_view file, char** buffer, size_t* size)
  {
    bp::getAclRequest req;
    req.set_file(file.data(), file.size());

    bp::getAclResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->getAcl(&ctx, req, &resp);

    if (!status.ok()) {
      DebugLog(50,
               FMT_STRING("rpc did not succeed for event getAcl ({}): Err={}"),
               int(status.error_code()), status.error_message());
      return bRC_Error;
    }

    auto& data = resp.content().data();
    *buffer = reinterpret_cast<char*>(malloc(data.size() + 1));
    *size = data.size();

    memcpy(*buffer, data.data(), data.size() + 1);

    return bRC_OK;
  }

  bRC setXattr(std::string_view file,
               std::string_view key,
               std::string_view value)
  {
    bp::setXattrRequest req;
    req.set_file(file.data(), file.size());
    auto* xattr = req.mutable_attribute();
    xattr->set_key(key.data(), key.size());
    xattr->set_value(value.data(), value.size());

    bp::setXattrResponse resp;
    grpc::ClientContext ctx;
    grpc::Status status = stub_->setXattr(&ctx, req, &resp);

    if (!status.ok()) { return bRC_Error; }

    return bRC_OK;
  }

  bRC getXattr(std::string_view file,
               char** name_buf,
               size_t* name_size,
               char** value_buf,
               size_t* value_size)
  {
    // The idea here is that we grab all xattributes at once
    // and then trickle them out for each call

    if (current_xattr_index == std::numeric_limits<size_t>::max()) {
      // we need to grab them now
      bp::getXattrRequest req;
      req.set_file(file.data(), file.size());

      bp::getXattrResponse resp;
      grpc::ClientContext ctx;
      grpc::Status status = stub_->getXattr(&ctx, req, &resp);

      if (!status.ok()) { return bRC_Error; }

      xattribute_cache.assign(
          std::make_move_iterator(std::begin(resp.attributes())),
          std::make_move_iterator(std::end(resp.attributes())));

      current_xattr_index = 0;
    }

    if (current_xattr_index == xattribute_cache.size()) {
      current_xattr_index = -1;  // reset
      return bRC_Error;
    }

    auto& current = xattribute_cache[current_xattr_index++];

    auto& key = current.key();
    auto& val = current.value();

    *name_size = key.size();
    *name_buf = reinterpret_cast<char*>(malloc(key.size() + 1));
    memcpy(*name_buf, key.data(), key.size() + 1);

    *value_size = val.size();
    *value_buf = reinterpret_cast<char*>(malloc(val.size() + 1));
    memcpy(*value_buf, val.data(), val.size() + 1);

    if (current_xattr_index == xattribute_cache.size()) {
      current_xattr_index = -1;  // reset
      return bRC_OK;
    } else {
      return bRC_More;
    }
  }

 private:
  std::unique_ptr<bp::Plugin::Stub> stub_{};
  PluginContext* core{nullptr};

  size_t current_xattr_index{std::numeric_limits<size_t>::max()};
  std::vector<bp::Xattribute> xattribute_cache{};

  template <typename... Args>
  void DebugLog(Severity severity,
                fmt::format_string<Args...> fmt,
                Args&&... args)
  {
    ::DebugLog(core, severity, std::move(fmt), std::forward<Args>(args)...);
  }
};

}  // namespace

struct grpc_connection_members {
  PluginClient client;
  std::vector<std::unique_ptr<grpc::Service>> services;
  std::shared_ptr<grpc::Channel> channel;
  std::unique_ptr<grpc::Server> server;

  grpc_connection_members() = delete;
};

struct connection_builder {
  PluginContext* ctx;
  std::shared_ptr<grpc::Channel> channel{};
  std::optional<PluginClient> opt_client{};
  std::unique_ptr<grpc::Server> opt_server{};
  std::vector<std::unique_ptr<grpc::Service>> services{};

  template <typename... Args>
  connection_builder(PluginContext* pctx, Args&&... args) : ctx{pctx}
  {
    (services.emplace_back(std::forward<Args>(args)), ...);
  }

  connection_builder& connect_client(Socket s)
  {
    // TODO: test what happens if the child is already dead at this point
    //       or if it does not create the server
    channel = grpc::CreateInsecureChannelFromFd("", s.get());

    if (channel) {
      ::DebugLog(ctx, 100, FMT_STRING("could connect to client over socket {}"),
                 s.get());

      s.release();
      opt_client.emplace(channel, ctx);

    } else {
      ::DebugLog(ctx, 50,
                 FMT_STRING("could not connect to client over socket {}"),
                 s.get());
    }

    return *this;
  }

  connection_builder& connect_server(Socket s)
  {
    try {
      grpc::ServerBuilder builder;

      for (auto& service : services) { builder.RegisterService(service.get()); }

      opt_server = builder.BuildAndStart();

      if (!opt_server) {
        ::DebugLog(ctx, 50, FMT_STRING("grpc server could not get started"),
                   s.get());
        return *this;
      }

      grpc::AddInsecureChannelFromFd(opt_server.get(), s.release());

    } catch (const std::exception& e) {
      ::DebugLog(ctx, 50,
                 FMT_STRING("could not attach socket {} to server: Err={}"),
                 s.get(), e.what());
      opt_server.reset();
    } catch (...) {
      opt_server.reset();
    }

    return *this;
  }


  std::optional<grpc_connection> build()
  {
    if (!opt_client) { return std::nullopt; }
    if (!opt_server) { return std::nullopt; }

    grpc_connection con{};

    con.members = new grpc_connection_members{
        std::move(opt_client.value()), std::move(services), std::move(channel),
        std::move(opt_server)};

    return con;
  }
};

struct grpc_connection_builder {
  PluginContext* ctx;

  template <typename... Args>
  void DebugLog(Severity severity,
                fmt::format_string<Args...> fmt,
                Args&&... args)
  {
    ::DebugLog(ctx, severity, std::move(fmt), std::forward<Args>(args)...);
  }

  template <typename... Args>
  void JobLog(Type type, fmt::format_string<Args...> fmt, Args&&... args)
  {
    auto formatted = fmt::format(fmt, args...);

    ::JobLog(ctx, type, std::move(fmt), std::forward<Args>(args)...);
  }

  std::optional<std::pair<Socket, Socket>> unix_socket_pair()
  {
    int sockets[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
      DebugLog(50, FMT_STRING("could not create socket pair: {}"),
               strerror(errno));
      return std::nullopt;
    }

    return std::make_pair(sockets[0], sockets[1]);
  }

  std::optional<std::pair<OSFile, OSFile>> unix_pipe()
  {
    int fds[2];

    if (pipe(fds) < 0) {
      DebugLog(50, FMT_STRING("could not create pipe: {}"), strerror(errno));
      return std::nullopt;
    }

    return std::make_pair(fds[0], fds[1]);
  }

  struct grpc_connections {
    OSFile std_out;
    OSFile std_err;

    Socket grpc_parent;  // parent = server
    Socket grpc_child;   // child = server
    Socket grpc_io;
  };

  void set_close_on_exec(grpc_connections& io)
  {
    if (fcntl(io.std_out.get(), F_SETFD, FD_CLOEXEC) < 0) {
      DebugLog(
          50,
          FMT_STRING("could not set CLOEXEC on program output pipe {}. Err={}"),
          io.std_out.get(), strerror(errno));
    }
    if (fcntl(io.std_err.get(), F_SETFD, FD_CLOEXEC) < 0) {
      DebugLog(
          50,
          FMT_STRING("could not set CLOEXEC on program error pipe {}. Err={}"),
          io.std_err.get(), strerror(errno));
    }
    if (fcntl(io.grpc_parent.get(), F_SETFD, FD_CLOEXEC) < 0) {
      DebugLog(50,
               FMT_STRING("could not set CLOEXEC on parent socket {}. Err={}"),
               io.grpc_parent.get(), strerror(errno));
    }
    if (fcntl(io.grpc_child.get(), F_SETFD, FD_CLOEXEC) < 0) {
      DebugLog(50,
               FMT_STRING("could not set CLOEXEC on child socket {}. Err={}"),
               io.grpc_child.get(), strerror(errno));
    }
    if (fcntl(io.grpc_io.get(), F_SETFD, FD_CLOEXEC) < 0) {
      DebugLog(50, FMT_STRING("could not set CLOEXEC on io socket {}. Err={}"),
               io.grpc_io.get(), strerror(errno));
    }
  }

  std::optional<std::pair<grpc_connections, grpc_connections>>
  make_grpc_connections()
  {
    auto std_out = unix_pipe();
    auto std_err = unix_pipe();

    auto grpc_parent = unix_socket_pair();
    auto grpc_child = unix_socket_pair();
    auto grpc_io = unix_socket_pair();

    if (!std_out || !std_err || !grpc_parent || !grpc_child || !grpc_io) {
      return std::nullopt;
    }
    return std::make_pair(
        grpc_connections{std::move(std_out->first), std::move(std_err->first),
                         std::move(grpc_parent->first),
                         std::move(grpc_child->first),
                         std::move(grpc_io->first)},
        grpc_connections{std::move(std_out->second), std::move(std_err->second),
                         std::move(grpc_parent->second),
                         std::move(grpc_child->second),
                         std::move(grpc_io->second)});
  }

  enum class predefined_fd : int
  {
    // these are from the perspective of the plugin
    In = 0,
    Out = 1,
    Err = 2,

    GrpcIn = 3,
    GrpcOut = 4,
    GrpcIo = 5,
  };

  bool IsPredefinedFD(int fd)
  {
    // 0 = stdin
    // 1 = stdout
    // 2 = stderr
    // 3 = grpc in
    // 4 = grpc out
    // 5 = grpc io
    return 0 <= fd && fd < 6;
  }

  bool FixupBadFD(int& fd, int& dummy)
  {
    if (!IsPredefinedFD(fd)) { return true; }
    // we first need to get it out of our range
    // later we put it into the right place
    int newfd = dup2(fd, dummy);  // this closes dummy if it refered to a file,
    // but we chose dummy in such a way that this does not
    // matter

    if (newfd < 0) {
      // TODO: what to do here ?
      return false;
    }

    if (newfd == fd) {
      // this should never happen
      dummy *= 2;
      return true;
    }

    close(fd);
    fd = newfd;
    dummy += 1;
    return true;
  }

  template <typename... Args> int supremum(Args... ints)
  {
    int sup = 0;

    ((sup = std::max(sup, ints)), ...);

    return sup;
  }

  bool move_fd(int fd, int newfd)
  {
    if (dup2(fd, newfd) != newfd) {
      return false;
    }

    close(fd);

    return true;
  }

  std::optional<pid_t> StartDuplexGrpc(std::string_view program_path,
                                       grpc_connections io)
  {
    DebugLog(100,
             FMT_STRING("trying to start {} with io {{Parent = {}, Child = {}, "
                        "Io = {}, Out = {}, Err = {}}}"),
             program_path, io.grpc_parent.get(), io.grpc_child.get(),
             io.grpc_io.get(), io.std_out.get(), io.std_err.get());

    pid_t child = fork();

    if (child < 0) {
      return std::nullopt;
    } else if (child == 0) {
      int std_in = open("/dev/null", O_RDONLY);

      int dummy_fd = supremum(io.grpc_parent.get(), io.grpc_child.get(),
                              io.grpc_io.get(), std_in, io.std_out.get(),
                              io.std_err.get())
                     + 1;  // we dont care
                           // about fds
                           // apart from these 6

      if (!FixupBadFD(io.grpc_parent.get(), dummy_fd)
          || !FixupBadFD(io.grpc_child.get(), dummy_fd)
          || !FixupBadFD(io.grpc_io.get(), dummy_fd)
          || !FixupBadFD(std_in, dummy_fd)
          || !FixupBadFD(io.std_out.get(), dummy_fd)
          || !FixupBadFD(io.std_err.get(), dummy_fd)) {
        return std::nullopt;
      }

      if (!move_fd(std_in, (int)predefined_fd::In)
          || !move_fd(io.std_out.get(), (int)predefined_fd::Out)
          || !move_fd(io.std_err.get(), (int)predefined_fd::Err)
          || !move_fd(io.grpc_parent.release(), (int)predefined_fd::GrpcOut)
          || !move_fd(io.grpc_child.release(), (int)predefined_fd::GrpcIn)
          || !move_fd(io.grpc_io.release(), (int)predefined_fd::GrpcIo)) {
        return std::nullopt;
      }

#if defined(HAVE_FCNTL_F_CLOSEM)
      // fcntl(fd, F_CLOSEM) needs the lowest filedescriptor to close.
      fcntl(7, F_CLOSEM);
#elif defined(HAVE_CLOSEFROM)
      // closefrom needs the lowest filedescriptor to close.
      closefrom(7);
#else
      for (int i = 7; i <= 32; i++) { /* close any open file descriptors */
        close(i);
      }
#endif

      // we have now setup the file descriptors

      std::string copy(program_path);

      int res = execl(copy.c_str(), copy.data(), nullptr);

      fprintf(stderr, "execl(%s, %s, nullptr) returned %d: Err=%s\n\n\n",
              copy.c_str(), copy.c_str(), res, strerror(errno));

      // exec* only returns if the new process could not be started!
      exit(99);
    } else {
      DebugLog(100, FMT_STRING("Child pid = {}"), child);

      return child;
    }
  }

  std::optional<grpc_connection> make_connection_from(grpc_connections& io)
  {
    return connection_builder{ctx, std::make_unique<BareosCore>(ctx)}
        .connect_client(std::move(io.grpc_child))
        .connect_server(std::move(io.grpc_parent))
        .build();
  }

  bool SetNonBlocking(Socket& s)
  {
    int flags = fcntl(s.get(), F_GETFL);
    if (flags == -1) {
      DebugLog(50, FMT_STRING("could not get flags for socket {}: Err={}"),
               s.get(), strerror(errno));
      return false;
    }

    if (fcntl(s.get(), F_SETFL, flags | O_NONBLOCK) < 0) {
      DebugLog(
          50,
          FMT_STRING("could not add non blocking flags to socket {}: Err={}"),
          s.get(), strerror(errno));
      return false;
    }

    return true;
  }

  std::optional<grpc_child> build(std::string_view program_path)
  {
    // We want to create a two way grpc connection, where both ends act as both
    // a server and a client.  We do this by using two socket pairs.

    DebugLog(100, FMT_STRING("creating connection to {} ..."), program_path);

    auto total_io = make_grpc_connections();

    if (!total_io) {
      DebugLog(50,
               FMT_STRING("abort creation of connection to {} as socket pairs "
                          "could not be created"),
               program_path);
      return std::nullopt;
    }

    auto& parent_io = total_io->first;
    auto& child_io = total_io->second;

    if (!SetNonBlocking(parent_io.grpc_parent)
        || !SetNonBlocking(parent_io.grpc_child)
        || !SetNonBlocking(child_io.grpc_parent)
        || !SetNonBlocking(child_io.grpc_child)) {
      return std::nullopt;
    }

    DebugLog(100, FMT_STRING("Created pipes Out: {} <> {}, Err: {} <> {}"),
             parent_io.std_out.get(), child_io.std_out.get(),
             parent_io.std_err.get(), child_io.std_err.get());
    DebugLog(
        100,
        FMT_STRING("Created socket pairs Parent: {} <> {}, Child: {} <> {}, "
                   "Io: {} <> {}"),
        parent_io.grpc_parent.get(), child_io.grpc_parent.get(),
        parent_io.grpc_child.get(), child_io.grpc_child.get(),
        parent_io.grpc_io.get(), child_io.grpc_io.get());

    set_close_on_exec(parent_io);


    joining_thread stdio_thread{do_std_io, ctx, std::move(parent_io.std_out),
                                std::move(parent_io.std_err)};

    auto child = StartDuplexGrpc(program_path, std::move(child_io));

    if (!child) {
      DebugLog(50,
               FMT_STRING("abort creation of connection to {} as program could "
                          "not get started"),
               program_path);
      return std::nullopt;
    }

    process p{*child};
    child.reset();

    auto con = make_connection_from(parent_io);

    if (!con) {
      DebugLog(50, FMT_STRING("no connection for me :("));
      return std::nullopt;
    }

    DebugLog(100, FMT_STRING("a connection for me.  Finishing setup..."));

    if (con->Setup() == bRC_Error) {
      DebugLog(100, FMT_STRING("... unsuccessfully."));
      return std::nullopt;
    }


    DebugLog(100, FMT_STRING("... successfully."));

    return grpc_child{std::move(stdio_thread), ctx, std::move(p),
                      std::move(con.value()), std::move(parent_io.grpc_io)};
  }
};

std::optional<grpc_child> make_connection(PluginContext* ctx,
                                          std::string_view program_path)
{
  return grpc_connection_builder{ctx}.build(program_path);
}

process::~process()
{
  if (pid < 0) { return; }

  kill(pid, SIGKILL);

  // wait for the child to close (for now)
  for (;;) {
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
      DebugLog(50, FMT_STRING("wait pid failed. Err={}"), strerror(errno));
      break;
    } else {
      if (WIFEXITED(status)) {
        DebugLog(100, FMT_STRING("child exit status = {}"),
                 WEXITSTATUS(status));

        break;
      } else if (WIFSIGNALED(status)) {
        DebugLog(100, FMT_STRING("child signaled with {}"), WTERMSIG(status));
        break;
      } else {
        DebugLog(100, FMT_STRING("got status = {}"), status);
      }
    }
  }
}

grpc_connection::~grpc_connection() { delete members; }
bRC grpc_connection::handlePluginEvent(filedaemon::bEventType type, void* data)
{
  PluginClient* client = &members->client;
  return client->handlePluginEvent(type, data);
}
bRC grpc_connection::startBackupFile(filedaemon::save_pkt* pkt)
{
  PluginClient* client = &members->client;
  return client->startBackupFile(pkt);
}
bRC grpc_connection::endBackupFile()
{
  PluginClient* client = &members->client;
  return client->endBackupFile();
}
bRC grpc_connection::startRestoreFile(std::string_view cmd)
{
  PluginClient* client = &members->client;
  return client->startRestoreFile(cmd);
}
bRC grpc_connection::endRestoreFile()
{
  PluginClient* client = &members->client;
  return client->endRestoreFile();
}

std::optional<int> receive_fd(int unix_socket, int expected_name)
{
  char name_buf[sizeof(
      expected_name)];  // the "name" is just the plugin fd number
                        // this is used to make sure that we actually got the
                        // correct file descriptor
  int fd;
  int name;
  struct msghdr msg = {};
  char buf[CMSG_SPACE(sizeof(fd))] = {};

  iovec io = {.iov_base = name_buf, .iov_len = sizeof(name_buf)};

  msg.msg_iov = &io;
  msg.msg_iovlen = 1;

  msg.msg_control = buf;
  msg.msg_controllen = sizeof(buf);

  auto res = recvmsg(unix_socket, &msg, MSG_WAITALL | MSG_NOSIGNAL);
  if (res < 0) {
    DebugLog(50, FMT_STRING("recvmsg failed ({}): Err={}"), res,
             strerror(errno));
    return std::nullopt;
  }

  if (res != sizeof(name_buf)) {
    DebugLog(50, FMT_STRING("short message received (len = {})"), res);
    name = -1;
  } else {
    static_assert(sizeof(name) == sizeof(name_buf));
    memcpy(&name, name_buf, sizeof(name));
    DebugLog(100, FMT_STRING("received name = {}"), name);
  }

  if (name != expected_name && expected_name != -1) {
    DebugLog(50, FMT_STRING("names do not match got = {}, expected = {}"), name,
             expected_name);
    return std::nullopt;
  } else {
    DebugLog(100, FMT_STRING("name {} matches expected {}"), name,
             expected_name);
  }

  if ((msg.msg_flags & MSG_CTRUNC) != 0) {
    DebugLog(
        50,
        FMT_STRING("some control messages were truncated! space given = {}"),
        sizeof(buf));
  }

  DebugLog(100, FMT_STRING("received msg with clen {}"), msg.msg_controllen);


  auto* control = CMSG_FIRSTHDR(&msg);

  if (!control) {
    DebugLog(50, FMT_STRING("no control msg received (len = {})"), res);
    return std::nullopt;
  }

  if (control->cmsg_len != CMSG_LEN(sizeof(fd))) {
    DebugLog(50,
             FMT_STRING("control msg is too small (len = {}, expected = {})"),
             control->cmsg_len, sizeof(fd));
    return std::nullopt;
  }

  DebugLog(100, FMT_STRING("control msg {{type = {}, level = {}}}"),
           control->cmsg_type, control->cmsg_level);
  const unsigned char* data = CMSG_DATA(control);

  // size checked above
  memcpy(&fd, data, sizeof(fd));

  DebugLog(100, FMT_STRING("received control message {}"), fd);

  if (auto* c2 = CMSG_NXTHDR(&msg, control); c2) {
    DebugLog(50, FMT_STRING("encountered second control message {}!"),
             (void*)c2);
  }

  if (fcntl(fd, F_GETFD) < 0) {
    DebugLog(50, FMT_STRING("{} is not an fd"), fd);
    return std::nullopt;
  } else {
    DebugLog(100, FMT_STRING("{} is an fd"), fd);
    return std::make_optional(fd);
  }
}

bool send_fd(int unix_socket, int fd)
{
  struct msghdr msg = {};
  char buf[CMSG_SPACE(sizeof(fd))] = {};
  char name_buf[sizeof(fd)];
  memcpy(name_buf, &fd, sizeof(fd));
  iovec io = {.iov_base = name_buf, .iov_len = sizeof(name_buf)};


  msg.msg_iov = &io;
  msg.msg_iovlen = 1;
  msg.msg_control = buf;
  msg.msg_controllen = sizeof(buf);

  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(fd));

  memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));

  msg.msg_controllen = CMSG_SPACE(sizeof(fd));

  if (auto res = sendmsg(unix_socket, &msg, MSG_NOSIGNAL); res < 0) {
    DebugLog(50, FMT_STRING("could not send fd {}. Err={}"), fd,
             strerror(errno));
  }

  return false;
}

static bool full_read(int fd, char* data, size_t size)
{
  size_t bytes_read = 0;
  while (bytes_read < size) {
    auto res = read(fd, data + bytes_read, size - bytes_read);
    DebugLog(100, FMT_STRING("read(fd = {}, buffer = {}, count = {}) -> {}"),
             fd, (void*)(data + bytes_read), size - bytes_read, res);
    if (res < 0) { return false; }
    bytes_read += res;
  }
  return true;
}

bRC grpc_connection::pluginIO(filedaemon::io_pkt* pkt, int iosock)
{
  PluginClient* client = &members->client;

  switch (pkt->func) {
    case filedaemon::IO_OPEN: {
      bool io_in_core;
      auto res
          = client->FileOpen(pkt->fname, pkt->flags, pkt->mode, &io_in_core);
      if (res != bRC_OK) {
        pkt->io_errno = EIO;
        return res;
      }
      if (io_in_core) {
        DebugLog(100, FMT_STRING("using io_in_core"));
        std::optional fd = receive_fd(iosock, -1);

        if (!fd) {
          JobLog(nullptr, M_FATAL,
                 FMT_STRING("plugin wanted to do io_in_core, but did not send "
                            "an fd: Err={}"),
                 strerror(errno));

          pkt->io_errno = EIO;

          return bRC_Error;
        }
        pkt->filedes = *fd;
        pkt->status = IoStatus::do_io_in_core;
        do_io_in_core = true;
      } else {
        DebugLog(100, FMT_STRING("not using io_in_core"));
        pkt->filedes = kInvalidFiledescriptor;
        pkt->status = !IoStatus::do_io_in_core;
      }
      return res;
    } break;
    case filedaemon::IO_READ: {
      auto iter = client->FileRead(pkt->count);

      DebugLog(100, FMT_STRING("trying to read {} bytes"), pkt->count);

      size_t bytes_read = 0;
      size_t current = 0;
      while (iter.next(&current)) {
        DebugLog(100, FMT_STRING("received {} bytes"), current);
        if (!full_read(iosock, pkt->buf + bytes_read, current)) {
          JobLog(nullptr, M_FATAL,
                 FMT_STRING("could not read additional {} bytes from socket "
                            "({} were already read): Err={}"),
                 current, bytes_read, strerror(errno));
          pkt->io_errno = errno;
          return bRC_Error;
        }
        DebugLog(100, FMT_STRING("read {} bytes successfully"), current);
        bytes_read += current;
      }

      auto res = iter.result();

      if (res == bRC_Error) { return res; }

      if ((ssize_t)bytes_read > pkt->count) {
        JobLog(nullptr, M_FATAL,
               FMT_STRING(
                   "plugin wrote to many bytes (wanted = {}, received = {})"),
               pkt->count, bytes_read);
        return bRC_Error;
      }

      pkt->status = bytes_read;
      return bRC_OK;
    } break;
    case filedaemon::IO_WRITE: {
      DebugLog(100, FMT_STRING("writing {} bytes into socket"), pkt->count);


      struct aiocb control = {};
      control.aio_fildes = iosock;
      control.aio_buf = pkt->buf;
      control.aio_nbytes = pkt->count;

      if (aio_write(&control) < 0) {
        JobLog(nullptr, M_FATAL,
               FMT_STRING("could not request async io: Err={}"),
               strerror(errno));
      }

      size_t bytes_written = 0;
      auto res = client->FileWrite(pkt->count, &bytes_written);
      if (res == bRC_Error) {
        aio_cancel(control.aio_fildes, &control);
        return res;
      }
      pkt->status = bytes_written;
      DebugLog(100, FMT_STRING("{} bytes were read from socket"), pkt->status);

      struct aiocb* arr[] = {&control};

      for (;;) {
        auto aio_res = aio_error(&control);
        if (aio_res == EINPROGRESS) {
          if (aio_suspend(arr, 1, NULL) < 0) {
            JobLog(nullptr, M_FATAL,
                   FMT_STRING("could not wait for async io: Err={}"),
                   strerror(errno));
          }
        } else {
          if (aio_res < 0) {
            JobLog(nullptr, M_FATAL, FMT_STRING("async io call error: {}"),
                   -aio_res);
            pkt->status = -1;
            return bRC_Error;
          } else if (aio_res > 0) {
            JobLog(nullptr, M_FATAL, FMT_STRING("async io error: Err={}"),
                   strerror(aio_res));

            errno = aio_res;
            pkt->status = -1;
            return bRC_Error;
          }
          break;
        }
      }

      int num_bytes = aio_return(&control);

      if (num_bytes != (int)bytes_written) {
        JobLog(
            nullptr, M_FATAL,
            FMT_STRING(
                "could not write file data (written = {}, wanted = {}) Err={}"),
            num_bytes, pkt->count, strerror(errno));
      }

      return res;
    } break;
    case filedaemon::IO_CLOSE: {
      if (do_io_in_core) {
        // why is the plugin not told if it used io in core or not ?!
        DebugLog(100, FMT_STRING("closing core fd {}"), pkt->filedes);
        close(pkt->filedes);
        do_io_in_core = false;
      }
      return client->FileClose();
    } break;
    case filedaemon::IO_SEEK: {
      return client->FileSeek(pkt->whence, pkt->offset);
    } break;
    default: {
      return bRC_Error;
    } break;
  }
}
bRC grpc_connection::createFile(filedaemon::restore_pkt* pkt)
{
  PluginClient* client = &members->client;
  return client->createFile(pkt);
}
bRC grpc_connection::setFileAttributes(filedaemon::restore_pkt* pkt)
{
  PluginClient* client = &members->client;

  bool do_in_core = false;

  auto res = client->setFileAttributes(pkt->ofname, pkt->statp, pkt->attrEx,
                                       pkt->where, &do_in_core);
  if (res != bRC_Error && do_in_core) { pkt->create_status = CF_CORE; }

  return res;
}
bRC grpc_connection::Setup()
{
  PluginClient* client = &members->client;
  return client->Setup();
}
bRC grpc_connection::checkFile(const char* fname)
{
  PluginClient* client = &members->client;
  return client->checkFile(fname);
}
bRC grpc_connection::getAcl(filedaemon::acl_pkt* pkt)
{
  PluginClient* client = &members->client;
  size_t size = 0;
  bRC result = client->getAcl(pkt->fname, &pkt->content, &size);
  pkt->content_length = size;
  return result;
}
bRC grpc_connection::setAcl(filedaemon::acl_pkt* pkt)
{
  PluginClient* client = &members->client;
  return client->setAcl(pkt->fname,
                        std::string_view{pkt->content, pkt->content_length});
}
bRC grpc_connection::getXattr(filedaemon::xattr_pkt* pkt)
{
  PluginClient* client = &members->client;
  size_t name_len = 0, value_len = 0;

  bRC result = client->getXattr(pkt->fname, &pkt->name, &name_len, &pkt->value,
                                &value_len);

  pkt->name_length = name_len;
  pkt->value_length = value_len;

  return result;
}
bRC grpc_connection::setXattr(filedaemon::xattr_pkt* pkt)
{
  PluginClient* client = &members->client;

  return client->setXattr(pkt->fname,
                          std::string_view{pkt->name, pkt->name_length},
                          std::string_view{pkt->value, pkt->value_length});
}
