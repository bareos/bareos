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

#include "common.pb.h"
#include "events.pb.h"
#include "plugin.pb.h"
#include "bareos.pb.h"
#include "backup.pb.h"
#include "restore.pb.h"

#include "include/baconfig.h"
#include "filed/fd_plugins.h"
#include "prototools.h"
#include "util.h"

#include "plugins/filed/grpc/grpc_impl.h"
#include <fcntl.h>
#include <future>
#include <thread>
#include <sys/poll.h>
#include <sys/wait.h>

#include <aio.h>

#include <chrono>

#include "bareos_api.h"

#include "include/filetypes.h"

namespace {
namespace bp = bareos::plugin;
namespace bb = bareos::backup;
namespace br = bareos::restore;

namespace bc = bareos::core;
namespace bco = bareos::common;

[[maybe_unused]] std::string_view strip_prefix(std::string_view v)
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
  enum
  {
    OUTPUT_DEBUG,
    OUTPUT_JOB_ERROR,
  } output;

  // this buffer is used for line buffering
  // ie we read into this buffer until we hit an \n,
  // then we output that line, and continue
  std::array<char, 4096> buffer{};
  size_t buffer_free{buffer.size()};

  void reset_buffer() { buffer_free = buffer.size(); }

  void remove_printed_bytes(size_t byte_count)
  {
    ASSERT(byte_count <= read_size());
    // this is the size of the unprinted leftovers in the buffer
    auto leftover_size = read_size() - byte_count;

    // we printed something, so move everything back to create space
    memmove(buffer.data() + byte_count, buffer.data(), leftover_size);

    buffer_free += byte_count;
  }

  void advance_write_head(size_t byte_count)
  {
    ASSERT(buffer_free >= byte_count);
    buffer_free -= byte_count;
  }

  char* write_start() { return buffer.data() + (buffer.size() - buffer_free); }

  size_t write_size() const noexcept { return buffer_free; }

  const char* read_start() const noexcept { return buffer.data(); }

  size_t read_size() const noexcept { return buffer.size() - buffer_free; }

  void print_msg(PluginContext* ctx, const char* msg) const noexcept
  {
    // TODO: fix this, outputting to a jobmessage is _not_
    //       thread safe
    //       We should instead buffer them and have the "main" thread
    //       regularly flush them.  Sadly plugins do not have access
    //       to Qmsg

    switch (output) {
      case flush_fd::OUTPUT_DEBUG: {
        DebugLog(ctx, 100, msg);
      } break;
      case flush_fd::OUTPUT_JOB_ERROR: {
        // JobLog(ctx, M_ERROR, msg);
        DebugLog(ctx, 100, msg);
      } break;
    }
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
      flush_fd{err.get(), "stderr", flush_fd::OUTPUT_DEBUG},
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

    std::array pfds
        = {pollfd{fds[0].fd, POLLIN, 0}, pollfd{fds[1].fd, POLLIN, 0}};

    auto num_fired = poll(pfds.data(), fd_count, 500);
    if (num_fired < 0) {
      DebugLog(ctx, 50, FMT_STRING("io thread poll returned {}: Err={}"),
               num_fired, strerror(errno));
      break;
    }

    if (num_fired == 0) {
      DebugLog(ctx, 500, FMT_STRING("io thread - no news"));
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

        ssize_t bytes_read
            = read(fds[i].fd, fds[i].write_start(), fds[i].write_size());

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

            fds[i].print_msg(
                ctx,
                fmt::format(FMT_STRING("{}: {}"), fds[i].name, line).c_str());
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

            std::string_view content{fds[i].read_start(), fds[i].read_size()};


            fds[i].print_msg(ctx, fmt::format(FMT_STRING("{} (full): {}"),
                                              fds[i].name, content)
                                      .c_str());
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
      auto content = std::string_view{buf.read_start(), buf.read_size()};
      buf.print_msg(
          ctx,
          fmt::format(FMT_STRING("{} (dump): {}"), buf.name, content).c_str());
    }
  }
}

enum StatusCode
{
  ALL_OK,
  INVALID_ARGUMENT,
  INTERNAL,
};

struct Status {
  Status(StatusCode, std::string) {}
  bool ok() const { return true; }
  Status() {};

  static Status OK;
};

namespace {
constexpr std::string_view backup_step_str(bb::Backup::StepCase step)
{
  switch (step) {
    case bareos::backup::Backup::kBeginObject:
      return "begin-object";
    case bareos::backup::Backup::kFileData:
      return "file-data";
    case bareos::backup::Backup::kAcl:
      return "acl";
    case bareos::backup::Backup::kXattr:
      return "xattr";
    case bareos::backup::Backup::kFileError:
      return "file-error";
    case bareos::backup::Backup::kDone:
      return "done";
    case bareos::backup::Backup::STEP_NOT_SET:
      return "unset";
  }

  return "internal-error";
}
};  // namespace


struct BackupStateMachine {
 public:
  BackupStateMachine(PluginContext* ctx, prototools::ProtoBidiStream* stream_)
      : stream{stream_}, core{ctx}
  {
    arena = std::make_unique<google::protobuf::Arena>();
  }

  bool done() const { return is_done; }

  const bb::BeginObject* start_backup_file()
  {
    DebugLog(150, "start_backup_file()");

    bb::Backup* data = pop();
    if (data == nullptr) { return nullptr; }


    if (data->has_begin_object()) { return &data->begin_object(); }

    return nullptr;
  }

