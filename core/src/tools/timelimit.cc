/*-
 * Copyright (c) 2001, 2007 - 2010  Peter Pentchev
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


/* we hope all OS's have those..*/
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>

#ifdef HAVE_ERR
#include <err.h>
#endif /* HAVE_ERR */

#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#else
#define EX_OK           0       /* successful termination */
#define EX__BASE        64      /* base value for error messages */
#define EX_USAGE        64      /* command line usage error */
#define EX_DATAERR      65      /* data format error */
#define EX_NOINPUT      66      /* cannot open input */
#define EX_NOUSER       67      /* addressee unknown */
#define EX_NOHOST       68      /* host name unknown */
#define EX_UNAVAILABLE  69      /* service unavailable */
#define EX_SOFTWARE     70      /* internal software error */
#define EX_OSERR        71      /* system error (e.g., can't fork) */
#define EX_OSFILE       72      /* critical OS file missing */
#define EX_CANTCREAT    73      /* can't create (user) output file */
#define EX_IOERR        74      /* input/output error */
#define EX_TEMPFAIL     75      /* temp failure; user is invited to retry */
#define EX_PROTOCOL     76      /* remote error in protocol */
#define EX_NOPERM       77      /* permission denied */
#define EX_CONFIG       78      /* configuration error */
#define EX__MAX 78      /* maximum listed value */
#endif /* HAVE_SYSEXITS_H */

#ifndef __unused
#ifdef __GNUC__
#define __unused __attribute__((unused))
#else  /* __GNUC__ */
#define __unused
#endif /* __GNUC__ */
#endif /* __unused */

#ifndef __dead2
#ifdef __GNUC__
#define __dead2 __attribute__((noreturn))
#else  /* __GNUC__ */
#define __dead2
#endif /* __GNUC__ */
#endif /* __dead2 */

#define PARSE_CMDLINE

unsigned long   warntime, warnmsec, killtime, killmsec;
unsigned long   warnsig, killsig;
volatile int    fdone, falarm, fsig, sigcaught;
int             propagate, quiet;

static struct {
        const char      *name, opt, issig;
        unsigned long   *sec, *msec;
} envopts[] = {
        {"KILLSIG",     'S',    1,      &killsig, NULL},
        {"KILLTIME",    'T',    0,      &killtime, &killmsec},
        {"WARNSIG",     's',    1,      &warnsig, NULL},
        {"WARNTIME",    't',    0,      &warntime, &warnmsec},
        {NULL,          0,      0,      NULL, NULL}
};

static struct {
        const char      *name;
        int              num;
} signals[] = {
        /* We kind of assume that the POSIX-mandated signals are present */
        {"ABRT",        SIGABRT},
        {"ALRM",        SIGALRM},
        {"BUS",         SIGBUS},
        {"CHLD",        SIGCHLD},
        {"CONT",        SIGCONT},
        {"FPE",         SIGFPE},
        {"HUP",         SIGHUP},
        {"ILL",         SIGILL},
        {"INT",         SIGINT},
        {"KILL",        SIGKILL},
        {"PIPE",        SIGPIPE},
        {"QUIT",        SIGQUIT},
        {"SEGV",        SIGSEGV},
        {"STOP",        SIGSTOP},
        {"TERM",        SIGTERM},
        {"TSTP",        SIGTSTP},
        {"TTIN",        SIGTTIN},
        {"TTOU",        SIGTTOU},
        {"USR1",        SIGUSR1},
        {"USR2",        SIGUSR2},
        {"PROF",        SIGPROF},
        {"SYS",         SIGSYS},
        {"TRAP",        SIGTRAP},
        {"URG",         SIGURG},
        {"VTALRM",      SIGVTALRM},
        {"XCPU",        SIGXCPU},
        {"XFSZ",        SIGXFSZ},

        /* Some more signals found on a Linux 2.6 system */
#ifdef SIGIO
        {"IO",          SIGIO},
#endif
#ifdef SIGIOT
        {"IOT",         SIGIOT},
#endif
#ifdef SIGLOST
        {"LOST",        SIGLOST},
#endif
#ifdef SIGPOLL
        {"POLL",        SIGPOLL},
#endif
#ifdef SIGPWR
        {"PWR",         SIGPWR},
#endif
#ifdef SIGSTKFLT
        {"STKFLT",      SIGSTKFLT},
#endif
#ifdef SIGWINCH
        {"WINCH",       SIGWINCH},
#endif

        /* Some more signals found on a FreeBSD 8.x system */
#ifdef SIGEMT
        {"EMT",         SIGEMT},
#endif
#ifdef SIGINFO
        {"INFO",        SIGINFO},
#endif
#ifdef SIGLWP
        {"LWP",         SIGLWP},
#endif
#ifdef SIGTHR
        {"THR",         SIGTHR},
#endif
};
#define SIGNALS (sizeof(signals) / sizeof(signals[0]))

