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

#include "plugin_service.h"
#include <fcntl.h>
#include <poll.h>
#include "bareos.pb.h"
#include "common.pb.h"
#include "backup.pb.h"
#include "restore.pb.h"
#include "filed/fd_plugins.h"
#include "include/filetypes.h"
#include "plugin.pb.h"
#include "bridge_module.h"
#include "../util.h"
#include "plugins/filed/grpc/prototools.h"

#include <sys/sendfile.h>

#include <filesystem>
#include <fmt/format.h>
#include <thread>
#include <variant>

#include <sys/socket.h>

#include <dlfcn.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"

namespace {

constexpr struct {
  int write_mode = S_IRUSR | S_IWUSR;
  int write_flags = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
} constants;

std::optional<int> from_grpc(bco::ObjectType ot)
{
  switch (ot) {
    case bareos::common::OBJECT_TYPE_UNSPECIFIED:
      return std::nullopt;
    case bareos::common::Blob:
      return FT_RESTORE_FIRST;
    case bareos::common::Config:
      return FT_PLUGIN_CONFIG;
    case bareos::common::FilledConfig:
      return FT_PLUGIN_CONFIG_FILLED;
    case bareos::common::ObjectType_INT_MIN_SENTINEL_DO_NOT_USE_:
      return std::nullopt;
    case bareos::common::ObjectType_INT_MAX_SENTINEL_DO_NOT_USE_:
      return std::nullopt;
  }

  return std::nullopt;
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

    case bareos::common::FILE_TYPE_UNSPECIFIED:
    case bareos::common::UnchangedFile:
    case bareos::common::UnchangedDirectory:
    case bareos::common::RestoreObject:
    case bareos::common::FileType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case bareos::common::FileType_INT_MAX_SENTINEL_DO_NOT_USE_:
      return std::nullopt;
  }
  return std::nullopt;
}

bool is_event_allowed(std::uint64_t allowed_event_types,
                      filedaemon::bEvent* event)
{
  return (event->eventType & (1 << allowed_event_types)) != 0;
}

std::string join(const std::vector<std::string>& x)
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

enum class UnhandledType
{
  Unknown,
  Plugin,
  Base,
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
      return bco::UnchangedFile;
    } break;
    case FT_DIRNOCHG: {
      return bco::UnchangedDirectory;
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
}  // namespace

namespace bb = bareos::backup;
namespace br = bareos::restore;

struct RestoreStateMachine {
  RestoreStateMachine(prototools::ProtoBidiStream* stream_,
                      std::string restore_cmd_,
                      filedaemon::PluginFunctions* funs_,
                      PluginContext* ctx_)
      : stream{stream_}
      , restore_cmd{std::move(restore_cmd_)}
      , funs{funs_}
      , ctx{ctx_}
  {
  }

  bool run()
  {
    for (;;) {
      if (have_msg(br::RestoreRequest::RequestCase::kDone)) {
        // we are done
        (void)pop_msg();
        break;
      }

      auto file = start_restore_file();

      if (!file.skip) {
        if (file.do_extract) {
          if (!file_open(&file)) { goto end_file; }

          set_file_attributes(&file);

          while (!file.data_done) { file_write(&file); }

          file_close(&file);
        } else {
          set_file_attributes(&file);
        }

        set_acls(&file);

        set_xattrs(&file);
      }

    end_file:
      end_restore_file(&file);
    }

    return true;
  }

 private:
  using ReqCase = br::RestoreRequest::RequestCase;

  struct FileInProgress {
    br::BeginFileRequest data;
    std::size_t offset;
    bool do_extract;
    bool data_done;
    bool skip;
  };

  struct RestoreError : std::runtime_error {
    template <typename... Args>
    RestoreError(Args&&... args)
        : std::runtime_error(std::forward<Args>(args)...)
    {
    }
  };

 private:
  void init_restore_pkt(filedaemon::restore_pkt* pkt,
                        const br::BeginFileRequest& req)
  {
    pkt->pkt_size = sizeof(*pkt);
    pkt->pkt_end = sizeof(*pkt);
    switch (req.replace()) {
      case bareos::common::REPLACE_IF_NEWER: {
        pkt->replace = REPLACE_IFNEWER;
      } break;
      case bareos::common::REPLACE_IF_OLDER: {
        pkt->replace = REPLACE_IFOLDER;
      } break;
      case bareos::common::REPLACE_NEVER: {
        pkt->replace = REPLACE_NEVER;
      } break;
      case bareos::common::REPLACE_ALWAYS: {
        pkt->replace = REPLACE_ALWAYS;
      } break;
      default: {
        throw RestoreError(
            fmt::format("core sent bad replace value {}", int(req.replace())));
      } break;
    }

    pkt->where = req.where().c_str();
    pkt->olname = req.soft_link_to().c_str();
    pkt->ofname = req.output_name().c_str();
    if (auto ft = from_grpc(req.file_type())) {
      pkt->type = *ft;
    } else {
      throw RestoreError(
          fmt::format("core sent bad file type {}", int(req.file_type())));
    }
    stat_grpc_to_native(&pkt->statp, req.stats());
  }


