/*
Subject: Re: send mail from chrooted Apache
 From: Wietse Venema (wietseporcupine.org)
 Date: Tue Jun 06 2000 - 07:43:31 CDT
Next message: Wietse Venema: "Re: -"
Previous message: Brad Knowles: "Re: performance issues"
In reply to: ISM Kolemanov, Ivan: "send mail from chrooted Apache"
 ISM Kolemanov, Ivan:
 > hi all,
 >
 > I've just prepared Apache Web Server in chroot env.
 > but now I have a problem, I can't send mails from the web server
 > I mean that on the web server I have some formulars,
 > which my clients are using to send me mail.
 >
 > I guess that there is any solution for that, but up to now I didn't find it.
 > If anybody knows it please respond.
 >
 > 10x in advance
 > Ivan Kolemanov

This is a quick-and-dirty stand-alone SMTP client from long ago.
 Works OK for lines up to BUFSIZ characters, nothing to worry about
 security-wise. You could probably do the same in PERL, but I wrote
 this for a web server that allowed no programming languages inside
 the jail.

        Wietse
 */
/*++
 /* NAME
 /* smtp 1
 /* SUMMARY
 /* simple smtp client
 /* SYNOPSIS
 /* smtp [options] recipient(s)...
 /* DESCRIPTION
 /* \fIsmtp\fP is a minimal smtp client that takes an email
 /* message body and passes it on to an smtp daemon (default
 /* the smtp daemon on the local host). Since it is
 /* completely self-supporting, the smtp client is especially
 /* suitable for use in restricted environments.
 /*
 /* Options:
 /* .TP
 /* -c carbon-copy
 /* Specifies one Cc: address to send one copy of the message to.
 /* .TP
 /* -e errors-to
 /* Specifies the Errors-To: address. This is where delivery
 /* problems should be reported.
 /* .TP
 /* -f from
 /* Sets the From: address. Default is "daemon", which is
 /* probably wrong.
 /* .TP
 /* -m mailhost
 /* Specifies where the mail should be posted. By default, the
 /* mail is posted to the smtp daemon on \fIlocalhost\fR.
 /* .TP
 /* -M
 /* Use MIME-style translation to quoted-printable (base 16).
 /* .TP
 /* -r reply-to
 /* Sets the Reply-To: address.
 /* .TP
 /* -s subject
 /* Specifies the message subject.
 /* .TP
 /* -v
 /* Turn on verbose logging to stdout.
 /* DIAGNOSTICS
 /* Non-zero exit status in case of problems. Errors are reported
 /* to the syslogd, with facility daemon.
 /* AUTHOR(S)
 /* W.Z. Venema
 /* Eindhoven University of Technology
 /* Department of Mathematics and Computer Science
 /* Den Dolech 2, P.O. Box 513, 5600 MB Eindhoven, The Netherlands
 /* CREATION DATE
 /* Wed Dec 1 14:51:13 MET 1993
 /* LAST UPDATE
 /* Fri Aug 11 12:29:23 MET DST 1995
 /*--*/

/* System libraries */

#include <sys/types.h>
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <syslog.h>
 #include <stdio.h>
 #include <netdb.h>
 #include <string.h>
 #include <ctype.h>
 #include <pwd.h>

#ifdef __STDC__
 #include <stdarg.h>
 #define VARARGS(func,arg,type) func(type arg, ...)
 #define VASTART(ap,name,type) { va_start(ap, name)
 #define VAEND(ap) va_end(ap); }
 #else
 #include <varargs.h>
 #define VARARGS(func,arg,type) func(va_alist) \
                                 va_dcl
 #define VASTART(ap,name,type) { type name; \
                                 va_start(ap); \
                                 name = va_arg(ap, type)
 #define VAEND(ap) va_end(ap); }
 #endif /* __STDC__ */

extern int optind;
 extern char *optarg;

/* Local stuff */

static char *cc_addr = 0;
 static char *err_addr = 0;
 static char *from_addr = "daemon";
 static char *mailhost = "localhost";
 static char *reply_addr = 0;
 static char *subject = 0;
 static int mime_style = 0;
 static int verbose = 0;

static FILE *sfp;
 static FILE *rfp;

#define dprintf if (verbose) printf
 #define dvprintf if (verbose) vprintf

void toqp();

/* usage - explain and bail out */

void usage()
 {
     syslog(LOG_ERR,
            "usage: smtp [-c cc] [-e errors-to] [-f from] [-m mailhost] [-M] [-r reply-to] [-s subject] [-v] recipents..\n");
     exit(1);
 }

