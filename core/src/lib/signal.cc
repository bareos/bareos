/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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
#  include <syslog.h>
#  include <sys/wait.h>
#  include <unistd.h>
#  include "include/bareos.h"
#  include "include/exit_codes.h"
#  include "lib/watchdog.h"
#  include "lib/berrno.h"
#  include "lib/bsignal.h"

#  ifndef _NSIG
#    define BA_NSIG 100
#  else
#    define BA_NSIG _NSIG
#  endif

extern char my_name[];
extern char* exepath;
extern char* exename;
extern bool prt_kaboom;

static const char* sig_names[BA_NSIG + 1];

typedef void(SIG_HANDLER)(int sig);
static SIG_HANDLER* exit_handler;

/* main process id */
static pid_t main_pid = 0;

const char* get_signal_name(int sig)
{
  if (sig < 0 || sig > BA_NSIG || !sig_names[sig]) {
    return T_("Invalid signal number");
  } else {
    return sig_names[sig];
  }
}

/* defined in jcr.c */
extern void DbgPrintJcr(FILE* fp);
/* defined in plugins.c */
extern void DbgPrintPlugin(FILE* fp);

/*
 * !!! WARNING !!!
 *
 * This function should be used ONLY after a violent signal. We walk through the
 * JobControlRecord chain without locking, BAREOS should not be running.
 */
static void dbg_print_bareos()
{
  char buf[512];

  snprintf(buf, sizeof(buf), "%s/%s.%d.bactrace", working_directory, my_name,
           (int)getpid());
  FILE* fp = fopen(buf, "a+");
  if (!fp) { fp = stderr; }

  fprintf(stderr, "Dumping: %s\n", buf);

  /* Print also BareosDb and RWLOCK structure
   * Can add more info about JobControlRecord with DbgJcrAddHook()
   */
  DbgPrintJcr(fp);
  DbgPrintPlugin(fp);

  if (fp != stderr) {
    if (prt_kaboom) {
      rewind(fp);
      printf("\n\n ==== bactrace output ====\n\n");
      while (fgets(buf, (int)sizeof(buf), fp) != NULL) { printf("%s", buf); }
      printf(" ==== End baktrace output ====\n\n");
    }
    fclose(fp);
  }
}

