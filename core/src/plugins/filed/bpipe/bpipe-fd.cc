/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2023 Bareos GmbH & Co. KG

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
// Kern Sibbald, October 2007
/**
 * @file
 * A simple pipe plugin for the Bareos File Daemon
 */

#include <unistd.h>

#include "include/fcntl_def.h"
#include "include/bareos.h"
#include "include/filetypes.h"
#include "filed/fd_plugins.h"
#include "plugins/include/common.h"
#include "lib/bpipe.h"

namespace filedaemon {

static const int debuglevel = 150;

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Kern Sibbald"
#define PLUGIN_DATE "January 2014"
#define PLUGIN_VERSION "2"
#define PLUGIN_DESCRIPTION "Bareos Pipe File Daemon Plugin"
#define PLUGIN_USAGE                                                   \
  "bpipe:file=<filepath>:reader=<readprogram>:writer=<writeprogram>\n" \
  " readprogram runs on backup and its stdout is saved\n"              \
  " writeprogram runs on restore and gets restored data into stdin\n"  \
  " the data is internally stored as filepath (e.g. mybackup/backup1)"

/* Forward referenced functions */
static bRC newPlugin(PluginContext* ctx);
static bRC freePlugin(PluginContext* ctx);
static bRC getPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC setPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC handlePluginEvent(PluginContext* ctx, bEvent* event, void* value);
static bRC startBackupFile(PluginContext* ctx, save_pkt* sp);
static bRC endBackupFile(PluginContext* ctx);
static bRC pluginIO(PluginContext* ctx, io_pkt* io);
static bRC startRestoreFile(PluginContext* ctx, const char* cmd);
static bRC endRestoreFile(PluginContext* ctx);
static bRC createFile(PluginContext* ctx, restore_pkt* rp);
static bRC setFileAttributes(PluginContext* ctx, restore_pkt* rp);
static bRC checkFile(PluginContext* ctx, char* fname);
static bRC getAcl(PluginContext* ctx, acl_pkt* ap);
static bRC setAcl(PluginContext* ctx, acl_pkt* ap);
static bRC getXattr(PluginContext* ctx, xattr_pkt* xp);
static bRC setXattr(PluginContext* ctx, xattr_pkt* xp);

static char* apply_rp_codes(PluginContext* ctx);
static bRC parse_plugin_definition(PluginContext* ctx, void* value);
static bRC plugin_has_all_arguments(PluginContext* ctx);

/* Pointers to Bareos functions */
static CoreFunctions* bareos_core_functions = NULL;
static PluginApiDefinition* bareos_plugin_interface_version = NULL;

/* Plugin Information block */
static PluginInformation pluginInfo
    = {sizeof(pluginInfo), FD_PLUGIN_INTERFACE_VERSION,
       FD_PLUGIN_MAGIC,    PLUGIN_LICENSE,
       PLUGIN_AUTHOR,      PLUGIN_DATE,
       PLUGIN_VERSION,     PLUGIN_DESCRIPTION,
       PLUGIN_USAGE};

/* Plugin entry points for Bareos */
static PluginFunctions pluginFuncs
    = {sizeof(pluginFuncs), FD_PLUGIN_INTERFACE_VERSION,

       /* Entry points into plugin */
       newPlugin,  /* new plugin instance */
       freePlugin, /* free plugin instance */
       getPluginValue, setPluginValue, handlePluginEvent, startBackupFile,
       endBackupFile, startRestoreFile, endRestoreFile, pluginIO, createFile,
       setFileAttributes, checkFile, getAcl, setAcl, getXattr, setXattr};

// Plugin private context
struct plugin_ctx {
  boffset_t offset;
  Bpipe* pfd;           /* bpipe() descriptor */
  char* plugin_options; /* Override of plugin options passed in */
  char* fname;          /* Filename to "backup/restore" */
  char* reader;         /* Reader program for backup */
  char* writer;         /* Writer program for backup */