/* GetResponse - examine message from server */

void GetResponse()
 {
     char buf[BUFSIZ];

    while (fgets(buf, sizeof(buf), rfp)) {
         buf[strlen(buf) - 1] = 0;
         dprintf(">>>> %s\n", buf);
         if (!isdigit(buf[0]) || buf[0] > '3') {
             syslog(LOG_ERR, "unexpected reply: %s", buf);
             exit(1);
         }
         if (buf[4] != '-')
             break;
     }
 }

/* chat - say something to server and check the response */

void VARARGS(chat, fmt, char *)
 {
     va_list ap;

    /* Format the message. */

    VASTART(ap, fmt, char *);
     vfprintf(sfp, fmt, ap);
     VAEND(ap);

    VASTART(ap, fmt, char *);
     dvprintf(fmt, ap);
     VAEND(ap);

    /* Send message to server and parse its response. */

    fflush(sfp);
     GetResponse();
 }

main(argc, argv)
 int argc;
 char **argv;
 {
     char buf[BUFSIZ];
     char my_name[BUFSIZ];
     struct sockaddr_in sin;
     struct hostent *hp;
     struct servent *sp;
     int c;
     int s;
     int r;
     int i;
     struct passwd *pwd;

    openlog(argv[0], LOG_PID, LOG_DAEMON);

    /* Go away when something gets stuck. */

    alarm(60);

    /* Parse JCL. */

    while ((c = getopt(argc, argv, "c:e:f:m:Mr:s:v")) != EOF) {
         switch (c) {
         case 'c': /* carbon copy */
             cc_addr = optarg;
             break;
         case 'e': /* errors-to */
             err_addr = optarg;
             break;
         case 'f': /* originator */
             from_addr = optarg;
             break;
         case 'm': /* mailhost */
             mailhost = optarg;
             break;
         case 'M': /* MIME quoted printable */
             mime_style = 1;
             break;
         case 'r': /* reply-to */
             reply_addr = optarg;
             break;
         case 's': /* subject */
             subject = optarg;
             break;
         case 'v': /* log protocol */
             verbose = 1;
             break;
         default:
             usage();
             /* NOTREACHED */
         }
     }
     if (argc == optind)
         usage();

    /* Find out my own host name for HELO; if possible, get the FQDN. */

    if (gethostname(my_name, sizeof(my_name) - 1) < 0) {
         syslog(LOG_ERR, "gethostname: %m");
         exit(1);
     }
     if ((hp = gethostbyname(my_name)) == 0) {
         syslog(LOG_ERR, "%s: unknown host\n", my_name);
         exit(1);
     }
     strncpy(my_name, hp->h_name, sizeof(my_name) - 1);

    /* Connect to smtp daemon on mailhost. */

    if ((hp = gethostbyname(mailhost)) == 0) {
         syslog(LOG_ERR, "%s: unknown host\n", mailhost);
         exit(1);
     }
     if (hp->h_addrtype != AF_INET) {
         syslog(LOG_ERR, "unknown address family: %d", hp->h_addrtype);
         exit(1);
     }
     memset((char *) &sin, 0, sizeof(sin));
     memcpy((char *) &sin.sin_addr, hp->h_addr, hp->h_length);
     sin.sin_family = hp->h_addrtype;
     if ((sp = getservbyname("smtp", "tcp")) != 0) {
         sin.sin_port = sp->s_port;
     } else {
         syslog(LOG_ERR, "smtp/tcp: unknown service");
         exit(1);
     }
     if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
         syslog(LOG_ERR, "socket: %m");
         exit(1);
     }
     if (connect(s, (struct sockaddr *) & sin, sizeof(sin)) < 0) {
         syslog(LOG_ERR, "connect: %m");
         exit(1);
     }
     if ((r = dup(s)) < 0) {
         syslog(LOG_ERR, "dup: %m");
         exit(1);
     }
     if ((sfp = fdopen(s, "w")) == 0) {
         syslog(LOG_ERR, "fdopen: %m");
         exit(1);
     }
     if ((rfp = fdopen(r, "r")) == 0) {
         syslog(LOG_ERR, "fdopen: %m");
         exit(1);
     }
     /* Speak SMTP. */

    GetResponse(); /* banner */

    chat("HELO %s\r\n", my_name);

    chat("MAIL FROM: <%s>\r\n", from_addr);

    for (i = optind; i < argc; i++)
         chat("RCPT TO: <%s>\r\n", argv[i]);
     if (cc_addr)
         chat("RCPT TO: <%s>\r\n", cc_addr);

    chat("DATA\r\n");

    /* Do message header. */

    fprintf(sfp, "From: %s\r\n", from_addr);

    if (subject)
         fprintf(sfp, "Subject: %s\r\n", subject);

    if (err_addr)
         fprintf(sfp, "Errors-To: %s\r\n", err_addr);

    if (reply_addr)
         fprintf(sfp, "Reply-To: %s\r\n", reply_addr);

    if ((pwd = getpwuid(getuid())) == 0) {
         fprintf(sfp, "Sender: userid-%d%s\r\n", getuid(), my_name);
     } else {
         fprintf(sfp, "Sender: %s%s\r\n", pwd->pw_name, my_name);
     }

    fprintf(sfp, "To: %s", argv[optind]);
     for (i = optind + 1; i < argc; i++)
         fprintf(sfp, ", %s", argv[i]);
     fprintf(sfp, "\r\n");

    if (cc_addr)
         fprintf(sfp, "Cc: %s\r\n", cc_addr);

    if (mime_style) {
         fprintf(sfp, "Mime-Version: 1.0\r\n");
         fprintf(sfp, "Content-Type: text/plain; charset=ISO-8859-1\r\n");
         fprintf(sfp, "Content-Transfer-Encoding: quoted-printable\r\n");
     }
     fprintf(sfp, "\r\n");

    /* Do message body. */

    if (mime_style) { /* MIME quoted-printable */
         toqp(stdin, sfp);
     } else { /* traditional... */
         while (fgets(buf, sizeof(buf), stdin)) {
             buf[strlen(buf) - 1] = 0;
             if (strcmp(buf, ".") == 0) { /* quote lone dots */
                 fprintf(sfp, "..\r\n");
             } else { /* pass thru mode */
                 fprintf(sfp, "%s\r\n", buf);
             }
         }
     }
     chat(".\r\n");
     chat("QUIT\r\n");
     exit(0);
 }

 /*
   * Following code was lifted from the metamail version 2.7 source code
   * (codes.c) and modified to emit \r\n at line boundaries.
   */

