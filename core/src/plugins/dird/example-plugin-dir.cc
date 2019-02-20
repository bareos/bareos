/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2010 Free Software Foundation Europe e.V.
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
 * Sample Plugin program
 */
#include "include/bareos.h"
#include "dir_plugins.h"

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Kern Sibbald"
#define PLUGIN_DATE "January 2008"
#define PLUGIN_VERSION "1"
#define PLUGIN_DESCRIPTION "Test Director Daemon Plugin"

namespace directordaemon {

/* Forward referenced functions */
static bRC newPlugin(bpContext* ctx);
static bRC freePlugin(bpContext* ctx);
static bRC getPluginValue(bpContext* ctx, pDirVariable var, void* value);
static bRC setPluginValue(bpContext* ctx, pDirVariable var, void* value);
static bRC handlePluginEvent(bpContext* ctx, bDirEvent* event, void* value);


/* Pointers to Bareos functions */
static bDirFuncs* bfuncs = NULL;
static bDirInfo* binfo = NULL;

static genpInfo pluginInfo = {sizeof(pluginInfo), DIR_PLUGIN_INTERFACE_VERSION,
                              DIR_PLUGIN_MAGIC,   PLUGIN_LICENSE,
                              PLUGIN_AUTHOR,      PLUGIN_DATE,
                              PLUGIN_VERSION,     PLUGIN_DESCRIPTION};

static pDirFuncs pluginFuncs = {
    sizeof(pluginFuncs), DIR_PLUGIN_INTERFACE_VERSION,

    /* Entry points into plugin */
    newPlugin,  /* new plugin instance */
    freePlugin, /* free plugin instance */
    getPluginValue, setPluginValue, handlePluginEvent};

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
bRC loadPlugin(bDirInfo* lbinfo,
               bDirFuncs* lbfuncs,
               genpInfo** pinfo,
               pDirFuncs** pfuncs)
{
  bfuncs = lbfuncs; /* set Bareos funct pointers */
  binfo = lbinfo;
  printf("plugin: Loaded: size=%d version=%d\n", bfuncs->size, bfuncs->version);

  *pinfo = &pluginInfo;   /* return pointer to our info */
  *pfuncs = &pluginFuncs; /* return pointer to our functions */

  return bRC_OK;
}

/**
 * External entry point to unload the plugin
 */
bRC unloadPlugin()
{
  printf("plugin: Unloaded\n");
  return bRC_OK;
}

#ifdef __cplusplus
}
#endif

static bRC newPlugin(bpContext* ctx)
{
  int JobId = 0;
  bfuncs->getBareosValue(ctx, bDirVarJobId, (void*)&JobId);
  printf("plugin: newPlugin JobId=%d\n", JobId);
  bfuncs->registerBareosEvents(ctx, 2, bDirEventJobStart, bDirEventJobEnd);
  return bRC_OK;
}

static bRC freePlugin(bpContext* ctx)
{
  int JobId = 0;
  bfuncs->getBareosValue(ctx, bDirVarJobId, (void*)&JobId);
  printf("plugin: freePlugin JobId=%d\n", JobId);
  return bRC_OK;
}

static bRC getPluginValue(bpContext* ctx, pDirVariable var, void* value)
{
  printf("plugin: getPluginValue var=%d\n", var);
  return bRC_OK;
}

static bRC setPluginValue(bpContext* ctx, pDirVariable var, void* value)
{
  printf("plugin: setPluginValue var=%d\n", var);
  return bRC_OK;
}

static bRC handlePluginEvent(bpContext* ctx, bDirEvent* event, void* value)
{
  char* name;
  int val;
  switch (event->eventType) {
    case bDirEventJobStart:
      printf("plugin: HandleEvent JobStart\n");
      break;
    case bDirEventJobEnd:
      printf("plugin: HandleEvent JobEnd\n");
      bfuncs->getBareosValue(ctx, bDirVarJob, (void*)&name);
      printf("plugin: bDirVarJob=%s\n", name);
      bfuncs->getBareosValue(ctx, bDirVarJobId, (void*)&val);
      printf("plugin: bDirVarJobId=%d\n", val);
      bfuncs->getBareosValue(ctx, bDirVarType, (void*)&val);
      printf("plugin: bDirVarType=%c\n", val);
      bfuncs->getBareosValue(ctx, bDirVarLevel, (void*)&val);
      printf("plugin: bDirVarLevel=%c\n", val);
      bfuncs->getBareosValue(ctx, bDirVarClient, (void*)&name);
      printf("plugin: bDirVarClient=%s\n", name);
      bfuncs->getBareosValue(ctx, bDirVarCatalog, (void*)&name);
      printf("plugin: bDirVarCatalog=%s\n", name);
      bfuncs->getBareosValue(ctx, bDirVarPool, (void*)&name);
      printf("plugin: bDirVarPool=%s\n", name);
      bfuncs->getBareosValue(ctx, bDirVarStorage, (void*)&name);
      printf("plugin: bDirVarStorage=%s\n", name);
      bfuncs->getBareosValue(ctx, bDirVarJobErrors, (void*)&val);
      printf("plugin: bDirVarJobErrors=%d\n", val);
      bfuncs->getBareosValue(ctx, bDirVarJobFiles, (void*)&val);
      printf("plugin: bDirVarJobFiles=%d\n", val);
      bfuncs->getBareosValue(ctx, bDirVarNumVols, (void*)&val);
      printf("plugin: bDirVarNumVols=%d\n", val);
      break;
  }
  bfuncs->getBareosValue(ctx, bDirVarJobName, (void*)&name);
  printf("Job Name=%s\n", name);
  bfuncs->JobMessage(ctx, __FILE__, __LINE__, M_INFO, 0, "JobMesssage message");
  bfuncs->DebugMessage(ctx, __FILE__, __LINE__, 1, "DebugMesssage message");
  return bRC_OK;
}
} /* namespace directordaemon */
