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

#include "plugin_service_python.h"
#include <fcntl.h>
#include <poll.h>
#include <grpc/compression.h>
#include "bareos.pb.h"
#include "common.pb.h"
#include "filed/fd_plugins.h"
#include "include/filetypes.h"
#include "plugin.pb.h"
#include "test_module_python.h"

#include <sys/sendfile.h>

#include <filesystem>
#include <thread>
#include <variant>

#include <sys/socket.h>

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

  DebugLog(100, FMT_STRING("got fd = {}"), fd);

  if (fcntl(fd, F_GETFD) < 0) {
    DebugLog(50, FMT_STRING("got bad fd = {}"), fd);
    return std::nullopt;
  } else {
    DebugLog(100, FMT_STRING("got fd = {}"), fd);
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
    return false;
  }

  DebugLog(50, FMT_STRING("sent fd {} to core. control len = {}/{}"), fd,
           CMSG_LEN(sizeof(fd)), msg.msg_controllen);
  return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"

auto PluginService::Setup(ServerContext*,
                          const bp::SetupRequest*,
                          bp::SetupResponse*) -> Status
{
  auto events = std::array{
      bc::EventType::Event_JobStart,       bc::EventType::Event_JobEnd,
      bc::EventType::Event_PluginCommand,  bc::EventType::Event_BackupCommand,
      bc::EventType::Event_StartBackupJob, bc::EventType::Event_StartRestoreJob,
      bc::EventType::Event_RestoreCommand,
  };

  if (!WaitForReady()) { return Status::CANCELLED; }

  if (!Register({events.data(), events.size()})) {
    DebugLog(50, FMT_STRING("could not register events!"));
  } else {
    DebugLog(100, FMT_STRING("managed to register my events!"));
  }

  if (funcs.newPlugin(ctx) != bRC_OK) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        "newPlugin returned bad value");
  }
  return Status::OK;
}

enum class UnhandledType
{
  Unknown,
  Plugin,
  Base,
  UnchangedFile,
  UnchangedDirectory,
  DirectoryStart,
};

std::variant<UnhandledType, bco::FileType, bco::FileErrorType, bco::ObjectType>
to_grpc(int32_t type)
{
  switch (type) {
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

    case FT_NOACCESS: {
      return bco::CouldNotAccessFile;
    } break;
    case FT_NOFOLLOW: {
      return bco::CouldNotFollowLink;
    } break;
    case FT_NOSTAT: {
      return bco::CouldNotStat;
    } break;
    case FT_NORECURSE: {
      return bco::RecursionDisabled;
    } break;
    case FT_NOFSCHG: {
      return bco::CouldNotChangeFilesystem;
    } break;
    case FT_NOOPEN: {
      return bco::CouldNotOpenDirectory;
    } break;
    case FT_INVALIDFS: {
      return bco::InvalidFileSystem;
    } break;
    case FT_INVALIDDT: {
      return bco::InvalidDriveType;
    } break;
    case FT_RESTORE_FIRST: {
      return bco::Blob;
    } break;
    case FT_PLUGIN_CONFIG: {
      return bco::Config;
    } break;
    case FT_PLUGIN_CONFIG_FILLED: {
      return bco::FilledConfig;
    } break;

    case FT_PLUGIN: {
      return UnhandledType::Plugin;
    } break;
    case FT_BASE: {
      return UnhandledType::Base;
    } break;
    case FT_NOCHG: {
      return UnhandledType::UnchangedFile;
    } break;
    case FT_DIRNOCHG: {
      return UnhandledType::UnchangedDirectory;
    } break;
    case FT_DIRBEGIN: {
      return UnhandledType::DirectoryStart;
    } break;
  }
  return UnhandledType::Unknown;
}

bp::ReturnCode to_grpc(bRC res)
{
  switch (res) {
    case bRC_OK:
      return bp::RC_OK;
    case bRC_Stop:
      return bp::RC_Stop;
    case bRC_Error:
      return bp::RC_Error;
    case bRC_More:
      return bp::RC_More;
    case bRC_Term:
      return bp::RC_Term;
    case bRC_Seen:
      return bp::RC_Seen;
    case bRC_Core:
      return bp::RC_Core;
    case bRC_Skip:
      return bp::RC_Skip;
    case bRC_Cancel:
      return bp::RC_Cancel;
    default:
      return bp::RC_Error;
  }
}

