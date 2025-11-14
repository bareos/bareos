/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#include <unistd.h>
#include <type_traits>
#define NEW_RESTORE
#if !defined(PLUGIN_NAME)
#  error PLUGIN_NAME not set
#endif

#include "include/bareos.h"
#include "filed/fd_plugins.h"
#include "bareos_api.h"
#include "include/filetypes.h"
#include "parser.h"
#include "restore.h"
#include "plugin.h"
#include "util.h"

#include <sstream>

#include <fcntl.h>

#include <tl/expected.hpp>

#define err_msg(ctx, ...) JobLog((ctx), M_ERROR, __VA_ARGS__)
#define info_msg(ctx, ...) JobLog((ctx), M_INFO, __VA_ARGS__)
#define fatal_msg(ctx, ...) JobLog((ctx), M_FATAL, __VA_ARGS__)
#define d_msg(ctx, level, ...) DebugLog((ctx), (level), __VA_ARGS__)

bool AmICompatibleWith(filedaemon::PluginApiDefinition* core_info)
{
  if (core_info->size != sizeof(*core_info)
      || core_info->version != FD_PLUGIN_INTERFACE_VERSION) {
    return false;
  }

  return true;
}

const PluginInformation my_info = {
    .size = sizeof(my_info),
    .version = FD_PLUGIN_INTERFACE_VERSION,
    .plugin_magic = FD_PLUGIN_MAGIC,
    .plugin_license = "Bareos AGPLv3",
    .plugin_author = "Sebastian Sura",
    .plugin_date = "July 2025",
    .plugin_version = "1.0.0",
    .plugin_description
    = "This plugin allows you to backup your windows system for disaster "
      "recovery.",
    .plugin_usage = R"(barri-fd:<target>,
  where <target> is one of the following:
    files=file1,file2,file3 - restore the disks to the chosen files
    directory=dir           - restore the disks into the directory
    copy=path               - copy the barri image to the following path

  The copy option allows you to restore the image via the barri cli tool instead
  of this filedaemon plugin.
)",
};

struct plugin_arguments {
  using directory = std::string;
  using files = std::vector<std::string>;

  using Error = tl::unexpected<std::string>;

  template <typename... Args>
  static Error err(fmt::format_string<Args...> fmt, Args&&... args)
  {
    return Error{fmt::format(fmt, std::forward<Args>(args)...)};
  }

  static std::optional<Error> parse_flag(plugin_arguments& parsed,
                                         std::string_view flag)
  {
    (void)parsed;
    return err("unknown flag {}", flag);
  }

  static std::optional<Error> parse_kv(plugin_arguments& parsed,
                                       std::string_view key,
                                       std::string_view value)
  {
    static constexpr auto success = std::nullopt;

    if (key == "files") {
      if (parsed.target_set()) {
        return err("cannot have more than one target");
      }

      auto& vec = parsed.target.emplace<files>();

      while (!value.empty()) {
        auto comma_pos = value.find_first_of(",");
        if (comma_pos == 0) {
          return err("files[{}] cannot be set to an empty string", vec.size());
        } else if (comma_pos == value.npos) {
          vec.emplace_back(value);
          value = {};
        } else {
          auto file = value.substr(0, comma_pos);
          vec.emplace_back(file);
          value.remove_prefix(comma_pos + 1);
        }
      }

      return success;
    } else if (key == "directory") {
      if (parsed.target_set()) {
        return err("cannot have more than one target");
      }

      if (value.empty()) {
        return err("directory cannot be set to an empty string");
      }

      auto& dir = parsed.target.emplace<directory>();

      dir.assign(value);

      return success;
    } else if (key == "copy") {
      if (parsed.target_set()) {
        return err(
            "cannot have more than one directory/files/copy value total");
      }

      if (value.empty()) {
        return err(
            "copy cannot be set to an empty string; it should be the filename "
            "of the copy");
      }

      parsed.copy_location = value;

      return success;
    }
    return err("unknown key {} (= {})", key, value);
  }

