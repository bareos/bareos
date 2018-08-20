/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
 * Written by Marco van Wieringen, April 2014
 */
/**
 * @file
 * BAREOS Director -- User agent auditing.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"

namespace directordaemon {

/* Forward referenced functions */

/**
 * See if we need to audit this event.
 */
bool UaContext::AuditEventWanted(bool audit_event_enabled)
{
   if (!me->audit_events) {
      return audit_event_enabled;
   }

   if (audit_event_enabled) {
      const char *event;

      foreach_alist(event, me->audit_events) {
         if (Bstrcasecmp(event, argk[0])) {
            return true;
         }
      }
   }

   return false;
}

/**
 * Log an audit event for a console that accesses an resource or cmd that is not allowed.
 */
static inline void LogAuditEventAclMsg(UaContext *ua, const char *audit_msg, int acl, const char *item)
{
   const char *console_name;
   const char *host;
   const char *acl_type_name;

   console_name = (ua->cons) ? ua->cons->name() : "default";
   host = (ua->UA_sock) ? ua->UA_sock->host() : "unknown";

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

   Emsg4(M_AUDIT, 0, audit_msg, console_name, host, acl_type_name, item);
}

void UaContext::LogAuditEventAclFailure(int acl, const char *item)
{
   if (!me->auditing) {
      return;
   }

   LogAuditEventAclMsg(this, _("Console [%s] from [%s], Audit acl failure %s %s\n"), acl, item);
}

void UaContext::LogAuditEventAclSuccess(int acl, const char *item)
{
   if (!me->auditing) {
      return;
   }

   LogAuditEventAclMsg(this, _("Console [%s] from [%s], Audit acl success %s %s\n"), acl, item);
}

/**
 * Log an audit event
 */
void UaContext::LogAuditEventCmdline()
{
   const char *console_name;
   const char *host;

   if (!me->auditing) {
      return;
   }

   console_name = cons ? cons->name() : "default";
   host = UA_sock ? UA_sock->host() : "unknown";

   Emsg3(M_AUDIT, 0, _("Console [%s] from [%s] cmdline %s\n"), console_name, host, cmd);
}
} /* namespace directordaemon */
