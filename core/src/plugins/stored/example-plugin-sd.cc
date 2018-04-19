/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 *  Kern Sibbald, October 2007
 */
/**
 * @file
 * Sample Storage daemon Plugin program
 */
#include "include/bareos.h"
#include "stored/stored.h"

#define PLUGIN_LICENSE      "Bareos AGPLv3"
#define PLUGIN_AUTHOR       "Kern Sibbald"
#define PLUGIN_DATE         "November 2011"
#define PLUGIN_VERSION      "2"
#define PLUGIN_DESCRIPTION  "Test Storage Daemon Plugin"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, psdVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, psdVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bsdEvent *event, void *value);


/* Pointers to Bareos functions */
static bsdFuncs *bfuncs = NULL;
static bsdInfo  *binfo = NULL;

static genpInfo pluginInfo = {
   sizeof(pluginInfo),
   SD_PLUGIN_INTERFACE_VERSION,
   SD_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION
};

static psdFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   SD_PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,                         /* new plugin instance */
   freePlugin,                        /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bareos can directly call these two entry points
 *  they are common to all Bareos plugins.
 *
 * External entry point called by Bareos to "load" the plugin
 */
bRC DLL_IMP_EXP loadPlugin(bsdInfo *lbinfo,
                           bsdFuncs *lbfuncs,
                           genpInfo **pinfo,
                           psdFuncs **pfuncs)
{
   bfuncs = lbfuncs;                /* set Bareos funct pointers */
   binfo  = lbinfo;
   printf("example-plugin-sd: Loaded: size=%d version=%d\n", bfuncs->size, bfuncs->version);
   *pinfo  = &pluginInfo;             /* return pointer to our info */
   *pfuncs = &pluginFuncs;            /* return pointer to our functions */
   printf("example-plugin-sd: Loaded\n");
   return bRC_OK;
}

/**
 * External entry point to unload the plugin
 */
bRC DLL_IMP_EXP unloadPlugin()
{
   printf("example-plugin-sd: Unloaded\n");
   return bRC_OK;
}

#ifdef __cplusplus
}
#endif

/**
 * The following entry points are accessed through the function
 *   pointers we supplied to Bareos. Each plugin type (dir, fd, sd)
 *   has its own set of entry points that the plugin must define.
 */
/**
 * Create a new instance of the plugin i.e. allocate our private storage
 */
static bRC newPlugin(bpContext *ctx)
{
   int JobId = 0;
   bfuncs->getBareosValue(ctx, bsdVarJobId, (void *)&JobId);
   printf("example-plugin-sd: newPlugin JobId=%d\n", JobId);
   bfuncs->registerBareosEvents(ctx,
                                2,
                                bsdEventJobStart,
                                bsdEventJobEnd);
   return bRC_OK;
}

/**
 * Free a plugin instance, i.e. release our private storage
 */
static bRC freePlugin(bpContext *ctx)
{
   int JobId = 0;
   bfuncs->getBareosValue(ctx, bsdVarJobId, (void *)&JobId);
   printf("example-plugin-sd: freePlugin JobId=%d\n", JobId);
   return bRC_OK;
}

/**
 * Return some plugin value (none defined)
 */
static bRC getPluginValue(bpContext *ctx, psdVariable var, void *value)
{
   printf("example-plugin-sd: getPluginValue var=%d\n", var);
   return bRC_OK;
}

/**
 * Set a plugin value (none defined)
 */
static bRC setPluginValue(bpContext *ctx, psdVariable var, void *value)
{
   printf("example-plugin-sd: setPluginValue var=%d\n", var);
   return bRC_OK;
}

/**
 * Handle an event that was generated in Bareos
 */
static bRC handlePluginEvent(bpContext *ctx, bsdEvent *event, void *value)
{
   char *name;
   switch (event->eventType) {
   case bsdEventJobStart:
      printf("example-plugin-sd: HandleEvent JobStart :%s:\n", (char *)value);
      break;
   case bsdEventJobEnd:
      printf("example-plugin-sd: HandleEvent JobEnd\n");
      break;
   }
   bfuncs->getBareosValue(ctx, bsdVarJobName, (void *)&name);
   printf("Job Name=%s\n", name);
   bfuncs->JobMessage(ctx, __FILE__, __LINE__, 1, 0, "JobMesssage message");
   bfuncs->DebugMessage(ctx, __FILE__, __LINE__, 1, "DebugMesssage message");
   return bRC_OK;
}