  bool end_backup_file()
  {
    DebugLog(150, "end_backup_file()");
    for (;;) {
      bb::Backup* data = pop();

      if (!data) { return false; }

      if (data->has_done()) {
        is_done = true;
        return true;
      }

      if (data->has_begin_object()) {
        push(data);
        return true;
      }
    }
  }

  bool file_open()
  {
    DebugLog(150, "file_open()");
    bb::Backup* data = pop();

    if (data == nullptr) { return false; }

    // here we only care to check if there was an error opening the file

    if (data->has_file_error()
        && data->file_error().on_action()
               == bco::FileAction::FILE_ACTION_OPEN) {
      DebugLog(50, "could not open file: Err={}", data->file_error().message());
      return false;
    }

    // we didnt care about this event, so push it back
    push(data);
    return true;
  }

  std::optional<std::size_t> file_read(std::span<char> buffer)
  {
    DebugLog(150, "file_read()");
    bb::Backup* data = pop();

    if (data == nullptr) { return std::nullopt; }

    if (data->has_file_error()
        && data->file_error().on_action()
               == bco::FileAction::FILE_ACTION_READ) {
      DebugLog(50, "could not read file: Err={}", data->file_error().message());
      return std::nullopt;
    }

    if (data->has_file_data()) {
      const auto& fdata = data->file_data().data();
      if (buffer.size() < fdata.size()) {
        JobLog(
            core, M_FATAL,
            "Communication error: received {} bytes, but expected at most {}",
            fdata.size(), buffer.size());

        return std::nullopt;
      }

      std::memcpy(buffer.data(), fdata.data(), fdata.size());

      return fdata.size();
    }

    // we got something we didnt care about, so the file is done
    // just return 0 bytes, to tell the daemon that we are done

    DebugLog(100,
             "expected file_data but got something else.  Returning 0 bytes");

    push(data);

    return 0;
  }

  bool file_close()
  {
    DebugLog(150, "file_close()");
    bb::Backup* data = pop();
    if (data == nullptr) { return false; }

    if (data->has_file_error()
        && data->file_error().on_action()
               == bco::FileAction::FILE_ACTION_CLOSE) {
      DebugLog(50, "could not close file: Err={}",
               data->file_error().message());
      return false;
    }

    push(data);

    return true;
  }

  bool get_acl(filedaemon::acl_pkt* acl)
  {
    DebugLog(150, "get_acl()");
    bb::Backup* data = pop();
    if (data == nullptr) { return false; }

    if (data->has_file_error()
        && data->file_error().on_action() == bco::FileAction::FILE_ACTION_ACL) {
      DebugLog(50, "could get acls from file: Err={}",
               data->file_error().message());
      return false;
    }

    if (data->has_acl()) {
      const auto& adata = data->acl().acl_data();

      acl->content = reinterpret_cast<char*>(malloc(adata.size() + 1));
      std::memcpy(acl->content, adata.data(), adata.size() + 1);
      acl->content_length = adata.size();

      return true;
    }

    push(data);
    acl->content = nullptr;
    acl->content_length = 0;
    return true;
  }

  bool get_xattr(filedaemon::xattr_pkt* xattr)
  {
    DebugLog(150, "get_xattr()");
    bb::Backup* data = pop();
    if (data == nullptr) { return false; }

    if (data->has_file_error()
        && data->file_error().on_action()
               == bco::FileAction::FILE_ACTION_XATTR) {
      DebugLog(50, "could get acls from file: Err={}",
               data->file_error().message());
      return false;
    }

    if (data->has_xattr()) {
      const auto& x = data->xattr();
      const auto& xvalue = x.content();
      const auto& xname = x.name();

      xattr->value = reinterpret_cast<char*>(malloc(xvalue.size() + 1));
      std::memcpy(xattr->value, xvalue.data(), xvalue.size() + 1);
      xattr->value_length = xvalue.size();

      xattr->name = reinterpret_cast<char*>(malloc(xname.size() + 1));
      std::memcpy(xattr->name, xname.data(), xname.size() + 1);
      xattr->name_length = xname.size();

      return true;
    }

    push(data);
    xattr->value = nullptr;
    xattr->value_length = 0;

    xattr->name = nullptr;
    xattr->name_length = 0;
    return true;
  }

 private:
  bb::Backup* pop()
  {
    if (last_read) {
      auto* ptr = std::exchange(last_read, nullptr);
      return ptr;
    }

    arena->Reset();
    auto* data = google::protobuf::Arena::Create<bb::Backup>(arena.get());

    if (!stream->Read(*data)) {
      JobLog(core, M_FATAL, "internal communication error");
      return nullptr;
    }

    DebugLog(200, "received object of type {}",
             backup_step_str(data->step_case()));

    return data;
  }

  void push(bb::Backup* read)
  {
    ASSERT(last_read == nullptr);
    last_read = read;
  }

 private:
  prototools::ProtoBidiStream* stream{nullptr};
  PluginContext* core;
  std::unique_ptr<google::protobuf::Arena> arena{};
  bb::Backup* last_read{};
  bool is_done{false};
};

class BareosCore {
 public:
  BareosCore(PluginContext* ctx,
             std::unique_ptr<prototools::ProtoBidiStream> stream)
      : core{ctx}
      , connection{std::move(stream)}
      , thrd{std::jthread(BareosCore::do_answer_calls,
                          connection.get(),
                          core,
                          this)}
  {
  }

  BareosCore& operator=(const BareosCore&) = delete;
  BareosCore(const BareosCore&) = delete;
  BareosCore& operator=(BareosCore&&) = delete;
  BareosCore(BareosCore&&) = delete;

