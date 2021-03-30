/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
 * BAREOS message handling routines
 *
 * NOTE: don't use any Jmsg or Qmsg calls within this file,
 * except in q_msg or j_msg (setup routines),
 * otherwise you may get into recursive calls if there are
 * errors, and that can lead to looping or deadlocks.
 *
 * Kern Sibbald, April 2000
 */

#include <vector>

#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/berrno.h"
#include "lib/bsock.h"
#include "lib/util.h"
#include "lib/watchdog.h"
#include "lib/recent_job_results_list.h"
#include "lib/messages_resource.h"
#include "lib/message_destination_info.h"
#include "lib/message_queue_item.h"
#include "lib/thread_specific_data.h"

// globals
const char* working_directory = NULL; /* working directory path stored here */
int verbose = 0;                      /* increase User messages */
int debug_level = 0;                  /* debug level */
bool dbg_timestamp = false;           /* print timestamp in debug output */
bool prt_kaboom = false;              /* Print kaboom output */
utime_t daemon_start_time = 0;        /* Daemon start time */
char my_name[128] = {0};              /* daemon name is stored here */
char host_name[256] = {0};            /* host machine name */
char* exepath = (char*)NULL;
char* exename = (char*)NULL;
int console_msg_pending = false;
char con_fname[500]; /* Console filename */
FILE* con_fd = NULL; /* Console file descriptor */
brwlock_t con_lock;  /* Console lock structure */
job_code_callback_t message_job_code_callback = NULL;  // Only used by director

/* Exclude spaces but require .mail at end */
#define MAIL_REGEX "^[^ ]+\\.mail$"

static MessagesResource* daemon_msgs; /* Global messages */
static char* catalog_db = NULL;       /* Database type */
static const char* log_timestamp_format = "%d-%b %H:%M";
static void (*message_callback)(int type, const char* msg) = NULL;
static FILE* trace_fd = NULL;
#if defined(HAVE_WIN32)
static bool trace = true;
#else
static bool trace = false;
#endif
static bool hangup = false;

/*
 * Walk back in a string from end looking for a
 * path separator.
 *
 * This routine is passed the start of the string and
 * the end of the string, it returns either the beginning
 * of the string or where it found a path separator.
 */
static const char* bstrrpath(const char* start, const char* end)
{
  while (end > start) {
    end--;
    if (IsPathSeparator(*end)) { break; }
  }
  return end;
}

static void DeliveryError(const char* fmt, ...)
{
  va_list ap;
  int i, len, maxlen;
  POOLMEM* pool_buf;
  char dt[MAX_TIME_LENGTH];

  pool_buf = GetPoolMemory(PM_EMSG);

  bstrftime(dt, sizeof(dt), time(NULL), log_timestamp_format);
  bstrncat(dt, " ", sizeof(dt));

  i = Mmsg(pool_buf, "%s Message delivery ERROR: ", dt);

  while (1) {
    maxlen = SizeofPoolMemory(pool_buf) - i - 1;
    va_start(ap, fmt);
    len = Bvsnprintf(pool_buf + i, maxlen, fmt, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      pool_buf = ReallocPoolMemory(pool_buf, maxlen + i + maxlen / 2);
      continue;
    }

    break;
  }

  fputs(pool_buf, stdout); /* print this here to INSURE that it is printed */
  fflush(stdout);
  syslog(LOG_DAEMON | LOG_ERR, "%s", pool_buf);
  FreeMemory(pool_buf);
}

void RegisterMessageCallback(void msg_callback(int type, const char* msg))
{
  message_callback = msg_callback;
}

/*
 * Set daemon name. Also, find canonical execution
 * path.  Note, exepath has spare room for tacking on
 * the exename so that we can reconstruct the full name.
 *
 * Note, this routine can get called multiple times
 * The second time is to put the name as found in the
 * Resource record. On the second call, generally,
 * argv is NULL to avoid doing the path code twice.
 */
void MyNameIs(int argc, char* argv[], const char* name)
{
  char *l, *p, *q;
  char cpath[1024];
  int len;

  if (gethostname(host_name, sizeof(host_name)) != 0) {
    bstrncpy(host_name, "Hostname unknown", sizeof(host_name));
  }
  bstrncpy(my_name, name, sizeof(my_name));
  if (argc > 0 && argv && argv[0]) {
    /*
     * Strip trailing filename and save exepath
     */
    for (l = p = argv[0]; *p; p++) {
      if (IsPathSeparator(*p)) { l = p; /* set pos of last slash */ }
    }
    if (IsPathSeparator(*l)) {
      l++;
    } else {
      l = argv[0];
#if defined(HAVE_WIN32)
      /*
       * On Windows allow c: drive specification
       */
      if (l[1] == ':') { l += 2; }
#endif
    }
    len = strlen(l) + 1;
    if (exename) { free(exename); }
    exename = (char*)malloc(len);
    strcpy(exename, l);

    if (exepath) { free(exepath); }
    exepath = (char*)malloc(strlen(argv[0]) + 1 + len);
    for (p = argv[0], q = exepath; p < l;) { *q++ = *p++; }
    *q = 0;
    if (strchr(exepath, '.') || !IsPathSeparator(exepath[0])) {
      if (getcwd(cpath, sizeof(cpath))) {
        free(exepath);
        exepath = (char*)malloc(strlen(cpath) + 1 + len);
        strcpy(exepath, cpath);
      }
    }
    Dmsg2(500, "exepath=%s\nexename=%s\n", exepath, exename);
  }
}

void SetDbType(const char* name)
{
  if (catalog_db != NULL) { free(catalog_db); }
  catalog_db = strdup(name);
}

/*
 * Initialize message handler for a daemon or a Job
 * We make a copy of the MessagesResource resource passed, so it belongs
 * to the job or daemon and thus can be modified.
 *
 * NULL for jcr -> initialize global messages for daemon
 * non-NULL     -> initialize jcr using Message resource
 */