static char basis_hex[] = "0123456789ABCDEF";

/* toqp - transform to MIME-style quoted printable */

void toqp(infile, outfile)
 FILE *infile,
        *outfile;
 {
     int c,
             ct = 0,
             prevc = 255;

    while ((c = getc(infile)) != EOF) {
         if ((c < 32 && (c != '\n' && c != '\t'))
             || (c == '=')
             || (c >= 127)

        /*
          * Following line is to avoid single periods alone on lines, which
          * messes up some dumb smtp implementations, sigh...
          */
             || (ct == 0 && c == '.')) {
             putc('=', outfile);
             putc(basis_hex[c >> 4], outfile);
             putc(basis_hex[c & 0xF], outfile);
             ct += 3;
             prevc = 'A'; /* close enough */
         } else if (c == '\n') {
             if (prevc == ' ' || prevc == '\t') {
                 putc('=', outfile); /* soft & hard lines */
                 putc(c, outfile);
             }
             putc(c, outfile);
             ct = 0;
             prevc = c;
         } else {
             if (c == 'F' && prevc == '\n') {

                /*
                  * HORRIBLE but clever hack suggested by MTR for
                  * sendmail-avoidance
                  */
                 c = getc(infile);
                 if (c == 'r') {
                     c = getc(infile);
                     if (c == 'o') {
                         c = getc(infile);
                         if (c == 'm') {
                             c = getc(infile);
                             if (c == ' ') {
                                 /* This is the case we are looking for */
                                 fputs("=46rom", outfile);
                                 ct += 6;
                             } else {
                                 fputs("From", outfile);
                                 ct += 4;
                             }
                         } else {
                             fputs("Fro", outfile);
                             ct += 3;
                         }
                     } else {
                         fputs("Fr", outfile);
                         ct += 2;
                     }
                 } else {
                     putc('F', outfile);
                     ++ct;
                 }
                 ungetc(c, infile);
                 prevc = 'x'; /* close enough -- printable */
             } else { /* END horrible hack */
                 putc(c, outfile);
                 ++ct;
                 prevc = c;
             }
         }
         if (ct > 72) {
             putc('=', outfile);
             putc('\r', outfile); /* XXX */
             putc('\n', outfile);
             ct = 0;
             prevc = '\n';
         }
     }
     if (ct) {
         putc('=', outfile);
         putc('\r', outfile); /* XXX */
         putc('\n', outfile);
     }
 }