  ~BareosCore()
  {
    if (connection) {
      DebugLog(core, 100, "server = {{ writes = {}, reads = {}}}",
               connection->write_count, connection->read_count);
    }
  }

  static void do_answer_calls(prototools::ProtoBidiStream* stream,
                              PluginContext* ctx,
                              BareosCore* core)
  {
    DebugLog(ctx, 100, "starting answering requests");
    google::protobuf::Arena arena;

    for (;;) {
      auto* req = google::protobuf::Arena::Create<bc::CoreRequest>(&arena);

      if (!stream->Read(*req)) {
        DebugLog(ctx, 100, "error during reading => stop listening");
        break;
      }
      auto* resp = google::protobuf::Arena::Create<bc::CoreResponse>(&arena);
      switch (core->HandleRequest(req, resp)) {
        case CallResult::Response: {
          stream->Write(*resp);
        } break;
        case CallResult::NoResponse: {
          // nothing to do
        } break;
        case CallResult::Error: {
          // sent some kind of error message ?
          assert(0);
        } break;
      }

      arena.Reset();
    }

    // TODO: the socket should get killed here
  }

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
      case bc::EVENT_TYPE_UNSPECIFIED:
      case bc::EventType_INT_MIN_SENTINEL_DO_NOT_USE_:
      case bc::EventType_INT_MAX_SENTINEL_DO_NOT_USE_:
        break;
    }
    return std::nullopt;
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
      case bco::UnchangedFile:
        return FT_NOCHG;
      case bco::UnchangedDirectory:
        return FT_DIRNOCHG;
      case bco::RestoreObject:
        return FT_RESTORE_FIRST;

      case bco::FILE_TYPE_UNSPECIFIED:
      case bco::FileType_INT_MIN_SENTINEL_DO_NOT_USE_:
      case bco::FileType_INT_MAX_SENTINEL_DO_NOT_USE_:
        break;
    }
    return std::nullopt;
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
      case bareos::core::BAREOS_STRING_VARIABLE_UNSPECIFIED:
      case bareos::core::BareosStringVariable_INT_MIN_SENTINEL_DO_NOT_USE_:
      case bareos::core::BareosStringVariable_INT_MAX_SENTINEL_DO_NOT_USE_:
        break;
    }
    return std::nullopt;
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
      case bareos::core::BAREOS_INT_VARIABLE_UNSPECIFIED:
      case bareos::core::BareosIntVariable_INT_MIN_SENTINEL_DO_NOT_USE_:
      case bareos::core::BareosIntVariable_INT_MAX_SENTINEL_DO_NOT_USE_:
        break;
    }
    return std::nullopt;
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
  enum class CallResult
  {
    Response,
    NoResponse,
    Error,
  };


  CallResult HandleRequest(const bc::CoreRequest* outer_req,
                           bc::CoreResponse* outer_resp)
  {
    switch (outer_req->request_case()) {
      case bareos::core::CoreRequest::kRegister: {
        auto status = this->Events_Register(&outer_req->register_(),
                                            outer_resp->mutable_register_());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      case bareos::core::CoreRequest::kUnregister: {
        auto status = this->Events_Register(&outer_req->register_(),
                                            outer_resp->mutable_register_());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      case bareos::core::CoreRequest::kGetInstanceCount: {
        auto status = this->Bareos_getInstanceCount(
            &outer_req->getinstancecount(),
            outer_resp->mutable_getinstancecount());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      case bareos::core::CoreRequest::kCheckChanges: {
        auto status = this->Bareos_checkChanges(
            &outer_req->checkchanges(), outer_resp->mutable_checkchanges());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      case bareos::core::CoreRequest::kAcceptFile: {
        auto status = this->Bareos_AcceptFile(&outer_req->acceptfile(),
                                              outer_resp->mutable_acceptfile());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      case bareos::core::CoreRequest::kSetSeen: {
        auto status = this->Bareos_SetSeen(&outer_req->setseen(),
                                           outer_resp->mutable_setseen());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      case bareos::core::CoreRequest::kClearSeen: {
        auto status = this->Bareos_ClearSeen(&outer_req->clearseen(),
                                             outer_resp->mutable_clearseen());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      case bareos::core::CoreRequest::kJobMessage: {
        auto status = this->Bareos_JobMessage(&outer_req->jobmessage(),
                                              outer_resp->mutable_jobmessage());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      case bareos::core::CoreRequest::kDebugMessage: {
        auto status = this->Bareos_DebugMessage(
            &outer_req->debugmessage(), outer_resp->mutable_debugmessage());
        return status.ok() ? CallResult::NoResponse : CallResult::Error;
      }
      case bareos::core::CoreRequest::kSetString: {
        auto status = this->Bareos_SetString(&outer_req->setstring(),
                                             outer_resp->mutable_setstring());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      case bareos::core::CoreRequest::kGetString: {
        auto status = this->Bareos_GetString(&outer_req->getstring(),
                                             outer_resp->mutable_getstring());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      case bareos::core::CoreRequest::kSetInt: {
        auto status = this->Bareos_SetInt(&outer_req->setint(),
                                          outer_resp->mutable_setint());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      case bareos::core::CoreRequest::kGetInt: {
        auto status = this->Bareos_GetInt(&outer_req->getint(),
                                          outer_resp->mutable_getint());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      case bareos::core::CoreRequest::kSetFlag: {
        auto status = this->Bareos_SetFlag(&outer_req->setflag(),
                                           outer_resp->mutable_setflag());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      case bareos::core::CoreRequest::kGetFlag: {
        auto status = this->Bareos_GetFlag(&outer_req->getflag(),
                                           outer_resp->mutable_getflag());
        return status.ok() ? CallResult::Response : CallResult::Error;
      } break;
      default: {
        // return grpc::Status(grpc::StatusCode::UNIMPLEMENTED,
        //                     "unknown core request");
        return CallResult::Error;
      } break;
    }
  }

  Status Events_Register(const bc::RegisterRequest* req, bc::RegisterResponse*)
  {
    for (auto event : req->event_types()) {
      if (!bc::EventType_IsValid(event)) {
        return Status(
            StatusCode::INVALID_ARGUMENT,
            fmt::format(FMT_STRING("event {} is not a valid bareos event"),
                        event));
      }
    }

    for (auto event : req->event_types()) {
      // for some reason event is an int ??
      std::optional bareos_type = from_grpc(static_cast<bc::EventType>(event));
      if (!bareos_type) {
        return Status(
            StatusCode::INTERNAL,
            fmt::format(
                FMT_STRING("could not convert valid event {} to bareos event"),
                event));
      }

      RegisterBareosEvent(core, *bareos_type);
    }

    return Status::OK;
  }

  Status Events_Unregister(const bc::UnregisterRequest* req,
                           bc::UnregisterResponse*)
  {
    for (auto event : req->event_types()) {
      if (!bc::EventType_IsValid(event)) {
        return Status(
            StatusCode::INVALID_ARGUMENT,
            fmt::format(FMT_STRING("event {} is not a valid bareos event"),
                        event));
      }
    }

    for (auto event : req->event_types()) {
      // for some reason event is an int ??
      std::optional bareos_type = from_grpc(static_cast<bc::EventType>(event));
      if (!bareos_type) {
        return Status(
            StatusCode::INTERNAL,
            fmt::format(
                FMT_STRING("could not convert valid event {} to bareos event"),
                event));
      }

      UnregisterBareosEvent(core, *bareos_type);
    }

    return Status::OK;
  }

  // Status Fileset_AddExclude( const
  // bc::AddExcludeRequest* request, bc::AddExcludeResponse* response)
  // {
  // }
  // Status Fileset_AddInclude( const
  // bc::AddIncludeRequest* request, bc::AddIncludeResponse* response)
  // {
  // }
  // Status Fileset_AddOptions( const
  // bc::AddOptionsRequest* request, bc::AddOptionsResponse* response)
  // {
  // }
  // Status Fileset_AddRegex( const
  // bc::AddRegexRequest* request, bc::AddRegexResponse* response) {
  // }
  // Status Fileset_AddWild( const
  // bc::AddWildRequest* request, bc::AddWildResponse* response) {
  // }
  // Status Fileset_NewOptions( const
  // bc::NewOptionsRequest* request, bc::NewOptionsResponse* response)
  // {
  // }
  // Status Fileset_NewInclude( const
  // bc::NewIncludeRequest* request, bc::NewIncludeResponse* response)
  // {
  // }
  // Status Fileset_NewPreInclude( const
  // bc::NewPreIncludeRequest* request, bc::NewPreIncludeResponse* response)
  // {
  // }
  Status Bareos_getInstanceCount(const bc::getInstanceCountRequest*,
                                 bc::getInstanceCountResponse* response)
  {
    // there is only one instance per process.  Its also pretty easy to give a
    // real answer here (i guess).
    response->set_instance_count(1);
    return Status::OK;
  }

  Status Bareos_checkChanges(const bc::checkChangesRequest* request,
                             bc::checkChangesResponse* response)
  {
    auto type = request->type();

    auto bareos_type = from_grpc(type);

    if (!bareos_type) {
      return Status(StatusCode::INVALID_ARGUMENT,
                    fmt::format(FMT_STRING("could not parse {} as bareos type"),
                                int(type)));
    }

    auto& stats = request->stats();
    struct stat statp;

    if (stats.size() != sizeof(statp)) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
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

    return Status::OK;
  }

  Status Bareos_SetString(const bc::SetStringRequest* request,
                          bc::SetStringResponse*)
  {
    auto var = request->var();

    std::optional bareos_var = from_grpc(var);

    if (!bareos_var) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("unknown string variable {}"), int(var)));
    }

    if (!SetBareosValue(core, *bareos_var,
                        const_cast<char*>(request->value().c_str()))) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("set not supported for {}"), int(var)));
    }

    return Status::OK;
  }

  Status Bareos_GetString(const bc::GetStringRequest* request,
                          bc::GetStringResponse* response)
  {
    auto var = request->var();

    std::optional bareos_var = from_grpc(var);

    if (!bareos_var) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("unknown string variable {}"), int(var)));
    }

    const char* str = nullptr;

    if (!GetBareosValue(core, *bareos_var, &str)) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("get not supported for {}"), int(var)));
    }

    if (str == nullptr) {
      return Status(StatusCode::INTERNAL, "nullptr returned by core");
    }

    response->set_value(str);

    return Status::OK;
  }

  Status Bareos_SetInt(const bc::SetIntRequest* request, bc::SetIntResponse*)
  {
    auto var = request->var();

    std::optional bareos_var = from_grpc(var);

    if (!bareos_var) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("unknown string variable {}"), int(var)));
    }

    int val = request->value();
    if (!SetBareosValue(core, *bareos_var, &val)) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("set not supported for {}"), int(var)));
    }

    return Status::OK;
  }

  Status Bareos_GetInt(const bc::GetIntRequest* request,
                       bc::GetIntResponse* response)
  {
    auto var = request->var();

    std::optional bareos_var = from_grpc(var);

    if (!bareos_var) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("unknown string variable {}"), int(var)));
    }

    int value{0};

    if (!GetBareosValue(core, *bareos_var, &value)) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("get not supported for {}"), int(var)));
    }

    response->set_value(value);

    return Status::OK;
  }

  Status Bareos_SetFlag(const bc::SetFlagRequest* request, bc::SetFlagResponse*)
  {
    auto var = request->var();

    std::optional bareos_var = from_grpc(var);

    if (!bareos_var) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("unknown string variable {}"), int(var)));
    }

    bool val = request->value();
    if (!SetBareosValue(core, *bareos_var, &val)) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("set not supported for {}"), int(var)));
    }

    return Status::OK;
  }

  Status Bareos_GetFlag(const bc::GetFlagRequest* request,
                        bc::GetFlagResponse* response)
  {
    auto var = request->var();

    std::optional bareos_var = from_grpc(var);

    if (!bareos_var) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("unknown string variable {}"), int(var)));
    }

    bool value{false};

    if (!GetBareosValue(core, *bareos_var, &value)) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
          fmt::format(FMT_STRING("get not supported for {}"), int(var)));
    }

    response->set_value(value);

    return Status::OK;
  }

  Status Bareos_AcceptFile(const bc::AcceptFileRequest* request,
                           bc::AcceptFileResponse* response)
  {
    auto& stats = request->stats();
    struct stat statp;

    if (stats.size() != sizeof(statp)) {
      return Status(
          StatusCode::INVALID_ARGUMENT,
          fmt::format(
              FMT_STRING(
                  "stats is not a valid stats object: size mismatch {} != {}"),
              stats.size(), sizeof(statp)));
    }

    memcpy(&statp, stats.data(), stats.size());

    auto result = AcceptFile(core, request->file(), statp);

    response->set_skip(!result);

    return Status::OK;
  }
  Status Bareos_SetSeen(const bc::SetSeenRequest* request, bc::SetSeenResponse*)
  {
    auto result = [&] {
      if (request->has_file()) {
        return SetSeenBitmap(core, false, request->file().c_str());
      } else {
        return SetSeenBitmap(core, true, nullptr);
      }
    }();

    if (result == bRC_Error) {
      return Status(StatusCode::INTERNAL, "something went wrong!");
    }
    return Status::OK;
  }
  Status Bareos_ClearSeen(const bc::ClearSeenRequest* request,
                          bc::ClearSeenResponse*)
  {
    auto result = [&] {
      if (request->has_file()) {
        return ClearSeenBitmap(core, false, request->file().c_str());
      } else {
        return ClearSeenBitmap(core, true, nullptr);
      }
    }();

    if (result == bRC_Error) {
      return Status(StatusCode::INTERNAL, "something went wrong!");
    }
    return Status::OK;
  }

  Status Bareos_JobMessage(const bc::JobMessageRequest* req,
                           bc::JobMessageResponse*)
  {
    JobLog(core, Type{req->type(), req->file().c_str(), (int)req->line()},
           FMT_STRING("{}"), req->msg());

    return Status::OK;
  }

  Status Bareos_DebugMessage(const bc::DebugMessageRequest* req,
                             bc::DebugMessageResponse*)
  {
    DebugLog(core,
             Severity{(int)req->level(), req->file().c_str(), (int)req->line()},
             req->msg().c_str());

    return Status::OK;
  }

 private:
  PluginContext* core{nullptr};
  std::unique_ptr<prototools::ProtoBidiStream> connection;
  std::jthread thrd;
};

