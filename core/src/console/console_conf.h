/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
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
// Kern Sibbald, Sep MM
/**
 * @file
 * Bareos User Agent specific configuration and defines
 */

#ifndef BAREOS_CONSOLE_CONSOLE_CONF_H_
#define BAREOS_CONSOLE_CONSOLE_CONF_H_

#include <string>
#include "lib/parse_conf.h"

class ConfigurationParser;

namespace console {

static const std::string default_config_filename("bconsole.conf");

// Resource codes -- they must be sequential for indexing

enum
{
  R_CONSOLE = 0,
  R_DIRECTOR,
  R_NUM  /* number of entries */
};

enum
{
  R_NAME = 0,
  R_ADDRESS,
  R_PASSWORD,
  R_TYPE,
  R_BACKUP
};

class ConsoleResource
    : public BareosResource
    , public TlsResource {
 public:
  ConsoleResource() = default;
  virtual ~ConsoleResource() = default;

  char* rc_file = nullptr;       /**< startup file */
  char* history_file = nullptr;  /**< command history file */
  uint32_t history_length = 0;   /**< readline history length */
  char* director = nullptr;      /**< bind to director */
  utime_t heartbeat_interval{0}; /**< Interval to send heartbeats to Dir */
};

class DirectorResource
    : public BareosResource
    , public TlsResource {
 public:
  DirectorResource() = default;
  virtual ~DirectorResource() = default;

  uint32_t DIRport = 0;          /**< UA server port */
  char* address = nullptr;       /**< UA server address */
  utime_t heartbeat_interval{0}; /**< Interval to send heartbeats to Dir */
};

ConfigurationParser* InitConsConfig(const char* configfile, int exit_code);
bool PrintConfigSchemaJson(PoolMem& buffer);

} /* namespace console */
#endif  // BAREOS_CONSOLE_CONSOLE_CONF_H_
