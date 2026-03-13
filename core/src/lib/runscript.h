/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2026 Bareos GmbH & Co. KG

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
// Eric Bollengier May 2006
/**
 * @file
 * BAREOS RunScript Structure definition for FileDaemon and Director
 */

#ifndef BAREOS_LIB_RUNSCRIPT_H_
#define BAREOS_LIB_RUNSCRIPT_H_

#include "jcr.h"
#include "lib/bareos_resource.h"
template <typename T> class alist;

/* Usage:
 *
 * #include "lib/runscript.h"
 *
 * RunScript *script = new RunScript;
 * script->SetCommand("/bin/sleep 20");
 * script->on_failure = true;
 * script->when = SCRIPT_After;
 *
 * script->Run("LabelBefore");  // the label must contain "Before" or "After"
 * special keyword FreeRunscript(script);
 */

// RunScript->when can take following bit values:
enum
{
  SCRIPT_Never = 0,
  SCRIPT_After = (1 << 0),    /* AfterJob */
  SCRIPT_Before = (1 << 1),   /* BeforeJob */
  SCRIPT_AfterVSS = (1 << 2), /* BeforeJob and After VSS */
  SCRIPT_Any = SCRIPT_Before | SCRIPT_After,
  SCRIPT_INVALID = 0xff
};

enum
{
  SHELL_CMD = '|',
  CONSOLE_CMD = '@'
};

struct TempParserCommand {
  TempParserCommand(const char* p, int32_t c) : command_(p), code_(c) {}
  std::string command_;
  int32_t code_ = 0;
};

class RunScript : public BareosResource {
 public:
  RunScript() = default;
  virtual ~RunScript() = default;
  RunScript(const RunScript& other) = default;

  std::string command;       /* Command string */
  std::string target;        /* Host target. Values:
                                Empty string: run locally.
                                "%c": (Client’s name). Run on client. */
  int when = SCRIPT_Never;   /* SCRIPT_Before|Script_After BEFORE/AFTER JOB*/
  int cmd_type = 0;          /* Command type -- Shell, Console */
  char level = 0;            /* Base|Full|Incr...|All (NYI) */
  bool short_form = false;   /* Run Script in short form */
  bool from_jobdef = false;  /* This RUN script comes from JobDef */
  bool on_success = true;    /* Execute command on job success (After) */
  bool on_failure = false;   /* Execute command on job failure (After) */
  bool fail_on_error = true; /* Abort job on error (Before) */
  job_code_callback_t job_code_callback = nullptr;
  std::vector<TempParserCommand> temp_parser_command_container;
  bool Run(JobControlRecord* job,
           const char* name
           = ""); /* name must contain "Before" or "After" keyword */
  void SetCommand(const std::string& cmd, int cmd_type = SHELL_CMD);
  void SetTarget(const std::string& client_name);
  bool IsLocal() const { return target.empty(); } /* true if no target host */
  void Debug() const;

  void SetJobCodeCallback(job_code_callback_t job_code_callback);
};

/* create new RunScript from another */
RunScript* DuplicateRunscript(RunScript* src);

/* launch each script from runscripts*/
int RunScripts(JobControlRecord* jcr,
               alist<RunScript*>* runscripts,
               const char* name,
               alist<const char*>* allowed_script_dirs = NULL);

void FreeRunscript(RunScript* script);

/* foreach_alist free RunScript */
void FreeRunscripts(alist<RunScript*>* runscripts); /* you have to free alist */

BAREOS_IMPORT bool (*console_command)(JobControlRecord* jcr, const char* cmd);

#endif  // BAREOS_LIB_RUNSCRIPT_H_
