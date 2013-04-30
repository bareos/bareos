/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2010 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *
 *   Bacula Director -- User Agent Input and scanning code
 *
 *     Kern Sibbald, October MMI
 *
 */

#include "bacula.h"
#include "dird.h"


/* Imported variables */


/* Exported functions */

/* 
 * If subprompt is set, we send a BNET_SUB_PROMPT signal otherwise
 *   send a BNET_TEXT_INPUT signal.
 */
int get_cmd(UAContext *ua, const char *prompt, bool subprompt)
{
   BSOCK *sock = ua->UA_sock;
   int stat;

   ua->cmd[0] = 0;
   if (!sock || ua->batch) {          /* No UA or batch mode */
      return 0;
   }
   if (!subprompt && ua->api) {
      sock->signal(BNET_TEXT_INPUT);
   }
   sock->fsend("%s", prompt);
   if (!ua->api || subprompt) {
      sock->signal(BNET_SUB_PROMPT);
   }
   for ( ;; ) {
      stat = sock->recv();
      if (stat == BNET_SIGNAL) {
         continue;                    /* ignore signals */
      }
      if (is_bnet_stop(sock)) {
         return 0;                    /* error or terminate */
      }
      pm_strcpy(ua->cmd, sock->msg);
      strip_trailing_junk(ua->cmd);
      if (strcmp(ua->cmd, ".messages") == 0) {
         qmessagescmd(ua, ua->cmd);
      }
      /* Lone dot => break */
      if (ua->cmd[0] == '.' && ua->cmd[1] == 0) {
         return 0;
      }
      break;
   }
   return 1;
}

/*
 * Get a positive integer
 *  Returns:  false if failure
 *            true  if success => value in ua->pint32_val
 */
bool get_pint(UAContext *ua, const char *prompt)
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
      if (ua->cmd[0] == 0 && strncmp(prompt, _("Enter slot"), strlen(_("Enter slot"))) == 0) {
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

/*
 * Test a yes or no response
 *  Returns:  false if failure
 *            true  if success => ret == 1 for yes
 *                                ret == 0 for no
 */
bool is_yesno(char *val, int *ret)
{
   *ret = 0;
   if ((strcasecmp(val,   _("yes")) == 0) ||
       (strcasecmp(val, NT_("yes")) == 0))
   {
      *ret = 1;
   } else if ((strcasecmp(val,   _("no")) == 0) ||
              (strcasecmp(val, NT_("no")) == 0))
   {
      *ret = 0;
   } else {
      return false;
   }

   return true;
}

/*
 * Gets a yes or no response
 *  Returns:  false if failure
 *            true  if success => ua->pint32_val == 1 for yes
 *                                ua->pint32_val == 0 for no
 */
bool get_yesno(UAContext *ua, const char *prompt)
{
   int len;
   int ret;
   ua->pint32_val = 0;
   for (;;) {
      if (ua->api) ua->UA_sock->signal(BNET_YESNO);
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

/* 
 *  Gets an Enabled value => 0, 1, 2, yes, no, archived
 *  Returns: 0, 1, 2 if OK
 *           -1 on error
 */
int get_enabled(UAContext *ua, const char *val) 
{
   int Enabled = -1;

   if (strcasecmp(val, "yes") == 0 || strcasecmp(val, "true") == 0) {
     Enabled = 1;
   } else if (strcasecmp(val, "no") == 0 || strcasecmp(val, "false") == 0) {
      Enabled = 0;
   } else if (strcasecmp(val, "archived") == 0) { 
      Enabled = 2;
   } else {
      Enabled = atoi(val);
   }
   if (Enabled < 0 || Enabled > 2) {
      ua->error_msg(_("Invalid Enabled value, it must be yes, no, archived, 0, 1, or 2\n"));
      return -1;     
   }
   return Enabled;
}

void parse_ua_args(UAContext *ua)
{
   parse_args(ua->cmd, &ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);
}

/*
 * Check if the comment has legal characters
 * If ua is non-NULL send the message
 */
bool is_comment_legal(UAContext *ua, const char *name)
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
