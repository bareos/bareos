/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

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
 * Signal handlers for BAREOS daemons
 *
 * Kern Sibbald, April 2000
 *
 * Note, we probably should do a core dump for the serious
 * signals such as SIGBUS, SIGPFE, ...
 * Also, for SIGHUP and SIGUSR1, we should re-read the
 * configuration file.  However, since this is a "general"
 * routine, we leave it to the individual daemons to
 * tweak their signals after calling this routine.
 */

#ifndef HAVE_WIN32
#include "include/bareos.h"
#include "lib/bsignal.h"

#ifndef _NSIG
#define BA_NSIG 100
#else
#define BA_NSIG _NSIG
#endif

extern char my_name[];
extern char *exepath;
extern char *exename;
extern bool prt_kaboom;

static const char *sig_names[BA_NSIG + 1];

typedef void (SIG_HANDLER)(int sig);
static SIG_HANDLER *exit_handler;

/* main process id */
static pid_t main_pid = 0;

const char *get_signal_name(int sig)
{
   if (sig < 0 || sig > BA_NSIG || !sig_names[sig]) {
      return _("Invalid signal number");
   } else {
      return sig_names[sig];
   }
}

/* defined in jcr.c */
extern void DbgPrintJcr(FILE *fp);
/* defined in plugins.c */
extern void DbgPrintPlugin(FILE *fp);
/* defined in lockmgr.c */
extern void DbgPrintLock(FILE *fp);

/*
 * !!! WARNING !!!
 *
 * This function should be used ONLY after a violent signal. We walk through the
 * JobControlRecord chain without locking, BAREOS should not be running.
 */
static void dbg_print_bareos()
{
   char buf[512];

   snprintf(buf, sizeof(buf), "%s/%s.%d.bactrace",
            working_directory, my_name, (int)getpid());
   FILE *fp = fopen(buf, "a+") ;
   if (!fp) {
      fp = stderr;
   }

   fprintf(stderr, "Dumping: %s\n", buf);

   /* Print also BareosDb and RWLOCK structure
    * Can add more info about JobControlRecord with DbgJcrAddHook()
    */
   DbgPrintLock(fp);
   DbgPrintJcr(fp);
   DbgPrintPlugin(fp);

   if (fp != stderr) {
#define direct_print
#ifdef direct_print
      if (prt_kaboom) {
         rewind(fp);
         printf("\n\n ==== bactrace output ====\n\n");
         while (fgets(buf, (int)sizeof(buf), fp) != NULL) {
            printf("%s", buf);
         }
         printf(" ==== End baktrace output ====\n\n");
      }
#else
      if (prt_kaboom) {
         char buf1[512];
         printf("\n\n ==== bactrace output ====\n\n");
         snprintf(buf1, sizeof(buf1), "/bin/cat %s", buf);
         system(buf1);
         printf(" ==== End baktrace output ====\n\n");
      }
#endif
      fclose(fp);
   }
}

/*
 * Handle signals here
 */
