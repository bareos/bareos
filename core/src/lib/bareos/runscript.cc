/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2011 Free Software Foundation Europe e.V.

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
 * Manipulation routines for RunScript list
 *
 * Eric Bollengier, May 2006
 */

#include "include/bareos.h"
#include "include/jcr.h"
#include "runscript.h"
#include "lib/util.h"

/*
 * This function pointer is set only by the Director (dird.c),
 * and is not set in the File daemon, because the File
 * daemon cannot run console commands.
 */
bool (*console_command)(JobControlRecord *jcr, const char *cmd) = NULL;


RunScript *NewRunscript()
{
   Dmsg0(500, "runscript: creating new RunScript object\n");
   RunScript *cmd = (RunScript *)malloc(sizeof(RunScript));
   memset(cmd, 0, sizeof(RunScript));
   cmd->ResetDefault();

   return cmd;
}

void RunScript::ResetDefault(bool free_strings)
{
   if (free_strings && command) {
     FreePoolMemory(command);
   }
   if (free_strings && target) {
     FreePoolMemory(target);
   }

   target = NULL;
   command = NULL;
   on_success = true;
   on_failure = false;
   fail_on_error = true;
   when = SCRIPT_Never;
   job_code_callback = NULL;
}

RunScript *copy_runscript(RunScript *src)
{
   Dmsg0(500, "runscript: creating new RunScript object from other\n");

   RunScript *dst = (RunScript *)malloc(sizeof(RunScript));
   memcpy(dst, src, sizeof(RunScript));

   dst->command = NULL;
   dst->target = NULL;

   dst->SetCommand(src->command, src->cmd_type);
   dst->SetTarget(src->target);

   return dst;
}

void FreeRunscript(RunScript *script)
{
   Dmsg0(500, "runscript: freeing RunScript object\n");

   if (script->command) {
      FreePoolMemory(script->command);
   }
   if (script->target) {
      FreePoolMemory(script->target);
   }
   free(script);
}

static inline bool ScriptDirAllowed(JobControlRecord *jcr, RunScript *script, alist *allowed_script_dirs)
{
   char *bp, *allowed_script_dir;
   bool allowed = false;
   PoolMem script_dir(PM_FNAME);

   /*
    * If there is no explicit list of allowed dirs allow any dir.
    */
   if (!allowed_script_dirs) {
      return true;
   }

   /*
    * Determine the dir the script is in.
    */
   PmStrcpy(script_dir, script->command);
   if ((bp = strrchr(script_dir.c_str(), '/'))) {
      *bp = '\0';
   }

   /*
    * Make sure there are no relative path elements in script dir by which the
    * user tries to escape the allowed dir checking. For scripts we only allow
    * absolute paths.
    */
   if (strstr(script_dir.c_str(), "..")) {
      Dmsg1(200, "ScriptDirAllowed: relative pathnames not allowed: %s\n", script_dir.c_str());
      return false;
   }

   /*
    * Match the path the script is in against the list of allowed script directories.
    */
   foreach_alist(allowed_script_dir, allowed_script_dirs) {
      if (Bstrcasecmp(script_dir.c_str(), allowed_script_dir)) {
         allowed = true;
         break;
      }
   }

   Dmsg2(200, "ScriptDirAllowed: script %s %s allowed by Allowed Script Dir setting",
         script->command, (allowed) ? "" : "NOT");

   return allowed;
}

int RunScripts(JobControlRecord *jcr, alist *runscripts, const char *label, alist *allowed_script_dirs)
{
   RunScript *script;
   bool runit;
   int when;

   Dmsg2(200, "runscript: running all RunScript object (%s) JobStatus=%c\n", label, jcr->JobStatus);

   if (strstr(label, NT_("Before"))) {
      when = SCRIPT_Before;
   } else if (bstrcmp(label, NT_("ClientAfterVSS"))) {
      when = SCRIPT_AfterVSS;
   } else {
      when = SCRIPT_After;
   }

   if (runscripts == NULL) {
      Dmsg0(100, "runscript: WARNING RUNSCRIPTS list is NULL\n");
      return 0;
   }

   foreach_alist(script, runscripts) {
      Dmsg2(200, "runscript: try to run %s:%s\n", NPRT(script->target), NPRT(script->command));
      runit = false;

      if ((script->when & SCRIPT_Before) && (when & SCRIPT_Before)) {
         if ((script->on_success && (jcr->JobStatus == JS_Running || jcr->JobStatus == JS_Created)) ||
             (script->on_failure && (JobCanceled(jcr) || jcr->JobStatus == JS_Differences))) {
            Dmsg4(200, "runscript: Run it because SCRIPT_Before (%s,%i,%i,%c)\n",
                  script->command, script->on_success, script->on_failure, jcr->JobStatus );
            runit = true;
         }
      }

      if ((script->when & SCRIPT_AfterVSS) && (when & SCRIPT_AfterVSS)) {
         if ((script->on_success && (jcr->JobStatus == JS_Blocked)) ||
             (script->on_failure && JobCanceled(jcr))) {
            Dmsg4(200, "runscript: Run it because SCRIPT_AfterVSS (%s,%i,%i,%c)\n",
                  script->command, script->on_success, script->on_failure, jcr->JobStatus );
            runit = true;
         }
      }

      if ((script->when & SCRIPT_After) && (when & SCRIPT_After)) {
         if ((script->on_success && jcr->IsTerminatedOk()) ||
             (script->on_failure && (JobCanceled(jcr) || jcr->JobStatus == JS_Differences))) {
            Dmsg4(200, "runscript: Run it because SCRIPT_After (%s,%i,%i,%c)\n",
                  script->command, script->on_success, script->on_failure, jcr->JobStatus );
            runit = true;
         }
      }

      if (!script->IsLocal()) {
         runit = false;
      }

      /*
       * We execute it
       */
      if (runit) {
         if (!ScriptDirAllowed(jcr, script, allowed_script_dirs)) {
            Dmsg1(200, "runscript: Not running script %s because its not in one of the allowed scripts dirs\n",
                  script->command);
            Jmsg(jcr, M_ERROR, 0, _("Runscript: run %s \"%s\" could not execute, "
                                    "not in one of the allowed scripts dirs\n"), label, script->command);
            jcr->setJobStatus(JS_ErrorTerminated);
            goto bail_out;
         }

         script->run(jcr, label);
      }
   }

bail_out:
   return 1;
}

