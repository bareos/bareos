/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2025 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_PARSE_CONF_CALLBACKS_H_
#define BAREOS_LIB_PARSE_CONF_CALLBACKS_H_

#include "include/compiler_macro.h"

struct ResourceItem;
class BareosResource;
class ConfigurationParser;

using sender = PRINTF_LIKE(2, 3) bool(void* user, const char* fmt, ...);
typedef bool (*SaveResourceCb_t)(int type, const ResourceItem* item, int pass);
typedef void (*DumpResourceCb_t)(int type,
                                 BareosResource* res,
                                 sender* sendmsg,
                                 void* sock,
                                 bool hide_sensitive_data,
                                 bool verbose);

typedef void (*FreeResourceCb_t)(BareosResource* res, int type);
typedef void (*ParseConfigBeforeCb_t)(ConfigurationParser&);
typedef void (*ParseConfigReadyCb_t)(ConfigurationParser&);

#endif  // BAREOS_LIB_PARSE_CONF_CALLBACKS_H_