  char where[512];
  int replace;
};

// This defines the arguments that the plugin parser understands.
enum plugin_argument_type
{
  argument_none = 0,
  argument_file,
  argument_reader,
  argument_writer
};

struct plugin_argument {
  const char* name;
  enum plugin_argument_type type;
  int cmp_length;
};

static plugin_argument plugin_arguments[] = {{"file=", argument_file, 4},
                                             {"reader=", argument_reader, 6},
                                             {"writer=", argument_writer, 6},
                                             {NULL, argument_none, 0}};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bareos can directly call these two entry points
 *  they are common to all Bareos plugins.
 */
// External entry point called by Bareos to "load" the plugin
bRC loadPlugin(PluginApiDefinition* lbareos_plugin_interface_version,
               CoreFunctions* lbareos_core_functions,
               PluginInformation** plugin_information,
               PluginFunctions** plugin_functions)
{
  bareos_core_functions
      = lbareos_core_functions; /* set Bareos funct pointers */
  bareos_plugin_interface_version = lbareos_plugin_interface_version;
  *plugin_information = &pluginInfo; /* return pointer to our info */
  *plugin_functions = &pluginFuncs;  /* return pointer to our functions */

  return bRC_OK;
}

// External entry point to unload the plugin
bRC unloadPlugin() { return bRC_OK; }

#ifdef __cplusplus
}
#endif

/**
 * The following entry points are accessed through the function
 *   pointers we supplied to Bareos. Each plugin type (dir, fd, sd)
 *   has its own set of entry points that the plugin must define.
 */
// Create a new instance of the plugin i.e. allocate our private storage
static bRC newPlugin(PluginContext* ctx)
{
  struct plugin_ctx* p_ctx
      = (struct plugin_ctx*)malloc(sizeof(struct plugin_ctx));
  if (!p_ctx) { return bRC_Error; }
  memset(p_ctx, 0, sizeof(struct plugin_ctx));
  ctx->plugin_private_context = (void*)p_ctx; /* set our context pointer */

  bareos_core_functions->registerBareosEvents(
      ctx, 6, bEventNewPluginOptions, bEventPluginCommand, bEventJobStart,
      bEventRestoreCommand, bEventEstimateCommand, bEventBackupCommand);

  return bRC_OK;
}

// Free a plugin instance, i.e. release our private storage
static bRC freePlugin(PluginContext* ctx)
{
  struct plugin_ctx* p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx) { return bRC_Error; }

  if (p_ctx->fname) { free(p_ctx->fname); }

  if (p_ctx->reader) { free(p_ctx->reader); }

  if (p_ctx->writer) { free(p_ctx->writer); }

  if (p_ctx->plugin_options) { free(p_ctx->plugin_options); }

  free(p_ctx); /* free our private context */
  p_ctx = NULL;
  return bRC_OK;
}

// Return some plugin value (none defined)
static bRC getPluginValue(PluginContext*, pVariable, void*) { return bRC_OK; }

// Set a plugin value (none defined)
static bRC setPluginValue(PluginContext*, pVariable, void*) { return bRC_OK; }

// Handle an event that was generated in Bareos
static bRC handlePluginEvent(PluginContext* ctx, bEvent* event, void* value)
{
  bRC retval = bRC_OK;
  struct plugin_ctx* p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx) { return bRC_Error; }

  switch (event->eventType) {
    case bEventJobStart:
      Dmsg(ctx, debuglevel, "bpipe-fd: JobStart=%s\n", (char*)value);
      break;
    case bEventRestoreCommand:
      // Fall-through wanted
    case bEventBackupCommand:
      // Fall-through wanted
    case bEventEstimateCommand:
      // Fall-through wanted
    case bEventPluginCommand:
      retval = parse_plugin_definition(ctx, value);
      break;
    case bEventNewPluginOptions:
      // Free any previous value.
      if (p_ctx->plugin_options) {
        free(p_ctx->plugin_options);
        p_ctx->plugin_options = NULL;
      }

      retval = parse_plugin_definition(ctx, value);

      // Save that we got a plugin override.
      p_ctx->plugin_options = strdup((char*)value);
      break;
    default:
      Jmsg(ctx, M_FATAL, "bpipe-fd: unknown event=%d\n", event->eventType);
      Dmsg(ctx, debuglevel, "bpipe-fd: unknown event=%d\n", event->eventType);
      retval = bRC_Error;
      break;
  }

  return retval;
}