bool RunScript::IsLocal()
{
   if (!target || bstrcmp(target, "")) {
      return true;
   } else {
      return false;
   }
}

/* set this->command to cmd */
void RunScript::SetCommand(const char *cmd, int acmd_type)
{
   Dmsg1(500, "runscript: setting command = %s\n", NPRT(cmd));

   if (!cmd) {
      return;
   }

   if (!command) {
      command = GetPoolMemory(PM_FNAME);
   }

   PmStrcpy(command, cmd);
   cmd_type = acmd_type;
}

/* set this->target to client_name */
void RunScript::SetTarget(const char *client_name)
{
   Dmsg1(500, "runscript: setting target = %s\n", NPRT(client_name));

   if (!client_name) {
      return;
   }

   if (!target) {
      target = GetPoolMemory(PM_FNAME);
   }

   PmStrcpy(target, client_name);
}

bool RunScript::run(JobControlRecord *jcr, const char *name)
{
   Dmsg1(100, "runscript: running a RunScript object type=%d\n", cmd_type);
   POOLMEM *ecmd = GetPoolMemory(PM_FNAME);
   int status;
   Bpipe *bpipe;
   PoolMem line(PM_NAME);

   ecmd = edit_job_codes(jcr, ecmd, this->command, "", this->job_code_callback);
   Dmsg1(100, "runscript: running '%s'...\n", ecmd);
   Jmsg(jcr, M_INFO, 0, _("%s: run %s \"%s\"\n"),
        cmd_type==SHELL_CMD?"shell command":"console command", name, ecmd);

   switch (cmd_type) {
   case SHELL_CMD:
      bpipe = OpenBpipe(ecmd, 0, "r");
      FreePoolMemory(ecmd);

      if (bpipe == NULL) {
         BErrNo be;
         Jmsg(jcr, M_ERROR, 0, _("Runscript: %s could not execute. ERR=%s\n"), name,
            be.bstrerror());
         goto bail_out;
      }

      while (fgets(line.c_str(), line.size(), bpipe->rfd)) {
         StripTrailingJunk(line.c_str());
         Jmsg(jcr, M_INFO, 0, _("%s: %s\n"), name, line.c_str());
      }

      status = CloseBpipe(bpipe);

      if (status != 0) {
         BErrNo be;
         Jmsg(jcr, M_ERROR, 0, _("Runscript: %s returned non-zero status=%d. ERR=%s\n"), name,
            be.code(status), be.bstrerror(status));
         goto bail_out;
      }

      Dmsg0(100, "runscript OK\n");
      break;
   case CONSOLE_CMD:
      if (console_command) {                 /* can we run console command? */
         if (!console_command(jcr, ecmd)) {  /* yes, do so */
            goto bail_out;
         }
      }
      break;
   }
   return true;

bail_out:
   /* cancel running job properly */
   if (fail_on_error) {
      jcr->setJobStatus(JS_ErrorTerminated);
   }
   Dmsg1(100, "runscript failed. fail_on_error=%d\n", fail_on_error);
   return false;
}

void FreeRunscripts(alist *runscripts)
{
   Dmsg0(500, "runscript: freeing all RUNSCRIPTS object\n");

   RunScript *elt;
   foreach_alist(elt, runscripts) {
      FreeRunscript(elt);
   }
}

void RunScript::debug()
{
   Dmsg0(200, "runscript: debug\n");
   Dmsg0(200,  _(" --> RunScript\n"));
   Dmsg1(200,  _("  --> Command=%s\n"), NPRT(command));
   Dmsg1(200,  _("  --> Target=%s\n"),  NPRT(target));
   Dmsg1(200,  _("  --> RunOnSuccess=%u\n"),  on_success);
   Dmsg1(200,  _("  --> RunOnFailure=%u\n"),  on_failure);
   Dmsg1(200,  _("  --> FailJobOnError=%u\n"),  fail_on_error);
   Dmsg1(200,  _("  --> RunWhen=%u\n"),  when);
}

void RunScript::SetJobCodeCallback(job_code_callback_t arg_job_code_callback)
{
   this->job_code_callback = arg_job_code_callback;
}