  FileInProgress start_restore_file()
  {
    if (!have_msg(ReqCase::kBeginFile)) {
      throw RestoreError("expected begin file");
    }

    auto& file = pop_msg()->begin_file();
    br::RestoreResponse answer[1];
    auto* response = answer->mutable_begin_file();

    bRC res = funs->startRestoreFile(ctx, restore_cmd.c_str());

    FileInProgress created{};
    created.data.CopyFrom(file);

    if (res != bRC_OK) {
      response->set_system_error(errno);
      if (!answer_with(answer)) {
        throw RestoreError("could not send BeginFile answer to core");
      }

      created.skip = true;
      return created;
    }

    filedaemon::restore_pkt pkt = {};
    init_restore_pkt(&pkt, created.data);

    res = funs->createFile(ctx, &pkt);
    if (res != bRC_OK) {
      response->set_system_error(errno);
      if (!answer_with(answer)) {
        throw RestoreError("could not send BeginFile answer to core");
      }

      created.skip = true;
      return created;
    }

    bool create_in_core = false;

    switch (pkt.create_status) {
      case CF_ERROR: {
        response->set_system_error(errno);
        created.skip = true;
      } break;
      case CF_SKIP: {
        response->set_success(br::CREATION_SKIP);
        created.skip = true;
      } break;
      case CF_CORE: {
        response->set_success(br::CREATION_CORE);
        create_in_core = true;
        created.do_extract = true;
      } break;
        // sadly a lot of plugins take advantage of the fact
        // that bareos doesnt check this value completely,
        // so 0 (and anything apart from the rest is also CF_EXTRACT)
      case 0:
        [[fallthrough]];
      case CF_EXTRACT: {
        response->set_success(br::CREATION_PLUGIN);
        created.do_extract = true;
      } break;
      case CF_CREATED: {
        response->set_success(br::CREATION_NODATA);
      } break;

        // we want to be strict here and disallow weird statuses;
        // we only allow 0 as _all_ python plugins use that
      default: {
        throw RestoreError(
            fmt::format("plugin returned create_status {}", pkt.create_status));
      } break;
    }

    if (!answer_with(answer)) {
      throw RestoreError("could not send BeginFile answer to core");
    }

    if (create_in_core) {
      if (!have_msg(ReqCase::kCoreCreationDone)) {
        throw RestoreError(
            "core did not respond correctly. Expected core_creation_done!");
        created.skip = true;
        return created;
      }

      auto& creation_done = pop_msg()->core_creation_done();
      if (creation_done.success()) {
        DebugLog(150, "core successfully created the file");
      } else {
        DebugLog(150, "core did not manage to create the file");
        created.skip = true;
      }
    }

    return created;
  }

  bool file_open(FileInProgress* file)
  {
    filedaemon::io_pkt io = {};
    io.func = filedaemon::IO_OPEN;
    io.fname = file->data.output_name().c_str();
    io.flags = constants.write_flags;
    io.mode = constants.write_mode;

    bRC res = funs->pluginIO(ctx, &io);

    if (res != bRC_OK) {
      JobLog(bc::JMSG_ERROR, "could not open {}", file->data.output_name());
      return false;
    }

    return true;
  }
  bool file_write(FileInProgress* file)
  {
    if (!have_msg(ReqCase::kFileData)) {
      file->data_done = true;
      return true;
    }

    auto& fdata = pop_msg()->file_data();

    if (fdata.offset() != file->offset) {
      filedaemon::io_pkt io = {};
      io.func = filedaemon::IO_SEEK;
      io.whence = SEEK_SET;
      io.offset = fdata.offset();

      bRC res = funs->pluginIO(ctx, &io);

      if (res != bRC_OK) {
        JobLog(bc::JMSG_ERROR, "could not seek to offset {} in file {}",
               fdata.offset(), file->data.output_name());
        return false;
      }
    }

    filedaemon::io_pkt io = {};
    io.func = filedaemon::IO_WRITE;
    io.count = fdata.data().size();
    io.buf = const_cast<char*>(fdata.data().data());
    io.status = 0;

    bRC res = funs->pluginIO(ctx, &io);

    if (res != bRC_OK) {
      JobLog(bc::JMSG_ERROR, "could not write {} bytes to file {} at offset {}",
             fdata.data().size(), file->data.output_name(), fdata.offset());
      return false;
    }

    if (io.status != io.count) {
      JobLog(bc::JMSG_ERROR,
             "short write of  {}/{} bytes to file {} at offset {}", io.count,
             fdata.data().size(), file->data.output_name(), fdata.offset());
      return false;
    }

    return true;
  }

