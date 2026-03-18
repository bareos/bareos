/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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

#include "lib/parse_conf.h"
#include "lib/resource_item.h"
#include "lib/edit.h"
#include "lib/util.h"
#include "lib/address_conf.h"
#include "lib/alist.h"

static void MakePathName(PoolMem& pathname, const char* str)
{
  if (str[0] != '|') {
    auto* expanded = DoShellExpansion(str);
    PmStrcpy(pathname, expanded);
    free(expanded);
  } else {
    PmStrcpy(pathname, str);
  }
}

void ConfigurationParser::SetResourceDefaultsParserPass1(
    const ResourceItem* item)
{
  Dmsg3(900, "Item=%s defval=%s\n", item->name,
        (item->default_value) ? item->default_value : "<None>");

  if (item->default_value) {
    switch (item->type) {
      case CFG_TYPE_BIT:
        if (Bstrcasecmp(item->default_value, "on")) {
          char* bitfield = GetItemVariablePointer<char*>(*item);
          SetBit(item->code, bitfield);
        } else if (Bstrcasecmp(item->default_value, "off")) {
          char* bitfield = GetItemVariablePointer<char*>(*item);
          ClearBit(item->code, bitfield);
        }
        break;
      case CFG_TYPE_BOOL:
        if (Bstrcasecmp(item->default_value, "yes")
            || Bstrcasecmp(item->default_value, "true")) {
          SetItemVariable<bool>(*item, true);
        } else if (Bstrcasecmp(item->default_value, "no")
                   || Bstrcasecmp(item->default_value, "false")) {
          SetItemVariable<bool>(*item, false);
        }
        break;
      case CFG_TYPE_PINT32:
      case CFG_TYPE_INT32:
      case CFG_TYPE_SIZE32:
        SetItemVariable<uint32_t>(*item, str_to_uint64(item->default_value));
        break;
      case CFG_TYPE_INT64:
        SetItemVariable<uint64_t>(*item, str_to_int64(item->default_value));
        break;
      case CFG_TYPE_SIZE64:
        SetItemVariable<uint64_t>(*item, str_to_uint64(item->default_value));
        break;
      case CFG_TYPE_SPEED:
        SetItemVariable<uint64_t>(*item, str_to_uint64(item->default_value));
        break;
      case CFG_TYPE_TIME: {
        SetItemVariable<utime_t>(*item, str_to_int64(item->default_value));
        break;
      }
      case CFG_TYPE_STRNAME:
      case CFG_TYPE_STR:
        SetItemVariable<char*>(*item, strdup(item->default_value));
        break;
      case CFG_TYPE_STDSTR:
        SetItemVariable<std::string>(*item, item->default_value);
        break;
      case CFG_TYPE_DIR: {
        PoolMem pathname(PM_FNAME);
        MakePathName(pathname, item->default_value);
        SetItemVariable<char*>(*item, strdup(pathname.c_str()));
        break;
      }
      case CFG_TYPE_STDSTRDIR: {
        PoolMem pathname(PM_FNAME);
        MakePathName(pathname, item->default_value);
        SetItemVariable<std::string>(*item, std::string(pathname.c_str()));
        break;
      }
      case CFG_TYPE_ADDRESSES: {
        dlist<IPADDR>** dlistvalue
            = GetItemVariablePointer<dlist<IPADDR>**>(*item);
        InitDefaultAddresses(dlistvalue, item->default_value);
        break;
      }
      default:
        if (init_res_) { init_res_(item, 1); }
        break;
    }
  }
}

void ConfigurationParser::SetResourceDefaultsParserPass2(
    const ResourceItem* item)
{
  Dmsg3(900, "Item=%s defval=%s\n", item->name,
        (item->default_value) ? item->default_value : "<None>");

  if (item->default_value) {
    switch (item->type) {
      case CFG_TYPE_ALIST_STR: {
        alist<const char*>** alistvalue
            = GetItemVariablePointer<alist<const char*>**>(*item);
        if (!alistvalue) {
          *(alistvalue) = new alist<const char*>(10, owned_by_alist);
        }
        (*alistvalue)->append(strdup(item->default_value));
        break;
      }
      case CFG_TYPE_ALIST_DIR: {
        alist<const char*>** alistvalue
            = GetItemVariablePointer<alist<const char*>**>(*item);

        if (!*alistvalue) {
          *alistvalue = new alist<const char*>(10, owned_by_alist);
        }

        if (item->default_value[0] != '|') {
          auto* expanded = DoShellExpansion(item->default_value);
          (*alistvalue)->append(expanded);
        } else {
          (*alistvalue)->append(strdup(item->default_value));
        }
        break;
      }
      case CFG_TYPE_STR_VECTOR_OF_DIRS: {
        std::vector<std::string>* list
            = GetItemVariablePointer<std::vector<std::string>*>(*item);

        if (item->default_value[0] != '|') {
          auto* expanded = DoShellExpansion(item->default_value);
          list->emplace_back(expanded);
          free(expanded);
        } else {
          list->emplace_back(item->default_value);
        }
        break;
      }
      default:
        if (init_res_) { init_res_(item, 2); }
        break;
    }
  }
}

void ConfigurationParser::SetAllResourceDefaultsIterateOverItems(
    int rcode,
    const ResourceItem items[],
    std::function<void(ConfigurationParser&, const ResourceItem*)> SetDefaults)
{
  int res_item_index = 0;

  while (items[res_item_index].name) {
    SetDefaults(*this, &items[res_item_index]);

    if (!omit_defaults_) {
      SetBit(res_item_index,
             (*items[res_item_index].allocated_resource)->inherit_content_);
    }

    (*items[res_item_index].allocated_resource)->rcode_ = rcode;

    res_item_index++;

    if (res_item_index >= MAX_RES_ITEMS) {
      Emsg1(M_ERROR_TERM, 0, T_("Too many items in %s resource\n"),
            resource_definitions_[rcode].name);
    }
  }
}

void ConfigurationParser::SetAllResourceDefaultsByParserPass(
    int rcode,
    const ResourceItem items[],
    int pass)
{
  std::function<void(ConfigurationParser&, const ResourceItem*)> SetDefaults;

  switch (pass) {
    case 1:
      SetDefaults = [rcode](ConfigurationParser& c, const ResourceItem* item) {
        (*item->allocated_resource)->rcode_ = rcode;
        (*item->allocated_resource)->refcnt_ = 1;
        c.SetResourceDefaultsParserPass1(item);
      };
      break;
    case 2:
      SetDefaults = &ConfigurationParser::SetResourceDefaultsParserPass2;
      break;
    default:
      ASSERT(false);
      break;
  }

  SetAllResourceDefaultsIterateOverItems(rcode, items, SetDefaults);
}

void ConfigurationParser::InitResource(
    int rcode,
    const ResourceItem items[],
    int pass,
    std::function<void()> ResourceSpecificInitializer)
{
  if (ResourceSpecificInitializer) { ResourceSpecificInitializer(); }

  SetAllResourceDefaultsByParserPass(rcode, items, pass);
}
