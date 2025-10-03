/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2013 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
// Marco van Wieringen, November 2013
/**
 * @file
 * SCSI Tape Alert Storage daemon Plugin
 */
#include "include/bareos.h"
#include "stored/device_control_record.h"
#include "stored/stored.h"
#include "lib/scsi_tapealert.h"
#include "scsitapealert-sd.h"

using namespace storagedaemon;

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Marco van Wieringen"
#define PLUGIN_DATE "November 2013"
#define PLUGIN_VERSION "1"
#define PLUGIN_DESCRIPTION "SCSI Tape Alert Storage Daemon Plugin"
#define PLUGIN_USAGE "(No usage yet)"

namespace {

void dispatch_messages(JobControlRecord* jcr,
                       const char* device,
                       const char* volume,
                       uint64_t flags)
{
  constexpr const char* job_msg
      = "TapeAlert on device \"%s\" with volume \"%s\": [%d] %s\n%s\n"
        "Possible cause: %s\n";
  constexpr const char* job_msg_novol
      = "TapeAlert on device \"%s\": [%d] %s\n%s\n"
        "Possible cause: %s\n";

  constexpr const char* print_msg
      = "TapeAlert on device \"%s\" with volume \"%s\" from jobid %lu: "
        "[%d] %s\n";
  constexpr const char* print_msg_novol
      = "TapeAlert on device \"%s\" from jobid %lu: [%d] %s\n";

  const uint64_t jobid{jcr ? jcr->JobId : 0};

  for (auto& flag : scsitapealert::flags) {
    if (flag.present_in(flags)) {
      if (volume && volume[0] != '\0') {
        Jmsg(jcr, flag.type, 0, job_msg, device, volume, flag.no, flag.name,
             flag.message, flag.cause);
        Pmsg0(-1, print_msg, device, volume, jobid, flag.no, flag.name);
      } else {
        Jmsg(jcr, flag.type, 0, job_msg_novol, device, flag.no, flag.name,
             flag.message, flag.cause);
        Pmsg0(-1, print_msg_novol, device, jobid, flag.no, flag.name);
      }
    }
  }
}

}  // namespace

// Forward referenced functions
static bRC newPlugin(PluginContext* ctx);
static bRC freePlugin(PluginContext* ctx);
static bRC getPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC setPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC handlePluginEvent(PluginContext* ctx, bSdEvent* event, void* value);
static bRC handle_tapealert_readout(void* value);

// Pointers to Bareos functions
static CoreFunctions* bareos_core_functions = NULL;
static PluginApiDefinition* bareos_plugin_interface_version = NULL;

static PluginInformation pluginInfo
    = {sizeof(pluginInfo), SD_PLUGIN_INTERFACE_VERSION,
       SD_PLUGIN_MAGIC,    PLUGIN_LICENSE,
       PLUGIN_AUTHOR,      PLUGIN_DATE,
       PLUGIN_VERSION,     PLUGIN_DESCRIPTION,
       PLUGIN_USAGE};

static PluginFunctions pluginFuncs
    = {sizeof(pluginFuncs), SD_PLUGIN_INTERFACE_VERSION,

       // Entry points into plugin
       newPlugin,  /* new plugin instance */
       freePlugin, /* free plugin instance */
       getPluginValue, setPluginValue, handlePluginEvent};

static int const debuglevel = 200;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bareos can directly call these two entry points
 *  they are common to all Bareos plugins.
 *
 * External entry point called by Bareos to "load the plugin
 */
BAREOS_EXPORT bRC
loadPlugin(PluginApiDefinition* lbareos_plugin_interface_version,
           CoreFunctions* lbareos_core_functions,
           PluginInformation** plugin_information,
           PluginFunctions** plugin_functions)
{
  bareos_core_functions
      = lbareos_core_functions; /* set Bareos funct pointers */
  bareos_plugin_interface_version = lbareos_plugin_interface_version;
  Dmsg2(debuglevel, "scsitapealert-sd: Loaded: size=%d version=%d\n",
        bareos_core_functions->size, bareos_core_functions->version);
  *plugin_information = &pluginInfo; /* return pointer to our info */
  *plugin_functions = &pluginFuncs;  /* return pointer to our functions */

  return bRC_OK;
}

// External entry point to unload the plugin
BAREOS_EXPORT bRC unloadPlugin() { return bRC_OK; }

#ifdef __cplusplus
}
#endif

