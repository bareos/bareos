/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2013 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
/*
 * Marco van Wieringen, November 2013
 */
/**
 * @file
 * SCSI Tape Alert Storage daemon Plugin
 */
#include "include/bareos.h"
#include "stored/stored.h"
#include "lib/scsi_tapealert.h"

#define PLUGIN_LICENSE      "Bareos AGPLv3"
#define PLUGIN_AUTHOR       "Marco van Wieringen"
#define PLUGIN_DATE         "November 2013"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "SCSI Tape Alert Storage Daemon Plugin"
#define PLUGIN_USAGE        "(No usage yet)"

/**
 * Forward referenced functions
 */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, psdVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, psdVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bsdEvent *event, void *value);
static bRC handle_tapealert_readout(void *value);

/**
 * Pointers to Bareos functions
 */
static bsdFuncs *bfuncs = NULL;
static bsdInfo *binfo = NULL;

static genpInfo pluginInfo = {
   sizeof(pluginInfo),
   SD_PLUGIN_INTERFACE_VERSION,
   SD_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION,
   PLUGIN_USAGE
};

static psdFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   SD_PLUGIN_INTERFACE_VERSION,

   /*
    * Entry points into plugin
    */
   newPlugin,        /* new plugin instance */
   freePlugin,       /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent
};

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
bRC DLL_IMP_EXP loadPlugin(bsdInfo *lbinfo,
                           bsdFuncs *lbfuncs,
                           genpInfo **pinfo,
                           psdFuncs **pfuncs)
{
   bfuncs = lbfuncs;       /* set Bareos funct pointers */
   binfo  = lbinfo;
   Dmsg2(debuglevel, "scsitapealert-sd: Loaded: size=%d version=%d\n", bfuncs->size, bfuncs->version);
   *pinfo  = &pluginInfo;  /* return pointer to our info */
   *pfuncs = &pluginFuncs; /* return pointer to our functions */

   return bRC_OK;
}

/**
 * External entry point to unload the plugin
 */
bRC DLL_IMP_EXP unloadPlugin()
{
   return bRC_OK;
}

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
static bRC newPlugin(bpContext *ctx)
{
   int JobId = 0;

   bfuncs->getBareosValue(ctx, bsdVarJobId, (void *)&JobId);
   Dmsg1(debuglevel, "scsitapealert-sd: newPlugin JobId=%d\n", JobId);

   /*
    * Only register plugin events we are interested in.
    */
   bfuncs->registerBareosEvents(ctx,
                                6,
                                bsdEventVolumeLoad,
                                bsdEventLabelVerified,
                                bsdEventReadError,
                                bsdEventWriteError,
                                bsdEventVolumeUnload,
                                bsdEventDeviceRelease);

   return bRC_OK;
}

/**
 * Free a plugin instance, i.e. release our private storage
 */
static bRC freePlugin(bpContext *ctx)
{
   int JobId = 0;

   bfuncs->getBareosValue(ctx, bsdVarJobId, (void *)&JobId);
   Dmsg1(debuglevel, "scsitapealert-sd: freePlugin JobId=%d\n", JobId);

   return bRC_OK;
}

/**
 * Return some plugin value (none defined)
 */
static bRC getPluginValue(bpContext *ctx, psdVariable var, void *value)
{
   Dmsg1(debuglevel, "scsitapealert-sd: getPluginValue var=%d\n", var);

   return bRC_OK;
}

/**
 * Set a plugin value (none defined)
 */
static bRC setPluginValue(bpContext *ctx, psdVariable var, void *value)
{
   Dmsg1(debuglevel, "scsitapealert-sd: setPluginValue var=%d\n", var);

   return bRC_OK;
}

/**
 * Handle an event that was generated in Bareos
 */
static bRC handlePluginEvent(bpContext *ctx, bsdEvent *event, void *value)
{
   switch (event->eventType) {
   case bsdEventLabelVerified:
   case bsdEventReadError:
   case bsdEventWriteError:
   case bsdEventVolumeUnload:
      return handle_tapealert_readout(value);
   default:
      Dmsg1(debuglevel, "scsitapealert-sd: Unknown event %d\n", event->eventType);
      return bRC_Error;
   }

   return bRC_OK;
}

static pthread_mutex_t tapealert_operation_mutex = PTHREAD_MUTEX_INITIALIZER;

static bRC handle_tapealert_readout(void *value)
{
   DeviceControlRecord *dcr;
   Device *dev;
   DeviceResource *device;
   uint64_t flags;

   /*
    * Unpack the arguments passed in.
    */
   dcr = (DeviceControlRecord *)value;
   if (!dcr) {
      return bRC_Error;
   }
   dev = dcr->dev;
   if (!dev) {
      return bRC_Error;
   }
   device = dev->device;
   if (!device) {
      return bRC_Error;
   }

   /*
    * See if drive tapealert is enabled.
    */
   if (!device->drive_tapealert_enabled) {
      Dmsg1(debuglevel, "scsitapealert-sd: tapealert is not enabled on device %s\n", dev->dev_name);
      return bRC_OK;
   }

   Dmsg1(debuglevel, "scsitapealert-sd: checking for tapealerts on device %s\n", dev->dev_name);
   P(tapealert_operation_mutex);
   GetTapealertFlags(dev->fd(), dev->dev_name, &flags);
   V(tapealert_operation_mutex);

   Dmsg1(debuglevel, "scsitapealert-sd: checking for tapealerts on device %s DONE\n", dev->dev_name);
   Dmsg1(debuglevel, "scsitapealert-sd: flags: %ld \n", flags);

   if (flags) {
      Dmsg1(debuglevel, "scsitapealert-sd: tapealerts on device %s, calling UpdateTapeAlerts\n", dev->dev_name);
      bfuncs->UpdateTapeAlert(dcr, flags);
   }

   return bRC_OK;
}
