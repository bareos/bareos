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

#define BUILD_PLUGIN
#define BUILDING_DLL /* required for Windows plugin */

#include <cinttypes>
#include <ctime>
#include "include/bareos.h"
#include "filed/fd_plugins.h"

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Sebastian Sura"
#define PLUGIN_DATE "January 2024"
#define PLUGIN_VERSION "1"
#define PLUGIN_DESCRIPTION "Spamming File Daemon Plugin"

namespace filedaemon {
namespace {
CoreFunctions* bareos = NULL;
thread_local std::size_t num_messages = 0;

#define Jmsg(ctx, type, fmt, ...)                                           \
  bareos->JobMessage((ctx), __FILE__, __LINE__, (type), std::time(nullptr), \
                     (fmt), __VA_ARGS__)

template <typename... Events>
void RegisterEvents(PluginContext* ctx, Events... events)
{
  bareos->registerBareosEvents(ctx, sizeof...(events), events...);
}

bRC newPlugin(PluginContext* ctx)
{
  RegisterEvents(ctx, bEventBackupCommand);
  return bRC_OK;
}
bRC freePlugin(PluginContext*) { return bRC_OK; }
bRC getPluginValue(PluginContext*, pVariable, void*) { return bRC_OK; }
bRC setPluginValue(PluginContext*, pVariable, void*) { return bRC_OK; }
bRC handlePluginEvent(PluginContext*, bEvent*, void*) { return bRC_OK; }
bRC startBackupFile(PluginContext* ctx, save_pkt*)
{
  if (num_messages < 50'000) {
    Jmsg(ctx, M_INFO, "I am spamming (%llu)\n", num_messages++);
    return bRC_Skip;
  }

  return bRC_Stop;
}
bRC endBackupFile(PluginContext*) { return bRC_OK; }
bRC pluginIO(PluginContext*, io_pkt* io)
{
  io->status = 0;
  io->io_errno = 0;
  return bRC_OK;
}

bRC startRestoreFile(PluginContext*, const char*) { return bRC_OK; }
bRC endRestoreFile(PluginContext*) { return bRC_OK; }
bRC createFile(PluginContext*, restore_pkt*) { return bRC_OK; }
bRC setFileAttributes(PluginContext*, restore_pkt*) { return bRC_OK; }
bRC getAcl(PluginContext*, acl_pkt*) { return bRC_OK; }
bRC setAcl(PluginContext*, acl_pkt*) { return bRC_OK; }
bRC getXattr(PluginContext*, xattr_pkt*) { return bRC_OK; }
bRC setXattr(PluginContext*, xattr_pkt*) { return bRC_OK; }
bRC checkFile(PluginContext*, char*) { return bRC_OK; }

PluginInformation pluginInfo = {sizeof(pluginInfo),
                                FD_PLUGIN_INTERFACE_VERSION,
                                FD_PLUGIN_MAGIC,
                                PLUGIN_LICENSE,
                                PLUGIN_AUTHOR,
                                PLUGIN_DATE,
                                PLUGIN_VERSION,
                                PLUGIN_DESCRIPTION,
                                nullptr};

PluginFunctions pluginFuncs = {sizeof(pluginFuncs),
                               FD_PLUGIN_INTERFACE_VERSION,
                               &newPlugin,
                               &freePlugin,
                               &getPluginValue,
                               &setPluginValue,
                               &handlePluginEvent,
                               &startBackupFile,
                               &endBackupFile,
                               &startRestoreFile,
                               &endRestoreFile,
                               &pluginIO,
                               &createFile,
                               &setFileAttributes,
                               &checkFile,
                               &getAcl,
                               &setAcl,
                               &getXattr,
                               &setXattr};
};  // namespace

extern "C" {

// Plugin called here when it is first loaded
bRC loadPlugin(PluginApiDefinition*,
               CoreFunctions* bareos_core_functions,
               PluginInformation** plugin_information,
               PluginFunctions** plugin_functions)
{
  bareos = bareos_core_functions;
  *plugin_information = &pluginInfo;
  *plugin_functions = &pluginFuncs;

  return bRC_OK;
}

/*
 * Plugin called here when it is unloaded, normally when
 *  Bareos is going to exit.
 */
bRC unloadPlugin() { return bRC_OK; }
}

} /* namespace filedaemon */
