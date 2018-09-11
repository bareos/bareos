/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * bpipe.c bi-directional pipe
 *
 * Kern Sibbald, November MMII
 */

#include "include/bareos.h"
#include "jcr.h"
#include "lib/bsys.h"
#include "lib/btimers.h"

int execvp_errors[] = {
        EACCES,
        ENOEXEC,
        EFAULT,
        EINTR,
        E2BIG,
        ENAMETOOLONG,
        ENOMEM,
#ifndef HAVE_WIN32
        ETXTBSY,
#endif
        ENOENT
};
int num_execvp_errors = (int)(sizeof(execvp_errors)/sizeof(int));

#define MAX_ARGV 100

#if !defined(HAVE_WIN32)
static void BuildArgcArgv(char *cmd, int *bargc, char *bargv[], int max_arg);

/*
 * Run an external program. Optionally wait a specified number
 * of seconds. Program killed if wait exceeded. We open
 * a bi-directional pipe so that the user can read from and
 * write to the program.
 */
Bpipe *OpenBpipe(char *prog, int wait, const char *mode, bool dup_stderr)
{
   char *bargv[MAX_ARGV];
   int bargc, i;
   int readp[2], writep[2];
   POOLMEM *tprog;
   int mode_read, mode_write;
   Bpipe *bpipe;
   int save_errno;

   bpipe = (Bpipe *)malloc(sizeof(Bpipe));
   memset(bpipe, 0, sizeof(Bpipe));
   mode_read = (mode[0] == 'r');
   mode_write = (mode[0] == 'w' || mode[1] == 'w');

   /*
    * Build arguments for running program.
    */
   tprog = GetPoolMemory(PM_FNAME);
   PmStrcpy(tprog, prog);
   BuildArgcArgv(tprog, &bargc, bargv, MAX_ARGV);

   /*
    * Each pipe is one way, write one end, read the other, so we need two
    */
   if (mode_write && pipe(writep) == -1) {
      save_errno = errno;
      free(bpipe);
      FreePoolMemory(tprog);
      errno = save_errno;
      return NULL;
   }
   if (mode_read && pipe(readp) == -1) {
      save_errno = errno;
      if (mode_write) {
         close(writep[0]);
         close(writep[1]);
      }
      free(bpipe);
      FreePoolMemory(tprog);
      errno = save_errno;
      return NULL;
   }

   /*
    * Start worker process
    */
   switch (bpipe->worker_pid = fork()) {
   case -1:                           /* error */
      save_errno = errno;
      if (mode_write) {
         close(writep[0]);
         close(writep[1]);
      }
      if (mode_read) {
         close(readp[0]);
         close(readp[1]);
      }
      free(bpipe);
      FreePoolMemory(tprog);
      errno = save_errno;
      return NULL;

   case 0:                            /* child */
      if (mode_write) {
         close(writep[1]);
         dup2(writep[0], 0);          /* Dup our write to his stdin */
      }
      if (mode_read) {
         close(readp[0]);             /* Close unused child fds */
         dup2(readp[1], 1);           /* dup our read to his stdout */
         if (dup_stderr) {
            dup2(readp[1], 2);        /*   and his stderr */
         }
      }

#if defined(HAVE_FCNTL_F_CLOSEM)
      /*
       * fcntl(fd, F_CLOSEM) needs the lowest filedescriptor to close.
       */
      fcntl(3, F_CLOSEM);
#elif defined(HAVE_CLOSEFROM)
      /*
       * closefrom needs the lowest filedescriptor to close.
       */
      closefrom(3);
#else
      for (i = 3; i <= 32; i++) {         /* close any open file descriptors */
         close(i);
      }
#endif

      execvp(bargv[0], bargv);        /* call the program */

      /*
       * Convert errno into an exit code for later analysis
       */
      for (i = 0; i < num_execvp_errors; i++) {
         if (execvp_errors[i] == errno) {
            exit(200 + i);            /* exit code => errno */
         }
      }
      exit(255);                      /* unknown errno */

   default:                           /* parent */
      break;
   }

   FreePoolMemory(tprog);

   if (mode_read) {
      close(readp[1]);                /* close unused parent fds */
      bpipe->rfd = fdopen(readp[0], "r"); /* open file descriptor */
   }

   if (mode_write) {
      close(writep[0]);
      bpipe->wfd = fdopen(writep[1], "w");
   }

   bpipe->worker_stime = time(NULL);
   bpipe->wait = wait;

   if (wait > 0) {
      bpipe->timer_id = start_child_timer(NULL, bpipe->worker_pid, wait);
   }

   return bpipe;
}