// Handle signals here
extern "C" void SignalHandler(int sig)
{
  static int already_dead = 0;
  int chld_status = -1;

  // If we come back more than once, get out fast!
  if (already_dead) { exit(BEXIT_FAILURE); }
  Dmsg2(900, "sig=%d %s\n", sig, sig_names[sig]);

  // Ignore certain signals -- SIGUSR2 used to interrupt threads
  if (sig == SIGCHLD || sig == SIGUSR2) { return; }
  already_dead++;

  // Don't use Emsg here as it may lock and thus block us
  if (sig == SIGTERM) {
    syslog(LOG_DAEMON | LOG_ERR, "Shutting down BAREOS service: %s ...\n",
           my_name);
  } else {
    fprintf(stderr, T_("BAREOS interrupted by signal %d: %s\n"), sig,
            get_signal_name(sig));
    syslog(LOG_DAEMON | LOG_ERR, T_("BAREOS interrupted by signal %d: %s\n"),
           sig, get_signal_name(sig));
  }

  if (sig != SIGTERM) {
    struct sigaction sigdefault;
    static char* argv[5];
    static char pid_buf[20];
    static char btpath[400];
    char buf[400];
    pid_t pid;
    int exelen = strlen(exepath);

    fprintf(stderr, T_("%s, %s got signal %d - %s. Attempting traceback.\n"),
            exename, my_name, sig, get_signal_name(sig));
    fprintf(stderr, T_("exepath=%s\n"), exepath);

    if (exelen + 12 > (int)sizeof(btpath)) {
      bstrncpy(btpath, "btraceback", sizeof(btpath));
    } else {
      bstrncpy(btpath, exepath, sizeof(btpath));
      if (IsPathSeparator(btpath[exelen - 1])) { btpath[exelen - 1] = 0; }
      bstrncat(btpath, "/btraceback", sizeof(btpath));
    }
    if (!IsPathSeparator(exepath[exelen - 1])) { strcat(exepath, "/"); }
    strcat(exepath, exename);
    if (!working_directory) {
      working_directory = buf;
      *buf = 0;
    }
    if (*working_directory == 0) { strcpy((char*)working_directory, "/tmp/"); }
    if (chdir(working_directory) != 0) { /* dump in working directory */
      Pmsg2(000, "chdir to %s failed. ERR=%s\n", working_directory,
            strerror(errno));
      strcpy((char*)working_directory, "/tmp/");
    }
    SecureErase(NULL, "./core"); /* get rid of any old core file */

    snprintf(pid_buf, 20, "%d", (int)main_pid);
    Dmsg1(300, "Working=%s\n", working_directory);
    Dmsg1(300, "btpath=%s\n", btpath);
    Dmsg1(300, "exepath=%s\n", exepath);
    switch (pid = fork()) {
      case -1: /* error */
        fprintf(stderr, T_("Fork error: ERR=%s\n"), strerror(errno));
        break;
      case 0:              /* child */
        argv[0] = btpath;  /* path to btraceback */
        argv[1] = exepath; /* path to exe */
        argv[2] = pid_buf;
        argv[3] = (char*)working_directory;
        argv[4] = (char*)NULL;
        fprintf(stderr, T_("Calling: %s %s %s %s\n"), btpath, exepath, pid_buf,
                working_directory);
        if (execv(btpath, argv) != 0) {
          printf(T_("execv: %s failed: ERR=%s\n"), btpath, strerror(errno));
        }
        exit(-1);
      default: /* parent */
        break;
    }

    // Parent continue here, waiting for child
    sigdefault.sa_flags = 0;
    sigdefault.sa_handler = SIG_DFL;
    sigfillset(&sigdefault.sa_mask);

    sigaction(sig, &sigdefault, NULL);
    if (pid > 0) {
      Dmsg0(500, "Doing waitpid\n");
      waitpid(pid, &chld_status, 0); /* wait for child to produce dump */
      Dmsg0(500, "Done waitpid\n");
    } else {
      Dmsg0(500, "Doing sleep\n");
      Bmicrosleep(30, 0);
    }
    if (WEXITSTATUS(chld_status) == 0) {
      fprintf(stderr, T_("It looks like the traceback worked...\n"));
    } else {
      fprintf(stderr, T_("The btraceback call returned %d\n"),
              WEXITSTATUS(chld_status));
    }

    // If we want it printed, do so
    if (prt_kaboom) {
      FILE* fd;

      snprintf(buf, sizeof(buf), "%s/bareos.%s.traceback", working_directory,
               pid_buf);
      fd = fopen(buf, "r");
      if (fd != NULL) {
        printf("\n\n ==== Traceback output ====\n\n");
        while (fgets(buf, (int)sizeof(buf), fd) != NULL) { printf("%s", buf); }
        fclose(fd);
        printf(" ==== End traceback output ====\n\n");
      }
    }

    dbg_print_bareos();
  }
  exit_handler(sig);
  Dmsg0(500, "Done exit_handler\n");
}

/*
 * Init stack dump by saving main process id -- needed by debugger to attach to
 * this program.
 */
void InitStackDump(void) { main_pid = getpid(); /* save main thread's pid */ }

