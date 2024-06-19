/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2016 Planets Communications B.V.
   Copyright (C) 2014-2024 Bareos GmbH & Co. KG

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
// Kern Sibbald, January MMIV
/**
 * @file
 * User Agent Access Control List (ACL) handling
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"

#include <string>

namespace directordaemon {

// Check if access is permitted to item in acl
bool UaContext::AclAccessOk(int acl, const char* item, bool audit_event)
{
  return AclAccessOk(acl, item, strlen(item), audit_event);
}

/**
 * Check if this is a regular expression.
 * A regexp uses the following chars:
 * ., (, ), [, ], |, ^, $, +, ?, *
 */
constexpr bool is_regex(std::string_view string_to_check)
{
  return string_to_check.npos != string_to_check.find_first_of(".()[]|^$+?*");
}

/**
 * acl_list_value: value of the ACL definition.
 * acl_list_compare_value: value ot compare against. This is identical to
 * acl_list_value or a modified acl_list_value to compare against.
 */
static inline bool CompareAclListValueWithItem(
    int acl,
    const char* acl_list_value,
    const char* acl_list_compare_value,
    const char* item,
    int item_length)
{
  // gives full access
  if (Bstrcasecmp("*all*", acl_list_compare_value)) {
    Dmsg2(1400, "Global ACL found in %d %s\n", acl, acl_list_value);
    return true;
  }

  if (Bstrcasecmp(item, acl_list_compare_value)) {
    // Explicit match.
    Dmsg3(1400, "ACL found %s in %d %s\n", item, acl, acl_list_value);
    return true;
  }

  /* If we didn't get an exact match see if we can use the pattern as a
   * regex. */
  if (is_regex(acl_list_compare_value)) {
    regex_t preg{};
    int rc = regcomp(&preg, acl_list_compare_value, REG_EXTENDED | REG_ICASE);
    if (rc != 0) {
      // Not a valid regular expression so skip it.
      Dmsg1(1400, "Not a valid regex %s, ignoring for regex compare\n",
            acl_list_value);
      return false;
    }

    int nmatch = 1;
    regmatch_t pmatch[1]{};
    if (regexec(&preg, item, nmatch, pmatch, 0) == 0) {
      // Make sure its not a partial match but a full match.
      Dmsg2(1400, "Found match start offset %d end offset %d\n",
            pmatch[0].rm_so, pmatch[0].rm_eo);
      if ((pmatch[0].rm_eo - pmatch[0].rm_so) >= item_length) {
        Dmsg3(1400, "ACL found %s in %d using regex %s\n", item, acl,
              acl_list_value);
        regfree(&preg);
        return true;
      }
    }
    regfree(&preg);
  }
  return false;
}

/**
 * Loop over the items in the alist and verify if they match the given item
 * that access was requested for.
 */
static inline std::optional<bool> FindInAclList(alist<const char*>* list,
                                                int acl,
                                                const char* item,
                                                int item_length)
{
  // See if we have an empty list.
  if (!list || list->empty()) { return std::nullopt; }

  // Search list for item
  foreach_alist (list_value, list) {
    // See if this is a deny acl.
    if (*list_value == '!') {
      if (CompareAclListValueWithItem(acl, list_value, list_value + 1, item,
                                      item_length)) {
        return false;
      }
    } else {
      if (CompareAclListValueWithItem(acl, list_value, list_value, item,
                                      item_length)) {
        return true;
      }
    }
  }

  return std::nullopt;
}

// This version expects the length of the item which we must check.
bool UaContext::AclAccessOk(int acl,
                            const char* item,
                            int item_length,
                            bool audit_event)
{
  std::optional<bool> retval;

  // The resource name contains nasty characters
  switch (acl) {
    case Where_ACL:
    case PluginOptions_ACL:
      break;
    default:
      if (!IsNameValid(item)) {
        Dmsg1(1400, "Access denied for item=%s\n", item);
        goto bail_out;
      }
      break;
  }

  // If no console resource => default console and all is permitted
  if (!user_acl) {
    Dmsg0(1400, "Root user access OK.\n");
    retval = true;
    goto bail_out;
  }

  retval = FindInAclList(user_acl->ACL_lists[acl], acl, item, item_length);

  /* If we didn't find a matching ACL try to use the profiles this console is
   * connected to. */
  if (!retval.has_value()) {
    if (user_acl->profiles && user_acl->profiles->size()) {
      ProfileResource* profile = nullptr;

      foreach_alist (profile, user_acl->profiles) {
        retval = FindInAclList(profile->ACL_lists[acl], acl, item, item_length);

        // If we found a match break the loop.
        if (retval.has_value()) { break; }
      }
    }
  }

bail_out:
  if (audit_event && !retval.value_or(false)) {
    LogAuditEventAclFailure(acl, item);
  }

  return retval.value_or(false);
}