std::optional<int> from_grpc(bco::FileType ft)
{
  switch (ft) {
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
    default: {
      return std::nullopt;
    }
  }
}

static const char* event_case_to_str(bp::events::Event::EventCase ec)
{
  switch (ec) {
    case bp::events::Event::kJobStart:
      return "JobStart";
    case bp::events::Event::kJobEnd:
      return "JobEnd";
    case bp::events::Event::kStartBackupJob:
      return "StartBackupJob";
    case bp::events::Event::kEndBackupJob:
      return "EndBackupJob";
    case bp::events::Event::kStartRestoreJob:
      return "StartRestoreJob";
    case bp::events::Event::kEndRestoreJob:
      return "EndRestoreJob";
    case bp::events::Event::kStartVerifyJob:
      return "StartVerifyJob";
    case bp::events::Event::kEndVerifyJob:
      return "EndVerifyJob";
    case bp::events::Event::kBackupCommand:
      return "BackupCommand";
    case bp::events::Event::kRestoreCommand:
      return "RestoreCommand";
    case bp::events::Event::kEstimateCommand:
      return "EstimateCommand";
    case bp::events::Event::kLevel:
      return "Level";
    case bp::events::Event::kSince:
      return "Since";
    case bp::events::Event::kCancelCommand:
      return "CancelCommand";
    case bp::events::Event::kRestoreObject:
      return "RestoreObject";
    case bp::events::Event::kEndRestoreObject:
      return "EndRestoreObject";
    case bp::events::Event::kEndFileset:
      return "EndFileset";
    case bp::events::Event::kPluginCommand:
      return "PluginCommand";
    case bp::events::Event::kOptionPlugin:
      return "OptionPlugin";
    case bp::events::Event::kHandleBackupFile:
      return "HandleBackupFile";
    case bp::events::Event::kNewPluginOptions:
      return "NewPluginOptions";
    case bp::events::Event::kVssInitBackup:
      return "VssInitBackup";
    case bp::events::Event::kVssInitRestore:
      return "VssInitRestore";
    case bp::events::Event::kVssSetBackupState:
      return "VssSetBackupState";
    case bp::events::Event::kVssPrepareForBackup:
      return "VssPrepareForBackup";
    case bp::events::Event::kVssBackupAddComponents:
      return "VssBackupAddComponents";
    case bp::events::Event::kVssPrepareSnapshot:
      return "VssPrepareSnapshot";
    case bp::events::Event::kVssCreateSnapshot:
      return "VssCreateSnapshot";
    case bp::events::Event::kVssRestoreLoadCompanentsMetadata:
      return "VssRestoreLoadCompanentsMetadata";
    case bp::events::Event::kVssRestoreSetComponentsSelected:
      return "VssRestoreSetComponentsSelected";
    case bp::events::Event::kVssCloseRestore:
      return "VssCloseRestore";
    case bp::events::Event::kVssBackupComplete:
      return "VssBackupComplete";
    case bp::events::Event::EVENT_NOT_SET:
      return "<unset>";
  }
  return "<unknown>";
}

