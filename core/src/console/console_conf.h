/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, Sep MM
 */
/**
 * @file
 * Bareos User Agent specific configuration and defines
 */

#define CONFIG_FILE "bconsole.conf"   /* default configuration file */

/**
 * Resource codes -- they must be sequential for indexing
 */

enum {
   R_CONSOLE = 1001,
   R_DIRECTOR,
   R_FIRST = R_CONSOLE,
   R_LAST = R_DIRECTOR                /* Keep this updated */
};

/**
 * Some resource attributes
 */
enum {
   R_NAME = 1020,
   R_ADDRESS,
   R_PASSWORD,
   R_TYPE,
   R_BACKUP
};

/* Definition of the contents of each Resource */

/* Console "globals" */
class ConsoleResource : public TlsResource {
   public:
      char *rc_file;                     /**< startup file */
      char *history_file;                /**< command history file */
      s_password password;               /**< UA server password */
      uint32_t history_length;           /**< readline history length */
      char *director;                    /**< bind to director */
      utime_t heartbeat_interval;        /**< Interval to send heartbeats to Dir */
      ConsoleResource() : TlsResource() {}
};

/* Director */
class DirectorResource : public TlsResource {
   public:
      uint32_t DIRport;                  /**< UA server port */
      char *address;                     /**< UA server address */
      utime_t heartbeat_interval;        /**< Interval to send heartbeats to Dir */
      DirectorResource() : TlsResource() {}
};

/* Define the Union of all the above
 * resource structure definitions.
 */
union UnionOfResources {
   DirectorResource res_dir;
   ConsoleResource res_cons;
   CommonResourceHeader hdr;

   UnionOfResources() {new(&hdr) CommonResourceHeader();}
   ~UnionOfResources() {}
};

extern ConsoleResource *me;                    /* "Global" Client resource */
extern ConfigurationParser *my_config;             /* Our Global config */

void InitConsConfig(ConfigurationParser *config, const char *configfile, int exit_code);
bool PrintConfigSchemaJson(PoolMem &buffer);
