/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2020 Bareos GmbH & Co. KG

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
/**
 * @file
 * Python module for the Bareos storagedaemon plugin
 */
#define PY_SSIZE_T_CLEAN
#define BUILD_PLUGIN

#if defined(HAVE_WIN32)
#include "include/bareos.h"
#include <Python.h>
#else
#include <Python.h>
#include "include/bareos.h"
#endif

#include "stored/sd_plugins.h"

#define BAREOSSD_MODULE
#include "bareossd.h"
#include "lib/edit.h"

namespace storagedaemon {

static const int debuglevel = 150;

static bRC set_bareos_core_functions(
    StorageDaemonCoreFunctions* new_bareos_core_functions);
static bRC set_plugin_context(PluginContext* new_plugin_context);
static bRC PyParsePluginDefinition(PluginContext* plugin_ctx, void* value);

static bRC PyGetPluginValue(PluginContext* plugin_ctx,
                            pVariable var,
                            void* value);
static bRC PySetPluginValue(PluginContext* plugin_ctx,
                            pVariable var,
                            void* value);
static bRC PyHandlePluginEvent(PluginContext* plugin_ctx,
                               bSdEvent* event,
                               void* value);

/* Pointers to Bareos functions */
static StorageDaemonCoreFunctions* bareos_core_functions = NULL;

#include "plugin_private_context.h"

#define NOPLUGINSETGETVALUE 1
/* functions common to all plugins */
#include "plugins/python_plugins_common.inc"


/* set the bareos_core_functions pointer to the given value */
static bRC set_bareos_core_functions(
    StorageDaemonCoreFunctions* new_bareos_core_functions)
{
  bareos_core_functions = new_bareos_core_functions;
  return bRC_OK;
}

/* set the plugin context pointer to the given value */
static bRC set_plugin_context(PluginContext* new_plugin_context)
{
  plugin_context = new_plugin_context;
  return bRC_OK;
}


} /* namespace storagedaemon*/