extern "C" void SignalHandler(int sig)
{
   static int already_dead = 0;
   int chld_status = -1;

   /*
    * If we come back more than once, get out fast!
    */
   if (already_dead) {
      exit(1);
   }
   Dmsg2(900, "sig=%d %s\n", sig, sig_names[sig]);

   /*
    * Ignore certain signals -- SIGUSR2 used to interrupt threads
    */
   if (sig == SIGCHLD || sig == SIGUSR2) {
      return;
   }
   already_dead++;

   /*
    * Don't use Emsg here as it may lock and thus block us
    */
   if (sig == SIGTERM) {
       syslog(LOG_DAEMON | LOG_ERR, "Shutting down BAREOS service: %s ...\n", my_name);
   } else {
      fprintf(stderr, _("BAREOS interrupted by signal %d: %s\n"), sig, get_signal_name(sig));
      syslog(LOG_DAEMON | LOG_ERR,
             _("BAREOS interrupted by signal %d: %s\n"), sig, get_signal_name(sig));
   }

#ifdef TRACEBACK
   if (sig != SIGTERM) {
      struct sigaction sigdefault;
      static char *argv[5];
      static char pid_buf[20];
      static char btpath[400];
      char buf[400];
      pid_t pid;
      int exelen = strlen(exepath);

      fprintf(stderr, _("%s, %s got signal %d - %s. Attempting traceback.\n"),
              exename, my_name, sig, get_signal_name(sig));
      fprintf(stderr, _("exepath=%s\n"), exepath);

      if (exelen + 12 > (int)sizeof(btpath)) {
         bstrncpy(btpath, "btraceback", sizeof(btpath));
      } else {
         bstrncpy(btpath, exepath, sizeof(btpath));
         if (IsPathSeparator(btpath[exelen-1])) {
            btpath[exelen-1] = 0;
         }
         bstrncat(btpath, "/btraceback", sizeof(btpath));
      }
      if (!IsPathSeparator(exepath[exelen - 1])) {
         strcat(exepath, "/");
      }
      strcat(exepath, exename);
      if (!working_directory) {
         working_directory = buf;
         *buf = 0;
      }
      if (*working_directory == 0) {
         strcpy((char *)working_directory, "/tmp/");
      }
      if (chdir(working_directory) != 0) {  /* dump in working directory */
         BErrNo be;
         Pmsg2(000, "chdir to %s failed. ERR=%s\n", working_directory,  be.bstrerror());
         strcpy((char *)working_directory, "/tmp/");
      }
      SecureErase(NULL, "./core");        /* get rid of any old core file */

#ifdef DEVELOPER /* When DEVELOPER not set, this is done below */
      /*
       * Print information about the current state into working/<file>.bactrace
       */
      dbg_print_bareos();
#endif

      sprintf(pid_buf, "%d", (int)main_pid);
      Dmsg1(300, "Working=%s\n", working_directory);
      Dmsg1(300, "btpath=%s\n", btpath);
      Dmsg1(300, "exepath=%s\n", exepath);
      switch (pid = fork()) {
      case -1:                        /* error */
         fprintf(stderr, _("Fork error: ERR=%s\n"), strerror(errno));
         break;
      case 0:                         /* child */
         argv[0] = btpath;            /* path to btraceback */
         argv[1] = exepath;           /* path to exe */
         argv[2] = pid_buf;
         argv[3] = (char *)working_directory;
         argv[4] = (char *)NULL;
         fprintf(stderr, _("Calling: %s %s %s %s\n"), btpath, exepath, pid_buf,
            working_directory);
         if (execv(btpath, argv) != 0) {
            BErrNo be;
            printf(_("execv: %s failed: ERR=%s\n"), btpath, be.bstrerror());
         }
         exit(-1);
      default:                        /* parent */
         break;
      }

      /*
       * Parent continue here, waiting for child
       */
      sigdefault.sa_flags = 0;
      sigdefault.sa_handler = SIG_DFL;
      sigfillset(&sigdefault.sa_mask);

      sigaction(sig, &sigdefault, NULL);
      if (pid > 0) {
         Dmsg0(500, "Doing waitpid\n");
         waitpid(pid, &chld_status, 0);   /* wait for child to produce dump */
         Dmsg0(500, "Done waitpid\n");
      } else {
         Dmsg0(500, "Doing sleep\n");
         Bmicrosleep(30, 0);
      }
      if (WEXITSTATUS(chld_status) == 0) {
         fprintf(stderr, _("It looks like the traceback worked...\n"));
      } else {
         fprintf(stderr, _("The btraceback call returned %d\n"),
                 WEXITSTATUS(chld_status));
      }

      /*
       * If we want it printed, do so
       */
#ifdef direct_print
      if (prt_kaboom) {
         FILE *fd;

         snprintf(buf, sizeof(buf), "%s/bareos.%s.traceback", working_directory, pid_buf);
         fd = fopen(buf, "r");
         if (fd != NULL) {
            printf("\n\n ==== Traceback output ====\n\n");
            while (fgets(buf, (int)sizeof(buf), fd) != NULL) {
               printf("%s", buf);
            }
            fclose(fd);
            printf(" ==== End traceback output ====\n\n");
         }
      }
#else
      if (prt_kaboom) {
         snprintf(buf, sizeof(buf), "/bin/cat %s/bareos.%s.traceback", working_directory, pid_buf);
         fprintf(stderr, "\n\n ==== Traceback output ====\n\n");
         system(buf);
         fprintf(stderr, " ==== End traceback output ====\n\n");
      }
#endif

#ifndef DEVELOPER /* When DEVELOPER set, this is done above */
      /*
       * Print information about the current state into working/<file>.bactrace
       */
      dbg_print_bareos();
#endif

   }
#endif
   exit_handler(sig);
   Dmsg0(500, "Done exit_handler\n");
}