  bool file_close(FileInProgress* file)
  {
    filedaemon::io_pkt io = {};
    io.func = filedaemon::IO_CLOSE;

    bRC res = funs->pluginIO(ctx, &io);

    if (res != bRC_OK) {
      JobLog(bc::JMSG_ERROR, "could not close {}", file->data.output_name());
      return false;
    }

    return true;
  }
  bool set_file_attributes(FileInProgress* file)
  {
    filedaemon::restore_pkt pkt = {};
    init_restore_pkt(&pkt, file->data);

    auto res = funs->setFileAttributes(ctx, &pkt);
    if (res != bRC_OK) {
      JobLog(bc::JMSG_ERROR, "could not set attributes on {}",
             file->data.output_name());
      return false;
    }

    if (pkt.create_status == CF_CORE) {
      JobLog(bc::JMSG_ERROR,
             "the grpc layer does not support set_file_attributes() in core, "
             "and neither does the core. Attributes of '{}' are not restored",
             file->data.output_name());
      return false;
    }

    return true;
  }
  bool set_acls(FileInProgress* file)
  {
    if (!have_msg(ReqCase::kAcl)) { return true; }

    auto& acl = pop_msg()->acl();

    filedaemon::acl_pkt pkt[1] = {};
    pkt->pkt_size = sizeof(*pkt);
    pkt->fname = file->data.output_name().c_str();
    pkt->content_length = acl.acl_data().size();
    pkt->content = const_cast<char*>(acl.acl_data().c_str());
    pkt->pkt_end = sizeof(*pkt);

    auto res = funs->setAcl(ctx, pkt);

    if (res != bRC_OK) {
      JobLog(bc::JMSG_ERROR, "could not set acls on {}",
             file->data.output_name());
      return false;
    }

    return true;
  }
  bool set_xattrs(FileInProgress* file)
  {
    while (have_msg(ReqCase::kXattr)) {
      auto& xattr = pop_msg()->xattr();

      filedaemon::xattr_pkt pkt[1] = {};
      pkt->pkt_size = sizeof(*pkt);
      pkt->fname = file->data.output_name().c_str();
      pkt->name_length = xattr.name().size();
      pkt->name = const_cast<char*>(xattr.name().c_str());
      pkt->value_length = xattr.content().size();
      pkt->value = const_cast<char*>(xattr.content().c_str());
      pkt->pkt_end = sizeof(*pkt);

      auto res = funs->setXattr(ctx, pkt);

      if (res != bRC_OK) {
        JobLog(bc::JMSG_ERROR, "could not set xattr {} on {}", xattr.name(),
               file->data.output_name());
        return false;
      }
    }

    return true;
  }
  bool end_restore_file(FileInProgress* file)
  {
    for (;;) {
      if (have_msg(ReqCase::kBeginFile) || have_msg(ReqCase::kDone)) {
        bRC res = funs->endRestoreFile(ctx);
        if (res != bRC_OK) {
          JobLog(bc::JMSG_ERROR, "error during end_restore_file() for {}",
                 file->data.output_name());
          return false;
        }
        return true;
      }

      // ignore the message
      (void)pop_msg();
    }
    return true;
  }

  inline bool pull_msg()
  {
    if (current_request) { return true; }

    arena.Reset();
    current_request
        = google::protobuf::Arena::Create<br::RestoreRequest>(&arena);

    if (!stream->Read(*current_request)) {
      current_request = nullptr;
      return false;
    }

    return true;
  }

  inline bool have_msg(br::RestoreRequest::RequestCase expected)
  {
    if (!pull_msg()) { return false; }

    return current_request->request_case() == expected;
  }

  inline br::RestoreRequest* pop_msg()
  {
    if (!pull_msg()) { return nullptr; }

    return std::exchange(current_request, nullptr);
  }

  bool answer_with(const br::RestoreResponse* resp)
  {
    return stream->Write(*resp);
  }

 private:
  google::protobuf::Arena arena{};
  br::RestoreRequest* current_request{nullptr};

  prototools::ProtoBidiStream* stream;
  std::string restore_cmd;
  filedaemon::PluginFunctions* funs;
  PluginContext* ctx;
};

struct cached_event {
  filedaemon::bEvent event{};

  intptr_t ptr_value;
  std::vector<char> data;

  filedaemon::bEvent* type() { return &event; }
  void* value()
  {
    if (data.empty()) {
      return reinterpret_cast<void*>(ptr_value);
    } else {
      return reinterpret_cast<void*>(data.data());
    }
  }
};

struct PluginThread {
  bool start_backup(const bp::BeginBackupRequest&);
  bool start_restore(const bp::BeginRestoreRequest&);
  bool start_verify(const bp::BeginVerifyRequest&) { return false; }
  bool start_estimate(const bp::BeginEstimateRequest&) { return false; }
  std::optional<bRC> handle_event(const bpe::Event&);

  PluginThread(const filedaemon::CoreFunctions* funs,
               prototools::ProtoBidiStream* stream,
               std::string_view plugin_stem)
      : data_transfer{stream}, plugin_path{plugin_stem}, core_funs{funs}
  {
  }
  PluginThread(PluginThread const&) = delete;
  PluginThread& operator=(PluginThread const&) = delete;
  PluginThread(PluginThread&&) = delete;
  PluginThread& operator=(PluginThread&&) = delete;

  void allow_events(std::uint64_t event_mask)
  {
    auto current_events = registered_events.load();
    for (;;) {
      auto next_events = current_events | event_mask;
      if (registered_events.compare_exchange_weak(current_events,
                                                  next_events)) {
        break;
      }
    }
  }

  void disallow_events(std::uint64_t event_mask)
  {
    auto current_events = registered_events.load();
    for (;;) {
      auto next_events = current_events & ~event_mask;
      if (registered_events.compare_exchange_weak(current_events,
                                                  next_events)) {
        break;
      }
    }
  }

  void start()
  {
    executor
        = std::thread([](PluginThread* plugin) { plugin->execute(); }, this);
  }

  ~PluginThread()
  {
    if (executor.joinable()) { executor.join(); }
  }

 private:
  struct end {};

  struct backup {
    std::string command;
    std::uint64_t max_record_size;
    std::vector<char> flags;

    bool backup_acl;
    bool backup_xattr;
    bool portable;
    bool no_atime;
  };

  struct restore {
    std::string command;
  };

  struct handle_event_task {
    filedaemon::bEventType type;
    union {
      const char* ptr;
      filedaemon::restore_object_pkt* pkt;
      intptr_t value;
    } data = {};
    std::size_t size;

    void set_data(intptr_t value)
    {
      data.value = value;
      size = 0;
    }

