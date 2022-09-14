/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

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

// Contributed in 2012 by Inteos sp. z o.o.

/**
 * Utility tool display various information about Bareos plugin,
 * including but not limited to:
 * - Name and Author of the plugin
 * - Plugin License
 * - Description
 * - API version
 * - Enabled functions, etc.
 */

#include "include/bareos.h"
#include "filed/fd_plugins.h"
#include "dird/dir_plugins.h"
#include "lib/cli.h"
#include "stored/sd_plugins.h"
#include "assert_macro.h"
#ifndef __WIN32__
#  include <dlfcn.h>
#endif
#ifndef PATH_MAX
#  define PATH_MAX 4096
#endif

extern "C" {
typedef int (*loadPlugin)(void* bareos_plugin_interface_version,
                          void* bareos_core_functions,
                          void** plugin_information,
                          void** plugin_functions);
typedef int (*unloadPlugin)(void);
}
#define DEFAULT_API_VERSION 1

enum plugintype
{
  DIRPLUGIN,
  FDPLUGIN,
  SDPLUGIN,
  ERRORPLUGIN
};

// PluginFunctions
typedef union _plugfuncs plugfuncs;
union _plugfuncs {
  directordaemon::PluginFunctions pdirfuncs;
  filedaemon::PluginFunctions pfdfuncs;
  storagedaemon::PluginFunctions psdfuncs;
};

// CoreFunctions
typedef struct _bareosfuncs bareosfuncs;
struct _bareosfuncs {
  uint32_t size;
  uint32_t version;
  int (*registerBareosEvents)(void* ctx, ...);
  int (*getBareosValue)(void* ctx, int var, void* value);
  int (*setBareosValue)(void* ctx, int var, void* value);
  int (*JobMessage)(void* ctx,
                    const char* file,
                    int line,
                    int type,
                    int64_t mtime,
                    const char* fmt,
                    ...);
  int (*DebugMessage)(void* ctx,
                      const char* file,
                      int line,
                      int level,
                      const char* fmt,
                      ...);
  void* (*bareosMalloc)(void* ctx, const char* file, int line, size_t size);
  void (*bareosFree)(void* ctx, const char* file, int line, void* mem);
};

// PluginApiDefinition
typedef union _bareosinfos bareosinfos;
union _bareosinfos {
  directordaemon::PluginApiDefinition bdirinfo;
  filedaemon::PluginApiDefinition bfdinfo;
  storagedaemon::PluginApiDefinition bsdinfo;
};

typedef struct _progdata progdata;
struct _progdata {
  bool verbose;
  bool listinfo;
  bool listfunc;
  char* pluginfile;
  void* pluginhandle;
  int bapiversion;
  int bplugtype;
  gen_pluginInfo* plugin_information;
  plugfuncs* plugin_functions;
};

/* memory allocation/deallocation */
#define MALLOC(size) (char*)malloc(size);

#define ASSERT_MEMORY(m)                         \
  if (m == NULL) {                               \
    printf("Error: memory allocation error!\n"); \
    exit(10);                                    \
  }

#define FREE(ptr)    \
  if (ptr != NULL) { \
    free(ptr);       \
    ptr = NULL;      \
  }

int registerBareosEvents([[maybe_unused]] void* ctx, ...) { return 0; };

int getBareosValue([[maybe_unused]] void* ctx,
                   [[maybe_unused]] int var,
                   [[maybe_unused]] void* value)
{
  return 0;
};

int setBareosValue([[maybe_unused]] void* ctx,
                   [[maybe_unused]] int var,
                   [[maybe_unused]] void* value)
{
  return 0;
};

int DebugMessage([[maybe_unused]] void* ctx,
                 [[maybe_unused]] const char* file,
                 [[maybe_unused]] int line,
                 [[maybe_unused]] int level,
                 [[maybe_unused]] const char* fmt,
                 ...)
{
#ifdef DEBUGMSG
  printf("DG: %s:%d %s\n", file, line, fmt);
#endif
  return 0;
};

