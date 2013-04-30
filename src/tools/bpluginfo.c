/*
 * Contributed in 2012 by Inteos sp. z o.o.
 * 
 * Utility tool display various information about Bacula plugin,
 * including but not limited to:
 * - Name and Author of the plugin
 * - Plugin License
 * - Description
 * - API version
 * - Enabled functions, etc.
 */
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2006-2012 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/

#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifndef __WIN32__
#include <dlfcn.h>
#endif
#include "bacula.h"
#include "../filed/fd_plugins.h"
#include "../dird/dir_plugins.h"
// I can't include sd_plugins.h here ...
#include "../stored/stored.h"
#include "assert_macro.h"

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
   ERRORPLUGIN,
};

/*
 * pDirInfo
 * pInfo
 * psdInfo
 */
typedef union _pluginfo pluginfo;
union _pluginfo {
   pDirInfo pdirinfo;
   pInfo pfdinfo;
   psdInfo psdinfo;
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
/*
 * TODO: change to union
 * 
typedef union _baculafuncs baculafuncs;
union _baculafuncs {
   bDirFuncs bdirfuncs;
   bFuncs bfdfuncs;
   bsdFuncs bsdfuncs;
};
*/
typedef struct _baculafuncs baculafuncs;
struct _baculafuncs {
   uint32_t size;
   uint32_t version;
   int (*registerBaculaEvents) (void *ctx, ...);
   int (*getBaculaValue) (void *ctx, int var, void *value);
   int (*setBaculaValue) (void *ctx, int var, void *value);
   int (*JobMessage) (void *ctx, const char *file, int line, int type, int64_t mtime,
            const char *fmt, ...);
   int (*DebugMessage) (void *ctx, const char *file, int line, int level,
         const char *fmt, ...);
   void *(*baculaMalloc) (void *ctx, const char *file, int line, size_t size);
   void (*baculaFree) (void *ctx, const char *file, int line, void *mem);
};

/* 
 * bDirInfo
 * bInfo
 * bsdInfo
 */
typedef union _baculainfos baculainfos;
union _baculainfos {
   bDirInfo bdirinfo;
   bInfo bfdinfo;
   bsdInfo bsdinfo;
};

/*
typedef struct _baculainfos baculainfos;
struct _baculainfos {
   uint32_t size;
   uint32_t version;
};
*/

typedef struct _progdata progdata;
struct _progdata {
   int verbose;
   int listinfo;
   int listfunc;
   char *pluginfile;
   void *pluginhandle;
   int bapiversion;
   int bplugtype;
   pluginfo *pinfo;
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

int registerBaculaEvents(void *ctx, ...)
{
   return 0;
};

int getBaculaValue(void *ctx, int var, void *value)
{
   return 0;
};

int setBaculaValue(void *ctx, int var, void *value)
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

void *baculaMalloc(void *ctx, const char *file, int line, size_t size)
{
   return MALLOC(size);
};

void baculaFree(void *ctx, const char *file, int line, void *mem)
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
     "       -a <api>    bacula api version (default %d)\n"
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
void freepdata(progdata * pdata)
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
 * -a    bacula api version (default 1)
 * -h    help screen
 */
void parse_args(progdata * pdata, int argc, char *argv[])
{

   int i;
   char *dirtmp;
   char *progdir;
   int api;
   int s;

   if (argc < 2) {
      /* TODO - add a real help screen */
      printf("\nNot enough parameters!\n");
      print_help(argc, argv);
      exit(1);
   }

   if (argc > 5) {
      /* TODO - add a real help screen */
      printf("\nToo many parameters!\n");
      print_help(argc, argv);
      exit(1);
   }

   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-h") == 0) {
         /* help screen */
         print_help(argc, argv);
         exit(0);
      }
      if (strcmp(argv[i], "-v") == 0) {
          /* verbose option */
          pdata->verbose = 1;
          continue;
      }
      if (strcmp(argv[i], "-f") == 0) {
         /* functions list */
         pdata->listfunc = 1;
         continue;
      }
      if (strcmp(argv[i], "-i") == 0) {
         /* header list */
         pdata->listinfo = 1;
         continue;
      }
      if (strcmp(argv[i], "-a") == 0) {
         /* bacula api version */
         if (i < argc - 1) {
            s = sscanf(argv[i + 1], "%d", &api);
            if (s == 1) {
               pdata->bapiversion = api;
               i++;
               continue;
            }
         }
         printf("\nAPI version number required!\n");
         print_help(argc, argv);
         exit(1);
      }
      if (!pdata->pluginfile) {
          if (argv[i][0] != '/') {
             dirtmp = MALLOC(PATH_MAX);
             ASSERT_MEMORY(dirtmp);
             progdir = MALLOC(PATH_MAX);
             ASSERT_MEMORY(progdir);
             dirtmp = getcwd(dirtmp, PATH_MAX);
      
             strcat(dirtmp, "/");
             strcat(dirtmp, argv[i]);
      
             if (realpath(dirtmp, progdir) == NULL) {
                /* error in resolving path */
                FREE(progdir);
                progdir = bstrdup(argv[i]);
             }
             pdata->pluginfile = bstrdup(progdir);
             FREE(dirtmp);
             FREE(progdir);
          } else {
             pdata->pluginfile = bstrdup(argv[i]);
          }
    continue;
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
int getplugintype(progdata * pdata)
{

   ASSERT_NVAL_RET_V(pdata, ERRORPLUGIN);

   pluginfo *pinfo = pdata->pinfo;

   ASSERT_NVAL_RET_V(pinfo, ERRORPLUGIN);

   if (pinfo->pdirinfo.plugin_magic &&
       strcmp(pinfo->pdirinfo.plugin_magic, DIR_PLUGIN_MAGIC) == 0) {
      return DIRPLUGIN;
   } else
      if (pinfo->pfdinfo.plugin_magic &&
     strcmp(pinfo->pfdinfo.plugin_magic, FD_PLUGIN_MAGIC) == 0) {
      return FDPLUGIN;
   } else
      if (pinfo->psdinfo.plugin_magic &&
     strcmp(pinfo->psdinfo.plugin_magic, SD_PLUGIN_MAGIC) == 0) {
      return SDPLUGIN;
   } else {
      return ERRORPLUGIN;
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
void dump_pluginfo(progdata * pdata)
{

   ASSERT_NVAL_RET(pdata);

   pluginfo *pinfo = pdata->pinfo;

   ASSERT_NVAL_RET(pinfo);

   plugfuncs *pfuncs = pdata->pfuncs;

   ASSERT_NVAL_RET(pfuncs);

   switch (pdata->bplugtype) {
   case DIRPLUGIN:
      printf("\nPlugin type:\t\tBacula Director plugin\n");
      if (pdata->verbose) {
         printf("Plugin magic:\t\t%s\n", NPRT(pinfo->pdirinfo.plugin_magic));
      }
      printf("Plugin version:\t\t%s\n", pinfo->pdirinfo.plugin_version);
      printf("Plugin release date:\t%s\n", NPRT(pinfo->pdirinfo.plugin_date));
      printf("Plugin author:\t\t%s\n", NPRT(pinfo->pdirinfo.plugin_author));
      printf("Plugin licence:\t\t%s\n", NPRT(pinfo->pdirinfo.plugin_license));
      printf("Plugin description:\t%s\n", NPRT(pinfo->pdirinfo.plugin_description));
      printf("Plugin API version:\t%d\n", pinfo->pdirinfo.version);
      break;
   case FDPLUGIN:
      printf("\nPlugin type:\t\tFile Daemon plugin\n");
      if (pdata->verbose) {
         printf("Plugin magic:\t\t%s\n", NPRT(pinfo->pfdinfo.plugin_magic));
      }
      printf("Plugin version:\t\t%s\n", pinfo->pfdinfo.plugin_version);
      printf("Plugin release date:\t%s\n", NPRT(pinfo->pfdinfo.plugin_date));
      printf("Plugin author:\t\t%s\n", NPRT(pinfo->pfdinfo.plugin_author));
      printf("Plugin licence:\t\t%s\n", NPRT(pinfo->pfdinfo.plugin_license));
      printf("Plugin description:\t%s\n", NPRT(pinfo->pfdinfo.plugin_description));
      printf("Plugin API version:\t%d\n", pinfo->pfdinfo.version);
      break;
   case SDPLUGIN:
      printf("\nPlugin type:\t\tBacula Storage plugin\n");
      if (pdata->verbose) {
         printf("Plugin magic:\t\t%s\n", NPRT(pinfo->psdinfo.plugin_magic));
      }
      printf("Plugin version:\t\t%s\n", pinfo->psdinfo.plugin_version);
      printf("Plugin release date:\t%s\n", NPRT(pinfo->psdinfo.plugin_date));
      printf("Plugin author:\t\t%s\n", NPRT(pinfo->psdinfo.plugin_author));
      printf("Plugin licence:\t\t%s\n", NPRT(pinfo->psdinfo.plugin_license));
      printf("Plugin description:\t%s\n", NPRT(pinfo->psdinfo.plugin_description));
      printf("Plugin API version:\t%d\n", pinfo->psdinfo.version);
      break;
   default:
      printf("\nUnknown plugin type or other Error\n\n");
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
void dump_plugfuncs(progdata * pdata)
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
   baculafuncs bfuncs = {
      sizeof(bfuncs),
      1,
      registerBaculaEvents,
      getBaculaValue,
      setBaculaValue,
      JobMessage,
      DebugMessage,
      baculaMalloc,
      baculaFree,
   };
   baculainfos binfos;

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
      printf("\nWhether the file is a really Bacula plugin?\n\n");
      freepdata(pdata);
      exit(2);
   }

   unloadplugfunc = (unloadPlugin) dlsym(pdata->pluginhandle, "unloadPlugin");
   if (unloadplugfunc == NULL) {
      printf("\nCannot find unloadPlugin function: %s\n", dlerror());
      printf("\nWhether the file is a really Bacula plugin?\n\n");
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
