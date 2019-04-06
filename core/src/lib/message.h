/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * Kern Sibbald, 2000
 */
/**
 * @file
 * Define Message Types for BAREOS
 */

#ifndef BAREOS_LIB_MESSAGE_H_
#define BAREOS_LIB_MESSAGE_H_

#include "lib/bits.h"
#include "lib/dlink.h"
#include "lib/rwlock.h"

#include <string>

#undef M_DEBUG
#undef M_ABORT
#undef M_FATAL
#undef M_ERROR
#undef M_WARNING
#undef M_INFO
#undef M_MOUNT
#undef M_ERROR_TERM
#undef M_TERM
#undef M_RESTORED
#undef M_SECURITY
#undef M_ALERT
#undef M_VOLMGMT
#undef M_AUDIT

/**
 * Most of these message levels are more or less obvious.
 * They have evolved somewhat during the development of BAREOS,
 * and here are some of the details of where I am trying to
 * head (in the process of changing the code) as of 15 June 2002.
 *
 * M_ABORT       BAREOS immediately aborts and tries to produce a traceback
 *               This is for really serious errors like segmentation fault.
 * M_ERROR_TERM  BAREOS immediately terminates but no dump. This is for
 *               "obvious" serious errors like daemon already running or
 *               cannot open critical file, ... where a dump is not wanted.
 * M_TERM        BAREOS daemon shutting down because of request (SIGTERM).
 * M_DEBUG       Debug Messages
 *
 * The remaining apply to Jobs rather than the daemon.
 *
 * M_FATAL       BAREOS detected a fatal Job error. The Job will be killed,
 *               but BAREOS continues running.
 * M_ERROR       BAREOS detected a Job error. The Job will continue running
 *               but the termination status will be error.
 * M_WARNING     Job warning message.
 * M_INFO        Job information message.
 * M_SAVED       Info on saved file
 * M_NOTSAVED    Info on not saved file
 * M_RESTORED    An ls -l of each restored file.
 * M_SKIPPED     File skipped during backup by option setting
 * M_SECURITY    For security viloations. This is equivalent to FATAL.
 * M_ALERT       For Tape Alert messages.
 * M_VOLMGMT     Volume Management message
 * M_AUDIT       Auditing message
 * M_MOUNT       Mount requests
 */
enum
{
  /*
   * Keep M_ABORT=1 for dlist.h
   */
  M_ABORT = 1,
  M_DEBUG,
  M_FATAL,
  M_ERROR,
  M_WARNING,
  M_INFO,
  M_SAVED,
  M_NOTSAVED,
  M_SKIPPED,
  M_MOUNT,
  M_ERROR_TERM,
  M_TERM,
  M_RESTORED,
  M_SECURITY,
  M_ALERT,
  M_VOLMGMT,
  M_AUDIT
};

#define M_MAX M_AUDIT /* keep this updated ! */
#define NR_MSG_TYPES NbytesForBits(M_MAX + 1)

/**
 * Define message destination structure
 */
/* *** FIXME **** where should be extended to handle multiple values */
struct DEST {
 public:
  DEST()
      : file_pointer_(nullptr)
      , dest_code_(0)
      , max_len_(0)
      , syslog_facility_(0)
      , msg_types_{0}
  {
    return;
  }

  FILE* file_pointer_;
  int dest_code_;                /* Destination (one of the MD_ codes) */
  int max_len_;                  /* Max mail line length */
  int syslog_facility_;          /* Syslog Facility */
  char msg_types_[NR_MSG_TYPES]; /* Message type mask */
  std::string where_;            /* Filename/program name */
  std::string mail_cmd_;         /* Mail command */
  std::string timestamp_format_; /* used in logging messages */
  std::string mail_filename_;    /* Unique mail filename */
};

/**
 * Message Destination values for dest field of DEST
 *
 * MD_SYSLOG          Send msg to syslog
 * MD_MAIL            Email group of messages
 * MD_FILE            Write messages to a file
 * MD_APPEND          Append messages to a file
 * MD_STDOUT          Print messages
 * MD_STDERR          Print messages to stderr
 * MD_DIRECTOR,       Send message to the Director
 * MD_OPERATOR        Email a single message to the operator
 * MD_CONSOLE         Send msg to UserAgent or console
 * MD_MAIL_ON_ERROR   Email messages if job errors
 * MD_MAIL_ON_SUCCESS Email messages if job succeeds
 * MD_CATALOG
 */
enum
{
  MD_SYSLOG = 1,
  MD_MAIL,
  MD_FILE,
  MD_APPEND,
  MD_STDOUT,
  MD_STDERR,
  MD_DIRECTOR,
  MD_OPERATOR,
  MD_CONSOLE,
  MD_MAIL_ON_ERROR,
  MD_MAIL_ON_SUCCESS,
  MD_CATALOG
};

/**
 * Queued message item
 */
class MessageQeueItem {
 public:
  MessageQeueItem() = default;
  virtual ~MessageQeueItem() = default;
  dlink link_;
  int type_ = 0;
  utime_t mtime_ = {0};
  std::string msg_;
};

class JobControlRecord;

extern "C" {
typedef char* (*job_code_callback_t)(JobControlRecord*, const char*);
}

void Jmsg(JobControlRecord* jcr, int type, utime_t mtime, const char* fmt, ...);
void Qmsg(JobControlRecord* jcr, int type, utime_t mtime, const char* fmt, ...);
bool GetTrace(void);
const char* get_basename(const char* pathname);
void SetLogTimestampFormat(const char* format);

typedef bool (*db_log_insert_func)(JobControlRecord* jcr,
                                   utime_t mtime,
                                   char* msg);
extern db_log_insert_func p_db_log_insert;


class MessagesResource;

extern int debug_level;
extern bool dbg_timestamp; /* print timestamp in debug output */
extern bool prt_kaboom;    /* Print kaboom output */
extern int verbose;
extern char my_name[];
extern const char* assert_msg; /* Assert error message */
extern const char* working_directory;
extern utime_t daemon_start_time;

extern int console_msg_pending;
extern FILE* con_fd;       /* Console file descriptor */
extern brwlock_t con_lock; /* Console lock structure */

void MyNameIs(int argc, char* argv[], const char* name);
void InitMsg(JobControlRecord* jcr,
             MessagesResource* msg,
             job_code_callback_t job_code_callback = NULL);
void TermMsg(void);
void CloseMsg(JobControlRecord* jcr);
void AddMsgDest(MessagesResource* msg,
                int dest,
                int type,
                const std::string& where,
                const std::string& mail_cmd,
                const std::string& timestamp_format);
void RemMsgDest(MessagesResource* msg,
                int dest,
                int type,
                const std::string& where);
void Jmsg(JobControlRecord* jcr, int type, utime_t mtime, const char* fmt, ...);
void DispatchMessage(JobControlRecord* jcr, int type, utime_t mtime, char* buf);
void InitConsoleMsg(const char* wd);
void FreeMsgsRes(MessagesResource* msgs);  // Ueb
void DequeueMessages(JobControlRecord* jcr);
void SetTrace(int trace_flag);
bool GetTrace(void);
void SetHangup(int hangup_value);
bool GetHangup(void);
void SetTimestamp(int timestamp_flag);
bool GetTimestamp(void);
void SetDbType(const char* name);
void RegisterMessageCallback(void msg_callback(int type, char* msg));

#endif /* BAREOS_LIB_MESSAGE_H_ */
