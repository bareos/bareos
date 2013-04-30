/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

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
 * Bacula message handling routines
 *
 * NOTE: don't use any Jmsg or Qmsg calls within this file,
 *   except in q_msg or j_msg (setup routines), 
 *   otherwise you may get into recursive calls if there are
 *   errors, and that can lead to looping or deadlocks.
 *
 *   Kern Sibbald, April 2000
 *
 */

#include "bacula.h"
#include "jcr.h"

sql_query_func p_sql_query = NULL;
sql_escape_func p_sql_escape = NULL;

#define FULL_LOCATION 1               /* set for file:line in Debug messages */

/*
 *  This is where we define "Globals" because all the
 *    daemons include this file.
 */
const char *working_directory = NULL;       /* working directory path stored here */
const char *assert_msg = (char *)NULL; /* ASSERT2 error message */
int verbose = 0;                      /* increase User messages */
int debug_level = 0;                  /* debug level */
bool dbg_timestamp = false;           /* print timestamp in debug output */
bool prt_kaboom = false;              /* Print kaboom output */
utime_t daemon_start_time = 0;        /* Daemon start time */
const char *version = VERSION " (" BDATE ")";
const char *dist_name = DISTNAME " " DISTVER;
int beef = BEEF;
char my_name[30] = {0};               /* daemon name is stored here */
char host_name[50] = {0};             /* host machine name */
char *exepath = (char *)NULL;
char *exename = (char *)NULL;
int console_msg_pending = false;
char con_fname[500];                  /* Console filename */
FILE *con_fd = NULL;                  /* Console file descriptor */
brwlock_t con_lock;                   /* Console lock structure */

/* Forward referenced functions */

/* Imported functions */
void create_jcr_key();

/* Static storage */

/* Exclude spaces but require .mail at end */
#define MAIL_REGEX "^[^ ]+\\.mail$"

/* Allow only one thread to tweak d->fd at a time */
static pthread_mutex_t fides_mutex = PTHREAD_MUTEX_INITIALIZER;
static MSGS *daemon_msgs;              /* global messages */
static char *catalog_db = NULL;       /* database type */
static void (*message_callback)(int type, char *msg) = NULL;
static FILE *trace_fd = NULL;
#if defined(HAVE_WIN32)
static bool trace = true;
#else
static bool trace = false;
#endif
static int hangup = 0;

/* Constants */
const char *host_os = HOST_OS;
const char *distname = DISTNAME;
const char *distver = DISTVER;

/*
 * Walk back in a string from end looking for a
 *  path separator.  
 *  This routine is passed the start of the string and
 *  the end of the string, it returns either the beginning
 *  of the string or where it found a path separator.
 */
static const char *bstrrpath(const char *start, const char *end)
{
   while ( end > start ) {
      end--;   
      if (IsPathSeparator(*end)) {
         break;
      }
   }
   return end;
}

/* Some message class methods */
void MSGS::lock()
{
   P(fides_mutex);
}

void MSGS::unlock()
{
   V(fides_mutex);
}

/*
 * Wait for not in use variable to be clear
 */
void MSGS::wait_not_in_use()     /* leaves fides_mutex set */
{
   lock();
   while (m_in_use || m_closing) {
      unlock();
      bmicrosleep(0, 200);         /* wait */
      lock();
   }
}

/*
 * Handle message delivery errors
 */