// Start the backup of a specific file
static bRC startBackupFile(PluginContext* ctx, save_pkt* sp)
{
  time_t now;
  struct plugin_ctx* p_ctx;

  if (plugin_has_all_arguments(ctx) != bRC_OK) { return bRC_Error; }

  p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;
  if (!p_ctx) { return bRC_Error; }

  now = time(NULL);
  sp->fname = p_ctx->fname;
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

// Done with backup of this file
static bRC endBackupFile(PluginContext*)
{
  /* We would return bRC_More if we wanted startBackupFile to be called again to
   * backup another file */
  return bRC_OK;
}

// Bareos is calling us to do the actual I/O
static bRC pluginIO(PluginContext* ctx, io_pkt* io)
{
  struct plugin_ctx* p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;
  if (!p_ctx) { return bRC_Error; }

  io->status = 0;
  io->io_errno = 0;
  switch (io->func) {
    case IO_OPEN:
      Dmsg(ctx, debuglevel, "bpipe-fd: IO_OPEN\n");
      if (io->flags & (O_CREAT | O_WRONLY)) {
        char* writer_codes = apply_rp_codes(ctx);

        p_ctx->pfd = OpenBpipe(writer_codes, 0, "w");
        Dmsg(ctx, debuglevel, "bpipe-fd: IO_OPEN fd=%p writer=%s\n", p_ctx->pfd,
             writer_codes);
        if (!p_ctx->pfd) {
          io->io_errno = errno;
          Jmsg(ctx, M_FATAL, "bpipe-fd: Open pipe writer=%s failed: ERR=%s\n",
               writer_codes, strerror(io->io_errno));
          Dmsg(ctx, debuglevel,
               "bpipe-fd: Open pipe writer=%s failed: ERR=%s\n", writer_codes,
               strerror(io->io_errno));
          if (writer_codes) { free(writer_codes); }
          return bRC_Error;
        }
        if (writer_codes) { free(writer_codes); }
      } else {
        p_ctx->pfd = OpenBpipe(p_ctx->reader, 0, "r", false);
        Dmsg(ctx, debuglevel, "bpipe-fd: IO_OPEN fd=%p reader=%s\n", p_ctx->pfd,
             p_ctx->reader);
        if (!p_ctx->pfd) {
          io->io_errno = errno;
          Jmsg(ctx, M_FATAL, "bpipe-fd: Open pipe reader=%s failed: ERR=%s\n",
               p_ctx->reader, strerror(io->io_errno));
          Dmsg(ctx, debuglevel,
               "bpipe-fd: Open pipe reader=%s failed: ERR=%s\n", p_ctx->reader,
               strerror(io->io_errno));
          return bRC_Error;
        }
      }
      sleep(1); /* let pipe connect */
      break;
    case IO_READ:
      if (!p_ctx->pfd) {
        Jmsg(ctx, M_FATAL, "bpipe-fd: Logic error: NULL read FD\n");
        Dmsg(ctx, debuglevel, "bpipe-fd: Logic error: NULL read FD\n");
        return bRC_Error;
      }
      io->status = fread(io->buf, 1, io->count, p_ctx->pfd->rfd);
      if (io->status == 0 && ferror(p_ctx->pfd->rfd)) {
        io->io_errno = errno;
        Jmsg(ctx, M_FATAL, "bpipe-fd: Pipe read error: ERR=%s\n",
             strerror(io->io_errno));
        Dmsg(ctx, debuglevel, "bpipe-fd: Pipe read error: ERR=%s\n",
             strerror(io->io_errno));
        return bRC_Error;
      }
      break;
    case IO_WRITE:
      if (!p_ctx->pfd) {
        Jmsg(ctx, M_FATAL, "bpipe-fd: Logic error: NULL write FD\n");
        Dmsg(ctx, debuglevel, "bpipe-fd: Logic error: NULL write FD\n");
        return bRC_Error;
      }
      io->status = fwrite(io->buf, 1, io->count, p_ctx->pfd->wfd);
      if (io->status == 0 && ferror(p_ctx->pfd->wfd)) {
        io->io_errno = errno;
        Jmsg(ctx, M_FATAL, "bpipe-fd: Pipe write error: ERR=%s\n",
             strerror(io->io_errno));
        Dmsg(ctx, debuglevel, "bpipe-fd: Pipe write error: ERR=%s\n",
             strerror(io->io_errno));
        return bRC_Error;
      }
      break;
    case IO_CLOSE:
      if (!p_ctx->pfd) {
        Jmsg(ctx, M_FATAL, "bpipe-fd: Logic error: NULL FD on bpipe close\n");
        Dmsg(ctx, debuglevel,
             "bpipe-fd: Logic error: NULL FD on bpipe close\n");
        return bRC_Error;
      }
      io->status = CloseBpipe(p_ctx->pfd);
      if (io->status) {
        Jmsg(ctx, M_FATAL,
             "bpipe-fd: Error closing stream for pseudo file %s: %d\n",
             p_ctx->fname, io->status);
        Dmsg(ctx, debuglevel,
             "bpipe-fd: Error closing stream for pseudo file %s: %d\n",
             p_ctx->fname, io->status);
      }
      break;
    case IO_SEEK:
      io->offset = p_ctx->offset;
      break;
  }

  return bRC_OK;
}

/**
 * Bareos is notifying us that a plugin name string was found, and
 *   passing us the plugin command, so we can prepare for a restore.
 */
static bRC startRestoreFile(PluginContext* ctx, const char*)
{
  if (plugin_has_all_arguments(ctx) != bRC_OK) { return bRC_Error; }

  return bRC_OK;
}

/**
 * Bareos is notifying us that the plugin data has terminated, so
 *  the restore for this particular file is done.
 */
static bRC endRestoreFile(PluginContext* ctx)
{
  struct plugin_ctx* p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;
  if (!p_ctx) { return bRC_Error; }

  return bRC_OK;
}

/**
 * This is called during restore to create the file (if necessary)
 * We must return in rp->create_status:
 *
 * CF_ERROR    -- error
 * CF_SKIP     -- skip processing this file
 * CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 * CF_CREATED  -- created, but no content to extract (typically directories)
 */
static bRC createFile(PluginContext* ctx, restore_pkt* rp)
{
  if (strlen(rp->where) > 512) {
    printf(
        "bpipe-fd: Restore target dir too long. Restricting to first 512 "
        "bytes.\n");
  }

  bstrncpy(((struct plugin_ctx*)ctx->plugin_private_context)->where, rp->where,
           513);
  ((struct plugin_ctx*)ctx->plugin_private_context)->replace = rp->replace;

  rp->create_status = CF_EXTRACT;
  return bRC_OK;
}

static bRC setFileAttributes(PluginContext*, restore_pkt*) { return bRC_OK; }

// When using Incremental dump, all previous dumps are necessary
static bRC checkFile(PluginContext*, char*) { return bRC_OK; }

static bRC getAcl(PluginContext*, acl_pkt*) { return bRC_OK; }

static bRC setAcl(PluginContext*, acl_pkt*) { return bRC_OK; }

static bRC getXattr(PluginContext*, xattr_pkt*) { return bRC_OK; }

static bRC setXattr(PluginContext*, xattr_pkt*) { return bRC_OK; }

/**
 * Apply codes in writer command:
 * %w -> "where"
 * %r -> "replace"
 *
 * Replace:
 * 'always' => 'a', chr(97)
 * 'ifnewer' => 'w', chr(119)
 * 'ifolder' => 'o', chr(111)
 * 'never' => 'n', chr(110)
 *
 * This function will allocate the required amount of memory with malloc.
 * Need to be free()d manually.
 *
 * Inspired by edit_job_codes in lib/util.c
 */
static char* apply_rp_codes(PluginContext* ctx)
{
  char add[10];
  const char* str;
  char *p, *q, *omsg, *imsg;
  int w_count = 0, r_count = 0;
  struct plugin_ctx* p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx) { return NULL; }

  imsg = p_ctx->writer;
  if (!imsg) { return NULL; }

  if ((p = imsg)) {
    while ((q = strstr(p, "%w"))) {
      w_count++;
      p = q + 1;
    }

    p = imsg;
    while ((q = strstr(p, "%r"))) {
      r_count++;
      p = q + 1;
    }
  }

  /* Required mem:
   * len(imsg)
   * + number of "where" codes * (len(where)-2)
   * - number of "replace" codes */
  omsg = (char*)malloc(strlen(imsg) + (w_count * (strlen(p_ctx->where) - 2))
                       - r_count + 1);
  if (!omsg) {
    Jmsg(ctx, M_FATAL, "bpipe-fd: Out of memory.");
    return NULL;
  }

  *omsg = 0;
  for (p = imsg; *p; p++) {
    if (*p == '%') {
      switch (*++p) {
        case '%':
          str = "%";
          break;
        case 'w':
          str = p_ctx->where;
          break;
        case 'r':
          snprintf(add, 2, "%c", p_ctx->replace);
          str = add;
          break;
        default:
          add[0] = '%';
          add[1] = *p;
          add[2] = 0;
          str = add;
          break;
      }
    } else {
      add[0] = *p;
      add[1] = 0;
      str = add;
    }
    strcat(omsg, str);
  }
  return omsg;
}

