/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2017 Bareos GmbH & Co. KG

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
 * Kern Sibbald, October MMI
 */
/**
 * @file
 * User Agent Input and scanning code
 */

#include "bareos.h"
#include "dird.h"
#include "dird/ua_input.h"
#include "dird/ua_cmds.h"
#include "dird/ua_select.h"

/* Imported variables */

/* Exported functions */

/**
 * If subprompt is set, we send a BNET_SUB_PROMPT signal otherwise
 *   send a BNET_TEXT_INPUT signal.
 */
bool get_cmd(UaContext *ua, const char *prompt, bool subprompt)
{
   int status;
   BareosSocket *sock = ua->UA_sock;

   ua->cmd[0] = 0;
   if (!sock || ua->batch) {          /* No UA or batch mode */
      return false;
   }
   if (!subprompt && ua->api) {
      sock->signal(BNET_TEXT_INPUT);
   }
   ua->send_msg("%s", prompt);
   if (!ua->api || subprompt) {
      sock->signal(BNET_SUB_PROMPT);
   }
   for ( ;; ) {
      status = sock->recv();
      if (status == BNET_SIGNAL) {
         continue;                    /* ignore signals */
      }
      if (is_bnet_stop(sock)) {
         return false;                /* error or terminate */
      }
      pm_strcpy(ua->cmd, sock->msg);
      strip_trailing_junk(ua->cmd);
      if (bstrcmp(ua->cmd, ".messages")) {
         dot_messages_cmd(ua, ua->cmd);
      }
      /* Lone dot => break */
      if (ua->cmd[0] == '.' && ua->cmd[1] == 0) {
         return false;
      }
      break;
   }
   return true;
}

/**
 * Get a positive integer
 *  Returns:  false if failure
 *            true  if success => value in ua->pint32_val
 */
bool get_pint(UaContext *ua, const char *prompt)
{
   double dval;
   ua->pint32_val = 0;
   ua->int64_val = 0;
   for (;;) {
      ua->cmd[0] = 0;
      if (!get_cmd(ua, prompt)) {
         return false;
      }
      /* Kludge for slots blank line => 0 */
      if (ua->cmd[0] == 0 && bstrncmp(prompt, _("Enter slot"), strlen(_("Enter slot")))) {
         return true;
      }
      if (!is_a_number(ua->cmd)) {
         ua->warning_msg(_("Expected a positive integer, got: %s\n"), ua->cmd);
         continue;
      }
      errno = 0;
      dval = strtod(ua->cmd, NULL);
      if (errno != 0 || dval < 0) {
         ua->warning_msg(_("Expected a positive integer, got: %s\n"), ua->cmd);
         continue;
      }
      ua->pint32_val = (uint32_t)dval;
      ua->int64_val = (int64_t)dval;
      return true;
   }
}

/**
 * Test a yes or no response
 *  Returns:  false if failure
 *            true  if success => ret == true for yes
 *                                ret == false for no
 */
bool is_yesno(char *val, bool *ret)
{
   *ret = 0;
   if (bstrcasecmp(val,   _("yes")) ||
       bstrcasecmp(val, NT_("yes"))) {
      *ret = true;
   } else if (bstrcasecmp(val,   _("no")) ||
              bstrcasecmp(val, NT_("no"))) {
      *ret = false;
   } else {
      return false;
   }

   return true;
}

/**
 * Gets a yes or no response
 * Returns:  false if failure
 *           true  if success => ua->pint32_val == 1 for yes
 *                               ua->pint32_val == 0 for no
 */
bool get_yesno(UaContext *ua, const char *prompt)
{
   int len;
   bool ret;

   ua->pint32_val = 0;
   for (;;) {
      if (ua->api) {
         ua->UA_sock->signal(BNET_YESNO);
      }

      if (!get_cmd(ua, prompt)) {
         return false;
      }

      len = strlen(ua->cmd);
      if (len < 1 || len > 3) {
         continue;
      }

      if (is_yesno(ua->cmd, &ret)) {
         ua->pint32_val = ret;
         return true;
      }

      ua->warning_msg(_("Invalid response. You must answer yes or no.\n"));
   }
}

/**
 * Checks for "yes" cmd parameter.
 * If not given, display prompt and gets user input "yes" or "no".
 *
 * Returns:  true  if cmd parameter "yes" is given
 *                 or user enters "yes"
 *           false otherwise
 */
bool get_confirmation(UaContext *ua, const char *prompt)
{
   if (find_arg(ua, NT_("yes")) >= 0) {
      return true;
   }

   if (ua->api || ua->batch) {
      return false;
   }

   if (get_yesno(ua, prompt)) {
      return (ua->pint32_val == 1);
   }

   return false;
}

/**
 * Gets an Enabled value => 0, 1, 2, yes, no, archived
 * Returns: 0, 1, 2 if OK
 *          -1 on error
 */
int get_enabled(UaContext *ua, const char *val)
{
   int Enabled = -1;

   if (bstrcasecmp(val, "yes") || bstrcasecmp(val, "true")) {
     Enabled = VOL_ENABLED;
   } else if (bstrcasecmp(val, "no") || bstrcasecmp(val, "false")) {
      Enabled = VOL_NOT_ENABLED;
   } else if (bstrcasecmp(val, "archived")) {
      Enabled = VOL_ARCHIVED;
   } else {
      Enabled = atoi(val);
   }

   if (Enabled < 0 || Enabled > 2) {
      ua->error_msg(_("Invalid Enabled value, it must be yes, no, archived, 0, 1, or 2\n"));
      return -1;
   }

   return Enabled;
}

void parse_ua_args(UaContext *ua)
{
   parse_args(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
}

/**
 * Check if the comment has legal characters
 * If ua is non-NULL send the message
 */
bool is_comment_legal(UaContext *ua, const char *name)
{
   int len;
   const char *p;
   const char *forbid = "'<>&\\\"";

   /* Restrict the characters permitted in the comment */
   for (p=name; *p; p++) {
      if (!strchr(forbid, (int)(*p))) {
         continue;
      }
      if (ua) {
         ua->error_msg(_("Illegal character \"%c\" in a comment.\n"), *p);
      }
      return 0;
   }
   len = strlen(name);
   if (len >= MAX_NAME_LENGTH) {
      if (ua) {
         ua->error_msg(_("Comment too long.\n"));
      }
      return 0;
   }
   if (len == 0) {
      if (ua) {
         ua->error_msg(_("Comment must be at least one character long.\n"));
      }
      return 0;
   }
   return 1;
}