auto PluginService::handlePluginEvent(
    ServerContext*,
    const bp::handlePluginEventRequest* request,
    bp::handlePluginEventResponse* response) -> Status
{
  auto& event = request->to_handle();

  DebugLog(100, FMT_STRING("got some {} event (num = {})"),
           event_case_to_str(event.event_case()), int(event.event_case()));

  if (event.has_level()) {
    auto& inner = event.level();
    intptr_t level = inner.level();
    filedaemon::bEvent e{filedaemon::bEventLevel};
    auto res = funcs.handlePluginEvent(ctx, &e, reinterpret_cast<void*>(level));
    response->set_res(to_grpc(res));
  } else if (event.has_since()) {
    auto& inner = event.since();
    time_t t = inner.since().seconds();
    filedaemon::bEvent e{filedaemon::bEventSince};
    auto res = funcs.handlePluginEvent(ctx, &e, reinterpret_cast<void*>(t));
    response->set_res(to_grpc(res));
  } else if (event.has_job_end()) {
    auto& inner = event.job_end();
    DebugLog(100, FMT_STRING("got job end event ({}). shutting down ..."),
             inner.DebugString());

    filedaemon::bEvent e{filedaemon::bEventJobEnd};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));

    funcs.freePlugin(ctx);
    shutdown.set_value();
  } else if (event.has_job_start()) {
    auto& inner = event.job_start();
    DebugLog(100, FMT_STRING("got job start event ({})."), inner.DebugString());
    filedaemon::bEvent e{filedaemon::bEventJobStart};
    auto res = funcs.handlePluginEvent(ctx, &e,
                                       const_cast<char*>(inner.data().c_str()));
    response->set_res(to_grpc(res));
  } else if (event.has_end_fileset()) {
    auto& inner = event.end_fileset();
    filedaemon::bEvent e{filedaemon::bEventEndFileSet};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_option_plugin()) {
    auto& inner = event.option_plugin();
    filedaemon::bEvent e{filedaemon::bEventOptionPlugin};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_backup_command()) {
    auto& inner = event.backup_command();
    filedaemon::bEvent e{filedaemon::bEventBackupCommand};
    auto res = funcs.handlePluginEvent(ctx, &e,
                                       const_cast<char*>(inner.data().c_str()));
    response->set_res(to_grpc(res));
  } else if (event.has_cancel_command()) {
    auto& inner = event.cancel_command();
    filedaemon::bEvent e{filedaemon::bEventCancelCommand};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_end_backup_job()) {
    auto& inner = event.end_backup_job();
    filedaemon::bEvent e{filedaemon::bEventEndBackupJob};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_end_verify_job()) {
    auto& inner = event.end_verify_job();
    filedaemon::bEvent e{filedaemon::bEventEndVerifyJob};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_plugin_command()) {
    auto& inner = event.plugin_command();
    filedaemon::bEvent e{filedaemon::bEventPluginCommand};
    auto res = funcs.handlePluginEvent(ctx, &e,
                                       const_cast<char*>(inner.data().c_str()));
    response->set_res(to_grpc(res));
  } else if (event.has_restore_object()) {
    auto& inner = event.restore_object();
    filedaemon::bEvent e{filedaemon::bEventRestoreObject};

    auto& grop = inner.rop();
    filedaemon::restore_object_pkt rop = {};

    rop.plugin_name = const_cast<char*>(grop.used_cmd_string().c_str());
    rop.JobId = grop.jobid();
    rop.object_len = grop.sent().data().size();
    rop.object = const_cast<char*>(grop.sent().data().data());
    rop.object_name = const_cast<char*>(grop.sent().name().c_str());
    // TODO: maybe this should be set by us ?
    // rop.object_type        = ;
    rop.object_index = grop.sent().index();
    // rop.object_full_len    =;
    // rop.object_compression =;

    auto res = funcs.handlePluginEvent(ctx, &e, &rop);
    response->set_res(to_grpc(res));
  } else if (event.has_end_restore_job()) {
    auto& inner = event.end_restore_job();
    filedaemon::bEvent e{filedaemon::bEventEndRestoreJob};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_restore_command()) {
    auto& inner = event.restore_command();
    filedaemon::bEvent e{filedaemon::bEventRestoreCommand};
    auto res = funcs.handlePluginEvent(ctx, &e,
                                       const_cast<char*>(inner.data().c_str()));
    response->set_res(to_grpc(res));
  } else if (event.has_vss_init_backup()) {
    auto& inner = event.vss_init_backup();
    filedaemon::bEvent e{filedaemon::bEventVssInitializeForBackup};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_estimate_command()) {
    auto& inner = event.estimate_command();
    filedaemon::bEvent e{filedaemon::bEventEstimateCommand};
    auto res = funcs.handlePluginEvent(ctx, &e,
                                       const_cast<char*>(inner.data().c_str()));
    response->set_res(to_grpc(res));
  } else if (event.has_start_backup_job()) {
    auto& inner = event.start_backup_job();
    filedaemon::bEvent e{filedaemon::bEventStartBackupJob};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_start_verify_job()) {
    auto& inner = event.start_verify_job();
    filedaemon::bEvent e{filedaemon::bEventStartVerifyJob};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_vss_init_restore()) {
    auto& inner = event.vss_init_restore();
    filedaemon::bEvent e{filedaemon::bEventVssInitializeForRestore};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_start_restore_job()) {
    auto& inner = event.start_restore_job();
    filedaemon::bEvent e{filedaemon::bEventStartRestoreJob};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_vss_close_restore()) {
    auto& inner = event.vss_close_restore();
    filedaemon::bEvent e{filedaemon::bEventVssCloseRestore};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_handle_backup_file()) {
    auto& inner = event.handle_backup_file();
    filedaemon::bEvent e{filedaemon::bEventHandleBackupFile};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_new_plugin_options()) {
    auto& inner = event.new_plugin_options();
    filedaemon::bEvent e{filedaemon::bEventNewPluginOptions};
    auto res = funcs.handlePluginEvent(ctx, &e,
                                       const_cast<char*>(inner.data().c_str()));
    response->set_res(to_grpc(res));
  } else if (event.has_vss_backup_complete()) {
    auto& inner = event.vss_backup_complete();
    filedaemon::bEvent e{filedaemon::bEventVssBackupComplete};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_vss_create_snapshot()) {
    auto& inner = event.vss_create_snapshot();
    filedaemon::bEvent e{filedaemon::bEventVssCreateSnapshots};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_vss_prepare_snapshot()) {
    auto& inner = event.vss_prepare_snapshot();
    filedaemon::bEvent e{filedaemon::bEventVssPrepareSnapshot};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_vss_set_backup_state()) {
    auto& inner = event.vss_set_backup_state();
    filedaemon::bEvent e{filedaemon::bEventVssSetBackupState};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_vss_prepare_for_backup()) {
    auto& inner = event.vss_prepare_for_backup();
    filedaemon::bEvent e{filedaemon::bEventVssPrepareForBackup};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_vss_backup_add_components()) {
    auto& inner = event.vss_backup_add_components();
    filedaemon::bEvent e{filedaemon::bEventVssBackupAddComponents};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_vss_restore_set_components_selected()) {
    auto& inner = event.vss_restore_set_components_selected();
    filedaemon::bEvent e{filedaemon::bEventVssRestoreSetComponentsSelected};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else if (event.has_vss_restore_load_companents_metadata()) {
    auto& inner = event.vss_restore_load_companents_metadata();
    filedaemon::bEvent e{filedaemon::bEventVssRestoreLoadComponentMetadata};
    auto res = funcs.handlePluginEvent(ctx, &e, nullptr);
    response->set_res(to_grpc(res));
  } else {
    return Status(grpc::StatusCode::INVALID_ARGUMENT, "unknown event type");
  }

  if (response->res() == bp::RETURN_CODE_UNSPECIFIED) {
    std::string emsg = fmt::format(FMT_STRING("i lied about handling event {}"),
                                   int(event.event_case()));

    return Status(grpc::StatusCode::UNIMPLEMENTED, emsg);
  }

  return Status::OK;
}