int JobMessage([[maybe_unused]] void* ctx,
               [[maybe_unused]] const char* file,
               [[maybe_unused]] int line,
               [[maybe_unused]] int type,
               [[maybe_unused]] int64_t mtime,
               [[maybe_unused]] const char* fmt,
               ...)
{
#ifdef DEBUGMSG
  printf("JM: %s:%d <%d> %s\n", file, line, type, fmt);
#endif
  return 0;
};

void* bareosMalloc([[maybe_unused]] void* ctx,
                   [[maybe_unused]] const char* file,
                   [[maybe_unused]] int line,
                   size_t size)
{
  return MALLOC(size);
};

void bareosFree([[maybe_unused]] void* ctx,
                [[maybe_unused]] const char* file,
                [[maybe_unused]] int line,
                void* mem)
{
  FREE(mem);
};

/* allocates and resets a main program data variable */
progdata* allocpdata(void)
{
  progdata* pdata;

  pdata = (progdata*)malloc(sizeof(progdata));
  ASSERT_MEMORY(pdata);
  memset(pdata, 0, sizeof(progdata));

  return pdata;
}

/* releases all allocated program data resources */
void Freepdata(progdata* pdata)
{
  if (pdata->pluginfile) { FREE(pdata->pluginfile); }
  FREE(pdata);
}

/*
 * parse execution arguments and fills required pdata structure fields
 *
 * input:
 *    pdata - pointer to program data structure
 *    argc, argv - execution envinroment variables
 * output:
 *    pdata - required structure fields
 *
 * supported options:
 * -v    verbose flag
 * -i    list plugin header info only (default)
 * -f    list implemented functions only
 * -a    bareos api version (default 1)
 * -h    help screen
 */
void ParseArgs(progdata* pdata, int argc, char* argv[])
{
  CLI::App bpluginfo_app;
  InitCLIApp(bpluginfo_app, "The Bareos Plugin Info tool.");

  bpluginfo_app
      .add_option("-a,--api", pdata->bapiversion,
                  "bareos api version (default :"
                      + std::to_string(DEFAULT_API_VERSION) + ").")
      ->check(CLI::NonNegativeNumber)
      ->type_name("<api>");

  bpluginfo_app.add_flag("-f,--list-functions", pdata->listfunc,
                         "list plugin functions information only.");

  bpluginfo_app.add_flag("-i,--list-headers", pdata->listinfo,
                         "list plugin header information only.");

  bpluginfo_app.add_flag("-v,--verbose", pdata->verbose, "Verbose.");

  std::string plugin_file{};
  bpluginfo_app.add_option("plugin_file.so", plugin_file, "Plugin file.")
      ->required();

  try {
    (bpluginfo_app).parse((argc), (argv));
  } catch (const CLI::ParseError& e) {
    exit((bpluginfo_app).exit(e));
  }

  char* dirtmp;
  char* progdir;
  if (!pdata->pluginfile) {
    if (plugin_file[0] != '/') {
      dirtmp = MALLOC(PATH_MAX);
      ASSERT_MEMORY(dirtmp);
      progdir = MALLOC(PATH_MAX);
      ASSERT_MEMORY(progdir);
      dirtmp = getcwd(dirtmp, PATH_MAX);

      strcat(dirtmp, "/");
      strcat(dirtmp, plugin_file.c_str());

      if (realpath(dirtmp, progdir) == NULL) {
        // Error in resolving path
        FREE(progdir);
        progdir = strdup(plugin_file.c_str());
      }
      pdata->pluginfile = strdup(progdir);
      FREE(dirtmp);
      FREE(progdir);
    } else {
      pdata->pluginfile = strdup(plugin_file.c_str());
    }
  }
}

/*
 * checks a plugin type based on a plugin magic string
 *
 * input:
 *    pdata - program data with plugin info structure
 * output:
 *    int - enum plugintype
 */
int Getplugintype(progdata* pdata)
{
  ASSERT_NVAL_RET_V(pdata, ERRORPLUGIN);

  gen_pluginInfo* plugin_information = pdata->plugin_information;

  ASSERT_NVAL_RET_V(plugin_information, ERRORPLUGIN);

  if (plugin_information->plugin_magic
      && bstrcmp(plugin_information->plugin_magic, DIR_PLUGIN_MAGIC)) {
    return DIRPLUGIN;
  } else {
    if (plugin_information->plugin_magic
        && bstrcmp(plugin_information->plugin_magic, FD_PLUGIN_MAGIC)) {
      return FDPLUGIN;
    } else {
      if (plugin_information->plugin_magic
          && bstrcmp(plugin_information->plugin_magic, SD_PLUGIN_MAGIC)) {
        return SDPLUGIN;
      } else {
        return ERRORPLUGIN;
      }
    }
  }
}

