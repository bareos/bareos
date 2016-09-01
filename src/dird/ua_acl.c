/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2014 Bareos GmbH & Co. KG

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
bool acl_access_ok(UAContext *ua, int acl, const char *item, bool audit_event)
{
   return acl_access_ok(ua, acl, item, strlen(item), audit_event);
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
bool acl_access_ok(UAContext *ua, int acl, const char *item, int len, bool audit_event)
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
      if (!is_name_valid(item, NULL)) {
         Dmsg1(1400, "Access denied for item=%s\n", item);
         goto bail_out;
      }
      break;
   }

   /*
    * If no console resource => default console and all is permitted
    */
   if (!ua->cons) {
      Dmsg0(1400, "Root cons access OK.\n");
      retval = true;
      goto bail_out;
   }

   retval = find_in_acl_list(ua->cons->ACL_lists[acl], acl, item, len);

   /*
    * If we didn't find a matching ACL try to use the profiles this console is connected to.
    */
   if (!retval && ua->cons->profiles && ua->cons->profiles->size()) {
      PROFILERES *profile;

      foreach_alist(profile, ua->cons->profiles) {
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
      log_audit_event_acl_failure(ua, acl, item);
   }

   return retval;
}