auto PluginService::startBackupFile(ServerContext*,
                                    const bp::startBackupFileRequest* request,
                                    bp::startBackupFileResponse* response)
    -> Status
{
  filedaemon::save_pkt sp;


  if (sizeof(sp.flags) != request->flags().size()) {
    return Status(grpc::StatusCode::INVALID_ARGUMENT,
                  "flags size is not matching");
  }
  memcpy(sp.flags, request->flags().data(), sizeof(sp.flags));

  sp.cmd = const_cast<char*>(request->cmd().c_str());
  sp.portable = request->portable();
  sp.no_read = request->no_read();

  auto res = funcs.startBackupFile(ctx, &sp);

  switch (res) {
    case bRC_OK: {
      response->set_result(bp::SBF_OK);
    } break;
    case bRC_Skip: {
      response->set_result(bp::SBF_Skip);
      return Status::OK;
    } break;
    case bRC_Stop: {
      response->set_result(bp::SBF_Stop);
      return Status::OK;
    } break;
    default: {
      return Status(grpc::StatusCode::INTERNAL, "bad return value");
    }
  }

  DebugLog(100, FMT_STRING("received save packet of type {}"), sp.type);

  auto type = to_grpc(sp.type);

  return std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, bco::FileType>) {
          if (!sp.fname) {
            return Status(grpc::StatusCode::INTERNAL, "no file name set");
          }
          if (arg == bco::FILE_TYPE_UNSPECIFIED) {
            return Status(grpc::StatusCode::INTERNAL, "bad file type");
          }

          auto* file = response->mutable_file();
          file->set_no_read(sp.no_read);
          file->set_portable(sp.portable);
          if (BitIsSet(FO_DELTA, sp.flags)) {
            file->set_delta_seq(sp.delta_seq);
          }
          file->set_offset_backup(BitIsSet(FO_OFFSETS, sp.flags));
          file->set_sparse_backup(BitIsSet(FO_SPARSE, sp.flags));
          file->set_ft(arg);
          file->set_stats(&sp.statp, sizeof(sp.statp));
          file->set_file(sp.fname);

          if (arg == bco::SoftLink) { file->set_link(sp.link); }

        } else if constexpr (std::is_same_v<T, bco::FileErrorType>) {
          auto* error = response->mutable_error();

          if (!sp.fname) {
            return Status(grpc::StatusCode::INTERNAL, "no file name set");
          }

          error->set_file(sp.fname);
          error->set_error(arg);

        } else if constexpr (std::is_same_v<T, bco::ObjectType>) {
          if (!sp.object_name) {
            return Status(grpc::StatusCode::INTERNAL, "no object name set");
          }
          if (arg == bco::OBJECT_TYPE_UNSPECIFIED) {
            return Status(grpc::StatusCode::INTERNAL, "bad object type");
          }

          auto* obj = response->mutable_object();
          obj->set_index(sp.index);
          obj->set_name(sp.object_name);
          obj->set_data(sp.object, sp.object_len);
          obj->set_type(arg);
        } else if constexpr (std::is_same_v<T, UnhandledType>) {
          switch (arg) {
            case UnhandledType::Unknown: {
              JobLog(bc::JMSG_ERROR, FMT_STRING("unknown file type {}"),
                     sp.type);
              return Status(grpc::StatusCode::INTERNAL, "unknown file type");
            } break;
            case UnhandledType::Plugin: {
              JobLog(bc::JMSG_INFO,
                     FMT_STRING("ignoring file of type 'Plugin'"));
            } break;
            case UnhandledType::Base: {
              JobLog(bc::JMSG_INFO,
                     FMT_STRING("ignoring file of type 'Plugin'"));
            } break;
            case UnhandledType::UnchangedFile: {
              JobLog(bc::JMSG_INFO,
                     FMT_STRING("ignoring file of type 'UnchangedFile'"));
            } break;
            case UnhandledType::UnchangedDirectory: {
              JobLog(bc::JMSG_INFO,
                     FMT_STRING("ignoring file of type 'UnchangedDirectory'"));
            } break;
            case UnhandledType::DirectoryStart: {
              JobLog(bc::JMSG_INFO,
                     FMT_STRING("ignoring file of type 'DirectoryStart'"));
            } break;
          }

          auto* file = response->mutable_file();
          file->set_file(sp.fname ? sp.fname : "<unknown file name>");

          response->set_result(bp::SBF_Skip);

        } else {
          static_assert(!std::is_same_v<T, T>, "unhandled type");
        }

        return Status::OK;
      },
      type);
}
auto PluginService::endBackupFile(ServerContext*,
                                  const bp::endBackupFileRequest* request,
                                  bp::endBackupFileResponse* response) -> Status
{
  auto res = funcs.endBackupFile(ctx);

  switch (res) {
    case bRC_OK: {
      response->set_result(bp::EBF_Done);
    } break;
    case bRC_More: {
      response->set_result(bp::EBF_More);
    } break;
    default: {
      return Status(grpc::StatusCode::INTERNAL, "bad return value");
    } break;
  }

  return Status::OK;
}
auto PluginService::startRestoreFile(ServerContext*,
                                     const bp::startRestoreFileRequest* request,
                                     bp::startRestoreFileResponse*) -> Status
{
  auto res = funcs.startRestoreFile(ctx, request->command().c_str());

  if (res != bRC_OK) {
    return Status(grpc::StatusCode::INTERNAL, "bad return value");
  }

  return Status::OK;
}
auto PluginService::endRestoreFile(ServerContext*,
                                   const bp::endRestoreFileRequest* request,
                                   bp::endRestoreFileResponse*) -> Status
{
  auto res = funcs.endRestoreFile(ctx);

  if (res != bRC_OK) {
    return Status(grpc::StatusCode::INTERNAL, "bad return value");
  }

  return Status::OK;
}
auto PluginService::FileOpen(ServerContext*,
                             const bp::fileOpenRequest* request,
                             bp::fileOpenResponse* resp) -> Status
{
  filedaemon::io_pkt pkt;
  pkt.func = filedaemon::IO_OPEN;
  pkt.fname = const_cast<char*>(request->file().c_str());
  pkt.mode = request->mode();
  pkt.flags = request->flags();
  auto res = funcs.pluginIO(ctx, &pkt);

  if (res != bRC_OK) {
    return Status(grpc::StatusCode::INTERNAL, "bad response");
  }

  if (pkt.status == IoStatus::do_io_in_core) {
    resp->set_io_in_core(true);
    if (!send_fd(io, pkt.filedes)) {
      JobLog(bc::JMSG_FATAL, "could not send fd {} to core. Err={}",
             pkt.filedes, strerror(errno));
      return Status(grpc::StatusCode::INTERNAL, "could not send fd to core");
    }
  }

  return Status::OK;
}
auto PluginService::FileSeek(ServerContext*,
                             const bp::fileSeekRequest* request,
                             bp::fileSeekResponse*) -> Status
{
  filedaemon::io_pkt pkt;
  pkt.func = filedaemon::IO_SEEK;
  pkt.offset = request->offset();

  switch (request->whence()) {
    case bp::SS_StartOfFile: {
      pkt.whence = SEEK_SET;
    } break;
    case bp::SS_CurrentPos: {
      pkt.whence = SEEK_CUR;
    } break;
    case bp::SS_EndOfFile: {
      pkt.whence = SEEK_END;
    } break;
    default:
      return Status(grpc::StatusCode::INVALID_ARGUMENT,
                    "invalid start position for seek");
  }

  auto res = funcs.pluginIO(ctx, &pkt);

  if (res != bRC_OK) {
    return Status(grpc::StatusCode::INTERNAL, "bad response");
  }

  return Status::OK;
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

struct non_blocking {
  int filedes;
  int flags;
  std::optional<int> error_num;

  non_blocking(int fd) : filedes{fd}
  {
    flags = fcntl(fd, F_GETFL);
    if (flags == -1) { error_num = errno; }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) { error_num = errno; }
  }

  std::optional<int> error() { return error_num; }

  void reset()
  {
    if (fcntl(filedes, F_SETFL, flags) < 0) { error_num = errno; }
  }

  ~non_blocking() { reset(); }
};