  static tl::expected<plugin_arguments, std::string> parse(
      std::string_view cmdline)
  {
    if (!cmdline.starts_with(PLUGIN_NAME)) {
      return err("received plugin options for wrong plugin: {}", cmdline);
    }

    auto args = cmdline.substr(strlen(PLUGIN_NAME));

    plugin_arguments parsed = {};

    while (!args.empty()) {
      if (args[0] != ':') {
        return err("expected ':' at index {}", args.data() - cmdline.data());
      }

      auto next_arg_start = args.find_first_of(":", 1);
      std::string_view current_arg = [&] {
        if (next_arg_start == args.npos) {
          // we have reached the last argument,
          auto res = args.substr(1);
          args = {};
          return res;
        } else {
          auto res = args.substr(1, next_arg_start);
          args.remove_prefix(next_arg_start);
          return res;
        }
      }();

      if (current_arg.size() == 0) {
        return err("empty argument detected at index {}; this is not supported",
                   current_arg.data() - cmdline.data());
      }

      auto eq_pos = current_arg.find_first_of("=");

      if (eq_pos == current_arg.npos) {
        // we have received a flag
        if (auto err = parse_flag(parsed, current_arg)) { return err.value(); }
      } else {
        // we have received a kv-pair

        std::string_view key = current_arg.substr(0, eq_pos);
        std::string_view value = current_arg.substr(eq_pos + 1);

        if (auto err = parse_kv(parsed, key, value)) { return err.value(); }
      }
    }

    return parsed;
  }


  bool target_set() const
  {
    if (!copy_location.empty()) { return true; }
    if (std::get_if<std::monostate>(&target) == nullptr) { return true; }
    return false;
  }

  std::variant<std::monostate, directory, files> target;
  std::string copy_location;
};

struct plugin_logger : public GenericLogger {
  using Clock = std::chrono::steady_clock;

  void Begin(std::size_t FileSize) override
  {
    current_file_size = FileSize;
    current_file_offset = 0;
    last_file_offset = 0;
    last_time_stamp = Clock::now();

    data_milestone = FileSize / 10;  // around ~10 prints
  }
  void Progressed(std::size_t Amount) override
  {
    current_file_offset += Amount;

    auto current_ts = Clock::now();

    bool time_elapsed = current_ts - last_time_stamp > time_milestone;
    bool milestone_reached
        = current_file_offset - last_file_offset > data_milestone;
    if (time_elapsed || milestone_reached || last_file_offset == 0) {
      print_progress(current_ts, current_file_offset);
    }
  }
  void End() override { current_file_size = 0; }

  void SetStatus(std::string_view Status) override { (void)Status; }
  void Output(Message message) override
  {
    if (message.is_trace) {
      DebugLog(ctx, 500, "{}", message.text);
    } else {
      JobLog(ctx, M_INFO, "{}", message.text);
    }

    if (messages.size() < max_messages_size) {
      // lets make sure that this does not grow too big.
      messages.insert(messages.end(), message.text.begin(), message.text.end());
      messages.push_back('\n');
    }
  }

  plugin_logger(PluginContext* ctx_) : GenericLogger{true}, ctx{ctx_} {}

  std::span<const char> log() const { return messages; }

 private:
  void print_progress(Clock::time_point current_ts, std::size_t current_offset)
  {
    if (!current_file_size) { return; }

    auto data_written = current_offset - last_file_offset;

    auto progress = static_cast<double>(current_offset)
                    / static_cast<double>(current_file_size);

    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        current_ts - last_time_stamp);

    if (seconds.count() == 0) { return; }

    auto approx_speed = data_written / seconds.count();

    std::string log_msg = format_progress_bar(progress, approx_speed, 20);

    Message msg = {log_msg, false};

    Output(msg);