    void* get_data()
    {
      void* ptr;
      static_assert(sizeof(ptr) == sizeof(data));
      memcpy(&ptr, &data, sizeof(ptr));
      return ptr;
    }

    const void* get_data() const
    {
      void* ptr;
      static_assert(sizeof(ptr) == sizeof(data));
      memcpy(&ptr, &data, sizeof(ptr));
      return ptr;
    }

    void set_data(const std::string& s)
    {
      data.ptr = s.data();
      size = s.size();
    }

    void set_data(filedaemon::restore_object_pkt* obj)
    {
      data.pkt = obj;
      // not cachable currently
      size = std::numeric_limits<std::size_t>::max();
    }

    std::promise<bRC> result;
  };

  using task_type = std::variant<backup, restore, handle_event_task, end>;

 private:
  void handle_task(const end&) { quit = true; }

  void handle_task(backup& task)
  {
#if 0
    if (task.flags.size() != sizeof(filedaemon::save_pkt::flags)) {
      JobLog(bc::JMSG_FATAL,
             "core send flags of wrong size: expected {}, got {}",
             sizeof(filedaemon::save_pkt::flags), task.flags.size());

      // send done

      bb::Backup done[1];
      done->mutable_done();
      send_backup_data(done);

      return;
    }
#endif

    std::string current_file_name;

    for (;;) {
      filedaemon::save_pkt pkt = {};
      pkt.cmd = const_cast<char*>(task.command.c_str());
      pkt.portable = task.portable;

#if 0
      memcpy(pkt.flags, task.flags.data(), sizeof(pkt.flags));
#endif

      bRC result = plugin_funs->startBackupFile(&ctx, &pkt);
      if (result == bRC_Skip) { continue; }
      if (result == bRC_Stop) { break; }
      if (result != bRC_OK) {
        JobLog(bc::JMSG_FATAL, "plugin returned {} on start_backup_file()",
               static_cast<int>(result));
        break;
      }

      std::variant type = to_grpc(pkt.type);

      bb::Backup data[1];
      bb::BeginObject* bo = data->mutable_begin_object();

      bool read_file = false;
      bool is_file = false;

      // TODO: these continues should still call end_backup_file()
      if (auto* object_type = std::get_if<bco::ObjectType>(&type)) {
        if (!pkt.object_name) {
          JobLog(bc::JMSG_ERROR,
                 "plugin did not set an object name in start_backup_file; "
                 "skipping");

          continue;
        }
        if (*object_type == bco::OBJECT_TYPE_UNSPECIFIED) {
          JobLog(bc::JMSG_FATAL, "internal error occured; aborting");

          break;
        }

        auto* obj = bo->mutable_object();
        obj->set_index(pkt.index);
        obj->set_name(pkt.object_name);
        obj->set_data(pkt.object, pkt.object_len);
        obj->set_type(*object_type);
      } else if (auto* file_type = std::get_if<bco::FileType>(&type)) {
        if (!pkt.fname) {
          JobLog(bc::JMSG_ERROR,
                 "plugin did not set name in start_backup_file; skipping");

          continue;
        }
        if (*file_type == bco::FILE_TYPE_UNSPECIFIED) {
          JobLog(bc::JMSG_FATAL, "internal error occured; aborting");
          break;
        }

        is_file = true;
        auto* file = bo->mutable_file();

        current_file_name.assign(pkt.fname);

        if (!pkt.no_read) { read_file = true; }
        file->set_no_read(pkt.no_read);
        file->set_portable(pkt.portable);
        if (BitIsSet(FO_DELTA, pkt.flags)) {
          file->set_delta_seq(pkt.delta_seq);
        }
        file->set_offset_backup(BitIsSet(FO_OFFSETS, pkt.flags));
        file->set_sparse_backup(BitIsSet(FO_SPARSE, pkt.flags));
        file->set_ft(*file_type);

        stat_native_to_grpc(file->mutable_stats(), &pkt.statp);

        file->set_file(pkt.fname);

        if (*file_type == bco::SoftLink) { file->set_link(pkt.link); }
      } else if (auto* error_type = std::get_if<bco::FileErrorType>(&type)) {
        if (!pkt.fname) {
          JobLog(bc::JMSG_ERROR,
                 "plugin did not set name in start_backup_file; skipping");

          continue;
        }
        if (*error_type == bco::FILE_ERROR_TYPE_UNSPECIFIED) {
          JobLog(bc::JMSG_FATAL, "internal error occured; aborting");
          break;
        }

        auto* error = bo->mutable_error();
        error->set_file(pkt.fname);
        error->set_error(*error_type);
      } else {
        JobLog(bc::JMSG_FATAL, "received unhandled type {} from the plugin",
               pkt.type);

        break;
      }

      send_backup_data(data);

      if (is_file) {
        if (read_file) {
          bool stop_reading = false;
          std::optional<int> io_in_core_fd = std::nullopt;

          // file_open
          {
            filedaemon::io_pkt io{};
            io.func = filedaemon::IO_OPEN;
            io.mode = 0;
            io.flags = O_RDONLY | O_BINARY;
            if (task.no_atime) { io.flags |= O_NOATIME; }
            io.fname = current_file_name.c_str();

            auto fo_res = plugin_funs->pluginIO(&ctx, &io);
            if (fo_res != bRC_OK) {
              JobLog(bc::JMSG_ERROR, "plugin could not open '{}'",
                     current_file_name);
              bb::Backup error[1];
              auto* ptr = error->mutable_file_error();
              ptr->set_on_action(bco::FILE_ACTION_OPEN);
              ptr->set_message(strerror(errno));
              send_backup_data(error);

              stop_reading = true;
            }

            if (io.status == IoStatus::do_io_in_core) {
              io_in_core_fd = io.filedes;
              // TODO: check that this is a valid fd ...
            }
          }


          // while true: file_read
          bb::Backup file_data[1];
          auto* buffer = file_data->mutable_file_data()->mutable_data();

          if (!stop_reading) {
            if (io_in_core_fd) {
              DebugLog(150, "saving data via fd");

              // make sure the test understands that we do "io in core"-lite
              DebugLog(150, "bread handled in core");
              for (;;) {
                buffer->resize(task.max_record_size);
                ssize_t bytes_read
                    = read(*io_in_core_fd, buffer->data(), buffer->size());
                if (bytes_read == 0) { break; }
                if (bytes_read < 0) {
                  std::string emsg = strerror(errno);
                  JobLog(bc::JMSG_ERROR,
                         "an error occured while reading {} via the fd "
                         "provided by the plugin: {}",
                         current_file_name, emsg);

                  bb::Backup error[1];
                  auto* ptr = error->mutable_file_error();
                  ptr->set_on_action(bco::FILE_ACTION_READ);
                  ptr->set_message(emsg);
                  send_backup_data(error);
                }
                buffer->resize(bytes_read);
                send_backup_data(file_data);
              }
            } else {
              DebugLog(150, "saving data via plugin");
              for (;;) {
                buffer->resize(task.max_record_size);
                filedaemon::io_pkt io{};
                io.func = filedaemon::IO_READ;
                io.count = buffer->size();
                io.buf = buffer->data();
                auto fr_res = plugin_funs->pluginIO(&ctx, &io);

                if (fr_res != bRC_OK || io.status < 0) {
                  JobLog(bc::JMSG_ERROR, "an error occured while reading '{}'",
                         current_file_name);

                  bb::Backup error[1];
                  auto* ptr = error->mutable_file_error();
                  ptr->set_on_action(bco::FILE_ACTION_READ);
                  ptr->set_message(strerror(errno));
                  send_backup_data(error);
                }

                if (io.status == 0) { break; }

                buffer->resize(io.status);
                send_backup_data(file_data);
              }
            }
          }

          // file_close
          {
            filedaemon::io_pkt io{};
            io.func = filedaemon::IO_CLOSE;
            auto fc_res = plugin_funs->pluginIO(&ctx, &io);

            if (fc_res != bRC_OK) {
              bb::Backup error[1];
              auto* ptr = error->mutable_file_error();
              ptr->set_on_action(bco::FILE_ACTION_CLOSE);
              ptr->set_message(strerror(errno));
              send_backup_data(error);
            }
          }
        }

        if (task.backup_acl) {
          DebugLog(150, "saving acls");


          filedaemon::acl_pkt acl_pkt{};
          acl_pkt.fname = current_file_name.c_str();
          auto ares = plugin_funs->getAcl(&ctx, &acl_pkt);

          if (ares == bRC_OK) {
            if (acl_pkt.content && acl_pkt.content_length) {
              bb::Backup acl;

              auto* ptr = acl.mutable_acl();
              ptr->set_acl_data(acl_pkt.content, acl_pkt.content_length);

              send_backup_data(&acl);
            }
          } else {
            JobLog(bc::JMSG_ERROR, "plugin could not backup acls of '{}'",
                   current_file_name);
            bb::Backup error[1];
            auto* ptr = error->mutable_file_error();
            ptr->set_on_action(bco::FILE_ACTION_ACL);
            ptr->set_message(strerror(errno));

            send_backup_data(error);
          }
        }

        if (task.backup_xattr) {
          DebugLog(150, "saving xattrs");

          filedaemon::xattr_pkt xattr_pkt{};
          for (;;) {
            xattr_pkt.fname = current_file_name.c_str();
            auto ares = plugin_funs->getXattr(&ctx, &xattr_pkt);

            if (ares == bRC_OK || ares == bRC_More) {
              if (xattr_pkt.name_length && xattr_pkt.name) {
                bb::Backup xattr;

                auto* ptr = xattr.mutable_xattr();
                ptr->set_content(xattr_pkt.value, xattr_pkt.value_length);
                ptr->set_name(xattr_pkt.name, xattr_pkt.name_length);

                send_backup_data(&xattr);
              }
            } else {
              JobLog(bc::JMSG_ERROR, "plugin could not backup xattrs of '{}'",
                     current_file_name);

              bb::Backup error[1];
              auto* ptr = error->mutable_file_error();
              ptr->set_on_action(bco::FILE_ACTION_XATTR);
              ptr->set_message(strerror(errno));
              send_backup_data(error);
            }

            if (ares != bRC_More) { break; }
          }
        }
      }

      auto ebf_res = plugin_funs->endBackupFile(&ctx);
      if (ebf_res == bRC_OK) { break; }
      if (ebf_res == bRC_Error) {
        JobLog(bc::JMSG_ERROR, "end_backup_file() returned error");
      }

      if (ebf_res != bRC_More) {
        JobLog(bc::JMSG_FATAL,
               "end_backup_file() returned unexpected return value {}",
               static_cast<int>(ebf_res));
        break;
      }
    }

    // send done

    bb::Backup done[1];
    done->mutable_done();
    send_backup_data(done);
  }

