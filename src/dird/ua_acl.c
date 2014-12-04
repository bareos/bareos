/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2008 Free Software Foundation Europe e.V.

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
 * This version expects the length of the item which we must check.
 */
bool acl_access_ok(UAContext *ua, int acl, const char *item, int len, bool audit_event)
{
   bool retval = false;
   alist *list;

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
      retval = true;                  /* No cons resource -> root console OK for everything */
      goto bail_out;
   }

   list = ua->cons->ACL_lists[acl];
   if (!list) {                       /* Empty list */
      /*
       * Empty list for Where => empty where accept anything.
       * For any other list, reject everything.
       */
      if (len == 0 && acl == Where_ACL) {
         retval = true;
      }
      goto bail_out;
   }

   /*
    * Special case *all* gives full access
    */
   if (list->size() == 1 && bstrcasecmp("*all*", (char *)list->get(0))) {
      retval = true;
      goto bail_out;
   }

   /*
    * Search list for item
    */
   for (int i=0; i<list->size(); i++) {
      if (bstrcasecmp(item, (char *)list->get(i))) {
         Dmsg3(1400, "ACL found %s in %d %s\n", item, acl, (char *)list->get(i));
         retval = true;
         goto bail_out;
      }
   }

bail_out:
   if (audit_event && !retval) {
      log_audit_event_acl_failure(ua, acl, item);
   }

   return retval;
}
