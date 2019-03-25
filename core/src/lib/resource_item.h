/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_RESOURCE_ITEM_H_
#define BAREOS_LIB_RESOURCE_ITEM_H_

struct s_password;
class alist;
class dlist;

/*
 * This is the structure that defines the record types (items) permitted within
 * each resource. It is used to define the configuration tables.
 */
struct ResourceItem {
  const char* name; /* Resource name i.e. Director, ... */
  const int type;
  union {
    char** value; /* Where to store the item */
    std::string* strValue;
    uint16_t* ui16value;
    uint32_t* ui32value;
    int16_t* i16value;
    int32_t* i32value;
    uint64_t* ui64value;
    int64_t* i64value;
    bool* boolvalue;
    utime_t* utimevalue;
    s_password* pwdvalue;
    BareosResource** resvalue;
    alist** alistvalue;
    dlist** dlistvalue;
    char* bitvalue;
  };
  BareosResource* static_resource;
  int32_t code;              /* Item code/additional info */
  uint32_t flags;            /* Flags: See CFG_ITEM_* */
  const char* default_value; /* Default value */
  /*
   * version string in format: [start_version]-[end_version]
   * start_version: directive has been introduced in this version
   * end_version:   directive is deprecated since this version
   */
  const char* versions;
  /*
   * description of the directive, used for the documentation.
   * Full sentence.
   * Every new directive should have a description.
   */
  const char* description;
};

#endif /* BAREOS_LIB_RESOURCE_ITEM_H_ */