#ifndef HAVE_ERR
static void     err(int, const char *, ...);
static void     errx(int, const char *, ...);
#endif /* !HAVE_ERR */

static void     Usage(void);

static void     init(int, char *[]);
static pid_t    doit(char *[]);
static void     child(char *[]);
static void     raisesignal(int) __dead2;
static void     setsig_fatal(int, void (*)(int));
static void     setsig_fatal_gen(int, void (*)(int), int, const char *);
static void     terminated(const char *);

#ifndef HAVE_ERR
static void
err(int code, const char *fmt, ...) {
        va_list v;

        va_start(v, fmt);
        vfprintf(stderr, fmt, v);
        va_end(v);

        fprintf(stderr, ": %s\n", strerror(errno));
        exit(code);
}

static void
errx(int code, const char *fmt, ...) {
        va_list v;

        va_start(v, fmt);
        vfprintf(stderr, fmt, v);
        va_end(v);

        fprintf(stderr, "\n");
        exit(code);
}

static void
warnx(const char *fmt, ...) {
        va_list v;

        va_start(v, fmt);
        vfprintf(stderr, fmt, v);
        va_end(v);

        fprintf(stderr, "\n");
}
#endif /* !HAVE_ERR */

static void
Usage(void) {
        errx(EX_USAGE, "usage: timelimit [-pq] [-S ksig] [-s wsig] "
            "[-T ktime] [-t wtime] command");
}

static void
atou_fatal(const char *s, unsigned long *sec, unsigned long *msec, int issig) {
        unsigned long v, vm, mul;
        const char *p;
        size_t i;

        if (s[0] < '0' || s[0] > '9') {
                if (s[0] == '\0' || !issig)
                        Usage();
                for (i = 0; i < SIGNALS; i++)
                        if (!strcmp(signals[i].name, s))
                                break;
                if (i == SIGNALS)
                        Usage();
                *sec = (unsigned long)signals[i].num;
                if (msec != NULL)
                        *msec = 0;
                return;
        }

        v = 0;
        for (p = s; (*p >= '0') && (*p <= '9'); p++)
                v = v * 10 + *p - '0';
        if (*p == '\0') {
                *sec = v;
                if (msec != NULL)
                        *msec = 0;
                return;
        } else if (*p != '.' || msec == NULL) {
                Usage();
        }
        p++;

        vm = 0;
        mul = 1000000;
        for (; (*p >= '0') && (*p <= '9'); p++) {
                vm = vm * 10 + *p - '0';
                mul = mul / 10;
        }
        if (*p != '\0')
                Usage();
        else if (mul < 1)
                errx(EX_USAGE, "no more than microsecond precision");
#ifndef HAVE_SETITIMER
        if (msec != 0)
                errx(EX_UNAVAILABLE,
                    "subsecond precision not supported on this platform");
#endif
        *sec = v;
        *msec = vm * mul;
}

static void
init(int argc, char *argv[]) {
#ifdef PARSE_CMDLINE
        int ch, listsigs;
#endif
        int optset;
        unsigned i;
        char *s;

        /* defaults */
        quiet = 0;
        warnsig = SIGTERM;
        killsig = SIGKILL;
        warntime = 900;
        warnmsec = 0;
        killtime = 5;
        killmsec = 0;

        optset = 0;

        /* process environment variables first */
        for (i = 0; envopts[i].name != NULL; i++)
                if ((s = getenv(envopts[i].name)) != NULL) {
                        atou_fatal(s, envopts[i].sec, envopts[i].msec,
                            envopts[i].issig);
                        optset = 1;
                }

#ifdef PARSE_CMDLINE
        listsigs = 0;
        while ((ch = getopt(argc, argv, "+lqpS:s:T:t:")) != -1) {
                switch (ch) {
                        case 'l':
                                listsigs = 1;
                                break;
                        case 'p':
                                propagate = 1;
                                break;
                        case 'q':
                                quiet = 1;
                                break;
                        default:
                                /* check if it's a recognized option */
                                for (i = 0; envopts[i].name != NULL; i++)
                                        if (ch == envopts[i].opt) {
                                                atou_fatal(optarg,
                                                    envopts[i].sec,
                                                    envopts[i].msec,
                                                    envopts[i].issig);
                                                optset = 1;
                                                break;
                                        }
                                if (envopts[i].name == NULL)
                                        Usage();
                }
        }

        if (listsigs) {
                for (i = 0; i < SIGNALS; i++)
                        printf("%s%c", signals[i].name,
                            i + 1 < SIGNALS? ' ': '\n');
                exit(EX_OK);
        }
#else
        optind = 1;
#endif

        if (!optset) /* && !quiet? */
                warnx("using defaults: warntime=%lu, warnsig=%lu, "
                    "killtime=%lu, killsig=%lu",
                    warntime, warnsig, killtime, killsig);

        argc -= optind;
        argv += optind;
        if (argc == 0)
                Usage();

        /* sanity checks */
        if ((warntime == 0 && warnmsec == 0) || (killtime == 0 && killmsec == 0))
                Usage();
}