    last_file_offset = current_offset;
    last_time_stamp = current_ts;
  }

  PluginContext* ctx;

  Clock::time_point last_time_stamp;
  std::size_t last_file_offset;
  std::size_t current_file_offset = 0;
  std::size_t current_file_size = 0;
  std::size_t data_milestone = 0;
  static constexpr Clock::duration time_milestone = std::chrono::minutes(5);

  static constexpr std::size_t max_messages_size = 1 << 30;  // 1GB
  std::vector<char> messages;
};

struct session_ctx {
  GenericLogger* logger;
  std::unique_ptr<GenericHandler> handler;
  restartable_parser* parser{};

  session_ctx(std::unique_ptr<GenericHandler> handler_, GenericLogger* logger_)
      : logger{logger_}
      , handler{std::move(handler_)}
      , parser{parse_begin(handler.get(), logger)}
  {
  }

  ~session_ctx()
  {
    if (parser) { parse_end(parser); }
  }
};

struct plugin_ctx {
  plugin_logger logger;

  plugin_ctx(PluginContext* ctx) : logger{ctx} {}

  void set_plugin_args(plugin_arguments&& new_args) { args.emplace(new_args); }

  bool begin_session()
  {
    // no arguments set
    if (!args) { return false; }
    // no target specified

    using namespace barri::restore;

    if (args->target.index() == 0) { return false; }
    auto handler = std::visit(
        [&](auto& tgt) {
          using T = std::remove_reference_t<decltype(tgt)>;

          if constexpr (std::is_same_v<T, plugin_arguments::directory>) {
            return GetHandler(&logger, restore_directory{tgt});
          } else if constexpr (std::is_same_v<T, plugin_arguments::files>) {
            return GetHandler(&logger, restore_files{tgt});
          } else if constexpr (std::is_same_v<T, std::monostate>) {
            // nothing was chosen, so just use a sensible default
            return GetHandler(&logger, restore_directory{"/tmp"});
          }
        },
        args->target);

    current_session.emplace(std::move(handler), &logger);

    return true;
  }

  std::size_t session_write(std::span<char> data)
  {
    if (!current_session) { return 0; }

    parse_data(current_session->parser, data);

    return data.size();
  }

  bool has_session() const { return current_session.has_value(); }

  std::optional<session_ctx> current_session;

  std::optional<plugin_arguments> args;

  enum class file_type
  {
    None,
    Dump,
    Log,
  };

  file_type current_file_type = file_type::None;

  // this is the debug level on which the backup log is dumped
  // if it is selected to be "restored"
  std::size_t log_level = 300;
  std::string log_line_buffer{};
};

plugin_ctx* get_private_context(PluginContext* ctx)
{
  return reinterpret_cast<plugin_ctx*>(ctx->plugin_private_context);
}

void set_private_context(PluginContext* ctx, plugin_ctx* priv_ctx)
{
  ctx->plugin_private_context = priv_ctx;
}

bRC newPlugin(PluginContext* ctx)
{
  set_private_context(ctx, new plugin_ctx{ctx});
  RegisterBareosEvent(ctx, filedaemon::bEventPluginCommand);
  RegisterBareosEvent(ctx, filedaemon::bEventNewPluginOptions);
  RegisterBareosEvent(ctx, filedaemon::bEventPluginCommand);
  RegisterBareosEvent(ctx, filedaemon::bEventJobStart);
  // RegisterBareosEvent(ctx, filedaemon::bEventStartBackupJob);
  RegisterBareosEvent(ctx, filedaemon::bEventRestoreCommand);
  RegisterBareosEvent(ctx, filedaemon::bEventRestoreObject);
  return bRC_OK;
}

bRC freePlugin(PluginContext* ctx)
{
  auto* pctx = get_private_context(ctx);
  set_private_context(ctx, nullptr);
  delete pctx;
  return bRC_OK;
}