/*
 * Close the write pipe only
 */
int CloseWpipe(Bpipe *bpipe)
{
   int status = 1;

   if (bpipe->wfd) {
      fflush(bpipe->wfd);
      if (fclose(bpipe->wfd) != 0) {
         status = 0;
      }
      bpipe->wfd = NULL;
   }
   return status;
}

/*
 * Close both pipes and free resources
 *
 * Returns: 0 on success
 *          BErrNo on failure
 */
int CloseBpipe(Bpipe *bpipe)
{
   int chldstatus = 0;
   int status = 0;
   int wait_option;
   int remaining_wait;
   pid_t wpid = 0;


   /*
    * Close pipes
    */
   if (bpipe->rfd) {
      fclose(bpipe->rfd);
      bpipe->rfd = NULL;
   }

   if (bpipe->wfd) {
      fclose(bpipe->wfd);
      bpipe->wfd = NULL;
   }

   if (bpipe->wait == 0) {
      wait_option = 0;                /* wait indefinitely */
   } else {
      wait_option = WNOHANG;          /* don't hang */
   }
   remaining_wait = bpipe->wait;

   /*
    * Wait for worker child to exit
    */
   for ( ;; ) {
      Dmsg2(800, "Wait for %d opt=%d\n", bpipe->worker_pid, wait_option);
      do {
         wpid = waitpid(bpipe->worker_pid, &chldstatus, wait_option);
      } while (wpid == -1 && (errno == EINTR || errno == EAGAIN));
      if (wpid == bpipe->worker_pid || wpid == -1) {
         BErrNo be;
         status = errno;
         Dmsg3(800, "Got break wpid=%d status=%d ERR=%s\n", wpid, chldstatus,
            wpid==-1?be.bstrerror():"none");
         break;
      }
      Dmsg3(800, "Got wpid=%d status=%d ERR=%s\n", wpid, chldstatus,
            wpid==-1?strerror(errno):"none");
      if (remaining_wait > 0) {
         Bmicrosleep(1, 0);           /* wait one second */
         remaining_wait--;
      } else {
         status = ETIME;              /* set error status */
         wpid = -1;
         break;                       /* don't wait any longer */
      }
   }

   if (wpid > 0) {
      if (WIFEXITED(chldstatus)) {    /* process exit()ed */
         status = WEXITSTATUS(chldstatus);
         if (status != 0) {
            Dmsg1(800, "Non-zero status %d returned from child.\n", status);
            status |= b_errno_exit;   /* exit status returned */
         }
         Dmsg1(800, "child status=%d\n", status & ~b_errno_exit);
      } else if (WIFSIGNALED(chldstatus)) {  /* process died */
         status = WTERMSIG(chldstatus);
         Dmsg1(800, "Child died from signal %d\n", status);
         status |= b_errno_signal;    /* exit signal returned */
      }
   }

   if (bpipe->timer_id) {
      StopChildTimer(bpipe->timer_id);
   }

   free(bpipe);
   Dmsg2(800, "returning status=%d,%d\n", status & ~(b_errno_exit|b_errno_signal), status);

   return status;
}

/*
 * Build argc and argv from a string
 */
static void BuildArgcArgv(char *cmd, int *bargc, char *bargv[], int max_argv)
{
   int i;
   char *p, *q, quote;
   int argc = 0;

   argc = 0;
   for (i=0; i<max_argv; i++)
      bargv[i] = NULL;

   p = cmd;
   quote = 0;
   while  (*p && (*p == ' ' || *p == '\t'))
      p++;
   if (*p == '\"' || *p == '\'') {
      quote = *p;
      p++;
   }
   if (*p) {
      while (*p && argc < MAX_ARGV) {
         q = p;
         if (quote) {
            while (*q && *q != quote)
            q++;
            quote = 0;
         } else {
            while (*q && *q != ' ')
            q++;
         }
         if (*q)
            *(q++) = '\0';
         bargv[argc++] = p;
         p = q;
         while (*p && (*p == ' ' || *p == '\t'))
            p++;
         if (*p == '\"' || *p == '\'') {
            quote = *p;
            p++;
         }
      }
   }
   *bargc = argc;
}
#endif /* HAVE_WIN32 */

/*
 * Run an external program. Optionally wait a specified number
 * of seconds. Program killed if wait exceeded. Optionally
 * return the output from the program (normally a single line).
 *
 * If the watchdog kills the program, fgets returns, and ferror is set
 * to 1 (=>SUCCESS), so we check if the watchdog killed the program.
 *
 * Contrary to my normal calling conventions, this program
 *
 * Returns: 0 on success
 *          non-zero on error == BErrNo status
 */