void InitMsg(JobControlRecord* jcr,
             MessagesResource* msg,
             job_code_callback_t job_code_callback)
{
  int i;

  if (jcr == NULL && msg == NULL) {
    /*
     * Setup a daemon key then set invalid jcr
     * Maybe we should give the daemon a jcr???
     */
    SetJcrInThreadSpecificData(nullptr);
  }

  message_job_code_callback = job_code_callback;

  if (!msg) {
    // initialize default chain for stdout and syslog
    daemon_msgs = new MessagesResource;
    for (i = 1; i <= M_MAX; i++) {
      daemon_msgs->AddMessageDestination(MessageDestinationCode::kStdout, i,
                                         std::string(), std::string(),
                                         std::string());
    }
    Dmsg1(050, "Create daemon global message resource %p\n", daemon_msgs);
    return;
  }

  if (jcr) {
    jcr->jcr_msgs = new MessagesResource;
    msg->DuplicateResourceTo(*jcr->jcr_msgs);
  } else {
    // replace the defaults
    if (daemon_msgs) { delete daemon_msgs; }
    daemon_msgs = new MessagesResource;
    msg->DuplicateResourceTo(*daemon_msgs);
  }

  Dmsg2(250, "Copied message resource %p\n", msg);
}

/*
 * Initialize so that the console (User Agent) can receive messages -- stored in
 * a file.
 */
void InitConsoleMsg(const char* wd)
{
  int fd;

  Bsnprintf(con_fname, sizeof(con_fname), "%s%c%s.conmsg", wd, PathSeparator,
            my_name);
  fd = open(con_fname, O_CREAT | O_RDWR | O_BINARY, 0600);
  if (fd == -1) {
    BErrNo be;
    Emsg2(M_ERROR_TERM, 0,
          _("Could not open console message file %s: ERR=%s\n"), con_fname,
          be.bstrerror());
  }
  if (lseek(fd, 0, SEEK_END) > 0) { console_msg_pending = 1; }
  close(fd);
  con_fd = fopen(con_fname, "a+b");
  if (!con_fd) {
    BErrNo be;
    Emsg2(M_ERROR, 0, _("Could not open console message file %s: ERR=%s\n"),
          con_fname, be.bstrerror());
  }
  if (RwlInit(&con_lock) != 0) {
    BErrNo be;
    Emsg1(M_ERROR_TERM, 0, _("Could not get con mutex: ERR=%s\n"),
          be.bstrerror());
  }
}

static void MakeUniqueMailFilename(JobControlRecord* jcr,
                                   POOLMEM*& name,
                                   MessageDestinationInfo* d)
{
  if (jcr) {
    Mmsg(name, "%s/%s.%s.%d.mail", working_directory, my_name, jcr->Job,
         (int)(intptr_t)d);
  } else {
    Mmsg(name, "%s/%s.%s.%d.mail", working_directory, my_name, my_name,
         (int)(intptr_t)d);
  }
  Dmsg1(850, "mailname=%s\n", name);
}

static Bpipe* open_mail_pipe(JobControlRecord* jcr,
                             POOLMEM*& cmd,
                             MessageDestinationInfo* d)
{
  Bpipe* bpipe;

  if (!d->mail_cmd_.empty()) {
    cmd = edit_job_codes(jcr, cmd, d->mail_cmd_.c_str(), d->where_.c_str(),
                         message_job_code_callback);
  } else {
    Mmsg(cmd, "/usr/lib/sendmail -F BAREOS %s", d->where_.c_str());
  }

  if ((bpipe = OpenBpipe(cmd, 120, "rw"))) {
    /*
     * If we had to use sendmail, add subject
     */
    if (d->mail_cmd_.empty()) {
      fprintf(bpipe->wfd, "Subject: %s\r\n\r\n", _("BAREOS Message"));
    }
  } else {
    BErrNo be;
    DeliveryError(_("open mail pipe %s failed: ERR=%s\n"), cmd, be.bstrerror());
  }

  return bpipe;
}

/*
 * Close the messages for this Messages resource, which means to close
 * any open files, and dispatch any pending email messages.
 */
