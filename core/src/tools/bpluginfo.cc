/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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

/*
 * Contributed in 2012 by Inteos sp. z o.o.
 */

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
#include "fd_plugins.h"
#include "dir_plugins.h"
#include "sd_plugins.h"
#include "assert_macro.h"
#ifndef __WIN32__
#include <dlfcn.h>
#endif
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

extern "C" {
   typedef int (*loadPlugin) (void *binfo, void *bfuncs, void **pinfo,
               void **pfuncs);
   typedef int (*unloadPlugin) (void);
}
#define DEFAULT_API_VERSION   1

enum plugintype {
   DIRPLUGIN,
   FDPLUGIN,
   SDPLUGIN,
   ERRORPLUGIN
};

/*
 * pDirFuncs
 * pFuncs
 * psdFuncs
 */
typedef union _plugfuncs plugfuncs;
union _plugfuncs {
   pDirFuncs pdirfuncs;
   pFuncs pfdfuncs;
   psdFuncs psdfuncs;
};

/*
 * bDirFuncs
 * bFuncs
 * bsdFuncs
 */
typedef struct _bareosfuncs bareosfuncs;
struct _bareosfuncs {
   uint32_t size;
   uint32_t version;
   int (*registerBareosEvents) (void *ctx, ...);
   int (*getBareosValue) (void *ctx, int var, void *value);
   int (*setBareosValue) (void *ctx, int var, void *value);
   int (*JobMessage) (void *ctx, const char *file, int line, int type, int64_t mtime,
            const char *fmt, ...);
   int (*DebugMessage) (void *ctx, const char *file, int line, int level,
         const char *fmt, ...);
   void *(*bareosMalloc) (void *ctx, const char *file, int line, size_t size);
   void (*bareosFree) (void *ctx, const char *file, int line, void *mem);
};

/*
 * bDirInfo
 * bInfo
 * bsdInfo
 */
typedef union _bareosinfos bareosinfos;
union _bareosinfos {
   bDirInfo bdirinfo;
   bInfo bfdinfo;
   bsdInfo bsdinfo;
};

typedef struct _progdata progdata;
struct _progdata {
   bool verbose;
   bool listinfo;
   bool listfunc;
   char *pluginfile;
   void *pluginhandle;
   int bapiversion;
   int bplugtype;
   gen_pluginInfo *pinfo;
   plugfuncs *pfuncs;
};

/* memory allocation/deallocation */
#define MALLOC(size) \
   (char *) bmalloc ( size );

#define ASSERT_MEMORY(m) \
   if ( m == NULL ){ \
      printf ( "Error: memory allocation error!\n" ); \
      exit (10); \
   }

#define FREE(ptr) \
   if ( ptr != NULL ){ \
      bfree ( ptr ); \
      ptr = NULL; \
   }

int registerBareosEvents(void *ctx, ...)
{
   return 0;
};

int getBareosValue(void *ctx, int var, void *value)
{
   return 0;
};

int setBareosValue(void *ctx, int var, void *value)
{
   return 0;
};

int DebugMessage(void *ctx, const char *file, int line, int level, const char *fmt, ...)
{
#ifdef DEBUGMSG
   printf("DG: %s:%d %s\n", file, line, fmt);
#endif
   return 0;
};

int JobMessage(void *ctx, const char *file, int line, int type, int64_t mtime,
          const char *fmt, ...)
{
#ifdef DEBUGMSG
   printf("JM: %s:%d <%d> %s\n", file, line, type, fmt);
#endif
   return 0;
};

void *bareosMalloc(void *ctx, const char *file, int line, size_t size)
{
   return MALLOC(size);
};

void bareosFree(void *ctx, const char *file, int line, void *mem)
{
   FREE(mem);
};

/*
 * displays a short help
 */
void print_help(int argc, char *argv[])
{

   printf("\n"
     "Usage: bpluginfo [options] <plugin_file.so>\n"
     "       -v          verbose\n"
     "       -i          list plugin header information only (default)\n"
     "       -f          list plugin functions information only\n"
     "       -a <api>    bareos api version (default %d)\n"
     "       -h          help screen\n" "\n", DEFAULT_API_VERSION);
}

/* allocates and resets a main program data variable */
progdata *allocpdata(void)
{

   progdata *pdata;

   pdata = (progdata *) bmalloc(sizeof(progdata));
   ASSERT_MEMORY(pdata);
   memset(pdata, 0, sizeof(progdata));

   return pdata;
}