static void delivery_error(const char *fmt,...)
{
   va_list   arg_ptr;
   int i, len, maxlen;
   POOLMEM *pool_buf;
   char dt[MAX_TIME_LENGTH];
   int dtlen;

   pool_buf = get_pool_memory(PM_EMSG);

   bstrftime_ny(dt, sizeof(dt), time(NULL));
   dtlen = strlen(dt);
   dt[dtlen++] = ' ';
   dt[dtlen] = 0;

   i = Mmsg(pool_buf, "%s Message delivery ERROR: ", dt);

   for (;;) {
      maxlen = sizeof_pool_memory(pool_buf) - i - 1;
      va_start(arg_ptr, fmt);
      len = bvsnprintf(pool_buf+i, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (len < 0 || len >= (maxlen-5)) {
         pool_buf = realloc_pool_memory(pool_buf, maxlen + i + maxlen/2);
         continue;
      }
      break;
   }

   fputs(pool_buf, stdout);  /* print this here to INSURE that it is printed */
   fflush(stdout);
   syslog(LOG_DAEMON|LOG_ERR, "%s", pool_buf);
   free_memory(pool_buf);
}                 

void register_message_callback(void msg_callback(int type, char *msg))
{
   message_callback = msg_callback;
}


/*
 * Set daemon name. Also, find canonical execution
 *  path.  Note, exepath has spare room for tacking on
 *  the exename so that we can reconstruct the full name.
 *
 * Note, this routine can get called multiple times
 *  The second time is to put the name as found in the
 *  Resource record. On the second call, generally,
 *  argv is NULL to avoid doing the path code twice.
 */
void my_name_is(int argc, char *argv[], const char *name)
{
   char *l, *p, *q;
   char cpath[1024];
   int len;

   if (gethostname(host_name, sizeof(host_name)) != 0) {
      bstrncpy(host_name, "Hostname unknown", sizeof(host_name));
   }
   bstrncpy(my_name, name, sizeof(my_name));
   if (argc>0 && argv && argv[0]) {
      /* strip trailing filename and save exepath */
      for (l=p=argv[0]; *p; p++) {
         if (IsPathSeparator(*p)) {
            l = p;                       /* set pos of last slash */
         }
      }
      if (IsPathSeparator(*l)) {
         l++;
      } else {
         l = argv[0];
#if defined(HAVE_WIN32)
         /* On Windows allow c: drive specification */
         if (l[1] == ':') {
            l += 2;
         }
#endif
      }
      len = strlen(l) + 1;
      if (exename) {
         free(exename);
      }
      exename = (char *)malloc(len);
      strcpy(exename, l);

      if (exepath) {
         free(exepath);
      }
      exepath = (char *)malloc(strlen(argv[0]) + 1 + len);
      for (p=argv[0],q=exepath; p < l; ) {
         *q++ = *p++;
      }
      *q = 0;
      if (strchr(exepath, '.') || !IsPathSeparator(exepath[0])) {
         if (getcwd(cpath, sizeof(cpath))) {
            free(exepath);
            exepath = (char *)malloc(strlen(cpath) + 1 + len);
            strcpy(exepath, cpath);
         }
      }
      Dmsg2(500, "exepath=%s\nexename=%s\n", exepath, exename);
   }
}

void
set_db_type(const char *name)
{
   if (catalog_db != NULL) {
      free(catalog_db);
   }
   catalog_db = bstrdup(name);
}

/*
 * Initialize message handler for a daemon or a Job
 *   We make a copy of the MSGS resource passed, so it belows
 *   to the job or daemon and thus can be modified.
 *
 *   NULL for jcr -> initialize global messages for daemon
 *   non-NULL     -> initialize jcr using Message resource
 */
void
init_msg(JCR *jcr, MSGS *msg)
{
   DEST *d, *dnew, *temp_chain = NULL;
   int i;

   if (jcr == NULL && msg == NULL) {
      init_last_jobs_list();
      /* Create a daemon key then set invalid jcr */
      /* Maybe we should give the daemon a jcr??? */
      create_jcr_key();
      set_jcr_in_tsd(INVALID_JCR);
   }

#if !defined(HAVE_WIN32)
   /*
    * Make sure we have fd's 0, 1, 2 open
    *  If we don't do this one of our sockets may open
    *  there and if we then use stdout, it could
    *  send total garbage to our socket.
    *
    */
   int fd;
   fd = open("/dev/null", O_RDONLY, 0644);
   if (fd > 2) {
      close(fd);
   } else {
      for(i=1; fd + i <= 2; i++) {
         dup2(fd, fd+i);
      }
   }

#endif
   /*
    * If msg is NULL, initialize global chain for STDOUT and syslog
    */
   if (msg == NULL) {
      daemon_msgs = (MSGS *)malloc(sizeof(MSGS));
      memset(daemon_msgs, 0, sizeof(MSGS));
      for (i=1; i<=M_MAX; i++) {
         add_msg_dest(daemon_msgs, MD_STDOUT, i, NULL, NULL);
      }
      Dmsg1(050, "Create daemon global message resource %p\n", daemon_msgs);
      return;
   }

   /*
    * Walk down the message resource chain duplicating it
    * for the current Job.
    */
   for (d=msg->dest_chain; d; d=d->next) {
      dnew = (DEST *)malloc(sizeof(DEST));
      memcpy(dnew, d, sizeof(DEST));
      dnew->next = temp_chain;
      dnew->fd = NULL;
      dnew->mail_filename = NULL;
      if (d->mail_cmd) {
         dnew->mail_cmd = bstrdup(d->mail_cmd);
      }
      if (d->where) {
         dnew->where = bstrdup(d->where);
      }
      temp_chain = dnew;
   }

   if (jcr) {
      jcr->jcr_msgs = (MSGS *)malloc(sizeof(MSGS));
      memset(jcr->jcr_msgs, 0, sizeof(MSGS));
      jcr->jcr_msgs->dest_chain = temp_chain;
      memcpy(jcr->jcr_msgs->send_msg, msg->send_msg, sizeof(msg->send_msg));
   } else {
      /* If we have default values, release them now */
      if (daemon_msgs) {
         free_msgs_res(daemon_msgs);
      }
      daemon_msgs = (MSGS *)malloc(sizeof(MSGS));
      memset(daemon_msgs, 0, sizeof(MSGS));
      daemon_msgs->dest_chain = temp_chain;
      memcpy(daemon_msgs->send_msg, msg->send_msg, sizeof(msg->send_msg));
   }

   Dmsg2(250, "Copy message resource %p to %p\n", msg, temp_chain);

}

/* Initialize so that the console (User Agent) can
 * receive messages -- stored in a file.
 */
void init_console_msg(const char *wd)
{
   int fd;

   bsnprintf(con_fname, sizeof(con_fname), "%s%c%s.conmsg", wd, PathSeparator, my_name);
   fd = open(con_fname, O_CREAT|O_RDWR|O_BINARY, 0600);
   if (fd == -1) {
      berrno be;
      Emsg2(M_ERROR_TERM, 0, _("Could not open console message file %s: ERR=%s\n"),
          con_fname, be.bstrerror());
   }
   if (lseek(fd, 0, SEEK_END) > 0) {
      console_msg_pending = 1;
   }
   close(fd);
   con_fd = fopen(con_fname, "a+b");
   if (!con_fd) {
      berrno be;
      Emsg2(M_ERROR, 0, _("Could not open console message file %s: ERR=%s\n"),
          con_fname, be.bstrerror());
   }
   if (rwl_init(&con_lock) != 0) {
      berrno be;
      Emsg1(M_ERROR_TERM, 0, _("Could not get con mutex: ERR=%s\n"),
         be.bstrerror());
   }
}

/*
 * Called only during parsing of the config file.
 *
 * Add a message destination. I.e. associate a message type with
 *  a destination (code).
 * Note, where in the case of dest_code FILE is a filename,
 *  but in the case of MAIL is a space separated list of
 *  email addresses, ...
 */
void add_msg_dest(MSGS *msg, int dest_code, int msg_type, char *where, char *mail_cmd)
{
   DEST *d;
   /*
    * First search the existing chain and see if we
    * can simply add this msg_type to an existing entry.
    */
   for (d=msg->dest_chain; d; d=d->next) {
      if (dest_code == d->dest_code && ((where == NULL && d->where == NULL) ||
                     (strcmp(where, d->where) == 0))) {
         Dmsg4(850, "Add to existing d=%p msgtype=%d destcode=%d where=%s\n",
             d, msg_type, dest_code, NPRT(where));
         set_bit(msg_type, d->msg_types);
         set_bit(msg_type, msg->send_msg);  /* set msg_type bit in our local */
         return;
      }
   }
   /* Not found, create a new entry */
   d = (DEST *)malloc(sizeof(DEST));
   memset(d, 0, sizeof(DEST));
   d->next = msg->dest_chain;
   d->dest_code = dest_code;
   set_bit(msg_type, d->msg_types);      /* set type bit in structure */
   set_bit(msg_type, msg->send_msg);     /* set type bit in our local */
   if (where) {
      d->where = bstrdup(where);
   }
   if (mail_cmd) {
      d->mail_cmd = bstrdup(mail_cmd);
   }
   Dmsg5(850, "add new d=%p msgtype=%d destcode=%d where=%s mailcmd=%s\n",
          d, msg_type, dest_code, NPRT(where), NPRT(d->mail_cmd));
   msg->dest_chain = d;
}

/*
 * Called only during parsing of the config file.
 *
 * Remove a message destination
 */
void rem_msg_dest(MSGS *msg, int dest_code, int msg_type, char *where)
{
   DEST *d;

   for (d=msg->dest_chain; d; d=d->next) {
      Dmsg2(850, "Remove_msg_dest d=%p where=%s\n", d, NPRT(d->where));
      if (bit_is_set(msg_type, d->msg_types) && (dest_code == d->dest_code) &&
          ((where == NULL && d->where == NULL) ||
                     (strcmp(where, d->where) == 0))) {
         Dmsg3(850, "Found for remove d=%p msgtype=%d destcode=%d\n",
               d, msg_type, dest_code);
         clear_bit(msg_type, d->msg_types);
         Dmsg0(850, "Return rem_msg_dest\n");
         return;
      }
   }
}


/*
 * Create a unique filename for the mail command
 */
static void make_unique_mail_filename(JCR *jcr, POOLMEM *&name, DEST *d)
{
   if (jcr) {
      Mmsg(name, "%s/%s.%s.%d.mail", working_directory, my_name,
                 jcr->Job, (int)(intptr_t)d);
   } else {
      Mmsg(name, "%s/%s.%s.%d.mail", working_directory, my_name,
                 my_name, (int)(intptr_t)d);
   }
   Dmsg1(850, "mailname=%s\n", name);
}

/*
 * Open a mail pipe
 */
static BPIPE *open_mail_pipe(JCR *jcr, POOLMEM *&cmd, DEST *d)
{
   BPIPE *bpipe;

   if (d->mail_cmd) {
      cmd = edit_job_codes(jcr, cmd, d->mail_cmd, d->where);
   } else {
      Mmsg(cmd, "/usr/lib/sendmail -F Bacula %s", d->where);
   }
   fflush(stdout);

   if ((bpipe = open_bpipe(cmd, 120, "rw"))) {
      /* If we had to use sendmail, add subject */
      if (!d->mail_cmd) {
         fprintf(bpipe->wfd, "Subject: %s\r\n\r\n", _("Bacula Message"));
      }
   } else {
      berrno be;
      delivery_error(_("open mail pipe %s failed: ERR=%s\n"),
         cmd, be.bstrerror());
   }
   return bpipe;
}

/*
 * Close the messages for this Messages resource, which means to close
 *  any open files, and dispatch any pending email messages.
 */
void close_msg(JCR *jcr)
{
   MSGS *msgs;
   DEST *d;
   BPIPE *bpipe;
   POOLMEM *cmd, *line;
   int len, stat;

   Dmsg1(580, "Close_msg jcr=%p\n", jcr);

   if (jcr == NULL) {                /* NULL -> global chain */
      msgs = daemon_msgs;
   } else {
      msgs = jcr->jcr_msgs;
      jcr->jcr_msgs = NULL;
   }
   if (msgs == NULL) {
      return;
   }

   /* Wait for item to be not in use, then mark closing */
   if (msgs->is_closing()) {
      return;
   }
   msgs->wait_not_in_use();          /* leaves fides_mutex set */
   /* Note get_closing() does not lock because we are already locked */
   if (msgs->get_closing()) {
      msgs->unlock();
      return;
   }
   msgs->set_closing();
   msgs->unlock();

   Dmsg1(850, "===Begin close msg resource at %p\n", msgs);
   cmd = get_pool_memory(PM_MESSAGE);
   for (d=msgs->dest_chain; d; ) {
      if (d->fd) {
         switch (d->dest_code) {
         case MD_FILE:
         case MD_APPEND:
            if (d->fd) {
               fclose(d->fd);            /* close open file descriptor */
               d->fd = NULL;
            }
            break;
         case MD_MAIL:
         case MD_MAIL_ON_ERROR:
         case MD_MAIL_ON_SUCCESS:
            Dmsg0(850, "Got MD_MAIL, MD_MAIL_ON_ERROR or MD_MAIL_ON_SUCCESS\n");
            if (!d->fd) {
               break;
            }

            switch (d->dest_code) {
            case MD_MAIL_ON_ERROR:
               if (jcr) {
                  switch (jcr->JobStatus) {
                  case JS_Terminated:
                  case JS_Warnings:
                     goto rem_temp_file;
                  default:
                     break;
                  }
               }
               break;
            case MD_MAIL_ON_SUCCESS:
               if (jcr) {
                  switch (jcr->JobStatus) {
                  case JS_Terminated:
                  case JS_Warnings:
                     break;
                  default:
                     goto rem_temp_file;
                  }
               }
               break;
            default:
               break;
            }

            if (!(bpipe=open_mail_pipe(jcr, cmd, d))) {
               Pmsg0(000, _("open mail pipe failed.\n"));
               goto rem_temp_file;
            }

            Dmsg0(850, "Opened mail pipe\n");
            len = d->max_len+10;
            line = get_memory(len);
            rewind(d->fd);
            while (fgets(line, len, d->fd)) {
               fputs(line, bpipe->wfd);
            }
            if (!close_wpipe(bpipe)) {       /* close write pipe sending mail */
               berrno be;
               Pmsg1(000, _("close error: ERR=%s\n"), be.bstrerror());
            }

            /*
             * Since we are closing all messages, before "recursing"
             * make sure we are not closing the daemon messages, otherwise
             * kaboom.
             */
            if (msgs != daemon_msgs) {
               /* read what mail prog returned -- should be nothing */
               while (fgets(line, len, bpipe->rfd)) {
                  delivery_error(_("Mail prog: %s"), line);
               }
            }

            stat = close_bpipe(bpipe);
            if (stat != 0 && msgs != daemon_msgs) {
               berrno be;
               be.set_errno(stat);
               Dmsg1(850, "Calling emsg. CMD=%s\n", cmd);
               delivery_error(_("Mail program terminated in error.\n"
                                 "CMD=%s\n"
                                 "ERR=%s\n"), cmd, be.bstrerror());
            }
            free_memory(line);
rem_temp_file:
            /* Remove temp file */
            if (d->fd) {
               fclose(d->fd);
               d->fd = NULL;
            }
            if (d->mail_filename) {
               /* Exclude spaces in mail_filename */
               safer_unlink(d->mail_filename, MAIL_REGEX);
               free_pool_memory(d->mail_filename);
               d->mail_filename = NULL;
            }
            Dmsg0(850, "end mail or mail on error\n");
            break;
         default:
            break;
         }
         d->fd = NULL;
      }
      d = d->next;                    /* point to next buffer */
   }
   free_pool_memory(cmd);
   Dmsg0(850, "Done walking message chain.\n");
   if (jcr) {
      free_msgs_res(msgs);
      msgs = NULL;
   } else {
      msgs->clear_closing();
   }
   Dmsg0(850, "===End close msg resource\n");
}

/*
 * Free memory associated with Messages resource
 */
void free_msgs_res(MSGS *msgs)
{
   DEST *d, *old;

   /* Walk down the message chain releasing allocated buffers */
   for (d=msgs->dest_chain; d; ) {
      if (d->where) {
         free(d->where);
      }
      if (d->mail_cmd) {
         free(d->mail_cmd);
      }
      old = d;                        /* save pointer to release */
      d = d->next;                    /* point to next buffer */
      free(old);                      /* free the destination item */
   }
   msgs->dest_chain = NULL;
   free(msgs);                        /* free the head */
}


/*
 * Terminate the message handler for good.
 * Release the global destination chain.
 *
 * Also, clean up a few other items (cons, exepath). Note,
 *   these really should be done elsewhere.
 */
void term_msg()
{
   Dmsg0(850, "Enter term_msg\n");
   close_msg(NULL);                   /* close global chain */
   free_msgs_res(daemon_msgs);        /* free the resources */
   daemon_msgs = NULL;
   if (con_fd) {
      fflush(con_fd);
      fclose(con_fd);
      con_fd = NULL;
   }
   if (exepath) {
      free(exepath);
      exepath = NULL;
   }
   if (exename) {
      free(exename);
      exename = NULL;
   }
   if (trace_fd) {
      fclose(trace_fd);
      trace_fd = NULL;
   }
   if (catalog_db) {
      free(catalog_db);
      catalog_db = NULL;
   }
   term_last_jobs_list();
}

static bool open_dest_file(JCR *jcr, DEST *d, const char *mode) 
{
   d->fd = fopen(d->where, mode);
   if (!d->fd) {
      berrno be;
      delivery_error(_("fopen %s failed: ERR=%s\n"), d->where, be.bstrerror());
      return false;
   }
   return true;
}

/* Split the output for syslog (it converts \n to ' ' and is
 *   limited to 1024 characters per syslog message
 */
static void send_to_syslog(int mode, const char *msg)
{
   int len;
   char buf[1024];
   const char *p2;
   const char *p = msg;

   while (*p && ((p2 = strchr(p, '\n')) != NULL)) {
      len = MIN((int)sizeof(buf) - 1, p2 - p + 1); /* Add 1 to keep \n */
      strncpy(buf, p, len);
      buf[len] = 0;
      syslog(mode, "%s", buf);
      p = p2+1;                 /* skip \n */
   }
   if (*p != 0) {               /* no \n at the end ? */
      syslog(mode, "%s", p);
   }
}

/*
 * Handle sending the message to the appropriate place
 */
void dispatch_message(JCR *jcr, int type, utime_t mtime, char *msg)
{
    DEST *d;
    char dt[MAX_TIME_LENGTH];
    POOLMEM *mcmd;
    int len, dtlen;
    MSGS *msgs;
    BPIPE *bpipe;
    const char *mode;

    Dmsg2(850, "Enter dispatch_msg type=%d msg=%s", type, msg);

    /*
     * Most messages are prefixed by a date and time. If mtime is
     *  zero, then we use the current time.  If mtime is 1 (special
     *  kludge), we do not prefix the date and time. Otherwise,
     *  we assume mtime is a utime_t and use it.
     */
    if (mtime == 0) {
       mtime = time(NULL);
    }
    if (mtime == 1) {
       *dt = 0;
       dtlen = 0;
       mtime = time(NULL);      /* get time for SQL log */
    } else {
       bstrftime_ny(dt, sizeof(dt), mtime);
       dtlen = strlen(dt);
       dt[dtlen++] = ' ';
       dt[dtlen] = 0;
    }

    /* If the program registered a callback, send it there */
    if (message_callback) {
       message_callback(type, msg);
       return;
    }

    /* For serious errors make sure message is printed or logged */
    if (type == M_ABORT || type == M_ERROR_TERM) {
       fputs(dt, stdout);
       fputs(msg, stdout);
       fflush(stdout);
       if (type == M_ABORT) {
          syslog(LOG_DAEMON|LOG_ERR, "%s", msg);
       }
    }


    /* Now figure out where to send the message */
    msgs = NULL;
    if (!jcr) {
       jcr = get_jcr_from_tsd();
    }
    if (jcr) {
       msgs = jcr->jcr_msgs;
    }
    if (msgs == NULL) {
       msgs = daemon_msgs;
    }
    /*
     * If closing this message resource, print and send to syslog,
     *   then get out.
     */
    if (msgs->is_closing()) {
       fputs(dt, stdout);
       fputs(msg, stdout);
       fflush(stdout);
       syslog(LOG_DAEMON|LOG_ERR, "%s", msg);
       return;
    }

    for (d=msgs->dest_chain; d; d=d->next) {
       if (bit_is_set(type, d->msg_types)) {
          switch (d->dest_code) {
             case MD_CATALOG:
                char ed1[50];
                if (!jcr || !jcr->db) {
                   break;
                }
                if (p_sql_query && p_sql_escape) {
                   POOLMEM *cmd = get_pool_memory(PM_MESSAGE);
                   POOLMEM *esc_msg = get_pool_memory(PM_MESSAGE);
                   
                   int len = strlen(msg) + 1;
                   esc_msg = check_pool_memory_size(esc_msg, len * 2 + 1);
                   if (p_sql_escape(jcr, jcr->db, esc_msg, msg, len)) {
                      bstrutime(dt, sizeof(dt), mtime);
                      Mmsg(cmd, "INSERT INTO Log (JobId, Time, LogText) VALUES (%s,'%s','%s')",
                            edit_int64(jcr->JobId, ed1), dt, esc_msg);
                      if (!p_sql_query(jcr, cmd)) {
                         delivery_error(_("Msg delivery error: Unable to store data in database.\n"));
                      }
                   } else {
                      delivery_error(_("Msg delivery error: Unable to store data in database.\n"));
                   }
                   
                   free_pool_memory(cmd);
                   free_pool_memory(esc_msg);
                }
                break;
             case MD_CONSOLE:
                Dmsg1(850, "CONSOLE for following msg: %s", msg);
                if (!con_fd) {
                   con_fd = fopen(con_fname, "a+b");
                   Dmsg0(850, "Console file not open.\n");
                }
                if (con_fd) {
                   Pw(con_lock);      /* get write lock on console message file */
                   errno = 0;
                   if (dtlen) {
                      (void)fwrite(dt, dtlen, 1, con_fd);
                   }
                   len = strlen(msg);
                   if (len > 0) {
                      (void)fwrite(msg, len, 1, con_fd);
                      if (msg[len-1] != '\n') {
                         (void)fwrite("\n", 2, 1, con_fd);
                      }
                   } else {
                      (void)fwrite("\n", 2, 1, con_fd);
                   }
                   fflush(con_fd);
                   console_msg_pending = true;
                   Vw(con_lock);
                }
                break;
             case MD_SYSLOG:
                Dmsg1(850, "SYSLOG for following msg: %s\n", msg);
                /*
                 * We really should do an openlog() here.
                 */
                send_to_syslog(LOG_DAEMON|LOG_ERR, msg);
                break;
             case MD_OPERATOR:
                Dmsg1(850, "OPERATOR for following msg: %s\n", msg);
                mcmd = get_pool_memory(PM_MESSAGE);
                if ((bpipe=open_mail_pipe(jcr, mcmd, d))) {
                   int stat;
                   fputs(dt, bpipe->wfd);
                   fputs(msg, bpipe->wfd);
                   /* Messages to the operator go one at a time */
                   stat = close_bpipe(bpipe);
                   if (stat != 0) {
                      berrno be;
                      be.set_errno(stat);
                      delivery_error(_("Msg delivery error: Operator mail program terminated in error.\n"
                            "CMD=%s\n"
                            "ERR=%s\n"), mcmd, be.bstrerror());
                   }
                }
                free_pool_memory(mcmd);
                break;
             case MD_MAIL:
             case MD_MAIL_ON_ERROR:
             case MD_MAIL_ON_SUCCESS:
                Dmsg1(850, "MAIL for following msg: %s", msg);
                if (msgs->is_closing()) {
                   break;
                }
                msgs->set_in_use();
                if (!d->fd) {
                   POOLMEM *name = get_pool_memory(PM_MESSAGE);
                   make_unique_mail_filename(jcr, name, d);
                   d->fd = fopen(name, "w+b");
                   if (!d->fd) {
                      berrno be;
                      delivery_error(_("Msg delivery error: fopen %s failed: ERR=%s\n"), name,
                            be.bstrerror());
                      free_pool_memory(name);
                      msgs->clear_in_use();
                      break;
                   }
                   d->mail_filename = name;
                }
                fputs(dt, d->fd);
                len = strlen(msg) + dtlen;;
                if (len > d->max_len) {
                   d->max_len = len;      /* keep max line length */
                }
                fputs(msg, d->fd);
                msgs->clear_in_use();
                break;
             case MD_APPEND:
                Dmsg1(850, "APPEND for following msg: %s", msg);
                mode = "ab";
                goto send_to_file;
             case MD_FILE:
                Dmsg1(850, "FILE for following msg: %s", msg);
                mode = "w+b";
send_to_file:
                if (msgs->is_closing()) {
                   break;
                }
                msgs->set_in_use();
                if (!d->fd && !open_dest_file(jcr, d, mode)) {
                   msgs->clear_in_use();
                   break;
                }
                fputs(dt, d->fd);
                fputs(msg, d->fd);
                /* On error, we close and reopen to handle log rotation */
                if (ferror(d->fd)) {
                   fclose(d->fd);
                   d->fd = NULL;
                   if (open_dest_file(jcr, d, mode)) {
                      fputs(dt, d->fd);
                      fputs(msg, d->fd);
                   }
                }
                msgs->clear_in_use();
                break;
             case MD_DIRECTOR:
                Dmsg1(850, "DIRECTOR for following msg: %s", msg);
                if (jcr && jcr->dir_bsock && !jcr->dir_bsock->errors) {
                   jcr->dir_bsock->fsend("Jmsg Job=%s type=%d level=%lld %s",
                      jcr->Job, type, mtime, msg);
                } else {
                   Dmsg1(800, "no jcr for following msg: %s", msg);
                }
                break;
             case MD_STDOUT:
                Dmsg1(850, "STDOUT for following msg: %s", msg);
                if (type != M_ABORT && type != M_ERROR_TERM) { /* already printed */
                   fputs(dt, stdout);
                   fputs(msg, stdout);
                   fflush(stdout);
                }
                break;
             case MD_STDERR:
                Dmsg1(850, "STDERR for following msg: %s", msg);
                fputs(dt, stderr);
                fputs(msg, stderr);
                fflush(stdout);
                break;
             default:
                break;
          }
       }
    }
}

/*********************************************************************
 *
 *  This subroutine returns the filename portion of a path.  
 *  It is used because some compilers set __FILE__ 
 *  to the full path.  Try to return base + next higher path.
 */

const char *get_basename(const char *pathname)
{
   const char *basename;
   
   if ((basename = bstrrpath(pathname, pathname+strlen(pathname))) == pathname) {
      /* empty */
   } else if ((basename = bstrrpath(pathname, basename-1)) == pathname) {
      /* empty */
   } else {
      basename++;
   }
   return basename;
}

/*
 * print or write output to trace file 
 */
static void pt_out(char *buf)
{
    /*
     * Used the "trace on" command in the console to turn on
     *  output to the trace file.  "trace off" will close the file.
     */
    if (trace) {
       if (!trace_fd) {
          char fn[200];
          bsnprintf(fn, sizeof(fn), "%s/%s.trace", working_directory ? working_directory : "./", my_name);
          trace_fd = fopen(fn, "a+b");
       }
       if (trace_fd) {
          fputs(buf, trace_fd);
          fflush(trace_fd);
          return;
       } else {
          /* Some problem, turn off tracing */
          trace = false;
       }
    }
    /* not tracing */
    fputs(buf, stdout);
    fflush(stdout);
}

/*********************************************************************
 *
 *  This subroutine prints a debug message if the level number
 *  is less than or equal the debug_level. File and line numbers
 *  are included for more detail if desired, but not currently
 *  printed.
 *
 *  If the level is negative, the details of file and line number
 *  are not printed.
 */
void
d_msg(const char *file, int line, int level, const char *fmt,...)
{
    char      buf[5000];
    int       len;
    va_list   arg_ptr;
    bool      details = true;
    utime_t   mtime;

    if (level < 0) {
       details = false;
       level = -level;
    }

    if (level <= debug_level) {
       if (dbg_timestamp) {
          mtime = time(NULL);
          bstrftimes(buf, sizeof(buf), mtime);
          len = strlen(buf);
          buf[len++] = ' ';
          buf[len] = 0;
          pt_out(buf);
       }
    
#ifdef FULL_LOCATION
       if (details) {
          len = bsnprintf(buf, sizeof(buf), "%s: %s:%d-%u ", 
                my_name, get_basename(file), line, get_jobid_from_tsd());
       } else {
          len = 0;
       }
#else
       len = 0;
#endif
       va_start(arg_ptr, fmt);
       bvsnprintf(buf+len, sizeof(buf)-len, (char *)fmt, arg_ptr);
       va_end(arg_ptr);

       pt_out(buf);
    }
}

/*
 * Set trace flag on/off. If argument is negative, there is no change
 */
void set_trace(int trace_flag)
{
   if (trace_flag < 0) {
      return;
   } else if (trace_flag > 0) {
      trace = true;
   } else {
      trace = false;
   }
   if (!trace && trace_fd) {
      FILE *ltrace_fd = trace_fd;
      trace_fd = NULL;
      bmicrosleep(0, 100000);         /* yield to prevent seg faults */
      fclose(ltrace_fd);
   }
}

void set_hangup(int hangup_value)
{
   if (hangup_value < 0) {
      return;
   } else {
      hangup = hangup_value;
   }
}

int get_hangup(void)
{
   return hangup;
}

bool get_trace(void)
{
   return trace;
}

/*********************************************************************
 *
 *  This subroutine prints a message regardless of the debug level
 *
 *  If the level is negative, the details of file and line number
 *  are not printed.
 */
void
p_msg(const char *file, int line, int level, const char *fmt,...)
{
    char      buf[5000];
    int       len;
    va_list   arg_ptr;

#ifdef FULL_LOCATION
    if (level >= 0) {
       len = bsnprintf(buf, sizeof(buf), "%s: %s:%d-%u ", 
             my_name, get_basename(file), line, get_jobid_from_tsd());
    } else {
       len = 0;
    }
#else
       len = 0;
#endif

    va_start(arg_ptr, fmt);
    bvsnprintf(buf+len, sizeof(buf)-len, (char *)fmt, arg_ptr);
    va_end(arg_ptr);

    pt_out(buf);     
}


/*********************************************************************
 *
 *  subroutine writes a debug message to the trace file if the level number
 *  is less than or equal the debug_level. File and line numbers
 *  are included for more detail if desired, but not currently
 *  printed.
 *
 *  If the level is negative, the details of file and line number
 *  are not printed.
 */
void
t_msg(const char *file, int line, int level, const char *fmt,...)
{
    char      buf[5000];
    int       len;
    va_list   arg_ptr;
    int       details = TRUE;

    if (level < 0) {
       details = FALSE;
       level = -level;
    }

    if (level <= debug_level) {
       if (!trace_fd) {
          bsnprintf(buf, sizeof(buf), "%s/%s.trace", working_directory ? working_directory : ".", my_name);
          trace_fd = fopen(buf, "a+b");
       }

#ifdef FULL_LOCATION
       if (details) {
          len = bsnprintf(buf, sizeof(buf), "%s: %s:%d ", my_name, get_basename(file), line);
       } else {
          len = 0;
       }
#else
       len = 0;
#endif
       va_start(arg_ptr, fmt);
       bvsnprintf(buf+len, sizeof(buf)-len, (char *)fmt, arg_ptr);
       va_end(arg_ptr);
       if (trace_fd != NULL) {
           fputs(buf, trace_fd);
           fflush(trace_fd);
       }
   }
}

/* *********************************************************
 *
 * print an error message
 *
 */
void
e_msg(const char *file, int line, int type, int level, const char *fmt,...)
{
    char     buf[5000];
    va_list   arg_ptr;
    int len;

    /*
     * Check if we have a message destination defined.
     * We always report M_ABORT and M_ERROR_TERM
     */
    if (!daemon_msgs || ((type != M_ABORT && type != M_ERROR_TERM) &&
                         !bit_is_set(type, daemon_msgs->send_msg))) {
       return;                        /* no destination */
    }
    switch (type) {
    case M_ABORT:
       len = bsnprintf(buf, sizeof(buf), _("%s: ABORTING due to ERROR in %s:%d\n"),
               my_name, get_basename(file), line);
       break;
    case M_ERROR_TERM:
       len = bsnprintf(buf, sizeof(buf), _("%s: ERROR TERMINATION at %s:%d\n"),
               my_name, get_basename(file), line);
       break;
    case M_FATAL:
       if (level == -1)            /* skip details */
          len = bsnprintf(buf, sizeof(buf), _("%s: Fatal Error because: "), my_name);
       else
          len = bsnprintf(buf, sizeof(buf), _("%s: Fatal Error at %s:%d because:\n"), my_name, get_basename(file), line);
       break;
    case M_ERROR:
       if (level == -1)            /* skip details */
          len = bsnprintf(buf, sizeof(buf), _("%s: ERROR: "), my_name);
       else
          len = bsnprintf(buf, sizeof(buf), _("%s: ERROR in %s:%d "), my_name, get_basename(file), line);
       break;
    case M_WARNING:
       len = bsnprintf(buf, sizeof(buf), _("%s: Warning: "), my_name);
       break;
    case M_SECURITY:
       len = bsnprintf(buf, sizeof(buf), _("%s: Security violation: "), my_name);
       break;
    default:
       len = bsnprintf(buf, sizeof(buf), "%s: ", my_name);
       break;
    }

    va_start(arg_ptr, fmt);
    bvsnprintf(buf+len, sizeof(buf)-len, (char *)fmt, arg_ptr);
    va_end(arg_ptr);

    dispatch_message(NULL, type, 0, buf);

    if (type == M_ABORT) {
       char *p = 0;
       p[0] = 0;                      /* generate segmentation violation */
    }
    if (type == M_ERROR_TERM) {
       exit(1);
    }
}

/* *********************************************************
 *
 * Generate a Job message
 *
 */
void
Jmsg(JCR *jcr, int type, utime_t mtime, const char *fmt,...)
{
    char     rbuf[5000];
    va_list   arg_ptr;
    int len;
    MSGS *msgs;
    uint32_t JobId = 0;


    Dmsg1(850, "Enter Jmsg type=%d\n", type);

    /* Special case for the console, which has a dir_bsock and JobId==0,
     *  in that case, we send the message directly back to the
     *  dir_bsock.
     */
    if (jcr && jcr->JobId == 0 && jcr->dir_bsock) {
       BSOCK *dir = jcr->dir_bsock;
       va_start(arg_ptr, fmt);
       dir->msglen = bvsnprintf(dir->msg, sizeof_pool_memory(dir->msg),
                                fmt, arg_ptr);
       va_end(arg_ptr);
       jcr->dir_bsock->send();
       return;
    }

    /* The watchdog thread can't use Jmsg directly, we always queued it */
    if (is_watchdog()) {
       va_start(arg_ptr, fmt);
       bvsnprintf(rbuf,  sizeof(rbuf), fmt, arg_ptr);
       va_end(arg_ptr);
       Qmsg(jcr, type, mtime, "%s", rbuf);
       return;
    }

    msgs = NULL;
    if (!jcr) {
       jcr = get_jcr_from_tsd();
    }
    if (jcr) {
       if (!jcr->dequeuing_msgs) { /* Avoid recursion */
          /* Dequeue messages to keep the original order  */
          dequeue_messages(jcr); 
       }
       msgs = jcr->jcr_msgs;
       JobId = jcr->JobId;
    }
    if (!msgs) {
       msgs = daemon_msgs;            /* if no jcr, we use daemon handler */
    }

    /*
     * Check if we have a message destination defined.
     * We always report M_ABORT and M_ERROR_TERM
     */
    if (msgs && (type != M_ABORT && type != M_ERROR_TERM) &&
         !bit_is_set(type, msgs->send_msg)) {
       return;                        /* no destination */
    }
    switch (type) {
    case M_ABORT:
       len = bsnprintf(rbuf, sizeof(rbuf), _("%s ABORTING due to ERROR\n"), my_name);
       break;
    case M_ERROR_TERM:
       len = bsnprintf(rbuf, sizeof(rbuf), _("%s ERROR TERMINATION\n"), my_name);
       break;
    case M_FATAL:
       len = bsnprintf(rbuf, sizeof(rbuf), _("%s JobId %u: Fatal error: "), my_name, JobId);
       if (jcr) {
          jcr->setJobStatus(JS_FatalError);
       }
       if (jcr && jcr->JobErrors == 0) {
          jcr->JobErrors = 1;
       }
       break;
    case M_ERROR:
       len = bsnprintf(rbuf, sizeof(rbuf), _("%s JobId %u: Error: "), my_name, JobId);
       if (jcr) {
          jcr->JobErrors++;
       }
       break;
    case M_WARNING:
       len = bsnprintf(rbuf, sizeof(rbuf), _("%s JobId %u: Warning: "), my_name, JobId);
       if (jcr) {
          jcr->JobWarnings++;
       }
       break;
    case M_SECURITY:
       len = bsnprintf(rbuf, sizeof(rbuf), _("%s JobId %u: Security violation: "), 
               my_name, JobId);
       break;
    default:
       len = bsnprintf(rbuf, sizeof(rbuf), "%s JobId %u: ", my_name, JobId);
       break;
    }

    va_start(arg_ptr, fmt);
    bvsnprintf(rbuf+len,  sizeof(rbuf)-len, fmt, arg_ptr);
    va_end(arg_ptr);

    dispatch_message(jcr, type, mtime, rbuf);

    if (type == M_ABORT){
       char *p = 0;
       printf("Bacula forced SEG FAULT to obtain traceback.\n");
       syslog(LOG_DAEMON|LOG_ERR, "Bacula forced SEG FAULT to obtain traceback.\n");
       p[0] = 0;                      /* generate segmentation violation */
    }
    if (type == M_ERROR_TERM) {
       exit(1);
    }
}

/*
 * If we come here, prefix the message with the file:line-number,
 *  then pass it on to the normal Jmsg routine.
 */
void j_msg(const char *file, int line, JCR *jcr, int type, utime_t mtime, const char *fmt,...)
{
   va_list   arg_ptr;
   int i, len, maxlen;
   POOLMEM *pool_buf;

   pool_buf = get_pool_memory(PM_EMSG);
   i = Mmsg(pool_buf, "%s:%d ", get_basename(file), line);

   for (;;) {
      maxlen = sizeof_pool_memory(pool_buf) - i - 1;
      va_start(arg_ptr, fmt);
      len = bvsnprintf(pool_buf+i, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (len < 0 || len >= (maxlen-5)) {
         pool_buf = realloc_pool_memory(pool_buf, maxlen + i + maxlen/2);
         continue;
      }
      break;
   }

   Jmsg(jcr, type, mtime, "%s", pool_buf);
   free_memory(pool_buf);
}


/*
 * Edit a message into a Pool memory buffer, with file:lineno
 */
int m_msg(const char *file, int line, POOLMEM **pool_buf, const char *fmt, ...)
{
   va_list   arg_ptr;
   int i, len, maxlen;

   i = sprintf(*pool_buf, "%s:%d ", get_basename(file), line);

   for (;;) {
      maxlen = sizeof_pool_memory(*pool_buf) - i - 1;
      va_start(arg_ptr, fmt);
      len = bvsnprintf(*pool_buf+i, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (len < 0 || len >= (maxlen-5)) {
         *pool_buf = realloc_pool_memory(*pool_buf, maxlen + i + maxlen/2);
         continue;
      }
      break;
   }
   return len;
}

int m_msg(const char *file, int line, POOLMEM *&pool_buf, const char *fmt, ...)
{
   va_list   arg_ptr;
   int i, len, maxlen;

   i = sprintf(pool_buf, "%s:%d ", get_basename(file), line);

   for (;;) {
      maxlen = sizeof_pool_memory(pool_buf) - i - 1;
      va_start(arg_ptr, fmt);
      len = bvsnprintf(pool_buf+i, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (len < 0 || len >= (maxlen-5)) {
         pool_buf = realloc_pool_memory(pool_buf, maxlen + i + maxlen/2);
         continue;
      }
      break;
   }
   return len;
}


/*
 * Edit a message into a Pool Memory buffer NO file:lineno
 *  Returns: string length of what was edited.
 */
int Mmsg(POOLMEM **pool_buf, const char *fmt, ...)
{
   va_list   arg_ptr;
   int len, maxlen;

   for (;;) {
      maxlen = sizeof_pool_memory(*pool_buf) - 1;
      va_start(arg_ptr, fmt);
      len = bvsnprintf(*pool_buf, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (len < 0 || len >= (maxlen-5)) {
         *pool_buf = realloc_pool_memory(*pool_buf, maxlen + maxlen/2);
         continue;
      }
      break;
   }
   return len;
}

int Mmsg(POOLMEM *&pool_buf, const char *fmt, ...)
{
   va_list   arg_ptr;
   int len, maxlen;

   for (;;) {
      maxlen = sizeof_pool_memory(pool_buf) - 1;
      va_start(arg_ptr, fmt);
      len = bvsnprintf(pool_buf, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (len < 0 || len >= (maxlen-5)) {
         pool_buf = realloc_pool_memory(pool_buf, maxlen + maxlen/2);
         continue;
      }
      break;
   }
   return len;
}

int Mmsg(POOL_MEM &pool_buf, const char *fmt, ...)
{
   va_list   arg_ptr;
   int len, maxlen;

   for (;;) {
      maxlen = pool_buf.max_size() - 1;
      va_start(arg_ptr, fmt);
      len = bvsnprintf(pool_buf.c_str(), maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (len < 0 || len >= (maxlen-5)) {
         pool_buf.realloc_pm(maxlen + maxlen/2);
         continue;
      }
      break;
   }
   return len;
}


/*
 * We queue messages rather than print them directly. This
 *  is generally used in low level routines (msg handler, bnet)
 *  to prevent recursion (i.e. if you are in the middle of
 *  sending a message, it is a bit messy to recursively call
 *  yourself when the bnet packet is not reentrant).
 */
void Qmsg(JCR *jcr, int type, utime_t mtime, const char *fmt,...)
{
   va_list   arg_ptr;
   int len, maxlen;
   POOLMEM *pool_buf;
   MQUEUE_ITEM *item;

   pool_buf = get_pool_memory(PM_EMSG);

   for (;;) {
      maxlen = sizeof_pool_memory(pool_buf) - 1;
      va_start(arg_ptr, fmt);
      len = bvsnprintf(pool_buf, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (len < 0 || len >= (maxlen-5)) {
         pool_buf = realloc_pool_memory(pool_buf, maxlen + maxlen/2);
         continue;
      }
      break;
   }
   item = (MQUEUE_ITEM *)malloc(sizeof(MQUEUE_ITEM) + strlen(pool_buf) + 1);
   item->type = type;
   item->mtime = time(NULL);
   strcpy(item->msg, pool_buf);
   if (!jcr) {
      jcr = get_jcr_from_tsd();
   }
   /* If no jcr or no queue or dequeuing send to syslog */
   if (!jcr || !jcr->msg_queue || jcr->dequeuing_msgs) {
      syslog(LOG_DAEMON|LOG_ERR, "%s", item->msg);
      free(item);
   } else {
      /* Queue message for later sending */
      P(jcr->msg_queue_mutex);
      jcr->msg_queue->append(item);
      V(jcr->msg_queue_mutex);
   }
   free_memory(pool_buf);
}

/*
 * Dequeue messages
 */
void dequeue_messages(JCR *jcr)
{
   MQUEUE_ITEM *item;
   if (!jcr->msg_queue) {
      return;
   }
   P(jcr->msg_queue_mutex);
   jcr->dequeuing_msgs = true;
   foreach_dlist(item, jcr->msg_queue) {
      Jmsg(jcr, item->type, item->mtime, "%s", item->msg);
   }
   /* Remove messages just sent */
   jcr->msg_queue->destroy();
   jcr->dequeuing_msgs = false;
   V(jcr->msg_queue_mutex);
}


/*
 * If we come here, prefix the message with the file:line-number,
 *  then pass it on to the normal Qmsg routine.
 */
void q_msg(const char *file, int line, JCR *jcr, int type, utime_t mtime, const char *fmt,...)
{
   va_list   arg_ptr;
   int i, len, maxlen;
   POOLMEM *pool_buf;

   pool_buf = get_pool_memory(PM_EMSG);
   i = Mmsg(pool_buf, "%s:%d ", get_basename(file), line);

   for (;;) {
      maxlen = sizeof_pool_memory(pool_buf) - i - 1;
      va_start(arg_ptr, fmt);
      len = bvsnprintf(pool_buf+i, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (len < 0 || len >= (maxlen-5)) {
         pool_buf = realloc_pool_memory(pool_buf, maxlen + i + maxlen/2);
         continue;
      }
      break;
   }

   Qmsg(jcr, type, mtime, "%s", pool_buf);
   free_memory(pool_buf);
}
