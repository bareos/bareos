/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2019-2024 Bareos GmbH & Co. KG

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
#include "lib/berrno.h"
#include "lib/util.h"
#include "lib/bpipe.h"

/*
 * This function pointer is set only by the Director (dird.c),
 * and is not set in the File daemon, because the File
 * daemon cannot run console commands.
 */
bool (*console_command)(JobControlRecord* jcr, const char* cmd) = NULL;

RunScript* DuplicateRunscript(RunScript* src)
{
  Dmsg0(500, "runscript: creating new RunScript object from other\n");

  RunScript* copy = new RunScript(*src);

  copy->command.clear();
  copy->SetCommand(src->command, src->cmd_type);
  copy->SetTarget(src->target);

  return copy;
}

void FreeRunscript(RunScript* script)
{
  Dmsg0(500, "runscript: freeing RunScript object\n");
  delete script;
}

static bool ScriptDirAllowed(JobControlRecord*,
                             RunScript* script,
                             alist<const char*>* allowed_script_dirs)
{
  bool allowed = false;
  PoolMem script_dir(PM_FNAME);

  // If there is no explicit list of allowed dirs allow any dir.
  if (!allowed_script_dirs) { return true; }

  // Determine the dir the script is in.
  PmStrcpy(script_dir, script->command.c_str());
  if (char* bp = strrchr(script_dir.c_str(), '/'); bp != nullptr) {
    *bp = '\0';
  }

  /* Make sure there are no relative path elements in script dir by which the
   * user tries to escape the allowed dir checking. For scripts we only allow
   * absolute paths. */
  if (strstr(script_dir.c_str(), "..")) {
    Dmsg1(200, "ScriptDirAllowed: relative pathnames not allowed: %s\n",
          script_dir.c_str());
    return false;
  }

  /* Match the path the script is in against the list of allowed script
   * directories. */
  for (auto* allowed_script_dir : *allowed_script_dirs) {
    if (Bstrcasecmp(script_dir.c_str(), allowed_script_dir)) {
      allowed = true;
      break;
    }
  }

  Dmsg2(200,
        "ScriptDirAllowed: script %s %s allowed by Allowed Script Dir setting",
        script->command.c_str(), (allowed) ? "" : "NOT");

  return allowed;
}

int RunScripts(JobControlRecord* jcr,
               alist<RunScript*>* runscripts,
               const char* label,
               alist<const char*>* allowed_script_dirs)
{
  bool runit;
  int when;

  Dmsg2(200, "runscript: running all RunScript object (%s) JobStatus=%c\n",
        label, jcr->getJobStatus());

  if (strstr(label, NT_("Before"))) {
    when = SCRIPT_Before;
  } else if (bstrcmp(label, NT_("ClientAfterVSS"))) {
    when = SCRIPT_AfterVSS;
  } else {
    when = SCRIPT_After;
  }

  if (!runscripts) {
    Dmsg0(100, "runscript: WARNING RUNSCRIPTS list is NULL\n");
    return 0;
  }

  for (auto* script : *runscripts) {
    Dmsg5(200,
          "runscript: try to run (Target=%s, OnSuccess=%i, OnFailure=%i, "
          "CurrentJobStatus=%c, command=%s)\n",
          NSTDPRNT(script->target), script->on_success, script->on_failure,
          jcr->getJobStatus(), NSTDPRNT(script->command));
    runit = false;

    if (!script->IsLocal()) {
      if (jcr->is_JobType(JT_ADMIN)) {
        Jmsg(jcr, M_WARNING, 0,
             "Invalid runscript definition (command=%s). Admin Jobs only "
             "support local runscripts.\n",
             script->command.c_str());
      }
    } else {
      if ((script->when & SCRIPT_Before) && (when & SCRIPT_Before)) {
        if ((script->on_success
             && (jcr->getJobStatus() == JS_Running
                 || jcr->getJobStatus() == JS_Created))
            || (script->on_failure
                && (jcr->IsJobCanceled()
                    || jcr->getJobStatus() == JS_Differences))) {
          Dmsg4(200, "runscript: Run it because SCRIPT_Before (%s,%i,%i,%c)\n",
                script->command.c_str(), script->on_success, script->on_failure,
                jcr->getJobStatus());
          runit = true;
        }
      }

      if ((script->when & SCRIPT_AfterVSS) && (when & SCRIPT_AfterVSS)) {
        if ((script->on_success && (jcr->getJobStatus() == JS_Blocked))
            || (script->on_failure && jcr->IsJobCanceled())) {
          Dmsg4(200,
                "runscript: Run it because SCRIPT_AfterVSS (%s,%i,%i,%c)\n",
                script->command.c_str(), script->on_success, script->on_failure,
                jcr->getJobStatus());
          runit = true;
        }
      }

      if ((script->when & SCRIPT_After) && (when & SCRIPT_After)) {
        if ((script->on_success && jcr->IsTerminatedOk())
            || (script->on_failure
                && (jcr->IsJobCanceled()
                    || jcr->getJobStatus() == JS_Differences))) {
          Dmsg4(200, "runscript: Run it because SCRIPT_After (%s,%i,%i,%c)\n",
                script->command.c_str(), script->on_success, script->on_failure,
                jcr->getJobStatus());
          runit = true;
        }
      }
    }

    // We execute it
    if (runit) {
      if (!ScriptDirAllowed(jcr, script, allowed_script_dirs)) {
        Dmsg1(200,
              "runscript: Not running script %s because its not in one of the "
              "allowed scripts dirs\n",
              script->command.c_str());
        Jmsg(jcr, M_ERROR, 0,
             T_("Runscript: run %s \"%s\" could not execute, "
                "not in one of the allowed scripts dirs\n"),
             label, script->command.c_str());
        jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
        goto bail_out;
      }

      script->Run(jcr, label);
    }
  }

bail_out:
  return 1;
}