auto PluginService::FileRead(ServerContext*,
                             const bp::fileReadRequest* request,
                             grpc::ServerWriter<bp::fileReadResponse>* writer)
    -> Status
{
  filedaemon::io_pkt pkt;
  pkt.func = filedaemon::IO_READ;
  pkt.count = request->num_bytes();
  pkt.buf = buffer(request->num_bytes());

  auto res = funcs.pluginIO(ctx, &pkt);

  if (res != bRC_OK) {
    return Status(grpc::StatusCode::INTERNAL, "bad response");
  }

  auto _ = non_blocking{io};

  if (_.error()) {
    JobLog(bc::JMSG_ERROR,
           FMT_STRING("could not set fd {} as nonblocking: Err={}"), io,
           strerror(_.error().value()));
    return Status(grpc::StatusCode::INTERNAL,
                  "could not set socket to be not blocking");
  }

  bp::fileReadResponse resp;
  resp.set_size(pkt.status);
  writer->Write(resp);
  int32_t bytes_sent = 0;
  while (bytes_sent < pkt.status) {
    auto written = write(io, pkt.buf + bytes_sent, pkt.status - bytes_sent);
    if (written < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        struct pollfd pfd = {};
        pfd.fd = io;
        pfd.events = POLLOUT;
        int timeout = 5 * 1000;  // we wait at most 5 seconds
        int poll_res = poll(&pfd, 1, 5 * 1000);
        if (poll_res < 0) {
          JobLog(bc::JMSG_ERROR,
                 FMT_STRING("could not write missing {} bytes to io socket {} "
                            "as poll failed: Err={}"),
                 pkt.status - bytes_sent, io, strerror(errno));
          return Status(grpc::StatusCode::INTERNAL, "io socket full");
        } else if (poll_res == 0) {
          JobLog(bc::JMSG_ERROR,
                 FMT_STRING("could not write missing {} bytes to io socket {} "
                            "after waiting {}ms for data to become available"),
                 pkt.status - bytes_sent, io, timeout);
          return Status(grpc::StatusCode::INTERNAL, "io socket full");
        }
      } else {
        JobLog(bc::JMSG_ERROR,
               FMT_STRING("could not write {} bytes to io socket {}: Err={}"),
               pkt.status - bytes_sent, io, strerror(errno));
        return Status(grpc::StatusCode::INTERNAL,
                      "io socket write not successful");
      }
    } else {
      bytes_sent += written;
    }
  }

  _.reset();
  if (_.error()) {
    JobLog(bc::JMSG_ERROR, FMT_STRING("could not reset fd {} flags: Err={}"),
           io, strerror(_.error().value()));
    return Status(grpc::StatusCode::INTERNAL, "could not reset socket flags");
  }
  return Status::OK;
}
auto PluginService::FileWrite(ServerContext*,
                              const bp::fileWriteRequest* request,
                              bp::fileWriteResponse* response) -> Status
{
  filedaemon::io_pkt pkt;
  pkt.func = filedaemon::IO_WRITE;
  pkt.count = request->bytes_written();
  pkt.buf = buffer(request->bytes_written());

  if (!full_read(io, pkt.buf, pkt.count)) {
    return Status(grpc::StatusCode::INTERNAL, "io socket read not successful");
  }

  auto res = funcs.pluginIO(ctx, &pkt);

  if (res != bRC_OK) {
    return Status(grpc::StatusCode::INTERNAL, "bad response");
  }

  response->set_bytes_written(pkt.status);

  return Status::OK;
}
auto PluginService::FileClose(ServerContext*,
                              const bp::fileCloseRequest* request,
                              bp::fileCloseResponse*) -> Status
{
  filedaemon::io_pkt pkt;
  pkt.func = filedaemon::IO_CLOSE;

  auto res = funcs.pluginIO(ctx, &pkt);

  if (res != bRC_OK) {
    return Status(grpc::StatusCode::INTERNAL, "bad response");
  }

  return Status::OK;
}