// Strip any backslashes in the string.
static inline void StripBackSlashes(char* value)
{
  char* bp;

  bp = value;
  while (*bp) {
    switch (*bp) {
      case '\\':
        bstrinlinecpy(bp, bp + 1);
        break;
      default:
        break;
    }

    bp++;
  }
}

// Only set destination to value when it has no previous setting.
static inline void SetStringIfNull(char** destination, char* value)
{
  if (!*destination) {
    *destination = strdup(value);
    StripBackSlashes(*destination);
  }
}

// Always set destination to value and clean any previous one.
static inline void SetString(char** destination, char* value)
{
  if (*destination) { free(*destination); }

  *destination = strdup(value);
  StripBackSlashes(*destination);
}

/**
 * Parse the plugin definition passed in.
 *
 * The definition is in this form:
 *
 * bpipe:file=<filepath>:read=<readprogram>:write=<writeprogram>
 */
static bRC parse_plugin_definition(PluginContext* ctx, void* value)
{
  int i, cnt;
  char *plugin_definition, *bp, *argument, *argument_value;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;
  bool keep_existing;
  bool compatible = true;

  if (!p_ctx || !value) { return bRC_Error; }

  keep_existing = (p_ctx->plugin_options) ? true : false;

  /* Parse the plugin definition.
   * Make a private copy of the whole string. */
  plugin_definition = strdup((char*)value);

  bp = strchr(plugin_definition, ':');
  if (!bp) {
    Jmsg(ctx, M_FATAL, "bpipe-fd: Illegal plugin definition %s\n",
         plugin_definition);
    Dmsg(ctx, debuglevel, "bpipe-fd: Illegal plugin definition %s\n",
         plugin_definition);
    goto bail_out;
  }

  // Skip the first ':'
  bp++;

  // See if we are parsing a new plugin definition e.g. one with keywords.
  argument = bp;
  while (argument) {
    if (strlen(argument) == 0) { break; }

    for (i = 0; plugin_arguments[i].name; i++) {
      if (bstrncasecmp(argument, plugin_arguments[i].name,
                       strlen(plugin_arguments[i].name))) {
        compatible = false;
        break;
      }
    }

    if (!plugin_arguments[i].name && !compatible) {
      // Parsing something fishy ? e.g. partly with known keywords.
      Jmsg(ctx, M_FATAL,
           "bpipe-fd: Found mixing of old and new syntax, please fix your "
           "plugin definition (%s)\n",
           plugin_definition);
      Dmsg(ctx, debuglevel,
           "bpipe-fd: Found mixing of old and new syntax, please fix your "
           "plugin definition (%s)\n",
           plugin_definition);
      goto bail_out;
    }

    argument = strchr(argument, ':');
    if (argument) { argument++; }
  }

  /* Start processing the definition, if compatible is left set we are
   * pretending that we are parsing a plugin definition in the old syntax and
   * hope for the best. */
  cnt = 1;
  while (bp) {
    if (strlen(bp) == 0) { break; }

    argument = bp;
    if (compatible) {
      char** str_destination = NULL;

      // See if there are more arguments and setup for the next run.
      do {
        bp = strchr(bp, ':');
        if (bp) {
          if (*(bp - 1) != '\\') {
            *bp++ = '\0';
            break;
          } else {
            bp++;
          }
        }
      } while (bp);

      // See which field this is in the argument string.
      switch (cnt) {
        case 1:
          str_destination = &p_ctx->fname;
          break;
        case 2:
          str_destination = &p_ctx->reader;
          break;
        case 3:
          str_destination = &p_ctx->writer;
          break;
        default:
          break;
      }

      if (str_destination) {
        if (keep_existing) {
          // Keep the first value, ignore any next setting.
          SetStringIfNull(str_destination, argument);
        } else {
          // Overwrite any existing value.
          SetString(str_destination, argument);
        }
      }
    } else {
      /* Each argument is in the form:
       *    <argument> = <argument_value>
       *
       * So we setup the right pointers here, argument to the beginning
       * of the argument, argument_value to the beginning of the argument_value.
       */
      argument_value = strchr(bp, '=');
      *argument_value++ = '\0';

      // See if there are more arguments and setup for the next run.
      bp = argument_value;
      do {
        bp = strchr(bp, ':');
        if (bp) {
          if (*(bp - 1) != '\\') {
            *bp++ = '\0';
            break;
          } else {
            bp++;
          }
        }
      } while (bp);

      for (i = 0; plugin_arguments[i].name; i++) {
        if (bstrncasecmp(argument, plugin_arguments[i].name,
                         plugin_arguments[i].cmp_length)) {
          char** str_destination = NULL;

          switch (plugin_arguments[i].type) {
            case argument_file:
              if (!PathContainsDirectory(argument_value)) {
                Jmsg(ctx, M_FATAL,
                     "bpipe-fd: file argument (%s) must contain a directory "
                     "structure. Please fix your plugin definition\n",
                     argument_value);
                Dmsg(ctx, debuglevel,
                     "bpipe-fd: file argument (%s) must contain a directory "
                     "structure. Please fix your plugin definition\n",
                     argument_value);
                goto bail_out;
              }
              str_destination = &p_ctx->fname;
              break;
            case argument_reader:
              str_destination = &p_ctx->reader;
              break;
            case argument_writer:
              str_destination = &p_ctx->writer;
              break;
            default:
              break;
          }

          if (str_destination) {
            if (keep_existing) {
              // Keep the first value, ignore any next setting.
              SetStringIfNull(str_destination, argument_value);
            } else {
              // Overwrite any existing value.
              SetString(str_destination, argument_value);
            }
          }

          // When we have a match break the loop.
          break;
        }
      }

      // Got an invalid keyword ?
      if (!plugin_arguments[i].name) {
        Jmsg(ctx, M_FATAL,
             "bpipe-fd: Illegal argument %s with value %s in plugin "
             "definition\n",
             argument, argument_value);
        Dmsg(ctx, debuglevel,
             "bpipe-fd: Illegal argument %s with value %s in plugin "
             "definition\n",
             argument, argument_value);
        goto bail_out;
      }
    }
    cnt++;
  }

  free(plugin_definition);
  return bRC_OK;

bail_out:
  free(plugin_definition);
  return bRC_Error;
}

static bRC plugin_has_all_arguments(PluginContext* ctx)
{
  bRC retval = bRC_OK;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx) { retval = bRC_Error; }

  if (!p_ctx->fname) {
    Jmsg(ctx, M_FATAL, T_("bpipe-fd: Plugin File argument not specified.\n"));
    Dmsg(ctx, debuglevel, "bpipe-fd: Plugin File argument not specified.\n");
    retval = bRC_Error;
  }

  if (!p_ctx->reader) {
    Jmsg(ctx, M_FATAL, T_("bpipe-fd: Plugin Reader argument not specified.\n"));
    Dmsg(ctx, debuglevel, "bpipe-fd: Plugin Reader argument not specified.\n");
    retval = bRC_Error;
  }

  if (!p_ctx->writer) {
    Jmsg(ctx, M_FATAL, T_("bpipe-fd: Plugin Writer argument not specified.\n"));
    Dmsg(ctx, debuglevel, "bpipe-fd: Plugin Writer argument not specified.\n");
    retval = bRC_Error;
  }

  return retval;
}
} /* namespace filedaemon */
