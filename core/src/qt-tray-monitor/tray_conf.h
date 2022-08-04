/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.
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
/*
 * Tray Monitor specific configuration and defines
 *
 * Adapted from dird_conf.c
 *
 * Nicolas Boichat, August MMIV
 */

/* NOTE:  #includes at the end of this file */

// Resource codes -- they must be sequential for indexing

#ifndef BAREOS_QT_TRAY_MONITOR_TRAY_CONF_H_
#define BAREOS_QT_TRAY_MONITOR_TRAY_CONF_H_

#include "lib/tls_conf.h"

extern ConfigurationParser* my_config;

enum Rescode
{
  R_MONITOR = 0,
  R_DIRECTOR,
  R_CLIENT,
  R_STORAGE,
  R_CONSOLE,
  R_CONSOLE_FONT,
  R_NUM, /* keep this updated */
  R_UNKNOWN = 0xff
};

// Some resource attributes
enum
{
  R_NAME = 0,
  R_ADDRESS,
  R_PASSWORD,
  R_TYPE,
  R_BACKUP
};

// Director Resource
class DirectorResource
    : public BareosResource
    , public TlsResource {
 public:
  DirectorResource() = default;
  virtual ~DirectorResource() = default;

  uint32_t DIRport = 0;    /* UA server port */
  char* address = nullptr; /* UA server address */
};

// Tray Monitor Resource
class MonitorResource
    : public BareosResource
    , public TlsResource {
 public:
  MonitorResource() = default;
  virtual ~MonitorResource() = default;

  MessagesResource* messages = nullptr; /* Daemon message handler */
  s_password password;                  /* UA server password */
  utime_t RefreshInterval = {0};        /* Status refresh interval */
  utime_t FDConnectTimeout = {0};       /* timeout for connect in seconds */
  utime_t SDConnectTimeout = {0};       /* timeout in seconds */
  utime_t DIRConnectTimeout = {0};      /* timeout in seconds */
};

// Client Resource
class ClientResource
    : public BareosResource
    , public TlsResource {
 public:
  ClientResource() = default;
  virtual ~ClientResource() = default;

  uint32_t FDport = 0; /* Where File daemon listens */
  char* address = nullptr;
  s_password password;
};

// Store Resource
class StorageResource
    : public BareosResource
    , public TlsResource {
 public:
  StorageResource() = default;
  virtual ~StorageResource() = default;

  uint32_t SDport = 0; /* port where Directors connect */
  char* address = nullptr;
  s_password password;
};

class ConsoleFontResource
    : public BareosResource
    , public TlsResource {
 public:
  ConsoleFontResource() = default;
  virtual ~ConsoleFontResource() = default;

  char* fontface = nullptr; /* Console Font specification */
};

ConfigurationParser* InitTmonConfig(const char* configfile, int exit_code);
bool PrintConfigSchemaJson(PoolMem& buffer);

#endif  // BAREOS_QT_TRAY_MONITOR_TRAY_CONF_H_