bRC getPluginValue(PluginContext*, filedaemon::pVariable, void*)
{
  /* UNUSED */
  return bRC_Error;
}
bRC setPluginValue(PluginContext*, filedaemon::pVariable, void*)
{
  /* UNUSED */
  return bRC_Error;
}

bRC handlePluginEvent(PluginContext* ctx, filedaemon::bEvent* event, void* data)
{
  auto* pctx = get_private_context(ctx);
  switch (event->eventType) {
    case filedaemon::bEventPluginCommand: {
      auto arguments = plugin_arguments::parse(static_cast<const char*>(data));
      if (!arguments) {
        err_msg(ctx, "could not parse arguments: {}", arguments.error());
        return bRC_Error;
      }
      pctx->set_plugin_args(std::move(arguments.value()));
      return bRC_OK;
    } break;

    case filedaemon::bEventNewPluginOptions: {
      auto arguments = plugin_arguments::parse(static_cast<const char*>(data));
      if (!arguments) {
        err_msg(ctx, "could not parse arguments: {}", arguments.error());
        return bRC_Error;
      }
      pctx->set_plugin_args(std::move(arguments.value()));
      return bRC_OK;
    } break;

    case filedaemon::bEventRestoreCommand: {
      if (!pctx->args) {
        // not sure what to do here,
        // these are the _backup_ arguments,
        // so we should normally just ignore them, as the restore
        // arguments are completely different
        auto arguments
            = plugin_arguments::parse(static_cast<const char*>(data));
        if (!arguments) {
          err_msg(ctx, "could not parse arguments: {}", arguments.error());
          return bRC_Error;
        }
        pctx->set_plugin_args(std::move(arguments.value()));
      }
      return bRC_OK;
    } break;
  }
  return bRC_Error;
}

bRC startBackupFile(PluginContext* ctx, filedaemon::save_pkt* sp)
{
  (void)sp;
  err_msg(ctx, "backups are only supported on windows");
  return bRC_Error;
}

bRC endBackupFile(PluginContext* ctx)
{
  (void)ctx;
  return bRC_Error;
}

bRC startRestoreFile(PluginContext* ctx, const char* file_name)
{
  (void)ctx;
  (void)file_name;
  return bRC_OK;
}

bRC endRestoreFile(PluginContext* ctx)
{
  auto* pctx = get_private_context(ctx);
  pctx->current_file_type = plugin_ctx::file_type::None;

  info_msg(ctx, "restore finished");
  return bRC_OK;
}

bRC pluginIO_Dump(PluginContext* ctx, filedaemon::io_pkt* pkt)
{
  auto* pctx = get_private_context(ctx);
  switch (pkt->func) {
    case filedaemon::IO_OPEN: {
      if (pctx->has_session()) {
        err_msg(ctx, "context can only be created once");
        pkt->status = -1;
        return bRC_Error;
      }

      info_msg(ctx, "restore started (file = {})", pkt->fname);

      if (!pctx->args->copy_location.empty()) {
        pkt->filedes
            = open(pctx->args->copy_location.c_str(), O_RDWR | O_TRUNC);
        pkt->status = IoStatus::do_io_in_core;

        return bRC_OK;
      }

      try {
        if (!pctx->begin_session()) {
          err_msg(ctx, "could not begin session");
          return bRC_Error;
        }
        pkt->status = 0;
        return bRC_OK;
      } catch (const std::exception& ex) {
        err_msg(ctx, "could not start: {}", ex.what());
        pkt->status = -1;
        return bRC_Error;
      } catch (...) {
        err_msg(ctx, "could not start: unknown error occured");
        pkt->status = -1;
        return bRC_Error;
      }
    } break;
    case filedaemon::IO_WRITE: {
      if (pkt->count < 0) {
        err_msg(ctx, "its impossible to read {} bytes...", pkt->count);
        pkt->status = -1;
        return bRC_Error;
      }
      pkt->status = pctx->session_write(
          std::span{pkt->buf, static_cast<std::size_t>(pkt->count)});
      return bRC_OK;
    } break;
    case filedaemon::IO_CLOSE: {
      if (!pctx->args->copy_location.empty()) {
        close(pkt->filedes);
      } else if (!pctx->has_session()) {
        err_msg(ctx, "context can only be closed, if its open");
        pkt->status = -1;
        return bRC_Error;
      }
      pkt->status = 0;
      return bRC_OK;
    } break;
  }

  fatal_msg(ctx, "unhandled code path: {}", pkt->func);
  return bRC_Error;
}