int RunProgram(char *prog, int wait, POOLMEM *&results)
{
   Bpipe *bpipe;
   int stat1, stat2;
   char *mode;

   mode = (char *)"r";
   bpipe = OpenBpipe(prog, wait, mode);
   if (!bpipe) {
      return ENOENT;
   }

   results[0] = 0;
   int len = SizeofPoolMemory(results) - 1;
   fgets(results, len, bpipe->rfd);
   results[len] = 0;

   if (feof(bpipe->rfd)) {
      stat1 = 0;
   } else {
      stat1 = ferror(bpipe->rfd);
   }

   if (stat1 < 0) {
      BErrNo be;
      Dmsg2(150, "Run program fgets stat=%d ERR=%s\n", stat1, be.bstrerror(errno));
   } else if (stat1 != 0) {
      Dmsg1(150, "Run program fgets stat=%d\n", stat1);
      if (bpipe->timer_id) {
         Dmsg1(150, "Run program fgets killed=%d\n", bpipe->timer_id->killed);
         /*
          * NB: I'm not sure it is really useful for RunProgram. Without the
          * following lines RunProgram would not detect if the program was killed
          * by the watchdog.
          */
         if (bpipe->timer_id->killed) {
            stat1 = ETIME;
            PmStrcpy(results, _("Program killed by BAREOS (timeout)\n"));
         }
      }
   }
   stat2 = CloseBpipe(bpipe);
   stat1 = stat2 != 0 ? stat2 : stat1;
   Dmsg1(150, "Run program returning %d\n", stat1);

   return stat1;
}

/*
 * Run an external program. Optionally wait a specified number
 * of seconds. Program killed if wait exceeded (it is done by the
 * watchdog, as fgets is a blocking function).
 *
 * If the watchdog kills the program, fgets returns, and ferror is set
 * to 1 (=>SUCCESS), so we check if the watchdog killed the program.
 *
 * Return the full output from the program (not only the first line).
 *
 * Contrary to my normal calling conventions, this program
 *
 * Returns: 0 on success
 *          non-zero on error == BErrNo status
 */
int RunProgramFullOutput(char *prog, int wait, POOLMEM *&results)
{
   Bpipe *bpipe;
   int stat1, stat2;
   char *mode;
   POOLMEM* tmp;
   char *buf;
   const int bufsize = 32000;


   Dsm_check(200);

   tmp = GetPoolMemory(PM_MESSAGE);
   buf = (char *)malloc(bufsize+1);

   results[0] = 0;
   mode = (char *)"r";
   bpipe = OpenBpipe(prog, wait, mode);
   if (!bpipe) {
      stat1 = ENOENT;
      goto bail_out;
   }

   Dsm_check(200);
   tmp[0] = 0;
   while (1) {
      buf[0] = 0;
      fgets(buf, bufsize, bpipe->rfd);
      buf[bufsize] = 0;
      PmStrcat(tmp, buf);
      if (feof(bpipe->rfd)) {
         stat1 = 0;
         Dmsg1(900, "Run program fgets stat=%d\n", stat1);
         break;
      } else {
         stat1 = ferror(bpipe->rfd);
      }
      if (stat1 < 0) {
         BErrNo be;
         Dmsg2(200, "Run program fgets stat=%d ERR=%s\n", stat1, be.bstrerror());
         break;
      } else if (stat1 != 0) {
         Dmsg1(900, "Run program fgets stat=%d\n", stat1);
         if (bpipe->timer_id && bpipe->timer_id->killed) {
            Dmsg1(250, "Run program saw fgets killed=%d\n", bpipe->timer_id->killed);
            break;
         }
      }
   }

   /*
    * We always check whether the timer killed the program. We would see
    * an eof even when it does so we just have to trust the killed flag
    * and set the timer values to avoid edge cases where the program ends
    * just as the timer kills it.
    */
   if (bpipe->timer_id && bpipe->timer_id->killed) {
      Dmsg1(150, "Run program fgets killed=%d\n", bpipe->timer_id->killed);
      PmStrcpy(tmp, _("Program killed by BAREOS (timeout)\n"));
      stat1 = ETIME;
   }

   PmStrcpy(results, tmp);
   Dmsg3(1900, "resadr=0x%x reslen=%d res=%s\n", results, strlen(results), results);
   stat2 = CloseBpipe(bpipe);
   stat1 = stat2 != 0 ? stat2 : stat1;

   Dmsg1(900, "Run program returning %d\n", stat1);
bail_out:
   FreePoolMemory(tmp);
   free(buf);
   return stat1;
}