  void handle_task(restore& task)
  {
    RestoreStateMachine sm{data_transfer, task.command, plugin_funs, &ctx};
    try {
      sm.run();
    } catch (const std::exception& ex) {
      JobLog(bc::JMSG_FATAL, "restore was aborted: {}", ex.what());
      // TODO: run until done, always responding with bRC_Error
      // sm.finish();
    }
  }

  void handle_task(handle_event_task& task)
  {
    filedaemon::bEvent event = {.eventType = task.type};

    switch (task.type) {
      case filedaemon::bEventRestoreObject: {
        if (auto* rop
            = static_cast<filedaemon::restore_object_pkt*>(task.get_data())) {
          std::string_view cmd = rop->plugin_name;
          DebugLog(100, FMT_STRING("got cmd string \"{}\""), cmd);
          std::optional pstring = inferior_setup(cmd);

          if (!pstring) {
            task.result.set_value(bRC_Error);
            return;
          }

          DebugLog(100, FMT_STRING("using cmd string \"{}\" for the plugin"),
                   pstring.value());

          auto* old = rop->plugin_name;
          rop->plugin_name = const_cast<char*>(pstring->data());
          auto res = plugin_funs->handlePluginEvent(&ctx, &event, (void*)rop);
          rop->plugin_name = old;

          task.result.set_value(res);
          return;
        } else {
          task.result.set_value(bRC_OK);
        }
      } break;
      case filedaemon::bEventPluginCommand:
        [[fallthrough]];
      case filedaemon::bEventEstimateCommand:
        [[fallthrough]];
      case filedaemon::bEventBackupCommand:
        [[fallthrough]];
      case filedaemon::bEventRestoreCommand:
        [[fallthrough]];
      case filedaemon::bEventNewPluginOptions: {
        std::string_view cmd{(char*)task.get_data()};
        DebugLog(100, FMT_STRING("got cmd string \"{}\""), cmd);
        std::optional pstring = inferior_setup(cmd);
        if (!pstring) {
          task.result.set_value(bRC_Error);
          return;
        }

        DebugLog(100, FMT_STRING("using cmd string \"{}\" for the plugin"),
                 pstring.value());

        auto cached = pop_cached_events();

        auto allowed_events = registered_events.load();

        DebugLog(100, FMT_STRING("inserting {} cached events ({:x})"),
                 cached.size(), allowed_events);

        bool cached_err = false;
        for (auto& cached_event : cached) {
          if (!is_event_allowed(allowed_events, cached_event.type())) {
            DebugLog(100, FMT_STRING("skipping cached {}-event ({})"),
                     cached_event.type()->eventType, cached_event.value());
            continue;
          }

          DebugLog(100, FMT_STRING("inserting cached {}-event ({})"),
                   cached_event.type()->eventType, cached_event.value());
          if (plugin_funs->handlePluginEvent(&ctx, cached_event.type(),
                                             cached_event.value())
              == bRC_Error) {
            DebugLog(100, FMT_STRING(" ... lead to failure"));
            cached_err = true;
          }
        }

        DebugLog(100, FMT_STRING("plugin funs = {}"), (void*)plugin_funs);
        DebugLog(100, FMT_STRING("handle plugin events = {}"),
                 (void*)plugin_funs->handlePluginEvent);
        auto result = plugin_funs->handlePluginEvent(&ctx, &event,
                                                     (void*)pstring->data());
        if (cached_err) {
          task.result.set_value(bRC_Error);
        } else {
          task.result.set_value(result);
        }

      } break;
      default: {
        if (!plugin_funs) {
          cache_event(task);
          task.result.set_value(bRC_OK);
        } else {
          bRC result
              = plugin_funs->handlePluginEvent(&ctx, &event, task.get_data());
          task.result.set_value(result);
        }
      } break;
    }
  }