/*
 * prints any available information about a plugin
 *
 * input:
 *    pdata - program data with plugin info structure
 * output:
 *    printed info
 */
void DumpPluginfo(progdata* pdata)
{
  ASSERT_NVAL_RET(pdata);

  gen_pluginInfo* plugin_information = pdata->plugin_information;

  ASSERT_NVAL_RET(plugin_information);

  plugfuncs* plugin_functions = pdata->plugin_functions;

  ASSERT_NVAL_RET(plugin_functions);

  switch (pdata->bplugtype) {
    case DIRPLUGIN:
      printf("\nPlugin type:\t\tBareos Director plugin\n");
      break;
    case FDPLUGIN:
      printf("\nPlugin type:\t\tBareos File Daemon plugin\n");
      break;
    case SDPLUGIN:
      printf("\nPlugin type:\t\tBareos Storage plugin\n");
      break;
    default:
      printf("\nUnknown plugin type or other Error\n\n");
      return;
  }

  if (pdata->verbose) {
    printf("Plugin magic:\t\t%s\n", NPRT(plugin_information->plugin_magic));
  }
  printf("Plugin version:\t\t%s\n", plugin_information->plugin_version);
  printf("Plugin release date:\t%s\n", NPRT(plugin_information->plugin_date));
  printf("Plugin author:\t\t%s\n", NPRT(plugin_information->plugin_author));
  printf("Plugin licence:\t\t%s\n", NPRT(plugin_information->plugin_license));
  printf("Plugin description:\t%s\n",
         NPRT(plugin_information->plugin_description));
  printf("Plugin API version:\t%d\n", plugin_information->version);
  if (plugin_information->plugin_usage) {
    printf("Plugin usage:\n%s\n", plugin_information->plugin_usage);
  }
}

/*
 * prints any available information about plugin' functions
 *
 * input:
 *    pdata - program data with plugin info structure
 * output:
 *    printed info
 */
void DumpPlugfuncs(progdata* pdata)
{
  ASSERT_NVAL_RET(pdata);

  plugfuncs* plugin_functions = pdata->plugin_functions;

  ASSERT_NVAL_RET(plugin_functions);

  printf("\nPlugin functions:\n");

  switch (pdata->bplugtype) {
    case DIRPLUGIN:
      if (pdata->verbose) {
        if (plugin_functions->pdirfuncs.newPlugin) { printf(" newPlugin()\n"); }
        if (plugin_functions->pdirfuncs.freePlugin) {
          printf(" freePlugin()\n");
        }
      }
      if (plugin_functions->pdirfuncs.getPluginValue) {
        printf(" getPluginValue()\n");
      }
      if (plugin_functions->pdirfuncs.setPluginValue) {
        printf(" setPluginValue()\n");
      }
      if (plugin_functions->pdirfuncs.handlePluginEvent) {
        printf(" handlePluginEvent()\n");
      }
      break;
    case FDPLUGIN:
      if (pdata->verbose) {
        if (plugin_functions->pfdfuncs.newPlugin) { printf(" newPlugin()\n"); }
        if (plugin_functions->pfdfuncs.freePlugin) {
          printf(" freePlugin()\n");
        }
      }
      if (plugin_functions->pfdfuncs.getPluginValue) {
        printf(" getPluginValue()\n");
      }
      if (plugin_functions->pfdfuncs.setPluginValue) {
        printf(" setPluginValue()\n");
      }
      if (plugin_functions->pfdfuncs.handlePluginEvent) {
        printf(" handlePluginEvent()\n");
      }
      if (plugin_functions->pfdfuncs.startBackupFile) {
        printf(" startBackupFile()\n");
      }
      if (plugin_functions->pfdfuncs.endBackupFile) {
        printf(" endBackupFile()\n");
      }
      if (plugin_functions->pfdfuncs.startRestoreFile) {
        printf(" startRestoreFile()\n");
      }
      if (plugin_functions->pfdfuncs.endRestoreFile) {
        printf(" endRestoreFile()\n");
      }
      if (plugin_functions->pfdfuncs.pluginIO) { printf(" pluginIO()\n"); }
      if (plugin_functions->pfdfuncs.createFile) { printf(" createFile()\n"); }
      if (plugin_functions->pfdfuncs.setFileAttributes) {
        printf(" setFileAttributes()\n");
      }
      if (plugin_functions->pfdfuncs.checkFile) { printf(" checkFile()\n"); }
      break;
    case SDPLUGIN:
      if (pdata->verbose) {
        if (plugin_functions->psdfuncs.newPlugin) { printf(" newPlugin()\n"); }
        if (plugin_functions->psdfuncs.freePlugin) {
          printf(" freePlugin()\n");
        }
      }
      if (plugin_functions->psdfuncs.getPluginValue) {
        printf(" getPluginValue()\n");
      }
      if (plugin_functions->psdfuncs.setPluginValue) {
        printf(" setPluginValue()\n");
      }
      if (plugin_functions->psdfuncs.handlePluginEvent) {
        printf(" handlePluginEvent()\n");
      }
      break;
    default:
      printf("\nUnknown plugin type or other Error\n\n");
  }
}