void RunScript::SetCommand(const std::string& cmd, int acmd_type)
{
  Dmsg1(500, "runscript: setting command = %s\n", NSTDPRNT(cmd));

  if (cmd.empty()) { return; }

  command = cmd;
  cmd_type = acmd_type;
}

void RunScript::SetTarget(const std::string& client_name)
{
  Dmsg1(500, "runscript: setting target = %s\n", NSTDPRNT(client_name));

  target = client_name;
}

bool RunScript::Run(JobControlRecord* jcr, const char* name)
{
  Dmsg1(100, "runscript: running a RunScript object type=%d\n", cmd_type);
  POOLMEM* ecmd = GetPoolMemory(PM_FNAME);
  int status;
  Bpipe* bpipe;
  PoolMem line(PM_NAME);

  ecmd
      = edit_job_codes(jcr, ecmd, command.c_str(), "", this->job_code_callback);
  Dmsg1(100, "runscript: running '%s'...\n", ecmd);
  Jmsg(jcr, M_INFO, 0, T_("%s: run %s \"%s\"\n"),
       cmd_type == SHELL_CMD ? "shell command" : "console command", name, ecmd);

  switch (cmd_type) {
    case SHELL_CMD:
      bpipe = OpenBpipe(ecmd, 0, "r");

      if (bpipe == NULL) {
        BErrNo be;
        Jmsg(jcr, M_ERROR, 0, T_("Runscript: %s could not execute. ERR=%s\n"),
             name, be.bstrerror());
        goto bail_out;
      }

      while (fgets(line.c_str(), line.size(), bpipe->rfd)) {
        StripTrailingJunk(line.c_str());
        Jmsg(jcr, M_INFO, 0, T_("%s: %s\n"), name, line.c_str());
      }

      status = CloseBpipe(bpipe);

      if (status != 0) {
        BErrNo be;
        Jmsg(jcr, M_ERROR, 0,
             T_("Runscript: %s returned non-zero status=%d. ERR=%s\n"), name,
             be.code(status), be.bstrerror(status));
        goto bail_out;
      }

      Dmsg0(100, "runscript OK\n");
      break;
    case CONSOLE_CMD:
      if (console_command) {               /* can we run console command? */
        if (!console_command(jcr, ecmd)) { /* yes, do so */
          goto bail_out;
        }
      }
      break;
  }
  FreePoolMemory(ecmd);
  return true;

bail_out:
  /* cancel running job properly */
  if (fail_on_error) { jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated); }
  Dmsg1(100, "runscript failed. fail_on_error=%d\n", fail_on_error);
  FreePoolMemory(ecmd);
  return false;
}

void FreeRunscripts(alist<RunScript*>* runscripts)
{
  Dmsg0(500, "runscript: freeing all RUNSCRIPTS object\n");

  if (runscripts) {
    for (auto* r : *runscripts) { FreeRunscript(r); }
  }
}

void RunScript::Debug() const
{
  Dmsg0(200, "runscript: debug\n");
  Dmsg0(200, T_(" --> RunScript\n"));
  Dmsg1(200, T_("  --> Command=%s\n"), NSTDPRNT(command));
  Dmsg1(200, T_("  --> Target=%s\n"), NSTDPRNT(target));
  Dmsg1(200, T_("  --> RunOnSuccess=%u\n"), on_success);
  Dmsg1(200, T_("  --> RunOnFailure=%u\n"), on_failure);
  Dmsg1(200, T_("  --> FailJobOnError=%u\n"), fail_on_error);
  Dmsg1(200, T_("  --> RunWhen=%u\n"), when);
}

void RunScript::SetJobCodeCallback(job_code_callback_t arg_job_code_callback)
{
  this->job_code_callback = arg_job_code_callback;
}
