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

#include "plugin_service.h"
#include <fcntl.h>
#include <grpc/compression.h>
#include "bareos.pb.h"
#include "common.pb.h"
#include "plugin.pb.h"
#include "test_module.h"

#include <sys/sendfile.h>
#include <sys/time.h>

#include <filesystem>
#include <thread>

#include <sys/socket.h>

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
      bc::EventType::Event_StartBackupJob, bc::EventType::Event_EndBackupJob,
      bc::EventType::Event_EndRestoreJob,  bc::EventType::Event_StartRestoreJob,
      bc::EventType::Event_RestoreCommand,
  };

  if (!Register({events.data(), events.size()})) {
    DebugLog(50, FMT_STRING("could not register events!"));
  } else {
    DebugLog(100, FMT_STRING("managed to register my events!"));
  }
  return Status::OK;
}

struct PluginArgParser {
  PluginArgParser(std::string_view s) : base{s} {}

  struct iter {
    iter() = default;
    iter(std::string_view base_) : base{base_} { find_next(); }

    void find_next()
    {
      // move to end of current key-value pair
      base = base.substr(next);

      auto pos = base.find_first_of(":");

      auto kv = base.substr(0, pos);

      // mark end of current key-value pair
      if (pos == base.npos) {
        next = base.size();
      } else {
        next = pos + 1;
      }

      DebugLog(100, FMT_STRING("kv = {}, base = {}"), kv, base);

      auto eq_pos = kv.find_first_of("=");

      std::string_view key_view = kv.substr(0, eq_pos);
      std::string_view val_view
          = (eq_pos == kv.npos) ? std::string_view{} : kv.substr(eq_pos + 1);

      key = std::string(key_view);
      val = std::string(val_view);
    }

    iter& operator++()
    {
      find_next();
      return *this;
    }

    std::pair<std::string, std::string> operator*() { return {key, val}; }

    friend bool operator!=(const iter& left, const iter& right)
    {
      return left.base.data() != right.base.data()
             || left.base.size() != right.base.size();
    }

    std::string key{};
    std::string val{};
    std::size_t next{};
    std::string_view base{};
  };

  iter begin() { return iter{base}; }
  iter end() { return iter{base.substr(base.size())}; }

  std::string_view base;
};


bool PluginService::handle_arguments(std::string_view args)
{
  for (auto [k, v] : PluginArgParser(args)) {
    if (k == "file") {
      files_to_backup.emplace_back(std::move(v));
    } else {
      JobLog(bc::JMSG_ERROR, FMT_STRING("unknown option {} with value {}"), k,
             v);
    }
  }


  return true;
}