/* releases all allocated program data resources */
void freepdata(progdata *pdata)
{

   if (pdata->pluginfile) {
      FREE(pdata->pluginfile);
   }
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
void parse_args(progdata *pdata, int argc, char *argv[])
{

   int ch;
   char *dirtmp;
   char *progdir;

   while ((ch = getopt(argc, argv, "a:fiv")) != -1) {
      switch (ch) {
      case 'a':
         pdata->bapiversion = atoi(optarg);
         break;

      case 'f':
         pdata->listfunc = true;
         break;

      case 'i':
         pdata->listinfo = true;
         break;

      case 'v':
         pdata->verbose = true;
         break;

      case '?':
      default:
         print_help(argc, argv);
         exit(0);
      }
   }
   argc -= optind;
   argv += optind;

   if (argc < 1) {
      print_help(argc, argv);
      exit(0);
   }

   if (!pdata->pluginfile) {
      if (argv[0][0] != '/') {
         dirtmp = MALLOC(PATH_MAX);
         ASSERT_MEMORY(dirtmp);
         progdir = MALLOC(PATH_MAX);
         ASSERT_MEMORY(progdir);
         dirtmp = getcwd(dirtmp, PATH_MAX);

         strcat(dirtmp, "/");
         strcat(dirtmp, argv[0]);

         if (realpath(dirtmp, progdir) == NULL) {
            /*
             * Error in resolving path
             */
            FREE(progdir);
            progdir = bstrdup(argv[0]);
         }
         pdata->pluginfile = bstrdup(progdir);
         FREE(dirtmp);
         FREE(progdir);
      } else {
         pdata->pluginfile = bstrdup(argv[0]);
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
int getplugintype(progdata *pdata)
{

   ASSERT_NVAL_RET_V(pdata, ERRORPLUGIN);

   gen_pluginInfo *pinfo = pdata->pinfo;

   ASSERT_NVAL_RET_V(pinfo, ERRORPLUGIN);

   if (pinfo->plugin_magic &&
       bstrcmp(pinfo->plugin_magic, DIR_PLUGIN_MAGIC)) {
      return DIRPLUGIN;
   } else {
      if (pinfo->plugin_magic &&
          bstrcmp(pinfo->plugin_magic, FD_PLUGIN_MAGIC)) {
      return FDPLUGIN;
      } else {
         if (pinfo->plugin_magic &&
             bstrcmp(pinfo->plugin_magic, SD_PLUGIN_MAGIC)) {
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
void dump_pluginfo(progdata *pdata)
{

   ASSERT_NVAL_RET(pdata);

   gen_pluginInfo *pinfo = pdata->pinfo;

   ASSERT_NVAL_RET(pinfo);

   plugfuncs *pfuncs = pdata->pfuncs;

   ASSERT_NVAL_RET(pfuncs);

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
      printf("Plugin magic:\t\t%s\n", NPRT(pinfo->plugin_magic));
   }
   printf("Plugin version:\t\t%s\n", pinfo->plugin_version);
   printf("Plugin release date:\t%s\n", NPRT(pinfo->plugin_date));
   printf("Plugin author:\t\t%s\n", NPRT(pinfo->plugin_author));
   printf("Plugin licence:\t\t%s\n", NPRT(pinfo->plugin_license));
   printf("Plugin description:\t%s\n", NPRT(pinfo->plugin_description));
   printf("Plugin API version:\t%d\n", pinfo->version);
   if (pinfo->plugin_usage) {
      printf("Plugin usage:\n%s\n", pinfo->plugin_usage);
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
void dump_plugfuncs(progdata *pdata)
{

   ASSERT_NVAL_RET(pdata);

   plugfuncs *pfuncs = pdata->pfuncs;

   ASSERT_NVAL_RET(pfuncs);

   printf("\nPlugin functions:\n");

   switch (pdata->bplugtype) {
   case DIRPLUGIN:
      if (pdata->verbose) {
          if (pfuncs->pdirfuncs.newPlugin) {
             printf(" newPlugin()\n");
          }
          if (pfuncs->pdirfuncs.freePlugin) {
             printf(" freePlugin()\n");
          }
      }
      if (pfuncs->pdirfuncs.getPluginValue) {
         printf(" getPluginValue()\n");
      }
      if (pfuncs->pdirfuncs.setPluginValue) {
         printf(" setPluginValue()\n");
      }
      if (pfuncs->pdirfuncs.handlePluginEvent) {
         printf(" handlePluginEvent()\n");
      }
      break;
   case FDPLUGIN:
      if (pdata->verbose) {
          if (pfuncs->pfdfuncs.newPlugin) {
             printf(" newPlugin()\n");
          }
          if (pfuncs->pfdfuncs.freePlugin) {
             printf(" freePlugin()\n");
          }
      }
      if (pfuncs->pfdfuncs.getPluginValue) {
         printf(" getPluginValue()\n");
      }
      if (pfuncs->pfdfuncs.setPluginValue) {
         printf(" setPluginValue()\n");
      }
      if (pfuncs->pfdfuncs.handlePluginEvent) {
         printf(" handlePluginEvent()\n");
      }
      if (pfuncs->pfdfuncs.startBackupFile) {
         printf(" startBackupFile()\n");
      }
      if (pfuncs->pfdfuncs.endBackupFile) {
         printf(" endBackupFile()\n");
      }
      if (pfuncs->pfdfuncs.startRestoreFile) {
         printf(" startRestoreFile()\n");
      }
      if (pfuncs->pfdfuncs.endRestoreFile) {
         printf(" endRestoreFile()\n");
      }
      if (pfuncs->pfdfuncs.pluginIO) {
         printf(" pluginIO()\n");
      }
      if (pfuncs->pfdfuncs.createFile) {
         printf(" createFile()\n");
      }
      if (pfuncs->pfdfuncs.setFileAttributes) {
         printf(" setFileAttributes()\n");
      }
      if (pfuncs->pfdfuncs.checkFile) {
         printf(" checkFile()\n");
      }
      break;
   case SDPLUGIN:
      if (pdata->verbose) {
          if (pfuncs->psdfuncs.newPlugin) {
             printf(" newPlugin()\n");
          }
          if (pfuncs->psdfuncs.freePlugin) {
             printf(" freePlugin()\n");
          }
      }
      if (pfuncs->psdfuncs.getPluginValue) {
         printf(" getPluginValue()\n");
      }
      if (pfuncs->psdfuncs.setPluginValue) {
         printf(" setPluginValue()\n");
      }
      if (pfuncs->psdfuncs.handlePluginEvent) {
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
int main(int argc, char *argv[])
{

   progdata *pdata;
   loadPlugin loadplugfunc;
   unloadPlugin unloadplugfunc;
   bareosfuncs bfuncs = {
      sizeof(bfuncs),
      1,
      registerBareosEvents,
      getBareosValue,
      setBareosValue,
      JobMessage,
      DebugMessage,
      bareosMalloc,
      bareosFree,
   };
   bareosinfos binfos;

   pdata = allocpdata();
   parse_args(pdata, argc, argv);

   binfos.bfdinfo.size = sizeof(binfos);
   binfos.bfdinfo.version = DEFAULT_API_VERSION;

   pdata->pluginhandle = dlopen(pdata->pluginfile, RTLD_LAZY);
   if (pdata->pluginhandle == NULL) {
      printf("\nCannot load a plugin: %s\n\n", dlerror());
      freepdata(pdata);
      exit(1);
   }

   loadplugfunc = (loadPlugin) dlsym(pdata->pluginhandle, "loadPlugin");
   if (loadplugfunc == NULL) {
      printf("\nCannot find loadPlugin function: %s\n", dlerror());
      printf("\nWhether the file is a really Bareos plugin?\n\n");
      freepdata(pdata);
      exit(2);
   }

   unloadplugfunc = (unloadPlugin) dlsym(pdata->pluginhandle, "unloadPlugin");
   if (unloadplugfunc == NULL) {
      printf("\nCannot find unloadPlugin function: %s\n", dlerror());
      printf("\nWhether the file is a really Bareos plugin?\n\n");
      freepdata(pdata);
      exit(3);
   }

   if (pdata->bapiversion > 0) {
      binfos.bdirinfo.version = pdata->bapiversion;
   }

   loadplugfunc(&binfos, &bfuncs, (void **)&pdata->pinfo, (void **)&pdata->pfuncs);

   pdata->bplugtype = getplugintype(pdata);

   if (!pdata->listfunc) {
      dump_pluginfo(pdata);
   }
   if ((!pdata->listinfo && pdata->listfunc) || pdata->verbose) {
      dump_plugfuncs(pdata);
   }
   printf("\n");

   unloadplugfunc();

   dlclose(pdata->pluginhandle);

   freepdata(pdata);

   return 0;
}