void CloseMsg(JobControlRecord* jcr)
{
  MessagesResource* msgs;
  Bpipe* bpipe;
  POOLMEM *cmd, *line;
  int len, status;

  Dmsg1(580, "Close_msg jcr=%p\n", jcr);

  if (jcr == NULL) { /* NULL -> global chain */
    msgs = daemon_msgs;
  } else {
    msgs = jcr->jcr_msgs;
    jcr->jcr_msgs = NULL;
  }
  if (msgs == NULL) { return; }

  /*
   * Wait for item to be not in use, then mark closing
   */
  if (msgs->IsClosing()) { return; }
  msgs->WaitNotInUse(); /* leaves fides_mutex set */

  /*
   * Note GetClosing() does not lock because we are already locked
   */
  if (msgs->GetClosing()) {
    msgs->Unlock();
    return;
  }
  msgs->SetClosing();
  msgs->Unlock();

  Dmsg1(850, "===Begin close msg resource at %p\n", msgs);
  cmd = GetPoolMemory(PM_MESSAGE);
  for (MessageDestinationInfo* d : msgs->dest_chain_) {
    if (d->file_pointer_) {
      switch (d->dest_code_) {
        case MessageDestinationCode::kFile:
        case MessageDestinationCode::kAppend:
          if (d->file_pointer_) {
            fclose(d->file_pointer_); /* close open file descriptor */
            d->file_pointer_ = NULL;
          }
          break;
        case MessageDestinationCode::kMail:
        case MessageDestinationCode::KMailOnError:
        case MessageDestinationCode::kMailOnSuccess:
          Dmsg0(850, "Got kMail, KMailOnError or kMailOnSuccess\n");
          if (!d->file_pointer_) { break; }

          switch (d->dest_code_) {
            case MessageDestinationCode::KMailOnError:
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
            case MessageDestinationCode::kMailOnSuccess:
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

          if (!(bpipe = open_mail_pipe(jcr, cmd, d))) {
            Pmsg0(000, _("open mail pipe failed.\n"));
            goto rem_temp_file;
          }

          Dmsg0(850, "Opened mail pipe\n");
          len = d->max_len_ + 10;
          line = GetMemory(len);
          rewind(d->file_pointer_);
          while (fgets(line, len, d->file_pointer_)) {
            fputs(line, bpipe->wfd);
          }
          if (!CloseWpipe(bpipe)) { /* close write pipe sending mail */
            BErrNo be;
            Pmsg1(000, _("close error: ERR=%s\n"), be.bstrerror());
          }

          /*
           * Since we are closing all messages, before "recursing"
           * make sure we are not closing the daemon messages, otherwise
           * kaboom.
           */
          if (msgs != daemon_msgs) {
            /*
             * Read what mail prog returned -- should be nothing
             */
            while (fgets(line, len, bpipe->rfd)) {
              DeliveryError(_("Mail prog: %s"), line);
            }
          }

          status = CloseBpipe(bpipe);
          if (status != 0 && msgs != daemon_msgs) {
            BErrNo be;
            be.SetErrno(status);
            Dmsg1(850, "Calling emsg. CMD=%s\n", cmd);
            DeliveryError(_("Mail program terminated in error.\n"
                            "CMD=%s\n"
                            "ERR=%s\n"),
                          cmd, be.bstrerror());
          }
          FreeMemory(line);
        rem_temp_file:
          /*
           * Remove temp file
           */
          if (d->file_pointer_) {
            fclose(d->file_pointer_);
            d->file_pointer_ = NULL;
          }
          if (!d->mail_filename_.empty()) {
            /*
             * Exclude spaces in mail_filename
             */
            SaferUnlink(d->mail_filename_.c_str(), MAIL_REGEX);
            d->mail_filename_.clear();
          }
          Dmsg0(850, "end mail or mail on error\n");
          break;
        default:
          break;
      }
      d->file_pointer_ = NULL;
    }
  }
  FreePoolMemory(cmd);
  Dmsg0(850, "Done walking message chain.\n");
  if (jcr) {
    delete msgs;
    msgs = NULL;
  } else {
    msgs->ClearClosing();
  }
  Dmsg0(850, "===End close msg resource\n");
}

/*
 * Terminate the message handler for good.
 * Release the global destination chain.
 *
 * Also, clean up a few other items (cons, exepath). Note,
 * these really should be done elsewhere.
 */
void TermMsg()
{
  Dmsg0(850, "Enter TermMsg\n");
  CloseMsg(NULL);     /* close global chain */
  delete daemon_msgs; /* f ree the resources */
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
  RecentJobResultsList::Cleanup();
  CleanupJcrChain();
}

static inline bool OpenDestFile(MessageDestinationInfo* d, const char* mode)
{
  d->file_pointer_ = fopen(d->where_.c_str(), mode);
  if (!d->file_pointer_) {
    BErrNo be;
    DeliveryError(_("fopen %s failed: ERR=%s\n"), d->where_.c_str(),
                  be.bstrerror());
    return false;
  }

  return true;
}

static struct syslog_facility_name {
  const char* name;
  int facility;
} syslog_facility_names[] = {
    {"kern", LOG_KERN},         {"user", LOG_USER},     {"mail", LOG_MAIL},
    {"daemon", LOG_DAEMON},     {"auth", LOG_AUTH},     {"syslog", LOG_SYSLOG},
    {"lpr", LOG_LPR},           {"news", LOG_NEWS},     {"uucp", LOG_UUCP},
#ifdef LOG_CRON
    {"cron", LOG_CRON},
#endif
#ifdef LOG_AUTHPRIV
    {"authpriv", LOG_AUTHPRIV},
#endif
#ifdef LOG_FTP
    {"ftp", LOG_FTP},
#endif
#ifdef LOG_NTP
    {"ntp", LOG_NTP},
#endif
#ifdef LOG_AUDIT
    {"audit", LOG_AUDIT},
#endif
#ifdef LOG_SECURITY
    {"security", LOG_SECURITY},
#endif
#ifdef LOG_CONSOLE
    {"console", LOG_CONSOLE},
#endif
    {"local0", LOG_LOCAL0},     {"local1", LOG_LOCAL1}, {"local2", LOG_LOCAL2},
    {"local3", LOG_LOCAL3},     {"local4", LOG_LOCAL4}, {"local5", LOG_LOCAL5},
    {"local6", LOG_LOCAL6},     {"local7", LOG_LOCAL7}, {NULL, -1}};

static inline bool SetSyslogFacility(JobControlRecord* jcr,
                                     MessageDestinationInfo* d)
{
  int i;

  if (!d->where_.empty()) {
    for (i = 0; syslog_facility_names[i].name; i++) {
      if (Bstrcasecmp(d->where_.c_str(), syslog_facility_names[i].name)) {
        d->syslog_facility_ = syslog_facility_names[i].facility;
        i = 0;
        break;
      }
    }

    /*
     * Make sure we got a match otherwise fallback to LOG_DAEMON
     */
    if (i != 0) { d->syslog_facility_ = LOG_DAEMON; }
  } else {
    d->syslog_facility_ = LOG_DAEMON;
  }

  return true;
}

/*
 * Split the output for syslog (it converts \n to ' ' and is
 * limited to 1024 characters per syslog message
 */
static void SendToSyslog_(int mode, const char* msg)
{
  int len;
  char buf[1024];
  const char* p2;
  const char* p = msg;

  while (*p && ((p2 = strchr(p, '\n')) != NULL)) {
    len = MIN((int)sizeof(buf) - 1, p2 - p + 1); /* Add 1 to keep \n */
    strncpy(buf, p, len);
    buf[len] = 0;
    syslog(mode, "%s", buf);
    p = p2 + 1; /* skip \n */
  }
  if (*p != 0) { /* no \n at the end ? */
    syslog(mode, "%s", p);
  }
}

static SyslogCallback SendToSyslog{SendToSyslog_};
void RegisterSyslogCallback(SyslogCallback c) { SendToSyslog = c; }

static DbLogInsertCallback SendToDbLog = NULL;
void SetDbLogInsertCallback(DbLogInsertCallback f) { SendToDbLog = f; }

/*
 * Handle sending the message to the appropriate place
 */
void DispatchMessage(JobControlRecord* jcr,
                     int type,
                     utime_t mtime,
                     const char* msg)
{
  char dt[MAX_TIME_LENGTH];
  POOLMEM* mcmd;
  int len, dtlen;
  MessagesResource* msgs;
  Bpipe* bpipe;
  const char* mode;
  bool dt_conversion = false;

  Dmsg2(850, "Enter DispatchMessage type=%d msg=%s", type, msg);

  /*
   * Most messages are prefixed by a date and time. If mtime is
   * zero, then we use the current time.  If mtime is 1 (special
   * kludge), we do not prefix the date and time. Otherwise,
   * we assume mtime is a utime_t and use it.
   */
  if (mtime == 0) { mtime = time(NULL); }

  *dt = 0;
  dtlen = 0;
  if (mtime == 1) {
    mtime = time(NULL); /* Get time for SQL log */
  } else {
    dt_conversion = true;
  }

  /*
   * If the program registered a callback, send it there
   */
  if (message_callback) {
    message_callback(type, msg);
    return;
  }

  /*
   * For serious errors make sure message is printed or logged
   */
  if (type == M_ABORT || type == M_ERROR_TERM) {
    fputs(dt, stdout);
    fputs(msg, stdout);
    fflush(stdout);
    if (type == M_ABORT) { syslog(LOG_DAEMON | LOG_ERR, "%s", msg); }
  }

  /*
   * Now figure out where to send the message
   */
  msgs = NULL;
  if (!jcr) { jcr = GetJcrFromThreadSpecificData(); }

  if (jcr) {
    /*
     * See if we need to suppress the messages.
     */
    if (jcr->suppress_output) {
      /*
       * See if this JobControlRecord has a controlling JobControlRecord and if
       * so redirect this message to the controlling JobControlRecord.
       */
      if (jcr->cjcr) {
        jcr = jcr->cjcr;
      } else {
        /*
         * Ignore this Job Message.
         */
        return;
      }
    }
    msgs = jcr->jcr_msgs;
  }

  if (msgs == NULL) { msgs = daemon_msgs; }

  if (!msgs) {
    Dmsg1(100, "Could not dispatch message: %s", msg);
    return;
  }

  /*
   * If closing this message resource, print and send to syslog, then get out.
   */
  if (msgs->IsClosing()) {
    if (dt_conversion) {
      bstrftime(dt, sizeof(dt), mtime, log_timestamp_format);
      bstrncat(dt, " ", sizeof(dt));
    }
    fputs(dt, stdout);
    fputs(msg, stdout);
    fflush(stdout);
    syslog(LOG_DAEMON | LOG_ERR, "%s", msg);
    return;
  }

  for (MessageDestinationInfo* d : msgs->dest_chain_) {
    if (BitIsSet(type, d->msg_types_)) {
      /*
       * See if a specific timestamp format was specified for this log resource.
       * Otherwise apply the global setting in log_timestamp_format.
       */
      if (dt_conversion) {
        if (!d->timestamp_format_.empty()) {
          bstrftime(dt, sizeof(dt), mtime, d->timestamp_format_.c_str());
        } else {
          bstrftime(dt, sizeof(dt), mtime, log_timestamp_format);
        }
        bstrncat(dt, " ", sizeof(dt));
        dtlen = strlen(dt);
      }

      switch (d->dest_code_) {
        case MessageDestinationCode::kCatalog:
          if (!jcr || !jcr->db) { break; }

          if (SendToDbLog) {
            if (!SendToDbLog(jcr, mtime, msg)) {
              DeliveryError(
                  _("Msg delivery error: Unable to store data in database.\n"));
            }
          }
          break;
        case MessageDestinationCode::kConsole:
          Dmsg1(850, "CONSOLE for following msg: %s", msg);
          if (!con_fd) {
            con_fd = fopen(con_fname, "a+b");
            Dmsg0(850, "Console file not open.\n");
          }
          if (con_fd) {
            Pw(con_lock); /* get write lock on console message file */
            errno = 0;
            if (dtlen) { (void)fwrite(dt, dtlen, 1, con_fd); }
            len = strlen(msg);
            if (len > 0) {
              (void)fwrite(msg, len, 1, con_fd);
              if (msg[len - 1] != '\n') { (void)fwrite("\n", 2, 1, con_fd); }
            } else {
              (void)fwrite("\n", 2, 1, con_fd);
            }
            fflush(con_fd);
            console_msg_pending = true;
            Vw(con_lock);
          }
          break;
        case MessageDestinationCode::kSyslog:
          Dmsg1(850, "SYSLOG for following msg: %s\n", msg);

          if (!d->syslog_facility_ && !SetSyslogFacility(jcr, d)) {
            msgs->ClearInUse();
            break;
          }

          /*
           * Dispatch based on our internal message type to a matching syslog
           * one.
           */
          switch (type) {
            case M_ERROR:
            case M_ERROR_TERM:
              SendToSyslog(d->syslog_facility_ | LOG_ERR, msg);
              break;
            case M_ABORT:
            case M_FATAL:
              SendToSyslog(d->syslog_facility_ | LOG_CRIT, msg);
              break;
            case M_WARNING:
              SendToSyslog(d->syslog_facility_ | LOG_WARNING, msg);
              break;
            case M_DEBUG:
              SendToSyslog(d->syslog_facility_ | LOG_DEBUG, msg);
              break;
            case M_INFO:
            case M_NOTSAVED:
            case M_RESTORED:
            case M_SAVED:
            case M_SKIPPED:
            case M_TERM:
              SendToSyslog(d->syslog_facility_ | LOG_INFO, msg);
              break;
            case M_ALERT:
            case M_AUDIT:
            case M_MOUNT:
            case M_SECURITY:
            case M_VOLMGMT:
              SendToSyslog(d->syslog_facility_ | LOG_NOTICE, msg);
              break;
            default:
              SendToSyslog(d->syslog_facility_ | LOG_ERR, msg);
              break;
          }
          break;
        case MessageDestinationCode::kOperator:
          Dmsg1(850, "OPERATOR for following msg: %s\n", msg);
          mcmd = GetPoolMemory(PM_MESSAGE);
          if ((bpipe = open_mail_pipe(jcr, mcmd, d))) {
            int status;
            fputs(dt, bpipe->wfd);
            fputs(msg, bpipe->wfd);
            /*
             * Messages to the operator go one at a time
             */
            status = CloseBpipe(bpipe);
            if (status != 0) {
              BErrNo be;
              be.SetErrno(status);
              DeliveryError(_("Msg delivery error: Operator mail program "
                              "terminated in error.\n"
                              "CMD=%s\nERR=%s\n"),
                            mcmd, be.bstrerror());
            }
          }
          FreePoolMemory(mcmd);
          break;
        case MessageDestinationCode::kMail:
        case MessageDestinationCode::KMailOnError:
        case MessageDestinationCode::kMailOnSuccess:
          Dmsg1(850, "MAIL for following msg: %s", msg);
          if (msgs->IsClosing()) { break; }
          msgs->SetInUse();
          if (!d->file_pointer_) {
            POOLMEM* name = GetPoolMemory(PM_MESSAGE);
            MakeUniqueMailFilename(jcr, name, d);
            d->file_pointer_ = fopen(name, "w+b");
            if (!d->file_pointer_) {
              BErrNo be;
              DeliveryError(_("Msg delivery error: fopen %s failed: ERR=%s\n"),
                            name, be.bstrerror());
              FreePoolMemory(name);
              msgs->ClearInUse();
              break;
            }
            d->mail_filename_ = name;
          }
          fputs(dt, d->file_pointer_);
          len = strlen(msg) + dtlen;
          if (len > d->max_len_) {
            d->max_len_ = len; /* keep max line length */
          }
          fputs(msg, d->file_pointer_);
          msgs->ClearInUse();
          break;
        case MessageDestinationCode::kAppend:
          Dmsg1(850, "APPEND for following msg: %s", msg);
          mode = "ab";
          goto send_to_file;
        case MessageDestinationCode::kFile:
          Dmsg1(850, "FILE for following msg: %s", msg);
          mode = "w+b";
        send_to_file:
          if (msgs->IsClosing()) { break; }
          msgs->SetInUse();
          if (!d->file_pointer_ && !OpenDestFile(d, mode)) {
            msgs->ClearInUse();
            break;
          }
          fputs(dt, d->file_pointer_);
          fputs(msg, d->file_pointer_);
          /*
           * On error, we close and reopen to handle log rotation
           */
          if (ferror(d->file_pointer_)) {
            fclose(d->file_pointer_);
            d->file_pointer_ = NULL;
            if (OpenDestFile(d, mode)) {
              fputs(dt, d->file_pointer_);
              fputs(msg, d->file_pointer_);
            }
          }
          fflush(d->file_pointer_);
          msgs->ClearInUse();
          break;
        case MessageDestinationCode::kDirector:
          Dmsg1(850, "DIRECTOR for following msg: %s", msg);
          if (jcr && jcr->dir_bsock && !jcr->dir_bsock->errors) {
            jcr->dir_bsock->fsend("Jmsg Job=%s type=%d level=%lld %s", jcr->Job,
                                  type, mtime, msg);
          } else {
            Dmsg1(800, "no jcr for following msg: %s", msg);
          }
          break;
        case MessageDestinationCode::kStdout:
          Dmsg1(850, "STDOUT for following msg: %s", msg);
          if (type != M_ABORT && type != M_ERROR_TERM) { /* already printed */
            fputs(dt, stdout);
            fputs(msg, stdout);
            fflush(stdout);
          }
          break;
        case MessageDestinationCode::kStderr:
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

/*
 * This subroutine returns the filename portion of a path.
 * It is used because some compilers set __FILE__
 * to the full path.  Try to return base + next higher path.
 */
const char* get_basename(const char* pathname)
{
  const char* basename;

  if ((basename = bstrrpath(pathname, pathname + strlen(pathname)))
      == pathname) {
    /* empty */
  } else if ((basename = bstrrpath(pathname, basename - 1)) == pathname) {
    /* empty */
  } else {
    basename++;
  }
  return basename;
}

/*
 * Print or write output to trace file
 */
static void pt_out(char* buf)
{
  /*
   * Used the "trace on" command in the console to turn on
   * output to the trace file.  "trace off" will close the file.
   */
  if (trace) {
    if (!trace_fd) {
      PoolMem fn(PM_FNAME);

      Mmsg(fn, "%s/%s.trace", TRACEFILEDIRECTORY, my_name);
      trace_fd = fopen(fn.c_str(), "a+b");
    }
    if (trace_fd) {
      fputs(buf, trace_fd);
      fflush(trace_fd);
      return;
    } else {
      /*
       * Some problem, turn off tracing
       */
      trace = false;
    }
  }

  /*
   * Not tracing
   */
  fputs(buf, stdout);
  fflush(stdout);
}

/*
 *  This subroutine prints a debug message if the level number is less than or
 *  equal the debug_level. File and line numbers are included for more detail if
 *  desired, but not currently printed.
 *
 *  If the level is negative, the details of file and line number are not
 * printed.
 */
void d_msg(const char* file, int line, int level, const char* fmt, ...)
{
  va_list ap;
  char ed1[50];
  int len, maxlen;
  btime_t mtime;
  uint32_t usecs;
  bool details = true;
  PoolMem buf(PM_EMSG), more(PM_EMSG);

  if (level < 0) {
    details = false;
    level = -level;
  }

  if (level <= debug_level) {
    if (dbg_timestamp) {
      mtime = GetCurrentBtime();
      usecs = mtime % 1000000;
      Mmsg(buf, "%s.%06d ", bstrftimes(ed1, sizeof(ed1), BtimeToUtime(mtime)),
           usecs);
      pt_out(buf.c_str());
    }

    if (details) {
      Mmsg(buf, "%s (%d): %s:%d-%u ", my_name, level, get_basename(file), line,
           GetJobIdFromThreadSpecificData());
    }

    while (1) {
      maxlen = more.MaxSize() - 1;
      va_start(ap, fmt);
      len = Bvsnprintf(more.c_str(), maxlen, fmt, ap);
      va_end(ap);

      if (len < 0 || len >= (maxlen - 5)) {
        more.ReallocPm(maxlen + maxlen / 2);
        continue;
      }

      break;
    }

    if (details) { pt_out(buf.c_str()); }

    pt_out(more.c_str());
  }
}

/*
 * Set trace flag on/off. If argument is negative, there is no change
 */
void SetTrace(int trace_flag)
{
  if (trace_flag < 0) {
    return;
  } else if (trace_flag > 0) {
    trace = true;
  } else {
    trace = false;
  }

  if (!trace && trace_fd) {
    FILE* ltrace_fd = trace_fd;
    trace_fd = NULL;
    Bmicrosleep(0, 100000); /* yield to prevent seg faults */
    fclose(ltrace_fd);
  }
}

void SetHangup(int hangup_value)
{
  if (hangup_value < 0) {
    return;
  } else {
    hangup = hangup_value;
  }
}

void SetTimestamp(int timestamp_flag)
{
  if (timestamp_flag < 0) {
    return;
  } else if (timestamp_flag > 0) {
    dbg_timestamp = true;
  } else {
    dbg_timestamp = false;
  }
}

bool GetHangup(void) { return hangup; }

bool GetTrace(void) { return trace; }

bool GetTimestamp(void) { return dbg_timestamp; }

/*
 * This subroutine prints a message regardless of the debug level
 *
 * If the level is negative, the details of file and line number are not
 * printed.
 */
void p_msg(const char* file, int line, int level, const char* fmt, ...)
{
  va_list ap;
  int len, maxlen;
  PoolMem buf(PM_EMSG), more(PM_EMSG);

  if (level >= 0) {
    Mmsg(buf, "%s: %s:%d-%u ", my_name, get_basename(file), line,
         GetJobIdFromThreadSpecificData());
  }

  while (1) {
    maxlen = more.MaxSize() - 1;
    va_start(ap, fmt);
    len = Bvsnprintf(more.c_str(), maxlen, fmt, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      more.ReallocPm(maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  if (level >= 0) { pt_out(buf.c_str()); }

  pt_out(more.c_str());
}

/*
 * This subroutine prints a message regardless of the debug level
 *
 * If the level is negative, the details of file and line number are not
 * printed. Special version of p_msg used with fixed buffers.
 */
void p_msg_fb(const char* file, int line, int level, const char* fmt, ...)
{
  char buf[256];
  int len = 0;
  va_list arg_ptr;

  if (level >= 0) {
    len = Bsnprintf(buf, sizeof(buf), "%s: %s:%d-%u ", my_name,
                    get_basename(file), line, GetJobIdFromThreadSpecificData());
  }

  va_start(arg_ptr, fmt);
  Bvsnprintf(buf + len, sizeof(buf) - len, (char*)fmt, arg_ptr);
  va_end(arg_ptr);

  pt_out(buf);
}

/*
 * Subroutine writes a debug message to the trace file if the level number is
 * less than or equal the debug_level. File and line numbers are included for
 * more detail if desired, but not currently printed.
 *
 * If the level is negative, the details of file and line number are not
 * printed.
 */
void t_msg(const char* file, int line, int level, const char* fmt, ...)
{
  va_list ap;
  int len, maxlen;
  bool details = true;
  PoolMem buf(PM_EMSG), more(PM_EMSG);

  if (level < 0) {
    details = false;
    level = -level;
  }

  if (level <= debug_level) {
    if (!trace_fd) {
      PoolMem fn(PM_FNAME);

      Mmsg(fn, "%s/%s.trace", TRACEFILEDIRECTORY, my_name);
      trace_fd = fopen(fn.c_str(), "a+b");
    }

    if (details) {
      Mmsg(buf, "%s: %s:%d-%u ", my_name, get_basename(file), line,
           GetJobIdFromThreadSpecificData());
    }

    while (1) {
      maxlen = more.MaxSize() - 1;
      va_start(ap, fmt);
      len = Bvsnprintf(more.c_str(), maxlen, fmt, ap);
      va_end(ap);

      if (len < 0 || len >= (maxlen - 5)) {
        more.ReallocPm(maxlen + maxlen / 2);
        continue;
      }

      break;
    }

    if (trace_fd != NULL) {
      if (details) { fputs(buf.c_str(), trace_fd); }
      fputs(more.c_str(), trace_fd);
      fflush(trace_fd);
    }
  }
}

/*
 * print an error message
 */
void e_msg(const char* file,
           int line,
           int type,
           int level,
           const char* fmt,
           ...)
{
  va_list ap;
  int len, maxlen;
  PoolMem buf(PM_EMSG), more(PM_EMSG), typestr(PM_EMSG);

  switch (type) {
    case M_ABORT:
      Mmsg(typestr, "ABORT");
      Mmsg(buf, _("%s: ABORTING due to ERROR in %s:%d\n"), my_name,
           get_basename(file), line);
      break;
    case M_ERROR_TERM:
      Mmsg(typestr, "ERROR TERMINATION");
      Mmsg(buf, _("%s: ERROR TERMINATION at %s:%d\n"), my_name,
           get_basename(file), line);
      break;
    case M_FATAL:
      Mmsg(typestr, "FATAL ERROR");
      if (level == -1) /* skip details */
        Mmsg(buf, _("%s: Fatal Error because: "), my_name);
      else
        Mmsg(buf, _("%s: Fatal Error at %s:%d because:\n"), my_name,
             get_basename(file), line);
      break;
    case M_ERROR:
      Mmsg(typestr, "ERROR");
      if (level == -1) /* skip details */
        Mmsg(buf, _("%s: ERROR: "), my_name);
      else
        Mmsg(buf, _("%s: ERROR in %s:%d "), my_name, get_basename(file), line);
      break;
    case M_WARNING:
      Mmsg(typestr, "WARNING");
      Mmsg(buf, _("%s: Warning: "), my_name);
      break;
    case M_SECURITY:
      Mmsg(typestr, "Security violation");
      Mmsg(buf, _("%s: Security violation: "), my_name);
      break;
    default:
      Mmsg(buf, "%s: ", my_name);
      break;
  }

  while (1) {
    maxlen = more.MaxSize() - 1;
    va_start(ap, fmt);
    len = Bvsnprintf(more.c_str(), maxlen, fmt, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      more.ReallocPm(maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  /*
   * show error message also as debug message (level 10)
   */
  d_msg(file, line, 10, "%s: %s", typestr.c_str(), more.c_str());

  /*
   * Check if we have a message destination defined.
   * We always report M_ABORT and M_ERROR_TERM
   */
  if (!daemon_msgs
      || ((type != M_ABORT && type != M_ERROR_TERM)
          && !BitIsSet(type, daemon_msgs->send_msg_types_))) {
    return; /* no destination */
  }

  PmStrcat(buf, more.c_str());
  DispatchMessage(NULL, type, 0, buf.c_str());

  if (type == M_ABORT) {
    abort();
  } else if (type == M_ERROR_TERM) {
    exit(1);
  }
}

/*
 * Generate a Job message
 */
void Jmsg(JobControlRecord* jcr, int type, utime_t mtime, const char* fmt, ...)
{
  va_list ap;
  MessagesResource* msgs;
  int len, maxlen;
  uint32_t JobId = 0;
  PoolMem buf(PM_EMSG), more(PM_EMSG);

  Dmsg1(850, "Enter Jmsg type=%d\n", type);

  /*
   * Special case for the console, which has a dir_bsock and JobId == 0,
   * in that case, we send the message directly back to the
   * dir_bsock.
   */
  if (jcr && jcr->JobId == 0 && jcr->dir_bsock) {
    BareosSocket* dir = jcr->dir_bsock;

    va_start(ap, fmt);
    dir->message_length
        = Bvsnprintf(dir->msg, SizeofPoolMemory(dir->msg), fmt, ap);
    va_end(ap);
    jcr->dir_bsock->send();

    return;
  }

  /*
   * The watchdog thread can't use Jmsg directly, we always queued it
   */
  if (IsWatchdog()) {
    while (1) {
      maxlen = buf.MaxSize() - 1;
      va_start(ap, fmt);
      len = Bvsnprintf(buf.c_str(), maxlen, fmt, ap);
      va_end(ap);

      if (len < 0 || len >= (maxlen - 5)) {
        buf.ReallocPm(maxlen + maxlen / 2);
        continue;
      }

      break;
    }
    Qmsg(jcr, type, mtime, "%s", buf.c_str());

    return;
  }

  msgs = NULL;
  if (!jcr) { jcr = GetJcrFromThreadSpecificData(); }

  if (jcr) {
    /*
     * Dequeue messages to keep the original order
     */
    if (!jcr->dequeuing_msgs) { /* Avoid recursion */
      DequeueMessages(jcr);
    }
    msgs = jcr->jcr_msgs;
    JobId = jcr->JobId;
  }

  if (!msgs) { msgs = daemon_msgs; /* if no jcr, we use daemon handler */ }

  /*
   * Check if we have a message destination defined.
   * We always report M_ABORT and M_ERROR_TERM
   */
  if (msgs && (type != M_ABORT && type != M_ERROR_TERM)
      && !BitIsSet(type, msgs->send_msg_types_)) {
    return; /* no destination */
  }

  switch (type) {
    case M_ABORT:
      Mmsg(buf, _("%s ABORTING due to ERROR\n"), my_name);
      break;
    case M_ERROR_TERM:
      Mmsg(buf, _("%s ERROR TERMINATION\n"), my_name);
      break;
    case M_FATAL:
      Mmsg(buf, _("%s JobId %u: Fatal error: "), my_name, JobId);
      if (jcr) { jcr->setJobStatus(JS_FatalError); }
      if (jcr && jcr->JobErrors == 0) { jcr->JobErrors = 1; }
      break;
    case M_ERROR:
      Mmsg(buf, _("%s JobId %u: Error: "), my_name, JobId);
      if (jcr) { jcr->JobErrors++; }
      break;
    case M_WARNING:
      Mmsg(buf, _("%s JobId %u: Warning: "), my_name, JobId);
      if (jcr) { jcr->JobWarnings++; }
      break;
    case M_SECURITY:
      Mmsg(buf, _("%s JobId %u: Security violation: "), my_name, JobId);
      break;
    default:
      Mmsg(buf, "%s JobId %u: ", my_name, JobId);
      break;
  }

  while (1) {
    maxlen = more.MaxSize() - 1;
    va_start(ap, fmt);
    len = Bvsnprintf(more.c_str(), maxlen, fmt, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      more.ReallocPm(maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  PmStrcat(buf, more.c_str());
  DispatchMessage(jcr, type, mtime, buf.c_str());

  if (type == M_ABORT) {
    printf("BAREOS aborting to obtain traceback.\n");
    syslog(LOG_DAEMON | LOG_ERR, "BAREOS aborting to obtain traceback.\n");
    abort();
  } else if (type == M_ERROR_TERM) {
    exit(1);
  }
}

/*
 * If we come here, prefix the message with the file:line-number,
 * then pass it on to the normal Jmsg routine.
 */
void j_msg(const char* file,
           int line,
           JobControlRecord* jcr,
           int type,
           utime_t mtime,
           const char* fmt,
           ...)
{
  va_list ap;
  int len, maxlen;
  PoolMem buf(PM_EMSG), more(PM_EMSG);

  Mmsg(buf, "%s:%d ", get_basename(file), line);
  while (1) {
    maxlen = more.MaxSize() - 1;
    va_start(ap, fmt);
    len = Bvsnprintf(more.c_str(), maxlen, fmt, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      more.ReallocPm(maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  PmStrcat(buf, more.c_str());

  Jmsg(jcr, type, mtime, "%s", buf.c_str());
}

/*
 * Edit a message into a Pool memory buffer, with file:lineno
 */
int msg_(const char* file, int line, POOLMEM*& pool_buf, const char* fmt, ...)
{
  va_list ap;
  int len, maxlen;
  PoolMem buf(PM_EMSG), more(PM_EMSG);

  Mmsg(buf, "%s:%d ", get_basename(file), line);
  while (1) {
    maxlen = more.MaxSize() - 1;
    va_start(ap, fmt);
    len = Bvsnprintf(more.c_str(), maxlen, fmt, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      more.ReallocPm(maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  PmStrcpy(pool_buf, buf.c_str());
  len = PmStrcat(pool_buf, more.c_str());

  return len;
}

/*
 * Edit a message into a Pool Memory buffer NO file:lineno
 *
 * Returns: string length of what was edited.
 *          -1 when the buffer isn't enough to hold the edited string.
 */
int Mmsg(POOLMEM*& pool_buf, const char* fmt, ...)
{
  int len, maxlen;
  va_list ap;

  while (1) {
    maxlen = SizeofPoolMemory(pool_buf) - 1;
    va_start(ap, fmt);
    len = Bvsnprintf(pool_buf, maxlen, fmt, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      pool_buf = ReallocPoolMemory(pool_buf, maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  return len;
}

int Mmsg(PoolMem& pool_buf, const char* fmt, ...)
{
  int len, maxlen;
  va_list ap;

  while (1) {
    maxlen = pool_buf.MaxSize() - 1;
    va_start(ap, fmt);
    len = Bvsnprintf(pool_buf.c_str(), maxlen, fmt, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      pool_buf.ReallocPm(maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  return len;
}

int Mmsg(PoolMem*& pool_buf, const char* fmt, ...)
{
  int len, maxlen;
  va_list ap;

  while (1) {
    maxlen = pool_buf->MaxSize() - 1;
    va_start(ap, fmt);
    len = Bvsnprintf(pool_buf->c_str(), maxlen, fmt, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      pool_buf->ReallocPm(maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  return len;
}

int Mmsg(std::vector<char>& msgbuf, const char* fmt, ...)
{
  va_list ap;

  size_t maxlen = msgbuf.size();
  size_t len = strlen(fmt);

  /* resize msgbuf so at least fmt fits in there.
   * this makes sure the rest of the code works with a zero-sized vector
   */
  if (maxlen < len) {
    msgbuf.resize(len);
    maxlen = len;
  }

  while (1) {
    va_start(ap, fmt);
    len = Bvsnprintf(msgbuf.data(), maxlen, fmt, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      maxlen += maxlen / 2;
      msgbuf.resize(maxlen);
      continue;
    }
    return len;
  }
}

/*
 * We queue messages rather than print them directly. This
 * is generally used in low level routines (msg handler, bnet)
 * to prevent recursion (i.e. if you are in the middle of
 * sending a message, it is a bit messy to recursively call
 * yourself when the bnet packet is not reentrant).
 */
void Qmsg(JobControlRecord* jcr, int type, utime_t mtime, const char* fmt, ...)
{
  va_list ap;
  int len, maxlen;
  PoolMem buf(PM_EMSG);
  MessageQueueItem* item;

  while (1) {
    maxlen = buf.MaxSize() - 1;
    va_start(ap, fmt);
    len = Bvsnprintf(buf.c_str(), maxlen, fmt, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      buf.ReallocPm(maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  item = (MessageQueueItem*)malloc(sizeof(MessageQueueItem));
  item->type_ = type;
  item->mtime_ = time(NULL);
  item->msg_ = new std::string(buf.c_str());

  if (!jcr) { jcr = GetJcrFromThreadSpecificData(); }

  /*
   * If no jcr  or no JobId or no queue or dequeuing send to syslog
   */
  if (!jcr || !jcr->JobId || !jcr->msg_queue || jcr->dequeuing_msgs) {
    syslog(LOG_DAEMON | LOG_ERR, "%s", item->msg_->c_str());
    delete item->msg_;
    free(item);
  } else {
    /*
     * Queue message for later sending
     */
    P(jcr->msg_queue_mutex);
    jcr->msg_queue->append(item);
    V(jcr->msg_queue_mutex);
  }
}

/*
 * Dequeue messages
 */
void DequeueMessages(JobControlRecord* jcr)
{
  MessageQueueItem* item;

  if (!jcr->msg_queue) { return; }

  P(jcr->msg_queue_mutex);
  jcr->dequeuing_msgs = true;
  foreach_dlist (item, jcr->msg_queue) {
    Jmsg(jcr, item->type_, item->mtime_, "%s", item->msg_->c_str());
    delete (item->msg_);
  }

  /*
   * Remove messages just sent
   */
  jcr->msg_queue->destroy();
  jcr->dequeuing_msgs = false;
  V(jcr->msg_queue_mutex);
}

/*
 * If we come here, prefix the message with the file:line-number,
 * then pass it on to the normal Qmsg routine.
 */
void q_msg(const char* file,
           int line,
           JobControlRecord* jcr,
           int type,
           utime_t mtime,
           const char* fmt,
           ...)
{
  va_list ap;
  int len, maxlen;
  PoolMem buf(PM_EMSG), more(PM_EMSG);

  Mmsg(buf, "%s:%d ", get_basename(file), line);
  while (1) {
    maxlen = more.MaxSize() - 1;
    va_start(ap, fmt);
    len = Bvsnprintf(more.c_str(), maxlen, fmt, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      more.ReallocPm(maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  PmStrcat(buf, more.c_str());

  Qmsg(jcr, type, mtime, "%s", buf.c_str());
}

/*
 * Set gobal date format used for log messages.
 */
void SetLogTimestampFormat(const char* format)
{
  log_timestamp_format = format;
}