// Initialize signals
void InitSignals(void Terminate(int sig))
{
  struct sigaction sighandle;
  struct sigaction sigignore;
  struct sigaction sigdefault;
#  ifdef _sys_nsig
  int i;

  exit_handler = Terminate;
  if (BA_NSIG < _sys_nsig) {
    Emsg2(M_ABORT, 0, T_("BA_NSIG too small (%d) should be (%d)\n"), BA_NSIG,
          _sys_nsig);
  }

  for (i = 0; i < _sys_nsig; i++) { sig_names[i] = _sys_siglist[i]; }
#  else
  exit_handler = Terminate;
  sig_names[0] = T_("UNKNOWN SIGNAL");
  sig_names[SIGHUP] = T_("Hangup");
  sig_names[SIGINT] = T_("Interrupt");
  sig_names[SIGQUIT] = T_("Quit");
  sig_names[SIGILL] = T_("Illegal instruction");
  sig_names[SIGTRAP] = T_("Trace/Breakpoint trap");
  sig_names[SIGABRT] = T_("Abort");
#    ifdef SIGEMT
  sig_names[SIGEMT] = T_("EMT instruction (Emulation Trap)");
#    endif
#    ifdef SIGIOT
  sig_names[SIGIOT] = T_("IOT trap");
#    endif
  sig_names[SIGBUS] = T_("BUS error");
  sig_names[SIGFPE] = T_("Floating-point exception");
  sig_names[SIGKILL] = T_("Kill, unblockable");
  sig_names[SIGUSR1] = T_("User-defined signal 1");
  sig_names[SIGSEGV] = T_("Segmentation violation");
  sig_names[SIGUSR2] = T_("User-defined signal 2");
  sig_names[SIGPIPE] = T_("Broken pipe");
  sig_names[SIGALRM] = T_("Alarm clock");
  sig_names[SIGTERM] = T_("Termination");
#    ifdef SIGSTKFLT
  sig_names[SIGSTKFLT] = T_("Stack fault");
#    endif
  sig_names[SIGCHLD] = T_("Child status has changed");
  sig_names[SIGCONT] = T_("Continue");
  sig_names[SIGSTOP] = T_("Stop, unblockable");
  sig_names[SIGTSTP] = T_("Keyboard stop");
  sig_names[SIGTTIN] = T_("Background read from tty");
  sig_names[SIGTTOU] = T_("Background write to tty");
  sig_names[SIGURG] = T_("Urgent condition on socket");
  sig_names[SIGXCPU] = T_("CPU limit exceeded");
  sig_names[SIGXFSZ] = T_("File size limit exceeded");
  sig_names[SIGVTALRM] = T_("Virtual alarm clock");
  sig_names[SIGPROF] = T_("Profiling alarm clock");
  sig_names[SIGWINCH] = T_("Window size change");
  sig_names[SIGIO] = T_("I/O now possible");
#    ifdef SIGPWR
  sig_names[SIGPWR] = T_("Power failure restart");
#    endif
#    ifdef SIGWAITING
  sig_names[SIGWAITING] = T_("No runnable lwp");
#    endif
#    ifdef SIGLWP
  sig_names[SIGLWP] = T_("SIGLWP special signal used by thread library");
#    endif
#    ifdef SIGFREEZE
  sig_names[SIGFREEZE] = T_("Checkpoint Freeze");
#    endif
#    ifdef SIGTHAW
  sig_names[SIGTHAW] = T_("Checkpoint Thaw");
#    endif
#    ifdef SIGCANCEL
  sig_names[SIGCANCEL] = T_("Thread Cancellation");
#    endif
#    ifdef SIGLOST
  sig_names[SIGLOST] = T_("Resource Lost (e.g. record-lock lost)");
#    endif
#  endif

  // Now setup signal handlers
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
#  ifdef SIGEMT
  sigaction(SIGEMT, &sighandle, NULL);
#  endif
#  ifdef SIGIOT
  sigaction(SIGIOT, &sighandle, NULL);
#  endif
  sigaction(SIGBUS, &sighandle, NULL);
  sigaction(SIGFPE, &sighandle, NULL);
  sigaction(SIGUSR1, &sighandle, NULL);
  sigaction(SIGSEGV, &sighandle, NULL);
  sigaction(SIGUSR2, &sighandle, NULL);
  sigaction(SIGALRM, &sighandle, NULL);
  sigaction(SIGTERM, &sighandle, NULL);
#  ifdef SIGSTKFLT
  sigaction(SIGSTKFLT, &sighandle, NULL);
#  endif
  sigaction(SIGTSTP, &sigdefault, NULL);
  sigaction(SIGTTIN, &sighandle, NULL);
  sigaction(SIGTTOU, &sighandle, NULL);
  sigaction(SIGURG, &sighandle, NULL);
  sigaction(SIGVTALRM, &sighandle, NULL);
#  ifdef SIGPWR
  sigaction(SIGPWR, &sighandle, NULL);
#  endif
#  ifdef SIGWAITING
  sigaction(SIGWAITING, &sighandle, NULL);
#  endif
#  ifdef SIGLWP
  sigaction(SIGLWP, &sighandle, NULL);
#  endif
#  ifdef SIGFREEZE
  sigaction(SIGFREEZE, &sighandle, NULL);
#  endif
#  ifdef SIGTHAW
  sigaction(SIGTHAW, &sighandle, NULL);
#  endif
#  ifdef SIGCANCEL
  sigaction(SIGCANCEL, &sighandle, NULL);
#  endif
#  ifdef SIGLOST
  sigaction(SIGLOST, &sighandle, NULL);
#  endif
}

extern "C" void TimeoutHandler(int)
{
  return; /* thus interrupting the function */
}

void SetTimeoutHandler()
{
  struct sigaction sigtimer;
  sigtimer.sa_flags = 0;
  sigtimer.sa_handler = TimeoutHandler;
  sigfillset(&sigtimer.sa_mask);
  sigaction(kTimeoutSignal, &sigtimer, nullptr);
}
#else   // HAVE_WIN32
void SetTimeoutHandler() {}
#endif  // !HAVE_WIN32
