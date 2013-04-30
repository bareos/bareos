/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2006-2008 Free Software Foundation Europe e.V.

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
 * Bacula RUNSCRIPT Structure definition for FileDaemon and Director
 * Eric Bollengier May 2006
 * Version $Id$
 */
 

#ifndef __RUNSCRIPT_H_
#define __RUNSCRIPT_H_ 1

#include "protos.h"

/* Usage:
 *
 * #define USE_RUNSCRIPT
 * #include "lib/runscript.h"
 * 
 * RUNSCRIPT *script = new_runscript();
 * script->set_command("/bin/sleep 20");
 * script->on_failure = true;
 * script->when = SCRIPT_After;
 * 
 * script->run("LabelBefore");  // the label must contain "Before" or "After" special keyword
 * free_runscript(script);
 */

/* 
 * RUNSCRIPT->when can take following bit values:
 */
enum {
   SCRIPT_Never  = 0,
   SCRIPT_After  = (1<<0),      /* AfterJob */
   SCRIPT_Before = (1<<1),      /* BeforeJob */
   SCRIPT_AfterVSS = (1<<2),	/* BeforeJob and After VSS */
   SCRIPT_Any    = SCRIPT_Before | SCRIPT_After
};

enum {
   SHELL_CMD   = '|',
   CONSOLE_CMD = '@' 
};

/*
 * Structure for RunScript ressource
 */
class RUNSCRIPT {
public:
   POOLMEM *command;            /* command string */
   POOLMEM *target;             /* host target */
   int  when;                   /* SCRIPT_Before|Script_After BEFORE/AFTER JOB*/
   int  cmd_type;               /* Command type -- Shell, Console */
   char level;                  /* Base|Full|Incr...|All (NYI) */
   bool on_success;             /* execute command on job success (After) */
   bool on_failure;             /* execute command on job failure (After) */
   bool fail_on_error;         /* abort job on error (Before) */
   /* TODO : drop this with bacula 1.42 */
   bool old_proto;              /* used by old 1.3X protocol */
   job_code_callback_t job_code_callback;
                                /* Optional callback function passed to edit_job_code */
   alist *commands;             /* Use during parsing */
   bool run(JCR *job, const char *name=""); /* name must contain "Before" or "After" keyword */
   bool can_run_at_level(int JobLevel) { return true;};        /* TODO */
   void set_command(const char *cmd, int cmd_type = SHELL_CMD);
   void set_target(const char *client_name);
   void reset_default(bool free_string = false);
   bool is_local();             /* true if running on local host */
   void debug();

   void set_job_code_callback(job_code_callback_t job_code_callback);
};

/* create new RUNSCRIPT (set all value to 0) */
RUNSCRIPT *new_runscript();           

/* create new RUNSCRIPT from an other */
RUNSCRIPT *copy_runscript(RUNSCRIPT *src);

/* launch each script from runscripts*/
int run_scripts(JCR *jcr, alist *runscripts, const char *name); 

/* free RUNSCRIPT (and all POOLMEM) */
void free_runscript(RUNSCRIPT *script);

/* foreach_alist free RUNSCRIPT */
void free_runscripts(alist *runscripts); /* you have to free alist */

extern DLL_IMP_EXP bool (*console_command)(JCR *jcr, const char *cmd);

#endif /* __RUNSCRIPT_H_ */