  void execute()
  {
    decltype(tasks_to_execute) current_tasks;

    while (!quit) {
      {
        std::unique_lock task_lock{task_mutex};

        if (tasks_to_execute.empty()) {
          task_added.wait(task_lock,
                          [&]() { return !tasks_to_execute.empty(); });
        }

        std::swap(tasks_to_execute, current_tasks);
      }

      for (auto& task : current_tasks) {
        std::visit([&](auto&& t) { return this->handle_task(t); }, task);
      }

      current_tasks.clear();
    }
  }

  std::optional<std::string_view> inferior_setup(std::string_view cmd)
  {
    ctx.core_private_context = this;

    if (!ctx.plugin) {
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

      ctx.plugin = &plugin_data;

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
        } else {
          DebugLog(150, FMT_STRING("could not load {}: {}"), candidate,
                   dlerror());
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

      auto load_res = (*func)(&core_api,
                              const_cast<filedaemon::CoreFunctions*>(core_funs),
                              &plugin_info, &plugin_funs);

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

      auto new_res = plugin_funs->newPlugin(&ctx);
      if (new_res != bRC_OK) {
        JobLog(bc::JMSG_ERROR, FMT_STRING("bad response to newPlugin"));
        return std::nullopt;
      }

      JobLog(bc::JMSG_INFO, FMT_STRING("loaded inferior successfully"));
    }

    return cmd;
  }

  bool cache_event(const handle_event_task& task)
  {
    auto& cached = cached_events.emplace_back();
    cached.event.eventType = task.type;
    if (task.size == std::numeric_limits<decltype(task.size)>::max()) {
      return false;
    } else if (task.size != 0) {
      DebugLog(200, FMT_STRING("caching data of size {} (event = {})"),
               task.size, static_cast<int>(task.type));
      cached.data.assign(static_cast<const char*>(task.get_data()),
                         static_cast<const char*>(task.get_data()) + task.size);
    } else {
      // size is 0 here, so no actual data was stored, i.e. we only care
      // about the value of the pointer
      cached.ptr_value = reinterpret_cast<intptr_t>(task.get_data());
    }
    return true;
  }