auto PluginService::createFile(ServerContext*,
                               const bp::createFileRequest* request,
                               bp::createFileResponse* response) -> Status
{
  filedaemon::restore_pkt rp = {};
  auto type = from_grpc(request->ft());

  if (!type) {
    JobLog(bc::JMSG_ERROR, FMT_STRING("received unknown file type {}"),
           int(request->ft()));
    return Status(grpc::StatusCode::FAILED_PRECONDITION, "unknown file type");
  }

  rp.type = type.value();
  if (request->stats().size() != sizeof(rp.statp)) {
    JobLog(bc::JMSG_ERROR,
           FMT_STRING("bad stats value (size = {}, should be {})"),
           request->stats().size(), sizeof(rp.statp));
    return Status(grpc::StatusCode::INVALID_ARGUMENT, "bad stats value");
  }
  memcpy(&rp.statp, request->stats().c_str(), sizeof(rp.statp));
  rp.ofname = request->output_name().c_str();
  rp.olname = request->soft_link_to().c_str();
  switch (request->replace()) {
    case bareos::common::ReplaceIfNewer: {
      rp.replace = REPLACE_IFNEWER;
    } break;
    case bareos::common::ReplaceIfOlder: {
      rp.replace = REPLACE_IFOLDER;
    } break;
    case bareos::common::ReplaceNever: {
      rp.replace = REPLACE_NEVER;
    } break;
    case bareos::common::ReplaceAlways: {
      rp.replace = REPLACE_ALWAYS;
    } break;
    default: {
      JobLog(bc::JMSG_ERROR, FMT_STRING("bad replace value {}"),
             int(request->replace()));
      return Status(grpc::StatusCode::INVALID_ARGUMENT, "bad replace value");
    } break;
  }
  rp.delta_seq = request->delta_seq();

  auto res = funcs.createFile(ctx, &rp);
  if (res != bRC_OK) {
    return Status(grpc::StatusCode::INTERNAL, "bad response");
  }

  switch (rp.create_status) {
    case CF_SKIP: {
      response->set_status(bp::CreateFileStatus::CF_Skip);
    } break;
    case CF_ERROR: {
      response->set_status(bp::CreateFileStatus::CF_Error);
    } break;
    case CF_EXTRACT: {
      response->set_status(bp::CreateFileStatus::CF_Extract);
    } break;
    case CF_CREATED: {
      response->set_status(bp::CreateFileStatus::CF_Created);
    } break;
    case CF_CORE: {
      response->set_status(bp::CreateFileStatus::CF_Core);
    } break;
  }


  return Status::OK;
}
auto PluginService::setFileAttributes(
    ServerContext*,
    const bp::setFileAttributesRequest* request,
    bp::setFileAttributesResponse* response) -> Status
{
  filedaemon::restore_pkt rp = {};
  if (sizeof(rp.statp) != request->stats().size()) {
    return Status(grpc::StatusCode::INVALID_ARGUMENT, "bad stats");
  }
  memcpy(&rp.statp, request->stats().c_str(), sizeof(rp.statp));
  rp.attrEx = request->extended_attributes().c_str();
  rp.ofname = request->file().c_str();
  rp.olname = request->file().c_str();
  rp.where = request->where().c_str();
  rp.create_status = CF_ERROR;
  rp.filedes = kInvalidFiledescriptor;
  auto res = funcs.setFileAttributes(ctx, &rp);
  if (res != bRC_OK) {
    return Status(grpc::StatusCode::INTERNAL, "bad response");
  }

  response->set_set_attributes_in_core(rp.create_status == CF_CORE);

  return Status::OK;
}
auto PluginService::checkFile(ServerContext*,
                              const bp::checkFileRequest* request,
                              bp::checkFileResponse* response) -> Status
{
  return Status::CANCELLED;
}
auto PluginService::getAcl(ServerContext*,
                           const bp::getAclRequest* request,
                           bp::getAclResponse* response) -> Status
{
  filedaemon::acl_pkt acl = {};
  acl.fname = request->file().c_str();

  auto res = funcs.getAcl(ctx, &acl);

  if (res != bRC_OK) {
    return Status(grpc::StatusCode::INTERNAL, "bad response");
  }

  auto* content = response->mutable_content();
  content->set_data(acl.content, acl.content_length);

  return Status::OK;
}
auto PluginService::setAcl(ServerContext*,
                           const bp::setAclRequest* request,
                           bp::setAclResponse*) -> Status
{
  filedaemon::acl_pkt acl = {};
  acl.fname = request->file().c_str();
  acl.content = const_cast<char*>(request->content().data().c_str());
  acl.content_length = request->content().data().size();

  auto res = funcs.setAcl(ctx, &acl);

  if (res != bRC_OK) {
    return Status(grpc::StatusCode::INTERNAL, "bad response");
  }

  return Status::OK;
}
auto PluginService::getXattr(ServerContext*,
                             const bp::getXattrRequest* request,
                             bp::getXattrResponse* response) -> Status
{
  filedaemon::xattr_pkt pkt = {};
  pkt.fname = request->file().c_str();

  auto* attrs = response->mutable_attributes();
  for (;;) {
    auto res = funcs.getXattr(ctx, &pkt);

    if (res != bRC_OK && res != bRC_More) {
      return Status(grpc::StatusCode::INTERNAL, "bad response");
    }

    bp::Xattribute attr{};
    attr.set_key(pkt.name, pkt.name_length);
    attr.set_value(pkt.value, pkt.value_length);

    attrs->Add(std::move(attr));

    if (res == bRC_OK) { break; }
  }
  return Status::OK;
}
auto PluginService::setXattr(ServerContext*,
                             const bp::setXattrRequest* request,
                             bp::setXattrResponse* response) -> Status
{
  filedaemon::xattr_pkt pkt = {};
  pkt.fname = request->file().c_str();

  auto& attr = request->attribute();
  pkt.name = const_cast<char*>(attr.key().c_str());
  pkt.name_length = attr.key().size();
  pkt.value = const_cast<char*>(attr.value().c_str());
  pkt.value_length = attr.value().size();

  auto res = funcs.setXattr(ctx, &pkt);

  if (res != bRC_OK) {
    return Status(grpc::StatusCode::INTERNAL, "bad response");
  }


  return Status::OK;
}

#pragma GCC diagnostic pop