bool make_event_request(filedaemon::bEventType type,
                        void* data,
                        bp::HandlePluginEventRequest& req)
{
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

        // rop->plugin_name is already the "sanitized" name, i.e.
        //  the name without grpc:<module-name>:

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

    default:
      return false;
  }
  return true;
}

class PluginClient {
 public:
  PluginClient(
      PluginContext* ctx,
      std::unique_ptr<prototools::ProtoBidiStream> core_event_stream,
      std::unique_ptr<prototools::ProtoBidiStream> data_transfer_stream)
      : data_transfer{std::move(data_transfer_stream)}
      , core_events{std::move(core_event_stream)}
      , core{ctx}
  {
    arena = std::make_unique<google::protobuf::Arena>();
  }

  std::unique_ptr<google::protobuf::Arena> arena;

  std::optional<bp::PluginResponse> submit_event(const bp::PluginRequest& req)
  {
    bp::PluginResponse resp;
    DebugLog(200, "submitting event");
    if (!core_events->Write(req)) { return std::nullopt; }
    DebugLog(200, "waiting for response event");
    if (!core_events->Read(resp)) { return std::nullopt; }
    DebugLog(200, "got a response event");
    return resp;
  }

  bRC Setup()
  {
    bp::PluginRequest req;

    (void)req.mutable_setup();

    auto resp = submit_event(req);
    if (!resp) { return bRC_Error; }

    if (!resp->has_setup()) { return bRC_Error; }

    return bRC_OK;
  }