auto PluginService::handlePluginEvent(
    ServerContext*,
    const bp::handlePluginEventRequest* request,
    bp::handlePluginEventResponse* response) -> Status
{
  auto& event = request->to_handle();

  DebugLog(100, FMT_STRING("got some event"));

  if (event.has_level()) {
    auto& inner = event.level();
    DebugLog(100, FMT_STRING("got level event {{level = {}}}"), inner.level());
  } else if (event.has_since()) {
    auto& inner = event.since();
    DebugLog(100, FMT_STRING("got since event {{time = {}}}"),
             inner.since().seconds());
  } else if (event.has_job_end()) {
    auto& inner = event.job_end();
    DebugLog(100, FMT_STRING("got job end event ({}). shutting down ..."),
             inner.DebugString());

    shutdown.set_value();
  } else if (event.has_job_start()) {
    auto& inner = event.job_start();
    DebugLog(100, FMT_STRING("got job start event ({})."), inner.DebugString());
  } else if (event.has_end_fileset()) {
    auto& inner = event.end_fileset();
  } else if (event.has_option_plugin()) {
    auto& inner = event.option_plugin();
  } else if (event.has_backup_command()) {
    auto& inner = event.backup_command();
    response->set_res(bp::ReturnCode::RC_OK);
  } else if (event.has_cancel_command()) {
    auto& inner = event.cancel_command();
  } else if (event.has_end_backup_job()) {
    auto& inner = event.end_backup_job();
    DebugLog(100, FMT_STRING("got backup end event ({})."),
             inner.DebugString());
    response->set_res(bp::RC_OK);
  } else if (event.has_end_verify_job()) {
    auto& inner = event.end_verify_job();
  } else if (event.has_plugin_command()) {
    auto& inner = event.plugin_command();

    auto& cmd = inner.data();
    if (handle_arguments(cmd)) {
      response->set_res(bp::RC_OK);
    } else {
      response->set_res(bp::RC_Error);
    }


  } else if (event.has_restore_object()) {
    auto& inner = event.restore_object();
  } else if (event.has_end_restore_job()) {
    auto& inner = event.end_restore_job();
    response->set_res(bp::RC_OK);
  } else if (event.has_restore_command()) {
    auto& inner = event.restore_command();
    response->set_res(bp::RC_OK);  // there is nothing for us to do
  } else if (event.has_vss_init_backup()) {
    auto& inner = event.vss_init_backup();
  } else if (event.has_estimate_command()) {
    auto& inner = event.estimate_command();
  } else if (event.has_start_backup_job()) {
    auto& inner = event.start_backup_job();
    DebugLog(100, FMT_STRING("got start backup job event ({})."),
             inner.DebugString());

    if (files_to_backup.empty()) {
      DebugLog(100, FMT_STRING("no files added -> stop"));
      response->set_res(bp::RC_Stop);
    } else {
      DebugLog(100, FMT_STRING("{} files added -> start"),
               files_to_backup.size());
      response->set_res(bp::RC_OK);
    }
  } else if (event.has_start_verify_job()) {
    auto& inner = event.start_verify_job();
  } else if (event.has_vss_init_restore()) {
    auto& inner = event.vss_init_restore();
  } else if (event.has_start_restore_job()) {
    auto& inner = event.start_restore_job();
  } else if (event.has_vss_close_restore()) {
    auto& inner = event.vss_close_restore();
  } else if (event.has_handle_backup_file()) {
    auto& inner = event.handle_backup_file();
  } else if (event.has_new_plugin_options()) {
    auto& inner = event.new_plugin_options();
  } else if (event.has_vss_backup_complete()) {
    auto& inner = event.vss_backup_complete();
  } else if (event.has_vss_create_snapshot()) {
    auto& inner = event.vss_create_snapshot();
  } else if (event.has_vss_prepare_snapshot()) {
    auto& inner = event.vss_prepare_snapshot();
  } else if (event.has_vss_set_backup_state()) {
    auto& inner = event.vss_set_backup_state();
  } else if (event.has_vss_prepare_for_backup()) {
    auto& inner = event.vss_prepare_for_backup();
  } else if (event.has_vss_backup_add_components()) {
    auto& inner = event.vss_backup_add_components();
  } else if (event.has_vss_restore_set_components_selected()) {
    auto& inner = event.vss_restore_set_components_selected();
  } else if (event.has_vss_restore_load_companents_metadata()) {
    auto& inner = event.vss_restore_load_companents_metadata();
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
  if (files_to_backup.size() == 0) {
    DebugLog(100, FMT_STRING("no more files left; we are done"));
    response->set_result(bp::StartBackupFileResult::SBF_Stop);
    return grpc::Status::OK;
  }

  for (;;) {
    auto& candidate = files_to_backup.back();
    if (!candidate.isdir() || candidate.added_children) {
      // we can use this candidate
      break;
    }

    // at this point we know that 'candidate' is a directory and
    // we have yet to inspect its children

    candidate.added_children = true;

    DebugLog(100, FMT_STRING("searching {}"), candidate.name);

    namespace fs = std::filesystem;
    for (const auto& entry : fs::directory_iterator(candidate.name)) {
      auto path = entry.path().string();

      DebugLog(100, FMT_STRING("adding {}"), path);

      files_to_backup.emplace_back(path);
    }
  }

  auto file = std::move(files_to_backup.back());
  files_to_backup.pop_back();

  JobLog(bc::JMSG_INFO, FMT_STRING("starting backup of file {}"), file.name);

  if (!file.ok) {
    JobLog(bc::JMSG_ERROR, FMT_STRING("could not stat {}"), file.name);
    response->set_result(bp::StartBackupFileResult::SBF_Skip);
    return grpc::Status::OK;
  }

  auto* f = response->mutable_file();
  f->set_stats(&file.s, sizeof(file.s));
  // f->set_delta_seq(0);
  if (file.isdir()) {
    f->set_ft(bco::Directory);
    f->set_no_read(true);
    DebugLog(100, FMT_STRING("{} is a directory"), file.name);
  } else if (file.islnk()) {
    DebugLog(100, FMT_STRING("{} is a link"), file.name);
    f->set_ft(bco::SoftLink);
    f->set_link("link target");
    f->set_no_read(true);
  } else {
    DebugLog(100, FMT_STRING("{} is a file (mode = {}, {}, {})"), file.name,
             file.s.st_mode, file.s.st_mode & S_IFMT, S_IFLNK);
    f->set_ft(bco::RegularFile);
    f->set_no_read(false);
  }
  f->set_portable(true);  // default value
  f->set_file(std::move(file.name));

  response->set_result(bp::SBF_OK);

  return Status::OK;
}
auto PluginService::endBackupFile(ServerContext*,
                                  const bp::endBackupFileRequest* request,
                                  bp::endBackupFileResponse* response) -> Status
{
  if (current_file) { current_file.reset(); }
  if (files_to_backup.size() > 0) {
    response->set_result(bp::EndBackupFileResult::EBF_More);
  } else {
    response->set_result(bp::EndBackupFileResult::EBF_Done);
  }
  return Status::OK;
  // im not sure if this is really the case!
  //  else {
  //   return Status(grpc::StatusCode::FAILED_PRECONDITION,
  //                 "No file is currently open!");
  // }
}
auto PluginService::startRestoreFile(ServerContext*,
                                     const bp::startRestoreFileRequest* request,
                                     bp::startRestoreFileResponse* response)
    -> Status
{
  auto command = request->command();
  // we currently dont have any reason to care for the command as we dont have
  // any options that one could set

  JobLog(bareos::core::JMsgType::JMSG_INFO,
         FMT_STRING("got command for restoring file: {}"), command);
  DebugLog(100, FMT_STRING("start restore file {}"), command);

  return Status::OK;
}
auto PluginService::endRestoreFile(ServerContext*,
                                   const bp::endRestoreFileRequest* request,
                                   bp::endRestoreFileResponse* response)
    -> Status
{
  DebugLog(100, FMT_STRING("stop restore file"));
  return Status::OK;
}
auto PluginService::FileOpen(ServerContext*,
                             const bp::fileOpenRequest* request,
                             bp::fileOpenResponse*) -> Status
{
  if (current_file) {
    DebugLog(100, FMT_STRING("trying to open {} while fd {} is still open"),
             request->file(), current_file->get());
    return Status(grpc::StatusCode::FAILED_PRECONDITION,
                  "there is still a file open");
  }

  int fd = open(request->file().c_str(), request->flags(), request->mode());

  DebugLog(100, FMT_STRING("open(file = {}, flags = {}, mode = {}) -> {}"),
           request->file(), request->flags(), request->mode(), fd);

  if (fd < 0) {
    return Status(grpc::StatusCode::INVALID_ARGUMENT,
                  "could not open specified file given flags/mode");
  }

  current_file = raii_fd(fd);

  return Status::OK;
}
auto PluginService::FileSeek(ServerContext*,
                             const bp::fileSeekRequest* request,
                             bp::fileSeekResponse*) -> Status
{
  if (!current_file) {
    DebugLog(100, FMT_STRING("trying to seek file while it is not open"));
    return Status(grpc::StatusCode::FAILED_PRECONDITION,
                  "there is no open file");
  }

  auto offset = request->offset();
  int whence = 0;

  switch (request->whence()) {
    case bp::SS_StartOfFile: {
      whence = SEEK_SET;
    } break;
    case bp::SS_CurrentPos: {
      whence = SEEK_CUR;
    } break;
    case bp::SS_EndOfFile: {
      whence = SEEK_END;
    } break;
    default:
      return Status(grpc::StatusCode::INVALID_ARGUMENT,
                    "invalid start position for seek");
  }
  auto res = lseek(current_file->get(), offset, whence);
  DebugLog(100,
           FMT_STRING("lseek(fd = {}, offset = {}, whence = {} ({})) -> {}"),
           current_file->get(), offset, whence,
           bp::SeekStart_Name(request->whence()), res);
  if (res < 0) {
    return Status(grpc::StatusCode::UNKNOWN,
                  fmt::format(FMT_STRING("lseek returned error {}: Err={}"),
                              res, strerror(errno)));
  }

  return Status::OK;
}

static bool full_write(int fd, const char* data, size_t size)
{
  size_t bytes_written = 0;
  while (bytes_written < size) {
    auto res = write(fd, data + bytes_written, size - bytes_written);
    DebugLog(100, FMT_STRING("write(fd = {}, buffer = {}, count = {}) -> {}"),
             fd, (void*)(data + bytes_written), size - bytes_written, res);
    if (res < 0) { return false; }
    bytes_written += res;
  }

  return true;
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

  auto res = recvmsg(unix_socket, &msg, MSG_WAITALL);
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

  if (name != expected_name && expected_name == -1) {
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

  return std::make_optional(fd);
}

auto PluginService::FileRead(ServerContext* ctx,
                             const bp::fileReadRequest* request,
                             grpc::ServerWriter<bp::fileReadResponse>* writer)
    -> Status
{
  if (!current_file) {
    DebugLog(50, FMT_STRING("trying to read file while it is not open"));
    return Status(grpc::StatusCode::FAILED_PRECONDITION,
                  "there is no open file");
  }

  auto max_size = request->num_bytes();

  DebugLog(100, FMT_STRING("reading from file {} in {} chunks"),
           current_file->get(), max_size);

  auto res = sendfile(io, current_file->get(), nullptr, max_size);

  if (res < 0) {
    // we need to abort here since we do not know what data was written to the
    // socket
    JobLog(bareos::core::JMsgType::JMSG_FATAL,
           FMT_STRING("Could not send chunk from {} to {}: Err={}"),
           current_file->get(), io, strerror(errno));
    return grpc::Status(grpc::StatusCode::INTERNAL, "Error while reading file");
  }
  bp::fileReadResponse resp;
  resp.set_size(res);
  writer->Write(resp);

  return Status::OK;
}
auto PluginService::FileWrite(ServerContext*,
                              const bp::fileWriteRequest* request,
                              bp::fileWriteResponse* response) -> Status
{
  if (!current_file) {
    DebugLog(50, FMT_STRING("trying to read file while it is not open"));
    return Status(grpc::StatusCode::FAILED_PRECONDITION,
                  "there is no open file");
  }

  auto num_bytes = request->bytes_written();

  auto togo = num_bytes;

  std::vector<char> buffer;
  buffer.resize(64 * 1024);


  while (togo > 0) {
    // we cannot use sendfile here as the infd needs to be mmapable,
    // which a socket is not
    auto chunk_size = std::min(togo, buffer.size());

    if (!full_read(io, buffer.data(), chunk_size)) {
      JobLog(bareos::core::JMsgType::JMSG_FATAL,
             FMT_STRING("Could not read {} bytes from {}: Err={}"), chunk_size,
             io, strerror(errno));
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          "Error while reading chunk");
    }

    if (!full_write(current_file->get(), buffer.data(), chunk_size)) {
      JobLog(bareos::core::JMsgType::JMSG_FATAL,
             FMT_STRING("Could not write {} bytes to {}: Err={}"), chunk_size,
             current_file->get(), strerror(errno));
      return grpc::Status(grpc::StatusCode::INTERNAL,
                          "Error while writing chunk");
    }

    togo -= chunk_size;
  }

  response->set_bytes_written(num_bytes - togo);
  return Status::OK;
}
auto PluginService::FileClose(ServerContext*,
                              const bp::fileCloseRequest* request,
                              bp::fileCloseResponse* response) -> Status
{
  if (!current_file) {
    DebugLog(100, FMT_STRING("trying to close file while it is not open"));
    return Status(grpc::StatusCode::FAILED_PRECONDITION,
                  "there is no open file");
  }

  current_file.reset();
  return Status::OK;
}
auto PluginService::createFile(ServerContext*,
                               const bp::createFileRequest* request,
                               bp::createFileResponse* response) -> Status
{
  JobLog(bareos::core::JMsgType::JMSG_INFO,
         FMT_STRING("{{output = {} (link to = {})}}"), request->output_name(),
         request->soft_link_to());

  response->set_status(bareos::plugin::CF_Core);

  return Status::OK;
}
auto PluginService::setFileAttributes(
    ServerContext*,
    const bp::setFileAttributesRequest* request,
    bp::setFileAttributesResponse* response) -> Status
{
  JobLog(bc::JMSG_INFO, FMT_STRING("setting attributes for {} in core"),
         request->file());

  if (!current_file) {
    DebugLog(100, FMT_STRING("trying to seek file while it is not open"));
    return Status(grpc::StatusCode::FAILED_PRECONDITION,
                  "there is no open file");
  }

  auto& name = request->file();
  auto& attrs = request->stats();

  struct stat s;
  if (attrs.size() != sizeof(s)) {
    return grpc::Status(
        grpc::StatusCode::INVALID_ARGUMENT,
        fmt::format(
            FMT_STRING(
                "stats is not a valid stats object: size mismatch {} != {}"),
            attrs.size(), sizeof(s)));
  }

  memcpy(&s, attrs.data(), attrs.size());


  if (fchown(current_file->get(), s.st_uid, s.st_gid) < 0) {
    return grpc::Status(grpc::StatusCode::INTERNAL,
                        fmt::format(FMT_STRING("failed setting owner. Err={}"),
                                    strerror(errno)));
  }
  if (fchmod(current_file->get(), s.st_mode) < 0) {
    return grpc::Status(
        grpc::StatusCode::INTERNAL,
        fmt::format(FMT_STRING("failed setting permissions. Err={}"),
                    strerror(errno)));
  }
  if (struct timeval times[2] = {{s.st_atime, 0}, {s.st_mtime, 0}};
      futimes(current_file->get(), times) < 0) {
    return grpc::Status(
        grpc::StatusCode::INTERNAL,
        fmt::format(FMT_STRING("failed setting file times. Err={}"),
                    strerror(errno)));
  }

  // response->set_set_attributes_in_core(true);
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
  (void)request;
  response->mutable_content()->set_data("");
  return Status::OK;
}
auto PluginService::setAcl(ServerContext*,
                           const bp::setAclRequest* request,
                           bp::setAclResponse* response) -> Status
{
  return Status::CANCELLED;
}
auto PluginService::getXattr(ServerContext*,
                             const bp::getXattrRequest* request,
                             bp::getXattrResponse* response) -> Status
{
  return Status::OK;
}
auto PluginService::setXattr(ServerContext*,
                             const bp::setXattrRequest* request,
                             bp::setXattrResponse* response) -> Status
{
  return Status::CANCELLED;
}

#pragma GCC diagnostic pop
