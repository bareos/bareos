/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2016 Planets Communications B.V.
   Copyright (C) 2014-2016 Bareos GmbH & Co. KG

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
 * BAREOS Director -- User Agent Access Control List (ACL) handling
 *
 * Kern Sibbald, January MMIV
 */

#include "bareos.h"
#include "dird.h"

/*
 * Check if access is permitted to item in acl
 */
bool UAContext::acl_access_ok(int acl, const char *item, bool audit_event)
{
   return acl_access_ok(acl, item, strlen(item), audit_event);
}

/*
 * Check if this is a regular expresion.
 * A regexp uses the following chars:
 * ., (, ), [, ], |, ^, $, +, ?, *
 */
static inline bool is_regex(const char *regexp)
{
   const char *p;
   bool retval = false;

   p = regexp;
   while (p) {
      switch (*p++) {
      case '.':
      case '(':
      case ')':
      case '[':
      case ']':
      case '|':
      case '^':
      case '$':
      case '+':
      case '?':
      case '*':
         retval = true;
         goto bail_out;
      default:
         break;
      }
   }

bail_out:
   return retval;
}

/*
 * Loop over the items in the alist and verify if they match the given item
 * that access was requested for.
 */
static inline bool find_in_acl_list(alist *list, int acl, const char *item, int len)
{
   int rc;
   regex_t preg;
   int nmatch = 1;
   bool retval = false;
   regmatch_t pmatch[1];
   const char *list_value;

   /*
    * See if we have an empty list.
    */
   if (!list) {
      /*
       * Empty list for Where => empty where accept anything.
       * For any other list, reject everything.
       */
      if (len == 0 && acl == Where_ACL) {
         Dmsg0(1400, "Empty Where_ACL allowing restore anywhere\n");
         retval = true;
      }
      goto bail_out;
   }

   /*
    * Search list for item
    */
   for (int i = 0; i < list->size(); i++) {
      list_value = (char *)list->get(i);

      /*
       * See if this is a deny acl.
       */
      if (*list_value == '!') {
         if (bstrcasecmp(item, list_value + 1)) {
            /*
             * Explicit deny.
             */
            Dmsg3(1400, "Deny ACL found %s in %d %s\n", item, acl, list_value);
            goto bail_out;
         }

         /*
          * If we didn't get an exact match see if we can use the pattern as a regex.
          */
         if (is_regex(list_value + 1)) {
            int match_length;

            match_length = strlen(item);
            rc = regcomp(&preg, list_value + 1, REG_EXTENDED | REG_ICASE);
            if (rc != 0) {
               /*
                * Not a valid regular expression so skip it.
                */
               Dmsg1(1400, "Not a valid regex %s, ignoring for regex compare\n", list_value);
               continue;
            }

            if (regexec(&preg, item, nmatch, pmatch, 0) == 0) {
               /*
                * Make sure its not a partial match but a full match.
                */
               Dmsg2(1400, "Found match start offset %d end offset %d\n", pmatch[0].rm_so, pmatch[0].rm_eo);
               if ((pmatch[0].rm_eo - pmatch[0].rm_so) >= match_length) {
                  Dmsg3(1400, "ACL found %s in %d using regex %s\n", item, acl, list_value);
                  regfree(&preg);
                  goto bail_out;
               }
            }

            regfree(&preg);
         }
      } else {
         /*
          * Special case *all* gives full access
          */
         if (bstrcasecmp("*all*", list_value)) {
            Dmsg2(1400, "Global ACL found in %d %s\n", acl, list_value);
            retval = true;
            goto bail_out;
         }

         if (bstrcasecmp(item, list_value)) {
            Dmsg3(1400, "ACL found %s in %d %s\n", item, acl, list_value);
            retval = true;
            goto bail_out;
         }

         /*
          * If we didn't get an exact match see if we can use the pattern as a regex.
          */
         if (is_regex(list_value)) {
            int match_length;

            match_length = strlen(item);
            rc = regcomp(&preg, list_value, REG_EXTENDED | REG_ICASE);
            if (rc != 0) {
               /*
                * Not a valid regular expression so skip it.
                */
               Dmsg1(1400, "Not a valid regex %s, ignoring for regex compare\n", list_value);
               continue;
            }

            if (regexec(&preg, item, nmatch, pmatch, 0) == 0) {
               /*
                * Make sure its not a partial match but a full match.
                */
               Dmsg2(1400, "Found match start offset %d end offset %d\n", pmatch[0].rm_so, pmatch[0].rm_eo);
               if ((pmatch[0].rm_eo - pmatch[0].rm_so) >= match_length) {
                  Dmsg3(1400, "ACL found %s in %d using regex %s\n", item, acl, list_value);
                  retval = true;
                  regfree(&preg);
                  goto bail_out;
               }
            }

            regfree(&preg);
         }
      }
   }

bail_out:
   return retval;
}

/*
 * This version expects the length of the item which we must check.
 */