  bRC handlePluginEvent(filedaemon::bEventType type,
                        bp::HandlePluginEventRequest* req)
  {
    bp::PluginRequest actual_req;
    *actual_req.mutable_handle_plugin_event() = std::move(*req);
    auto* creq = &actual_req.handle_plugin_event();

    auto resp = submit_event(actual_req);

    if (!resp || !resp->has_handle_plugin_event()) { return bRC_Error; }

    auto res = resp->handle_plugin_event().res();

    DebugLog(100, FMT_STRING("plugin handled event {} with res = {} ({})"),
             int(type), bp::ReturnCode_Name(res), int(res));


    if (creq->to_handle().has_backup_command() && res == bp::RC_OK) {
      auto& backup_command = creq->to_handle().backup_command();

      bp::PluginRequest sb_req;
      auto* begin_backup = sb_req.mutable_begin_backup();
      begin_backup->mutable_cmd()->assign(backup_command.data());
      begin_backup->set_backup_acl(false);
      begin_backup->set_backup_xattr(false);
      begin_backup->set_portable(true);
      begin_backup->set_no_atime(true);
      begin_backup->set_max_record_size(256 << 10);

      auto sb_resp = submit_event(sb_req);
      if (!sb_resp || !sb_resp->has_begin_backup()) {
        DebugLog(100, "expected begin-backup reponse, but got something else");
        return bRC_Error;
      }

      state_machine.emplace<BackupStateMachine>(core, data_transfer.get());
    }

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

  bRC handlePluginEvent(filedaemon::bEventType type, void* data)
  {
    bp::HandlePluginEventRequest req;

    switch (type) {
      case filedaemon::bEventHandleBackupFile: {
        DebugLog(50, "handle backup file not supported yet");
        return bRC_Error;
      } break;
      default: {
      } break;
    }

    make_event_request(type, data, req);

    return handlePluginEvent(type, &req);
  }


  struct cached_start_backup_file {
    std::optional<int> io_in_core_fd{};
  };

  std::optional<cached_start_backup_file> start_backup_file{};

  bRC startBackupFile(filedaemon::save_pkt* pkt)
  {
    auto* bsm = std::get_if<BackupStateMachine>(&state_machine);

    if (!bsm) {
      JobLog(core, M_FATAL,
             "cannot begin backing up file, if the backup was not formally "
             "started yet");
      return bRC_Error;
    }

    const bb::BeginObject* begin_obj = bsm->start_backup_file();

    if (!begin_obj) { return bRC_Error; }

    if (begin_obj->has_file()) {
      auto& file = begin_obj->file();

      DebugLog(100, FMT_STRING("received a file"));

      std::optional ft = BareosCore::from_grpc(file.ft());
      if (!ft) {
        DebugLog(50, FMT_STRING("could not convert filetype {} ({})"),
                 bco::FileType_Name(file.ft()), int(file.ft()));
        JobLog(core, M_FATAL, "fatal internal error occured");
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
        case bco::UnchangedFile:
          [[fallthrough]];
        case bco::RegularFile: {
          pkt->fname = strdup(file.file().c_str());
          pkt->link = nullptr;
        } break;
        case bco::UnchangedDirectory:
          [[fallthrough]];
        case bco::Directory: {
          std::string name = file.file();
          while (name.size() > 1 && name.back() == '/') { name.pop_back(); }

          // for directories:
          // pkt->fname _should not_ end in a '/'
          // pkt->link _should_ end in a '/'
          pkt->fname = strdup(name.c_str());

          if (name.back() != '/') { name += '/'; }

          pkt->link = strdup(name.c_str());
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
      stat_grpc_to_native(&pkt->statp, file.stats());
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
    } else if (begin_obj->has_object()) {
      DebugLog(100, FMT_STRING("received an object"));
      auto& object = begin_obj->object();
      pkt->type = FT_RESTORE_FIRST;
      if (pkt->object) {
        free(pkt->object);
        pkt->object = nullptr;
      }
      if (pkt->object_name) {
        free(pkt->object_name);
        pkt->object_name = nullptr;
      }

      pkt->object = get_object_storage(core, object.data().size());
      memcpy(pkt->object, object.data().c_str(), object.data().size());
      pkt->object_len = object.data().size();

      const auto& name = object.name();
      pkt->object_name = get_name_storage(core, name.size() + 1);
      memcpy(pkt->object_name, name.c_str(), name.size() + 1);
      pkt->index = object.index();
#if 0
    } else if (begin_obj->has_error()) {
      auto& err = begin_obj->error();

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
#endif
    } else {
      DebugLog(100, FMT_STRING("received nothing"));
      return bRC_Error;
    }

    return bRC_OK;
  }

  bRC endBackupFile()
  {
    if (auto* bsm = std::get_if<BackupStateMachine>(&state_machine)) {
      if (!bsm->end_backup_file()) { return bRC_Error; }

      if (bsm->done()) {
        state_machine.emplace<std::monostate>();
        return bRC_OK;
      }

      return bRC_More;
    }

    JobLog(core, M_FATAL, "this operation is only supported during backup");
    return bRC_Error;
  }

  template <typename... Types> void ignore(Types&&...) {}

  bRC startRestoreFile(std::string_view cmd)
  {
    ignore(cmd);
    return bRC_Error;
  }

  bRC endRestoreFile() { return bRC_Error; }

  bRC FileOpen(std::string_view name,
               int32_t flags,
               int32_t mode,
               std::optional<int>* io_in_core_fd)
  {
    ignore(name, flags, mode, io_in_core_fd);

    if (auto* bsm = std::get_if<BackupStateMachine>(&state_machine)) {
      if (!bsm->file_open()) { return bRC_Error; }

      *io_in_core_fd = std::nullopt;
      return bRC_OK;
    }
    // else if (auto *rsm = ...) {}

    JobLog(core, M_FATAL, "cannot open file in this state");
    return bRC_Error;
  }

  bRC FileSeek(int whence, int64_t offset)
  {
    ignore(whence, offset);
    return bRC_Error;
  }

  bRC FileRead(char* buf, size_t size, int32_t* bytes_read)
  {
    if (auto* bsm = std::get_if<BackupStateMachine>(&state_machine)) {
      std::optional bytes = bsm->file_read(std::span(buf, size));
      if (!bytes) { return bRC_Error; }

      // there has to be a better way than this ...
      ASSERT(*bytes <= static_cast<std::size_t>(
                 std::numeric_limits<
                     std::remove_reference_t<decltype(*bytes_read)>>::max()));

      *bytes_read = *bytes;

      return bRC_OK;
    }

    JobLog(core, M_FATAL, "this operation is only supported during backup");
    return bRC_Error;
  }

  bRC FileWrite(size_t size, size_t* num_bytes_written)
  {
    ignore(size, num_bytes_written);
    return bRC_Error;
  }

  bRC FileClose()
  {
    if (auto* bsm = std::get_if<BackupStateMachine>(&state_machine)) {
      if (!bsm->file_close()) { return bRC_Error; }

      return bRC_OK;
    }
    // else if (auto *rsm = ...) {}

    JobLog(core, M_FATAL, "cannot close file in this state");
    return bRC_Error;
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
    ignore(pkt);
    return bRC_Error;
  }
  bRC setFileAttributes(std::string_view name,
                        const struct stat& statp,
                        std::string_view extended_attributes,
                        std::string_view where,
                        bool* do_in_core)
  {
    ignore(name, statp, extended_attributes, where, do_in_core);
    return bRC_Error;
  }
  bRC checkFile(std::string_view name)
  {
    ignore(name);
    return bRC_Error;
  }

  bRC setAcl(std::string_view file, std::string_view content)
  {
    ignore(file, content);
    return bRC_Error;
  }

  bRC getAcl(std::string_view file, filedaemon::acl_pkt* pkt)
  {
    ignore(file);
    if (auto* bsm = std::get_if<BackupStateMachine>(&state_machine)) {
      if (!bsm->get_acl(pkt)) { return bRC_Error; }

      return bRC_OK;
    }

    JobLog(core, M_FATAL, "this operation is only supported during backup");
    return bRC_Error;
  }

  bRC setXattr(std::string_view file,
               std::string_view key,
               std::string_view value)
  {
    ignore(file, key, value);
    return bRC_Error;
  }

  bRC getXattr(std::string_view file, filedaemon::xattr_pkt* pkt)
  {
    ignore(file);
    if (auto* bsm = std::get_if<BackupStateMachine>(&state_machine)) {
      if (!bsm->get_xattr(pkt)) { return bRC_Error; }

      // we always say that we return more data, so that we dont have to check
      // here if the next chunk is also an xattr packet.
      // if there is no additional xattr packet, then we are allowed to just
      // return an "empty" xattr packet + bRC_OK to tell bareos that there
      // are actually no more xattr packets.
      if (pkt->name_length == 0) {
        return bRC_OK;
      } else {
        return bRC_More;
      }
    }

    JobLog(core, M_FATAL, "this operation is only supported during backup");
    return bRC_Error;
  }

  PluginClient(const PluginClient&) = delete;
  PluginClient& operator=(const PluginClient&) = delete;
  PluginClient(PluginClient&&) = default;
  PluginClient& operator=(PluginClient&&) = default;

  ~PluginClient()
  {
    if (data_transfer) {
      DebugLog(100, "client = {{ writes = {}, reads = {}}}",
               data_transfer->write_count, data_transfer->read_count);
    }

    if (core_events) {
      DebugLog(100, "client = {{ writes = {}, reads = {}}}",
               core_events->write_count, core_events->read_count);
    }
  }

 private:
  std::unique_ptr<prototools::ProtoBidiStream> data_transfer{};
  std::unique_ptr<prototools::ProtoBidiStream> core_events{};
  PluginContext* core{nullptr};

  template <typename... Args>
  void DebugLog(Severity severity,
                fmt::format_string<Args...> fmt,
                Args&&... args)
  {
    ::DebugLog(core, severity, std::move(fmt), std::forward<Args>(args)...);
  }

  std::variant<std::monostate, BackupStateMachine> state_machine;
};
}  // namespace

struct grpc_connection_members {
  // the order here is important
  // 1) destroy the plugin client.  This way our code has one last chance
  //    to send stuff to the inferior
  // 2) destroy the client_sock.  This way the inferior still has the chance
  //    to understand whats happening and send some debug/job messages
  // 3) destroy the server; this way we are sure that the other side
  //    has finished sending its messages.
  // 4) destroy the server socket.

  PluginContext* pctx{nullptr};
  Socket plugin_requests{};
  Socket core_events{};
  Socket data_transfer{};

  std::unique_ptr<BareosCore> server{};
  PluginClient client;

  grpc_connection_members(PluginContext* ctx,
                          Socket plugin_requests_,
                          Socket core_events_,
                          Socket data_transfer_,
                          PluginClient client_,
                          std::unique_ptr<BareosCore> server_)
      : pctx{ctx}
      , plugin_requests{std::move(plugin_requests_)}
      , core_events{std::move(core_events_)}
      , data_transfer{std::move(data_transfer_)}
      , server{std::move(server_)}
      , client{std::move(client_)}
  {
  }

  grpc_connection_members() = delete;

  ~grpc_connection_members()
  {
    data_transfer.close();
    core_events.close();
    plugin_requests.close();
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

  template <typename... Args> int maximum_of(Args... ints)
  {
    int sup = 0;

    ((sup = std::max(sup, ints)), ...);

    return sup;
  }

  bool move_fd(int fd, int newfd)
  {
    if (dup2(fd, newfd) != newfd) { return false; }

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

      int dummy_fd = maximum_of(io.grpc_parent.get(), io.grpc_child.get(),
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
    auto plugin_requests
        = std::make_unique<prototools::ProtoBidiStream>(io.grpc_parent.get());
    auto core = std::make_unique<BareosCore>(ctx, std::move(plugin_requests));
    auto core_events
        = std::make_unique<prototools::ProtoBidiStream>(io.grpc_child.get());
    auto data_transfer
        = std::make_unique<prototools::ProtoBidiStream>(io.grpc_io.get());
    PluginClient client{ctx, std::move(core_events), std::move(data_transfer)};
    auto members = std::make_unique<grpc_connection_members>(
        ctx, std::move(io.grpc_child), std::move(io.grpc_parent),
        std::move(io.grpc_io), std::move(client), std::move(core));
    grpc_connection con{std::move(members)};
    return con;
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


  auto max_wait = std::chrono::seconds(1);

  auto start = std::chrono::steady_clock::now();
  auto end_wait = start + max_wait;

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigset_t orig_mask;
  bool set_mask = sigprocmask(SIG_BLOCK, &mask, &orig_mask) >= 0;

  DebugLog(100, FMT_STRING("Managed to set mask = {}"),
           set_mask ? "Yes" : "No");

  size_t error_count = 0;
  for (;;) {
    auto now = std::chrono::steady_clock::now();

    if (end_wait <= now) { break; }

    // we know that end_wait > now, i.e. wait_left > 0
    auto wait_left = end_wait - now;

    struct timespec timeout;
    timeout.tv_sec
        = std::chrono::duration_cast<std::chrono::seconds>(wait_left).count();
    timeout.tv_nsec
        = std::chrono::duration_cast<std::chrono::nanoseconds>(wait_left)
              .count()
          - std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::seconds(timeout.tv_sec))
                .count();

    int status = sigtimedwait(&mask, NULL, &timeout);
    if (status < 0) {
      if (errno == EINTR) {
        /* Interrupted by a signal other than SIGCHLD. */
        continue;
      } else if (errno == EAGAIN) {
        DebugLog(50, FMT_STRING("Child {} is unresponsive; we are killing it"),
                 pid);
        kill(pid, SIGKILL);
      } else {
        DebugLog(50, FMT_STRING("Got error {} in sigtimedwait = {}"), errno,
                 strerror(errno));
        error_count += 1;
        if (error_count > 3) {
          // something weird is going on, lets just kill the child and
          // go on
          DebugLog(50, FMT_STRING("abandoning sigtimedwait, killing child..."));
          kill(pid, SIGKILL);
          break;
        }
        continue;
      }
    } else if (status == SIGCHLD) {
      DebugLog(100, FMT_STRING("Got SIGCHLD"));
      break;
    } else {
      DebugLog(100, FMT_STRING("Got signal {}"), status);
      continue;
    }
  }

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

  // if we managed to set the mask, we need to make sure to reset it again
  if (set_mask) {
    bool reset_mask = sigprocmask(SIG_SETMASK, &orig_mask, nullptr) >= 0;
    DebugLog(100, FMT_STRING("Managed to reset mask = {}"),
             reset_mask ? "Yes" : "No");
  }
}


grpc_connection::grpc_connection(grpc_connection&& other) = default;
grpc_connection& grpc_connection::operator=(grpc_connection&& other) = default;
grpc_connection::~grpc_connection() = default;
grpc_connection::grpc_connection(
    std::unique_ptr<grpc_connection_members> members_)
    : members{std::move(members_)}
{
}


bp::HandlePluginEventRequest* to_grpc(filedaemon::bEventType type, void* data)
{
  auto req = new bp::HandlePluginEventRequest;

  if (make_event_request(type, data, *req)) {
    return req;
  } else {
    delete req;
    return nullptr;
  }
}

void delete_request(bp::HandlePluginEventRequest* req) { delete req; }

bRC grpc_connection::handlePluginEvent(filedaemon::bEventType type,
                                       bp::HandlePluginEventRequest* req)
{
  PluginClient* client = &members->client;
  return client->handlePluginEvent(type, req);
}

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

bRC grpc_connection::pluginIO(filedaemon::io_pkt* pkt, int iosock)
{
  PluginClient* client = &members->client;

  switch (pkt->func) {
    case filedaemon::IO_OPEN: {
      std::optional<int> io_in_core_fd{};
      auto res
          = client->FileOpen(pkt->fname, pkt->flags, pkt->mode, &io_in_core_fd);
      if (res != bRC_OK) {
        pkt->io_errno = EIO;
        return res;
      }
      if (io_in_core_fd) {
        DebugLog(100, FMT_STRING("using io_in_core (fd = {})"), *io_in_core_fd);
        pkt->filedes = *io_in_core_fd;
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
      return client->FileRead(pkt->buf, pkt->count, &pkt->status);
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

  std::string_view where = pkt->where ? pkt->where : "";
  std::string_view attrEx = pkt->attrEx ? pkt->attrEx : "";
  std::string_view ofname = pkt->ofname ? pkt->ofname : "";

  auto res = client->setFileAttributes(ofname, pkt->statp, attrEx, where,
                                       &do_in_core);
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
  bRC result = client->getAcl(pkt->fname, pkt);
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

  bRC result = client->getXattr(pkt->fname, pkt);

  return result;
}
bRC grpc_connection::setXattr(filedaemon::xattr_pkt* pkt)
{
  PluginClient* client = &members->client;

  return client->setXattr(pkt->fname,
                          std::string_view{pkt->name, pkt->name_length},
                          std::string_view{pkt->value, pkt->value_length});
}