  std::vector<cached_event> pop_cached_events()
  {
    return std::exchange(cached_events, {});
  }

  bool send_backup_data(bb::Backup* data)
  {
    return data_transfer->Write(*data);
  }

 private:
  std::mutex task_mutex{};
  std::condition_variable task_added{};
  std::vector<task_type> tasks_to_execute;
  std::thread executor;

  std::atomic<std::uint64_t> registered_events;

  // the following values may only be touched by the plugin thread
  PluginContext ctx{};
  prototools::ProtoBidiStream* data_transfer{};

  bool quit = false;

  std::string plugin_path;

  std::vector<cached_event> cached_events;

  Plugin plugin_data{};
  filedaemon::PluginApiDefinition core_api{};
  const filedaemon::CoreFunctions* core_funs{};
  filedaemon::PluginFunctions* plugin_funs{};
};


bool Inferior::handle_core_event(const bp::PluginRequest* req,
                                 bp::PluginResponse* resp)
{
  switch (req->request_case()) {
    case bareos::plugin::PluginRequest::kSetup: {
      DebugLog(100, "setup called twice ...");
      return false;
    }
    case bareos::plugin::PluginRequest::kBeginBackup: {
      if (!plugin) { return false; }
      (void)resp->mutable_begin_backup();
      return plugin->start_backup(req->begin_backup());
    } break;
    case bareos::plugin::PluginRequest::kBeginRestore: {
      if (!plugin) { return false; }
      (void)resp->mutable_begin_restore();
      return plugin->start_restore(req->begin_restore());
    } break;
    case bareos::plugin::PluginRequest::kBeginEstimate: {
      if (!plugin) { return false; }
      (void)resp->mutable_begin_estimate();
      return plugin->start_estimate(req->begin_estimate());
    } break;
    case bareos::plugin::PluginRequest::kBeginVerify: {
      if (!plugin) { return false; }
      (void)resp->mutable_begin_verify();
      return plugin->start_verify(req->begin_verify());
    } break;
    case bareos::plugin::PluginRequest::kHandlePluginEvent: {
      auto& plugin_event = req->handle_plugin_event().to_handle();

      if (!plugin) {
        event_cache.emplace_back().CopyFrom(plugin_event);
        resp->mutable_handle_plugin_event()->set_res(bp::ReturnCode::RC_OK);
        return true;
      }


      // if (!is_event_allowed(plugin_event)) {
      //   resp->mutable_handle_plugin_event()->set_res(bp::ReturnCode::RC_OK);
      //   return true;
      // }
      std::optional result = plugin->handle_event(plugin_event);

      if (!result) { return false; }

      resp->mutable_handle_plugin_event()->set_res(to_grpc(*result));
      return true;
    } break;
    case bareos::plugin::PluginRequest::REQUEST_NOT_SET: {
      DebugLog(100, "bad plugin request; nothing set!");
      return false;
    }
  }

  DebugLog(100, "unhandled request type!");
  return false;
}

bool PluginThread::start_backup(const bp::BeginBackupRequest& req)
{
  std::unique_lock _{task_mutex};

  tasks_to_execute.emplace_back(
      backup{.command = req.cmd(),
             .max_record_size = req.max_record_size(),
             .flags = std::vector<char>(req.flags().begin(), req.flags().end()),
             .backup_acl = req.backup_acl(),
             .backup_xattr = req.backup_xattr(),
             .portable = req.portable(),
             .no_atime = req.no_atime()});

  task_added.notify_one();
  return true;
}

bool PluginThread::start_restore(const bp::BeginRestoreRequest& req)
{
  std::unique_lock _{task_mutex};

  tasks_to_execute.emplace_back(restore{.command = req.cmd()});

  task_added.notify_one();
  return true;
}

