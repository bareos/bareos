/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2025 Bareos GmbH & Co. KG

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

#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif

#include <sstream>

#include "include/fcntl_def.h"
#include "include/bareos.h"
#include "include/filetypes.h"
#include "filed/fd_plugins.h"
#include "plugins/include/common.h"
#include "lib/bool_string.h"
#include "lib/bpipe.h"

namespace filedaemon {

static const int debuglevel = 150;

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Kern Sibbald"
#define PLUGIN_DATE "January 2014"
#define PLUGIN_VERSION "2"
#define PLUGIN_DESCRIPTION "Bareos Pipe File Daemon Plugin"
#define PLUGIN_USAGE                                                   \
  "bpipe:file=<filepath>:reader=<readprogram>:writer=<writeprogram>[:usesuffix=true|false]\n" \
  " readprogram runs on backup and its stdout is saved\n"              \
  " writeprogram runs on restore and gets restored data into stdin\n"  \
  " usesuffix add unique suffix to filepath (optional)\n" \
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

static bRC parse_plugin_definition(PluginContext* ctx, void* value);
static bRC plugin_has_valid_arguments(PluginContext* ctx);
static void set_unique_name(PluginContext* ctx);

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
  bool  usesuffix;     /* append unique suffix to fname */

  char where[512];
  int replace;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bareos can directly call these two entry points
 *  they are common to all Bareos plugins.
 */
// External entry point called by Bareos to "load" the plugin
BAREOS_EXPORT bRC
loadPlugin(PluginApiDefinition* lbareos_plugin_interface_version,
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
BAREOS_EXPORT bRC unloadPlugin() { return bRC_OK; }

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

  if (plugin_has_valid_arguments(ctx) != bRC_OK) { return bRC_Error; }

  p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;
  if (!p_ctx) { return bRC_Error; }

  if(p_ctx->usesuffix) { set_unique_name(ctx); }

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
      {
        int env_job_id;
        bareos_core_functions->getBareosValue(ctx, bVarJobId, &env_job_id);
        char* env_client_name;
        bareos_core_functions->getBareosValue(ctx, bVarFDName,
                                              &env_client_name);
        int env_backup_level;
        bareos_core_functions->getBareosValue(ctx, bVarLevel,
                                              &env_backup_level);
        int env_job_type;
        bareos_core_functions->getBareosValue(ctx, bVarType, &env_job_type);
        int env_since_time;
        bareos_core_functions->getBareosValue(ctx, bVarSinceTime,
                                              &env_since_time);
	int env_replace = env_job_type == 'R' ? p_ctx->replace : ' ';

        std::unordered_map<std::string, std::string> env{
            {"BareosClientName", std::string{env_client_name}},
            {"BareosJobId", std::to_string(env_job_id)},
            {"BareosJobLevel",
             std::string{static_cast<char>(env_backup_level)}},
            {"BareosSinceTime", std::to_string(env_since_time)},
            {"BareosJobType", std::string{static_cast<char>(env_job_type)}},
            {"BareosWhere", std::string{p_ctx->where}},
            {"BareosReplace", std::string{static_cast<char>(env_replace)}},
        };

        if (io->flags & (O_CREAT | O_WRONLY)) {
          if (!p_ctx->writer) {
            // this shouldn't happen as we check for this on plugin creation!
            Jmsg(ctx, M_FATAL, "bpipe-fd: writer command is not set\n");
            return bRC_Error;
          }

          p_ctx->pfd = OpenBpipe(p_ctx->writer, 0, "w", true, env);
          Dmsg(ctx, debuglevel, "bpipe-fd: IO_OPEN fd=%p writer=%s\n",
               p_ctx->pfd, p_ctx->writer);
          if (!p_ctx->pfd) {
            io->io_errno = errno;
            Jmsg(ctx, M_FATAL, "bpipe-fd: Open pipe writer=%s failed: ERR=%s\n",
                 p_ctx->writer, strerror(io->io_errno));
            Dmsg(ctx, debuglevel,
                 "bpipe-fd: Open pipe writer=%s failed: ERR=%s\n",
                 p_ctx->writer, strerror(io->io_errno));
            return bRC_Error;
          }
        } else {
          p_ctx->pfd = OpenBpipe(p_ctx->reader, 0, "r", false, env);
          Dmsg(ctx, debuglevel, "bpipe-fd: IO_OPEN fd=%p reader=%s\n",
               p_ctx->pfd, p_ctx->reader);
          if (!p_ctx->pfd) {
            io->io_errno = errno;
            Jmsg(ctx, M_FATAL, "bpipe-fd: Open pipe reader=%s failed: ERR=%s\n",
                 p_ctx->reader, strerror(io->io_errno));
            Dmsg(ctx, debuglevel,
                 "bpipe-fd: Open pipe reader=%s failed: ERR=%s\n",
                 p_ctx->reader, strerror(io->io_errno));
            return bRC_Error;
          }
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
  if (plugin_has_valid_arguments(ctx) != bRC_OK) { return bRC_Error; }

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

// Always set destination to value and clean any previous one.
static inline void SetString(char** destination, const char* value)
{
  if (*destination) { free(*destination); }

  *destination = strdup(value);
  StripBackSlashes(*destination);
}

static char get_level_letter(int level) {
     switch (level) {
        case L_FULL: return 'F';
        case L_DIFFERENTIAL: return 'D';
        case L_INCREMENTAL: return 'I';
        case L_NONE: return 'N';
        default: return 'E';
      }
}

static void set_unique_name(PluginContext* ctx) {
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  int job_id = 0;
  const char* job_name = "undefined";
  int job_level = L_NONE;

  char new_fname[1024];

  bareos_core_functions->getBareosValue(ctx, bVarJobId, &job_id);
  bareos_core_functions->getBareosValue(ctx, bVarJobName, &job_name);
  bareos_core_functions->getBareosValue(ctx, bVarLevel, &job_level);

  snprintf(new_fname, sizeof(new_fname),
    "%s.%s.%c.%d",
    p_ctx->fname,
    job_name,
    get_level_letter(job_level),
    job_id);
  Dmsg(ctx, 100, "renamed pseudo file '%s' to '%s'\n", p_ctx->fname, new_fname);
  free(p_ctx->fname);
  p_ctx->fname = strdup(new_fname);
}

// duplicated from core/src/plugins/include/python_plugins_common.inc
static inline bool ParseBoolean(const char* argument_value)
{
  if (Bstrcasecmp(argument_value, "yes")
      || Bstrcasecmp(argument_value, "true")) {
    return true;
  } else {
    return false;
  }
}

static bool is_equal(std::string& a, const char *b) {
  return Bstrcasecmp(a.c_str(), b);
}

static bRC parse_single_option(std::string& key, std::string& value, plugin_ctx* p_ctx) {
  const char *c_str = value.c_str();

  if (is_equal(key, "file")) {
    SetString(&p_ctx->fname, c_str);
  } else if (is_equal(key, "reader")) {
    SetString(&p_ctx->reader, c_str);
  } else if (is_equal(key, "writer")) {
    SetString(&p_ctx->writer, c_str);
  } else if (is_equal(key, "usesuffix")) {
    p_ctx->usesuffix = ParseBoolean(c_str);
  }
  else {
    return bRC_Error;
  }

  return bRC_OK;
}

static void log_config_error(PluginContext *ctx, std::string& key, std::string& value) {
  Jmsg(ctx, M_FATAL,
       "bpipe-fd: Illegal argument %s with value %s in plugin "
       "definition\n",
       key.c_str(), value.c_str());
  Dmsg(ctx, debuglevel,
       "bpipe-fd: Illegal argument %s with value %s in plugin "
       "definition\n",
       key.c_str(), value.c_str());
}

/**
 * Parse the plugin definition passed in.
 *
 * The definition is in this form:
 *
 * bpipe:file=<filepath>:read=<readprogram>:write=<writeprogram>
 */
static bRC parse_plugin_definition(PluginContext* ctx, void* plugindef)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx || !plugindef) { return bRC_Error; }

  std::istringstream input((char *) plugindef);

  std::string part;

  // skip first token which is always the plugin name
  std::getline(input, part, ':');

  for (; std::getline(input, part, ':');) {
    std::istringstream partin(part);
    std::string key, value;

    std::getline(partin, key, '=');
    std::getline(partin, value);

    if (key.empty()) {
        log_config_error(ctx, key, value);
        return bRC_Error;
    }

    if (parse_single_option(key, value, p_ctx) != bRC_OK) {
        log_config_error(ctx, key, value);
        return bRC_Error;
    }
  }

  return bRC_OK;
}

static bRC plugin_has_valid_arguments(PluginContext* ctx)
{
  bRC retval = bRC_OK;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx) { return bRC_Error; }

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

  if (!PathContainsDirectory(p_ctx->fname)) {
    Jmsg(ctx, M_FATAL,
      "bpipe-fd: file argument (%s) must contain a directory "
      "structure. Please fix your plugin definition\n",
      p_ctx->fname);
    Dmsg(ctx, debuglevel,
      "bpipe-fd: file argument (%s) must contain a directory "
      "structure. Please fix your plugin definition\n",
      p_ctx->fname);
    retval = bRC_Error;
  }

  return retval;
}
} /* namespace filedaemon */
