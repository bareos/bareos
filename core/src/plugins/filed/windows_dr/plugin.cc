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

#include "include/bareos.h"
#include "filed/fd_plugins.h"
#include "bareos_api.h"
#include "include/filetypes.h"
#include "parser.h"
#include "dump.h"
#include <comdef.h>

#define err_msg(ctx, ...) JobLog((ctx), M_ERROR, __VA_ARGS__)

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
  static plugin_arguments parse(std::string_view) { return {}; }
};

struct plugin_logger : public GenericLogger {
  void Begin(std::size_t FileSize) override { (void)FileSize; }
  void Progressed(std::size_t Amount) override { (void)Amount; }
  void End() override {}

  void SetStatus(std::string_view Status) override { (void)Status; }
  void Info(std::string_view Message) override
  {
    JobLog(ctx, M_INFO, "{}", Message);
  }

  plugin_logger(PluginContext* ctx_) : ctx{ctx_} {}

 private:
  PluginContext* ctx;
};

struct session_ctx {
  plugin_logger logger;
  data_dumper* dumper{nullptr};

  session_ctx(PluginContext* ctx) : logger{ctx}, dumper{dumper_setup(&logger)}
  {
  }

  ~session_ctx()
  {
    if (dumper) { dumper_stop(dumper); }
  }
};

struct plugin_ctx {
  void set_plugin_args(plugin_arguments) {}

  void begin_session(PluginContext* ctx) { current_session.emplace(ctx); }

  std::size_t session_read(std::span<char> data)
  {
    if (!current_session) { return 0; }
    return dumper_write(current_session->dumper, data);
  }

  bool has_session() const { return current_session.has_value(); }

  std::optional<session_ctx> current_session;

  struct CoUninitializer {
    ~CoUninitializer() { CoUninitialize(); }
  };

  CoUninitializer unititializer{};
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
  auto hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

  if (FAILED(hr)) {
    _com_error err(hr);
    err_msg(ctx, "could not initialize com: {}", err.ErrorMessage());
  }

  set_private_context(ctx, new plugin_ctx);
  RegisterBareosEvent(ctx, filedaemon::bEventPluginCommand);
  RegisterBareosEvent(ctx, filedaemon::bEventNewPluginOptions);
  RegisterBareosEvent(ctx, filedaemon::bEventPluginCommand);
  RegisterBareosEvent(ctx, filedaemon::bEventJobStart);
  // RegisterBareosEvent(ctx, filedaemon::bEventStartBackupJob);
  RegisterBareosEvent(ctx, filedaemon::bEventRestoreCommand);
  RegisterBareosEvent(ctx, filedaemon::bEventEstimateCommand);
  RegisterBareosEvent(ctx, filedaemon::bEventBackupCommand);
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
      pctx->set_plugin_args(std::move(arguments));
      return bRC_OK;
    } break;

    case filedaemon::bEventNewPluginOptions:
      [[fallthrough]];
    case filedaemon::bEventBackupCommand:
      [[fallthrough]];
    case filedaemon::bEventEstimateCommand:
      [[fallthrough]];
    case filedaemon::bEventRestoreCommand: {
      auto arguments = plugin_arguments::parse(static_cast<const char*>(data));
      pctx->set_plugin_args(std::move(arguments));
      return bRC_OK;
    }
  }
  return bRC_Error;
}

bRC startBackupFile(PluginContext* ctx, filedaemon::save_pkt* sp)
{
  auto* pctx = get_private_context(ctx);
  (void)pctx;

  auto now = time(NULL);
  sp->fname = const_cast<char*>("disaster.img");
  sp->type = FT_REG;
  sp->statp.st_mode = 0700 | S_IFREG;
  sp->statp.st_ctime = now;
  sp->statp.st_mtime = now;
  sp->statp.st_atime = now;
  sp->statp.st_size = -1;
  sp->statp.st_blksize = 4096;
  sp->statp.st_blocks = 1;

  return bRC_OK;
}
bRC endBackupFile(PluginContext* ctx)
{
  auto* pctx = get_private_context(ctx);
  (void)pctx;
  return bRC_OK;
}
bRC startRestoreFile(PluginContext* ctx, const char* file_name)
{
  (void)ctx;
  (void)file_name;
  return bRC_Error;
}
bRC endRestoreFile(PluginContext* ctx)
{
  (void)ctx;
  return bRC_Error;
}
bRC pluginIO(PluginContext* ctx, filedaemon::io_pkt* pkt)
{
  auto* pctx = get_private_context(ctx);
  switch (pkt->func) {
    case filedaemon::IO_OPEN: {
      if (pctx->has_session()) {
        err_msg(ctx, "context can only be created once");
        pkt->status = -1;
        return bRC_Error;
      }
      try {
        pctx->begin_session(ctx);
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
      if (pkt->count < 0) {
        err_msg(ctx, "its impossible to read {} bytes...", pkt->count);
        pkt->status = -1;
        return bRC_Error;
      }
      pkt->status = pctx->session_read(
          std::span{pkt->buf, static_cast<std::size_t>(pkt->count)});
      return bRC_OK;
    } break;
    case filedaemon::IO_WRITE: {
      err_msg(ctx, "restores are not supported on windows");
      pkt->status = -1;
      return bRC_Error;
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
  return bRC_Error;
}
bRC setFileAttributes(PluginContext* ctx, filedaemon::restore_pkt* pkt)
{
  (void)ctx;
  (void)pkt;
  return bRC_Error;
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

extern "C" __declspec(dllexport) int loadPlugin(
    filedaemon::PluginApiDefinition* core_info,
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

extern "C" __declspec(dllexport) int unloadPlugin() { return 0; }