bool UAContext::acl_access_ok(int acl, const char *item, int len, bool audit_event)
{
   bool retval = false;

   /*
    * The resource name contains nasty characters
    */
   switch (acl) {
   case Where_ACL:
   case PluginOptions_ACL:
      break;
   default:
      if (!is_name_valid(item)) {
         Dmsg1(1400, "Access denied for item=%s\n", item);
         goto bail_out;
      }
      break;
   }

   /*
    * If no console resource => default console and all is permitted
    */
   if (!cons) {
      Dmsg0(1400, "Root cons access OK.\n");
      retval = true;
      goto bail_out;
   }

   retval = find_in_acl_list(cons->ACL_lists[acl], acl, item, len);

   /*
    * If we didn't find a matching ACL try to use the profiles this console is connected to.
    */
   if (!retval && cons->profiles && cons->profiles->size()) {
      PROFILERES *profile;

      foreach_alist(profile, cons->profiles) {
         retval = find_in_acl_list(profile->ACL_lists[acl], acl, item, len);

         /*
          * If we found a match break the loop.
          */
         if (retval) {
            break;
         }
      }
   }

bail_out:
   if (audit_event && !retval) {
      log_audit_event_acl_failure(acl, item);
   }

   return retval;
}

/*
 * This function returns if the authentication has any acl restrictions for a certain acltype.
 */
bool UAContext::acl_no_restrictions(int acl)
{
   const char *list_value;
   PROFILERES *profile;

   /*
    * If no console resource => default console and all is permitted
    */
   if (!cons) {
      return true;
   }

   if (cons->ACL_lists[acl]) {
      for (int i = 0; i < cons->ACL_lists[acl]->size(); i++) {
         list_value = (char *)cons->ACL_lists[acl]->get(i);

         if (*list_value == '!') {
            return false;
         }

         if (bstrcasecmp("*all*", list_value)) {
            return true;
         }
      }
   }

   foreach_alist(profile, cons->profiles) {
      if (profile->ACL_lists[acl]) {
         for (int i = 0; i < profile->ACL_lists[acl]->size(); i++) {
            list_value = (char *)profile->ACL_lists[acl]->get(i);

            if (*list_value == '!') {
               return false;
            }

            if (bstrcasecmp("*all*", list_value)) {
               return true;
            }
         }
      }
   }

   return false;
}

int UAContext::rcode_to_acltype(int rcode)
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

/*
 * This checks the right ACL if the UA has access to the wanted resource.
 */
bool UAContext::is_res_allowed(RES *res)
{
   int acl;

   acl = rcode_to_acltype(res->rcode);
   if (acl == -1) {
      /*
       * For all resources for which we don't know an explicit mapping
       * to the right ACL we check if the Command ACL has access to the
       * configure command just as we do for suppressing sensitive data.
       */
      return acl_access_ok(Command_ACL, "configure", false);
   }

   return acl_access_ok(acl, res->name, false);
}

/*
 * Try to get a resource and make sure the current ACL allows it to be retrieved.
 */
RES *UAContext::GetResWithName(int rcode, const char *name, bool audit_event, bool lock)
{
   int acl;

   acl = rcode_to_acltype(rcode);
   if (acl == -1) {
      /*
       * For all resources for which we don't know an explicit mapping
       * to the right ACL we check if the Command ACL has access to the
       * configure command just as we do for suppressing sensitive data.
       */
      if (!acl_access_ok(Command_ACL, "configure", false)) {
         goto bail_out;
      }
   } else {
      if (!acl_access_ok(acl, name, audit_event)) {
         goto bail_out;
      }
   }

   return ::GetResWithName(rcode, name, lock);

bail_out:
   return NULL;
}

POOLRES *UAContext::GetPoolResWithName(const char *name, bool audit_event, bool lock)
{
   return (POOLRES *)GetResWithName(R_POOL, name, audit_event, lock);
}

STORERES *UAContext::GetStoreResWithName(const char *name, bool audit_event, bool lock)
{
   return (STORERES *)GetResWithName(R_STORAGE, name, audit_event, lock);
}

STORERES *UAContext::GetStoreResWithId(DBId_t id, bool audit_event, bool lock)
{
   STORAGE_DBR storage_dbr;
   memset(&storage_dbr, 0, sizeof(storage_dbr));

   storage_dbr.StorageId = id;
   if (db->get_storage_record(jcr, &storage_dbr)) {
      return GetStoreResWithName(storage_dbr.Name, audit_event, lock);
   }
   return NULL;
}

CLIENTRES *UAContext::GetClientResWithName(const char *name, bool audit_event, bool lock)
{
   return (CLIENTRES *)GetResWithName(R_CLIENT, name, audit_event, lock);
}

JOBRES *UAContext::GetJobResWithName(const char *name, bool audit_event, bool lock)
{
   return (JOBRES *)GetResWithName(R_JOB, name, audit_event, lock);
}

FILESETRES *UAContext::GetFileSetResWithName(const char *name, bool audit_event, bool lock)
{
   return (FILESETRES *)GetResWithName(R_FILESET, name, audit_event, lock);
}

CATRES *UAContext::GetCatalogResWithName(const char *name, bool audit_event, bool lock)
{
   return (CATRES *)GetResWithName(R_CATALOG, name, audit_event, lock);
}

SCHEDRES *UAContext::GetScheduleResWithName(const char *name, bool audit_event, bool lock)
{
   return (SCHEDRES *)GetResWithName(R_SCHEDULE, name, audit_event, lock);
}