std::optional<bRC> PluginThread::handle_event(const bpe::Event& event)
{
  filedaemon::restore_object_pkt restore_object{};

  auto fut = [&]() -> std::optional<std::future<bRC>> {
    std::unique_lock _{task_mutex};

    auto& task = tasks_to_execute.emplace_back().emplace<handle_event_task>();

    switch (event.event_case()) {
      case bpe::Event::kJobStart: {
        task.type = filedaemon::bEventJobStart;
        auto& cmd = event.job_start().data();
        task.set_data(cmd);
      } break;
      case bpe::Event::kJobEnd: {
        task.type = filedaemon::bEventJobEnd;
      } break;
      case bpe::Event::kStartBackupJob: {
        task.type = filedaemon::bEventStartBackupJob;
      } break;
      case bpe::Event::kEndBackupJob: {
        task.type = filedaemon::bEventEndBackupJob;
      } break;
      case bpe::Event::kStartRestoreJob: {
        task.type = filedaemon::bEventStartRestoreJob;
      } break;
      case bpe::Event::kEndRestoreJob: {
        task.type = filedaemon::bEventEndRestoreJob;
      } break;
      case bpe::Event::kStartVerifyJob: {
        task.type = filedaemon::bEventStartVerifyJob;
      } break;
      case bpe::Event::kEndVerifyJob: {
        task.type = filedaemon::bEventEndVerifyJob;
      } break;
      case bpe::Event::kBackupCommand: {
        task.type = filedaemon::bEventBackupCommand;
        auto& cmd = event.backup_command().data();
        task.set_data(cmd);
      } break;
      case bpe::Event::kRestoreCommand: {
        task.type = filedaemon::bEventRestoreCommand;
        auto& cmd = event.restore_command().data();
        task.set_data(cmd);
      } break;
      case bpe::Event::kEstimateCommand: {
        task.type = filedaemon::bEventEstimateCommand;
        auto& cmd = event.estimate_command().data();
        task.set_data(cmd);
      } break;
      case bpe::Event::kLevel: {
        task.type = filedaemon::bEventLevel;
        task.set_data(event.level().level());
      } break;
      case bpe::Event::kSince: {
        task.type = filedaemon::bEventSince;
        task.set_data(event.since().since().seconds());
      } break;
      case bpe::Event::kCancelCommand: {
        task.type = filedaemon::bEventCancelCommand;
      } break;
      case bpe::Event::kRestoreObject: {
        task.type = filedaemon::bEventRestoreObject;

        auto& rop = event.restore_object().rop();
        std::optional objtype = from_grpc(rop.sent().type());

        if (!objtype) { return std::nullopt; }

        restore_object.pkt_size = sizeof(restore_object);
        restore_object.object_name
            = const_cast<char*>(rop.sent().name().data());
        restore_object.object = const_cast<char*>(rop.sent().data().data());
        restore_object.plugin_name
            = const_cast<char*>(rop.used_cmd_string().data());
        restore_object.object_type = *objtype;
        restore_object.object_len = rop.sent().data().size();
        restore_object.object_full_len = 0;
        restore_object.object_index = rop.sent().index();
        restore_object.object_compression = 0;
        restore_object.stream = 0;
        restore_object.JobId = rop.jobid();
        restore_object.pkt_end = sizeof(restore_object);

        task.set_data(&restore_object);
      } break;
      case bpe::Event::kEndRestoreObject: {
        task.type = filedaemon::bEventRestoreObject;
      } break;
      case bpe::Event::kEndFileset: {
        task.type = filedaemon::bEventEndFileSet;
      } break;
      case bpe::Event::kPluginCommand: {
        task.type = filedaemon::bEventPluginCommand;
        auto& cmd = event.plugin_command().data();
        task.set_data(cmd);
      } break;
      case bpe::Event::kOptionPlugin: {
        task.type = filedaemon::bEventOptionPlugin;
      } break;
      case bpe::Event::kHandleBackupFile: {
        task.type = filedaemon::bEventHandleBackupFile;
      } break;
      case bpe::Event::kNewPluginOptions: {
        task.type = filedaemon::bEventNewPluginOptions;
        auto& cmd = event.new_plugin_options().data();
        task.set_data(cmd);
      } break;
      case bpe::Event::kVssInitBackup: {
        task.type = filedaemon::bEventVssInitializeForBackup;
      } break;
      case bpe::Event::kVssInitRestore: {
        task.type = filedaemon::bEventVssInitializeForRestore;
      } break;
      case bpe::Event::kVssSetBackupState: {
        task.type = filedaemon::bEventVssSetBackupState;
      } break;
      case bpe::Event::kVssPrepareForBackup: {
        task.type = filedaemon::bEventVssPrepareForBackup;
      } break;
      case bpe::Event::kVssBackupAddComponents: {
        task.type = filedaemon::bEventVssBackupAddComponents;
      } break;
      case bpe::Event::kVssPrepareSnapshot: {
        task.type = filedaemon::bEventVssPrepareSnapshot;
      } break;
      case bpe::Event::kVssCreateSnapshot: {
        task.type = filedaemon::bEventVssCreateSnapshots;
      } break;
      case bpe::Event::kVssRestoreLoadCompanentsMetadata: {
        task.type = filedaemon::bEventVssRestoreLoadComponentMetadata;
      } break;
      case bpe::Event::kVssRestoreSetComponentsSelected: {
        task.type = filedaemon::bEventVssRestoreSetComponentsSelected;
      } break;
      case bpe::Event::kVssCloseRestore: {
        task.type = filedaemon::bEventVssCloseRestore;
      } break;
      case bpe::Event::kVssBackupComplete: {
        task.type = filedaemon::bEventVssBackupComplete;
      } break;
      case bpe::Event::kSetDebugLevel: {
        DebugLog(50, "SetDebugLevel should not get through to here!");

        return std::nullopt;
      } break;
      case bpe::Event::EVENT_NOT_SET: {
        DebugLog(50, "no event set!");

        return std::nullopt;
      } break;
      default: {
        DebugLog(50, "cannot handle unknown event type");
        return std::nullopt;
      }
    }

    task_added.notify_one();

    return task.result.get_future();
  }();

  if (!fut) { return std::nullopt; }
  return fut->get();
}

Inferior::Inferior(const filedaemon::CoreFunctions* funs,
                   prototools::ProtoBidiStream* data_transfer,
                   std::string_view plugin_path)
    : plugin{std::make_unique<PluginThread>(funs, data_transfer, plugin_path)}
{
  plugin->start();
}
Inferior::~Inferior() = default;

void allow_events(PluginThread* thread, std::uint64_t event_mask)
{
  if (thread) { thread->allow_events(event_mask); }
}
void disallow_events(PluginThread* thread, std::uint64_t event_mask)
{
  if (thread) { thread->disallow_events(event_mask); }
}

#pragma GCC diagnostic pop
