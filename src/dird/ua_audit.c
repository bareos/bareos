/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
 * BAREOS Director -- User agent auditing.
 *
 * Written by Marco van Wieringen, April 2014
 */

#include "bareos.h"
#include "dird.h"

/* Forward referenced functions */

/*
 * See if we need to audit this event.
 */
bool audit_event_wanted(UAContext *ua, bool audit_event_enabled)
{
   if (!me->audit_events) {
      return audit_event_enabled;
   }

   if (audit_event_enabled) {
      const char *event;

      foreach_alist(event, me->audit_events) {
         if (bstrcasecmp(event, ua->argk[0])) {
            return true;
         }
      }
   }

   return false;
}

/*
 * Log an audit event for a console that accesses an resource or cmd that is not allowed.
 */
static inline void log_audit_event_acl_msg(UAContext *ua, const char *audit_msg, int acl, const char *item)
{
   const char *console_name;
   const char *acl_type_name;

   console_name = (ua->cons) ? ua->cons->name() : "default";

   switch (acl) {
   case Job_ACL:
      acl_type_name = _("for Job");
      break;
   case Client_ACL:
      acl_type_name = _("for Client");
      break;
   case Storage_ACL:
      acl_type_name = _("for Storage");
      break;
   case Schedule_ACL:
      acl_type_name = _("for Schedule");
      break;
   case Run_ACL:
      acl_type_name = _("for Schedule");
      break;
   case Pool_ACL:
      acl_type_name = _("for Pool");
      break;
   case Command_ACL:
      acl_type_name = _("for Command");
      break;
   case FileSet_ACL:
      acl_type_name = _("for Fileset");
      break;
   case Catalog_ACL:
      acl_type_name = _("for Catalog");
      break;
   case Where_ACL:
      acl_type_name = _("for Where restore location");
      break;
   case PluginOptions_ACL:
      acl_type_name = _("for Plugin Options");
      break;
   default:
      acl_type_name = "";
      break;
   }

   Emsg3(M_AUDIT, 0, audit_msg, console_name, acl_type_name, item);
}

void log_audit_event_acl_failure(UAContext *ua, int acl, const char *item)
{
   if (!me->auditing) {
      return;
   }

   log_audit_event_acl_msg(ua, _("Console [%s], Audit acl failure %s %s\n"), acl, item);
}

void log_audit_event_acl_success(UAContext *ua, int acl, const char *item)
{
   if (!me->auditing) {
      return;
   }

   log_audit_event_acl_msg(ua, _("Console [%s], Audit acl success %s %s\n"), acl, item);
}

/*
 * Log an audit event
 */
void log_audit_event_cmdline(UAContext *ua)
{
   const char *console_name;

   if (!me->auditing) {
      return;
   }

   console_name = (ua->cons) ? ua->cons->name() : "default";

   Emsg2(M_AUDIT, 0, _("Console [%s] cmdline %s\n"), console_name, ua->cmd);
}