static void
sigchld(int sig __unused) {

        fdone = 1;
}

static void
sigalrm(int sig __unused) {

        falarm = 1;
}

static void
sighandler(int sig) {

        sigcaught = sig;
        fsig = 1;
}

static void
setsig_fatal(int sig, void (*handler)(int)) {

        setsig_fatal_gen(sig, handler, 1, "setting");
}

static void
setsig_fatal_gen(int sig, void (*handler)(int), int nocld, const char *what) {
#ifdef HAVE_SIGACTION
        struct sigaction act;

        memset(&act, 0, sizeof(act));
        act.sa_handler = handler;
        act.sa_flags = 0;
#ifdef SA_NOCLDSTOP
        if (nocld)
                act.sa_flags |= SA_NOCLDSTOP;
#endif /* SA_NOCLDSTOP */
        if (sigaction(sig, &act, NULL) < 0)
                err(EX_OSERR, "%s signal handler for %d", what, sig);
#else  /* HAVE_SIGACTION */
        if (signal(sig, handler) == SIG_ERR)
                err(EX_OSERR, "%s signal handler for %d", what, sig);
#endif /* HAVE_SIGACTION */
}

static void
settimer(const char *name, unsigned long sec, unsigned long msec)
{
#ifdef HAVE_SETITIMER
        struct itimerval tval;

        tval.it_interval.tv_sec = tval.it_interval.tv_usec = 0;
        tval.it_value.tv_sec = sec;
        tval.it_value.tv_usec = msec;
        if (setitimer(ITIMER_REAL, &tval, NULL) == -1)
                err(EX_OSERR, "could not set the %s timer", name);
#else
        alarm(sec);
#endif
}

static pid_t
doit(char *argv[]) {
        pid_t pid;

        /* install signal handlers */
        fdone = falarm = fsig = sigcaught = 0;
        setsig_fatal(SIGALRM, sigalrm);
        setsig_fatal(SIGCHLD, sigchld);
        setsig_fatal(SIGTERM, sighandler);
        setsig_fatal(SIGHUP, sighandler);
        setsig_fatal(SIGINT, sighandler);
        setsig_fatal(SIGQUIT, sighandler);

        /* fork off the child process */
        if ((pid = fork()) < 0)
                err(EX_OSERR, "fork");
        if (pid == 0)
                child(argv);

        /* sleep for the allowed time */
        settimer("warning", warntime, warnmsec);
        while (!(fdone || falarm || fsig))
                pause();
        alarm(0);

        /* send the warning signal */
        if (fdone)
                return (pid);
        if (fsig)
                terminated("run");
        falarm = 0;
        if (!quiet)
                warnx("sending warning signal %lu", warnsig);
        kill(pid, (int) warnsig);

#ifndef HAVE_SIGACTION
        /* reset our signal handlers, just in case */
        setsig_fatal(SIGALRM, sigalrm);
        setsig_fatal(SIGCHLD, sigchld);
        setsig_fatal(SIGTERM, sighandler);
        setsig_fatal(SIGHUP, sighandler);
        setsig_fatal(SIGINT, sighandler);
        setsig_fatal(SIGQUIT, sighandler);
#endif /* HAVE_SIGACTION */

        /* sleep for the grace time */
        settimer("kill", killtime, killmsec);
        while (!(fdone || falarm || fsig))
                pause();
        alarm(0);

        /* send the kill signal */
        if (fdone)
                return (pid);
        if (fsig)
                terminated("grace");
        if (!quiet)
                warnx("sending kill signal %lu", killsig);
        kill(pid, (int) killsig);
        setsig_fatal_gen(SIGCHLD, SIG_DFL, 0, "restoring");
        return (pid);
}

static void
terminated(const char *period) {

        errx(EX_SOFTWARE, "terminated by signal %d during the %s period",
            sigcaught, period);
}

static void
child(char *argv[]) {

        execvp(argv[0], argv);
        err(EX_OSERR, "executing %s", argv[0]);
}

static __dead2 void
raisesignal (int sig) {

        setsig_fatal_gen(sig, SIG_DFL, 0, "restoring");
        raise(sig);
        while (1)
                pause();
        /* NOTREACHED */
}

int
main(int argc, char *argv[]) {
        pid_t pid;
        int status;

        init(argc, argv);
        argc -= optind;
        argv += optind;
        pid = doit(argv);

        if (waitpid(pid, &status, 0) == -1)
                err(EX_OSERR, "could not get the exit status for process %ld",
                    (long)pid);
        if (WIFEXITED(status))
                return (WEXITSTATUS(status));
        else if (!WIFSIGNALED(status))
                return (EX_OSERR);
        if (propagate)
                raisesignal(WTERMSIG(status));
        else
                return (WTERMSIG(status) + 128);
}