/**
 * This function returns if the authentication has any acl restrictions for a
 * certain acltype.
 */
bool UaContext::AclNoRestrictions(int acl)
{
  const char* list_value;

  // If no console resource => default console and all is permitted
  if (!user_acl) { return true; }

  if (user_acl->ACL_lists[acl]) {
    for (int i = 0; i < user_acl->ACL_lists[acl]->size(); i++) {
      list_value = (char*)user_acl->ACL_lists[acl]->get(i);

      if (*list_value == '!') { return false; }

      if (Bstrcasecmp("*all*", list_value)) { return true; }
    }
  }

  foreach_alist (profile, user_acl->profiles) {
    if (profile) {
      if (profile->ACL_lists[acl]) {
        for (int i = 0; i < profile->ACL_lists[acl]->size(); i++) {
          list_value = (char*)profile->ACL_lists[acl]->get(i);

          if (*list_value == '!') { return false; }

          if (Bstrcasecmp("*all*", list_value)) { return true; }
        } /* for (int i = 0; */
      }   /* if (profile->ACL_lists[acl]) */
    }     /* if (profile) */
  }

  return false;
}

int UaContext::RcodeToAcltype(int rcode)
{
  int acl = -1;

  switch (rcode) {
    case R_CLIENT:
      acl = Client_ACL;
      break;
    case R_JOBDEFS:
    case R_JOB:
      acl = Job_ACL;
      break;
    case R_STORAGE:
      acl = Storage_ACL;
      break;
    case R_CATALOG:
      acl = Catalog_ACL;
      break;
    case R_SCHEDULE:
      acl = Schedule_ACL;
      break;
    case R_FILESET:
      acl = FileSet_ACL;
      break;
    case R_POOL:
      acl = Pool_ACL;
      break;
    default:
      break;
  }

  return acl;
}

// This checks the right ACL if the UA has access to the wanted resource.
bool UaContext::IsResAllowed(BareosResource* res)
{
  int acl;

  acl = RcodeToAcltype(res->rcode_);
  if (acl == -1) {
    /* For all resources for which we don't know an explicit mapping
     * to the right ACL we check if the Command ACL has access to the
     * configure command just as we do for suppressing sensitive data. */
    return AclAccessOk(Command_ACL, "configure", false);
  }

  return AclAccessOk(acl, res->resource_name_, false);
}

/**
 * Try to get a resource and make sure the current ACL allows it to be
 * retrieved.
 */
BareosResource* UaContext::GetResWithName(int rcode,
                                          const char* name,
                                          bool audit_event,
                                          bool lock)
{
  int acl;

  acl = RcodeToAcltype(rcode);
  if (acl == -1) {
    /* For all resources for which we don't know an explicit mapping
     * to the right ACL we check if the Command ACL has access to the
     * configure command just as we do for suppressing sensitive data. */
    if (!AclAccessOk(Command_ACL, "configure", false)) { goto bail_out; }
  } else {
    if (!AclAccessOk(acl, name, audit_event)) { goto bail_out; }
  }

  return my_config->GetResWithName(rcode, name, lock);

bail_out:
  return NULL;
}

PoolResource* UaContext::GetPoolResWithName(const char* name,
                                            bool audit_event,
                                            bool lock)
{
  return (PoolResource*)GetResWithName(R_POOL, name, audit_event, lock);
}

StorageResource* UaContext::GetStoreResWithName(const char* name,
                                                bool audit_event,
                                                bool lock)
{
  return (StorageResource*)GetResWithName(R_STORAGE, name, audit_event, lock);
}

ClientResource* UaContext::GetClientResWithName(const char* name,
                                                bool audit_event,
                                                bool lock)
{
  return (ClientResource*)GetResWithName(R_CLIENT, name, audit_event, lock);
}

JobResource* UaContext::GetJobResWithName(const char* name,
                                          bool audit_event,
                                          bool lock)
{
  return (JobResource*)GetResWithName(R_JOB, name, audit_event, lock);
}

FilesetResource* UaContext::GetFileSetResWithName(const char* name,
                                                  bool audit_event,
                                                  bool lock)
{
  return (FilesetResource*)GetResWithName(R_FILESET, name, audit_event, lock);
}

CatalogResource* UaContext::GetCatalogResWithName(const char* name,
                                                  bool audit_event,
                                                  bool lock)
{
  return (CatalogResource*)GetResWithName(R_CATALOG, name, audit_event, lock);
}

ScheduleResource* UaContext::GetScheduleResWithName(const char* name,
                                                    bool audit_event,
                                                    bool lock)
{
  return (ScheduleResource*)GetResWithName(R_SCHEDULE, name, audit_event, lock);
}

} /* namespace directordaemon */