/*
 * Init stack dump by saving main process id -- needed by debugger to attach to this program.
 */
void InitStackDump(void)
{
   main_pid = getpid();               /* save main thread's pid */
}

/*
 * Initialize signals
 */
void InitSignals(void Terminate(int sig))
{
   struct sigaction sighandle;
   struct sigaction sigignore;
   struct sigaction sigdefault;
#ifdef _sys_nsig
   int i;

   exit_handler = Terminate;
   if (BA_NSIG < _sys_nsig) {
      Emsg2(M_ABORT, 0, _("BA_NSIG too small (%d) should be (%d)\n"), BA_NSIG, _sys_nsig);
   }

   for (i = 0; i < _sys_nsig; i++) {
      sig_names[i] = _sys_siglist[i];
   }
#else
   exit_handler = Terminate;
   sig_names[0] = _("UNKNOWN SIGNAL");
   sig_names[SIGHUP] = _("Hangup");
   sig_names[SIGINT] = _("Interrupt");
   sig_names[SIGQUIT] = _("Quit");
   sig_names[SIGILL] = _("Illegal instruction");
   sig_names[SIGTRAP] = _("Trace/Breakpoint trap");
   sig_names[SIGABRT] = _("Abort");
#ifdef SIGEMT
   sig_names[SIGEMT] = _("EMT instruction (Emulation Trap)");
#endif
#ifdef SIGIOT
   sig_names[SIGIOT] = _("IOT trap");
#endif
   sig_names[SIGBUS] = _("BUS error");
   sig_names[SIGFPE] = _("Floating-point exception");
   sig_names[SIGKILL] = _("Kill, unblockable");
   sig_names[SIGUSR1] = _("User-defined signal 1");
   sig_names[SIGSEGV] = _("Segmentation violation");
   sig_names[SIGUSR2] = _("User-defined signal 2");
   sig_names[SIGPIPE] = _("Broken pipe");
   sig_names[SIGALRM] = _("Alarm clock");
   sig_names[SIGTERM] = _("Termination");
#ifdef SIGSTKFLT
   sig_names[SIGSTKFLT] = _("Stack fault");
#endif
   sig_names[SIGCHLD] = _("Child status has changed");
   sig_names[SIGCONT] = _("Continue");
   sig_names[SIGSTOP] = _("Stop, unblockable");
   sig_names[SIGTSTP] = _("Keyboard stop");
   sig_names[SIGTTIN] = _("Background read from tty");
   sig_names[SIGTTOU] = _("Background write to tty");
   sig_names[SIGURG] = _("Urgent condition on socket");
   sig_names[SIGXCPU] = _("CPU limit exceeded");
   sig_names[SIGXFSZ] = _("File size limit exceeded");
   sig_names[SIGVTALRM] = _("Virtual alarm clock");
   sig_names[SIGPROF] = _("Profiling alarm clock");
   sig_names[SIGWINCH] = _("Window size change");
   sig_names[SIGIO] = _("I/O now possible");
#ifdef SIGPWR
   sig_names[SIGPWR] = _("Power failure restart");
#endif
#ifdef SIGWAITING
   sig_names[SIGWAITING] = _("No runnable lwp");
#endif
#ifdef SIGLWP
   sig_names[SIGLWP] = _("SIGLWP special signal used by thread library");
#endif
#ifdef SIGFREEZE
   sig_names[SIGFREEZE] = _("Checkpoint Freeze");
#endif
#ifdef SIGTHAW
   sig_names[SIGTHAW] = _("Checkpoint Thaw");
#endif
#ifdef SIGCANCEL
   sig_names[SIGCANCEL] = _("Thread Cancellation");
#endif
#ifdef SIGLOST
   sig_names[SIGLOST] = _("Resource Lost (e.g. record-lock lost)");
#endif
#endif

   /*
    * Now setup signal handlers
    */
   sighandle.sa_flags = 0;
   sighandle.sa_handler = SignalHandler;
   sigfillset(&sighandle.sa_mask);
   sigignore.sa_flags = 0;
   sigignore.sa_handler = SIG_IGN;
   sigfillset(&sigignore.sa_mask);
   sigdefault.sa_flags = 0;
   sigdefault.sa_handler = SIG_DFL;
   sigfillset(&sigdefault.sa_mask);

   sigaction(SIGPIPE, &sigignore, NULL);
   sigaction(SIGCHLD, &sighandle, NULL);
   sigaction(SIGCONT, &sigignore, NULL);
   sigaction(SIGPROF, &sigignore, NULL);
   sigaction(SIGWINCH, &sigignore, NULL);
   sigaction(SIGIO, &sighandle, NULL);
   sigaction(SIGINT, &sigdefault, NULL);
   sigaction(SIGXCPU, &sigdefault, NULL);
   sigaction(SIGXFSZ, &sigdefault, NULL);
   sigaction(SIGHUP, &sigignore, NULL);
   sigaction(SIGQUIT, &sighandle, NULL);
   sigaction(SIGILL, &sighandle, NULL);
   sigaction(SIGTRAP, &sighandle, NULL);
   sigaction(SIGABRT, &sighandle, NULL);
#ifdef SIGEMT
   sigaction(SIGEMT, &sighandle, NULL);
#endif
#ifdef SIGIOT
   sigaction(SIGIOT, &sighandle, NULL);
#endif
   sigaction(SIGBUS, &sighandle, NULL);
   sigaction(SIGFPE, &sighandle, NULL);
   sigaction(SIGUSR1, &sighandle, NULL);
   sigaction(SIGSEGV, &sighandle, NULL);
   sigaction(SIGUSR2, &sighandle, NULL);
   sigaction(SIGALRM, &sighandle, NULL);
   sigaction(SIGTERM, &sighandle, NULL);
#ifdef SIGSTKFLT
   sigaction(SIGSTKFLT, &sighandle, NULL);
#endif
   sigaction(SIGTSTP, &sigdefault, NULL);
   sigaction(SIGTTIN, &sighandle, NULL);
   sigaction(SIGTTOU, &sighandle, NULL);
   sigaction(SIGURG, &sighandle, NULL);
   sigaction(SIGVTALRM, &sighandle, NULL);
#ifdef SIGPWR
   sigaction(SIGPWR, &sighandle, NULL);
#endif
#ifdef SIGWAITING
   sigaction(SIGWAITING,&sighandle, NULL);
#endif
#ifdef SIGLWP
   sigaction(SIGLWP, &sighandle, NULL);
#endif
#ifdef SIGFREEZE
   sigaction(SIGFREEZE, &sighandle, NULL);
#endif
#ifdef SIGTHAW
   sigaction(SIGTHAW, &sighandle, NULL);
#endif
#ifdef SIGCANCEL
   sigaction(SIGCANCEL, &sighandle, NULL);
#endif
#ifdef SIGLOST
   sigaction(SIGLOST, &sighandle, NULL);
#endif
}
#endif