bRC pluginIO_Log(PluginContext* ctx, filedaemon::io_pkt* pkt)
{
  static constexpr std::string_view backup_log_prefix = "--BACKUP LOG--";

  auto* pctx = get_private_context(ctx);
  switch (pkt->func) {
    case filedaemon::IO_OPEN: {
      info_msg(ctx,
               "starting restore of backup log into debug log (level = {})",
               pctx->log_level);
      return bRC_OK;
    } break;
    case filedaemon::IO_WRITE: {
      if (pkt->count < 0) {
        err_msg(ctx, "its impossible to read {} bytes...", pkt->count);
        pkt->status = -1;
        return bRC_Error;
      }
      auto& line_buffer = pctx->log_line_buffer;
      auto data
          = std::string_view{pkt->buf, static_cast<std::size_t>(pkt->count)};

      pkt->status = pkt->count;

      auto pos = data.find_first_of('\n');

      if (pos == data.npos) {
        line_buffer.insert(line_buffer.end(), data.begin(), data.end());
        return bRC_OK;
      }

      d_msg(ctx, pctx->log_level, "{} {}{}", backup_log_prefix, line_buffer,
            data.substr(0, pos));

      data = data.substr(pos + 1);

      while (data.size() > 0) {
        auto next_pos = data.find_first_of('\n');
        if (next_pos == data.npos) { break; }

        d_msg(ctx, pctx->log_level, "{} {}", backup_log_prefix,
              data.substr(0, next_pos));

        data = data.substr(next_pos + 1);
      }

      line_buffer.assign(data);


      return bRC_OK;
    } break;
    case filedaemon::IO_CLOSE: {
      auto& line_buffer = pctx->log_line_buffer;
      if (!line_buffer.empty()) {
        d_msg(ctx, pctx->log_level, "{} {}", backup_log_prefix, line_buffer);
      }
      line_buffer.clear();

      info_msg(ctx, "restore of backup log is done");

      return bRC_OK;
    } break;
  }

  fatal_msg(ctx, "unhandled code path: {}", pkt->func);
  return bRC_Error;
}