/*
 * input parameters:
 *    argv[0] [options] <plugin_filename.so>
 *
 * exit codes:
 *    0 - success
 *    1 - cannot load a plugin
 *    2 - cannot find a loadPlugin function
 *    3 - cannot find an unloadPlugin function
 *    10 - not enough memory
 */
int main(int argc, char* argv[])
{
  progdata* pdata;
  loadPlugin loadplugfunc;
  unloadPlugin unloadplugfunc;
  bareosfuncs bareos_core_functions = {
      sizeof(bareos_core_functions),
      1,
      registerBareosEvents,
      getBareosValue,
      setBareosValue,
      JobMessage,
      DebugMessage,
      bareosMalloc,
      bareosFree,
  };
  bareosinfos bareos_plugin_interface_version;

  pdata = allocpdata();
  ParseArgs(pdata, argc, argv);

  bareos_plugin_interface_version.bfdinfo.size
      = sizeof(bareos_plugin_interface_version);
  bareos_plugin_interface_version.bfdinfo.version = DEFAULT_API_VERSION;

  pdata->pluginhandle = dlopen(pdata->pluginfile, RTLD_LAZY);
  if (pdata->pluginhandle == NULL) {
    printf("\nCannot load a plugin: %s\n\n", dlerror());
    Freepdata(pdata);
    exit(1);
  }

  loadplugfunc = (loadPlugin)dlsym(pdata->pluginhandle, "loadPlugin");
  if (loadplugfunc == NULL) {
    printf("\nCannot find loadPlugin function: %s\n", dlerror());
    printf("\nWhether the file is a really Bareos plugin?\n\n");
    Freepdata(pdata);
    exit(2);
  }

  unloadplugfunc = (unloadPlugin)dlsym(pdata->pluginhandle, "unloadPlugin");
  if (unloadplugfunc == NULL) {
    printf("\nCannot find unloadPlugin function: %s\n", dlerror());
    printf("\nWhether the file is a really Bareos plugin?\n\n");
    Freepdata(pdata);
    exit(3);
  }

  if (pdata->bapiversion > 0) {
    bareos_plugin_interface_version.bdirinfo.version = pdata->bapiversion;
  }

  loadplugfunc(&bareos_plugin_interface_version, &bareos_core_functions,
               (void**)&pdata->plugin_information,
               (void**)&pdata->plugin_functions);

  pdata->bplugtype = Getplugintype(pdata);

  if (!pdata->listfunc) { DumpPluginfo(pdata); }
  if ((!pdata->listinfo && pdata->listfunc) || pdata->verbose) {
    DumpPlugfuncs(pdata);
  }
  printf("\n");

  unloadplugfunc();

  dlclose(pdata->pluginhandle);

  Freepdata(pdata);

  return 0;
}