/**
 * The following entry points are accessed through the function
 * pointers we supplied to Bareos. Each plugin type (dir, fd, sd)
 * has its own set of entry points that the plugin must define.
 *
 * Create a new instance of the plugin i.e. allocate our private storage
 */
static bRC newPlugin(PluginContext* ctx)
{
  int JobId = 0;

  bareos_core_functions->getBareosValue(ctx, bsdVarJobId, (void*)&JobId);
  Dmsg1(debuglevel, "scsitapealert-sd: newPlugin JobId=%d\n", JobId);

  // Only register plugin events we are interested in.
  bareos_core_functions->registerBareosEvents(
      ctx, 10, bSdEventDeviceInit, bSdEventVolumeLoad, bSdEventLabelVerified,
      bSdEventLabelWrite, bSdEventReadError, bSdEventWriteError,
      bSdEventVolumeUnload, bSdEventDeviceRelease, bSdEventDeviceOpen,
      bSdEventDeviceClose);

  return bRC_OK;
}

// Free a plugin instance, i.e. release our private storage
static bRC freePlugin(PluginContext* ctx)
{
  int JobId = 0;

  bareos_core_functions->getBareosValue(ctx, bsdVarJobId, (void*)&JobId);
  Dmsg1(debuglevel, "scsitapealert-sd: freePlugin JobId=%d\n", JobId);

  return bRC_OK;
}

// Return some plugin value (none defined)
static bRC getPluginValue(PluginContext*, pVariable var, void*)
{
  Dmsg1(debuglevel, "scsitapealert-sd: getPluginValue var=%d\n", var);

  return bRC_OK;
}

// Set a plugin value (none defined)
static bRC setPluginValue(PluginContext*, pVariable var, void*)
{
  Dmsg1(debuglevel, "scsitapealert-sd: setPluginValue var=%d\n", var);

  return bRC_OK;
}

// Handle an event that was generated in Bareos
static bRC handlePluginEvent(PluginContext*, bSdEvent* event, void* value)
{
  switch (event->eventType) {
    case bSdEventDeviceClose:
    case bSdEventDeviceInit:
    case bSdEventDeviceOpen:
    case bSdEventDeviceRelease:
    case bSdEventLabelVerified:
    case bSdEventLabelWrite:
    case bSdEventReadError:
    case bSdEventVolumeLoad:
    case bSdEventVolumeUnload:
    case bSdEventWriteError:
      return handle_tapealert_readout(value);
    default:
      Dmsg1(debuglevel, "scsitapealert-sd: Unknown event %d\n",
            event->eventType);
      return bRC_Error;
  }

  return bRC_OK;
}

static pthread_mutex_t tapealert_operation_mutex = PTHREAD_MUTEX_INITIALIZER;

static bRC handle_tapealert_readout(void* value)
{
  DeviceControlRecord* dcr;
  Device* dev;
  DeviceResource* device_resource;
  uint64_t flags;

  // Unpack the arguments passed in.
  dcr = (DeviceControlRecord*)value;
  if (!dcr) { return bRC_Error; }
  dev = dcr->dev;
  if (!dev) { return bRC_Error; }
  device_resource = dev->device_resource;
  if (!device_resource) { return bRC_Error; }

  // See if drive tapealert is enabled.
  if (!device_resource->drive_tapealert_enabled) {
    Dmsg1(debuglevel,
          "scsitapealert-sd: tapealert is not enabled on device %s\n",
          dev->archive_device_string);
    return bRC_OK;
  }

  Dmsg1(debuglevel, "scsitapealert-sd: checking for tapealerts on device %s\n",
        dev->archive_device_string);
  lock_mutex(tapealert_operation_mutex);
  GetTapealertFlags(dev->fd, dev->archive_device_string, &flags);
  unlock_mutex(tapealert_operation_mutex);

  Dmsg1(debuglevel,
        "scsitapealert-sd: checking for tapealerts on device %s DONE\n",
        dev->archive_device_string);
  Dmsg1(debuglevel, "scsitapealert-sd: flags: %ld \n", flags);

  if (flags) {
    Dmsg1(
        debuglevel,
        "scsitapealert-sd: tapealerts on device %s, calling UpdateTapeAlerts\n",
        dev->archive_device_string);
    bareos_core_functions->UpdateTapeAlert(dcr, flags);
    dispatch_messages(dcr->jcr, device_resource->resource_name_,
                      dev->getVolCatName(), flags);
  }

  return bRC_OK;
}