bRC pluginIO(PluginContext* ctx, filedaemon::io_pkt* pkt)
{
  auto* pctx = get_private_context(ctx);

  if (pkt->func == filedaemon::IO_OPEN) {
    std::string_view path{pkt->fname};

    auto ending_start = path.find_last_of('.');

    if (ending_start == path.npos) {
      fatal_msg(ctx, "cannot restore '{}': unknown file ending", path);
    }

    auto ending = path.substr(ending_start);

    if (ending == log_ending) {
      pctx->current_file_type = plugin_ctx::file_type::Log;
    } else if (ending == dump_ending) {
      pctx->current_file_type = plugin_ctx::file_type::Dump;
    } else {
      fatal_msg(ctx, "cannot restore '{}': unknown file ending ({})", path,
                ending);
      return bRC_Error;
    }
  }

  decltype(&pluginIO) handler = nullptr;
  switch (pctx->current_file_type) {
    case plugin_ctx::file_type::Log: {
      handler = &pluginIO_Log;
    } break;
    case plugin_ctx::file_type::Dump: {
      handler = &pluginIO_Dump;
    } break;
    case plugin_ctx::file_type::None: {
      fatal_msg(ctx, "cannot do plugin io on file of type None!");
    } break;
  }

  if (!handler) {
    fatal_msg(ctx, "internal error: bad file type {}",
              static_cast<std::size_t>(pctx->current_file_type));
    return bRC_Error;
  }

  switch (pkt->func) {
    case filedaemon::IO_OPEN:
      [[fallthrough]];
    case filedaemon::IO_WRITE:
      [[fallthrough]];
    case filedaemon::IO_CLOSE: {
      return handler(ctx, pkt);
    } break;

    case filedaemon::IO_READ: {
      err_msg(ctx, "backups are not supported on windows");
      pkt->status = -1;
      return bRC_Error;
    } break;
  }
  fatal_msg(ctx, "unknown function {}", pkt->func);
  return bRC_Error;
}
bRC createFile(PluginContext* ctx, filedaemon::restore_pkt* pkt)
{
  auto* pctx = get_private_context(ctx);

  bool handled = false;
  if (pctx->args) {
    if (auto& copy_loc = pctx->args->copy_location; !copy_loc.empty()) {
      // we just tell bareos to dump the stream into the file system

      if (creat(copy_loc.c_str(), 0600) < 0) {
        err_msg(ctx, "could not create {}: {}", copy_loc, strerror(errno));
        return bRC_Stop;
      }
      pkt->create_status = CF_EXTRACT;

      handled = true;
    }
  }

  if (!handled) { pkt->create_status = CF_EXTRACT; }
  return bRC_OK;
}
bRC setFileAttributes(PluginContext* ctx, filedaemon::restore_pkt* pkt)
{
  (void)ctx;
  (void)pkt;
  return bRC_OK;
}
bRC checkFile(PluginContext* ctx, char* file_name)
{
  (void)ctx;
  (void)file_name;
  return bRC_Error;
}
bRC getAcl(PluginContext* ctx, filedaemon::acl_pkt* pkt)
{
  (void)ctx;
  (void)pkt;
  return bRC_Error;
}
bRC setAcl(PluginContext* ctx, filedaemon::acl_pkt* pkt)
{
  (void)ctx;
  (void)pkt;
  return bRC_Error;
}
bRC getXattr(PluginContext* ctx, filedaemon::xattr_pkt* pkt)
{
  (void)ctx;
  (void)pkt;
  return bRC_Error;
}
bRC setXattr(PluginContext* ctx, filedaemon::xattr_pkt* pkt)
{
  (void)ctx;
  (void)pkt;
  return bRC_Error;
}

const filedaemon::PluginFunctions my_functions = {
    .size = sizeof(my_functions),
    .version = FD_PLUGIN_INTERFACE_VERSION,
    .newPlugin = &newPlugin,
    .freePlugin = &freePlugin,
    .getPluginValue = &getPluginValue,
    .setPluginValue = &setPluginValue,
    .handlePluginEvent = &handlePluginEvent,
    .startBackupFile = &startBackupFile,
    .endBackupFile = &endBackupFile,
    .startRestoreFile = &startRestoreFile,
    .endRestoreFile = &endRestoreFile,
    .pluginIO = &pluginIO,
    .createFile = &createFile,
    .setFileAttributes = &setFileAttributes,
    .checkFile = &checkFile,
    .getAcl = &getAcl,
    .setAcl = &setAcl,
    .getXattr = &getXattr,
    .setXattr = &setXattr,
};

extern "C" int loadPlugin(filedaemon::PluginApiDefinition* core_info,
                          filedaemon::CoreFunctions* core_funcs,
                          PluginInformation** plugin_info,
                          filedaemon::PluginFunctions** plugin_funcs)
{
  if (!SetupBareosApi(core_funcs)) { return -1; }

  if (!AmICompatibleWith(core_info)) { return -1; }

  *plugin_info = const_cast<PluginInformation*>(&my_info);
  *plugin_funcs = const_cast<filedaemon::PluginFunctions*>(&my_functions);

  return 0;
}

extern "C" int unloadPlugin() { return 0; }
