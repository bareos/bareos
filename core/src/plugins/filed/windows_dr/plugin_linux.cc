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

#include <tl/expected.hpp>

#define err_msg(ctx, ...) JobLog((ctx), M_ERROR, __VA_ARGS__)
#define info_msg(ctx, ...) JobLog((ctx), M_INFO, __VA_ARGS__)

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
    .plugin_date = "Juli 2025",
    .plugin_version = "0.1.0",
    .plugin_description
    = "This plugin allows you to backup your windows system for disaster "
      "recovery.",
    .plugin_usage = "...",
};

struct plugin_arguments {
  using directory = std::string;
  using files = std::vector<std::string>;

  std::variant<std::monostate, directory, files> target;

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
      if (parsed.target.index() != 0) {
        return err("cannot have more than one directory/files value total");
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
      if (parsed.target.index() != 0) {
        return err("cannot have more than one directory/files value total");
      }

      if (value.empty()) {
        return err("directory cannot be set to an empty string");
      }

      auto& dir = parsed.target.emplace<directory>();

      dir.assign(value);

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
          // we have reached the last argument

          auto res = args;
          args = {};
          return res;
        } else {
          auto res = args.substr(0, next_arg_start);
          args.remove_prefix(next_arg_start + 1);
          return res;
        }
      }();

      if (current_arg.size() == 0) {
        // ignore this for now, maybe this should be an error
        continue;
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
};

struct plugin_logger : public GenericLogger {
  void Begin(std::size_t FileSize) override { (void)FileSize; }
  void Progressed(std::size_t Amount) override { (void)Amount; }
  void End() override {}

  void SetStatus(std::string_view Status) override { (void)Status; }
  void Output(std::string_view Message) override
  {
    JobLog(ctx, M_INFO, "{}", Message);
  }

  plugin_logger(PluginContext* ctx_) : GenericLogger{false}, ctx{ctx_} {}

 private:
  PluginContext* ctx;
};

struct session_ctx {
  data_writer* writer{};

  session_ctx(restore_options options) : writer{writer_begin(options)} {}

  ~session_ctx()
  {
    if (writer) { writer_end(writer); }
  }
};

struct plugin_ctx {
  plugin_logger logger;

  plugin_ctx(PluginContext* ctx) : logger{ctx} {}

  void set_plugin_args(plugin_arguments) {}

  bool begin_session()
  {
    // no arguments set
    if (!args) { return false; }
    // no target specified

    restore_options options = std::visit(
        [&](auto& tgt) -> restore_options {
          using T = std::remove_reference_t<decltype(tgt)>;

          if constexpr (std::is_same_v<T, plugin_arguments::directory>) {
            return restore_options::into_directory(&logger, tgt);
          } else if constexpr (std::is_same_v<T, plugin_arguments::files>) {
            return restore_options::into_files(&logger, tgt);
          } else if constexpr (std::is_same_v<T, std::monostate>) {
            // nothing was chosen, so just use a sensible default
            return restore_options::into_directory(&logger, "/tmp");
          }
        },
        args->target);
    if (args->target.index() == 0) { return false; }

    current_session.emplace(options);

    return true;
  }

  std::size_t session_write(std::span<char> data)
  {
    if (!current_session) { return 0; }

    return writer_write(current_session->writer, data);
  }

  bool has_session() const { return current_session.has_value(); }

  std::optional<session_ctx> current_session;

  std::optional<plugin_arguments> args;
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

    case filedaemon::bEventNewPluginOptions:
      [[fallthrough]];
    case filedaemon::bEventRestoreCommand: {
      auto arguments = plugin_arguments::parse(static_cast<const char*>(data));
      if (!arguments) {
        err_msg(ctx, "could not parse arguments: {}", arguments.error());
        return bRC_Error;
      }
      pctx->set_plugin_args(std::move(arguments.value()));
      return bRC_OK;
    }
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
  auto* pctx = get_private_context(ctx);
  if (pctx->has_session()) {
    err_msg(ctx, "this plugin cannot restore more than one 'file' at once");
    return bRC_Error;
  }
  info_msg(ctx, "restore started (file = {})", file_name);
  return bRC_OK;
}
bRC endRestoreFile(PluginContext* ctx)
{
  info_msg(ctx, "restore finished");
  return bRC_OK;
}
bRC pluginIO(PluginContext* ctx, filedaemon::io_pkt* pkt)
{
  auto* pctx = get_private_context(ctx);
  (void)pctx;
  switch (pkt->func) {
    case filedaemon::IO_OPEN: {
      if (pctx->has_session()) {
        err_msg(ctx, "context can only be created once");
        pkt->status = -1;
        return bRC_Error;
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
    case filedaemon::IO_READ: {
      err_msg(ctx, "backups are not supported on windows");
      pkt->status = -1;
      return bRC_Error;
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
      if (!pctx->has_session()) {
        err_msg(ctx, "context can only be closed, if its open");
        pkt->status = -1;
        return bRC_Error;
      }
      pkt->status = 0;
      return bRC_OK;
    } break;
  }
  return bRC_Error;
}
bRC createFile(PluginContext* ctx, filedaemon::restore_pkt* pkt)
{
  (void)ctx;
  (void)pkt;
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
