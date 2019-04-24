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
#include "lib/message_destination_info.h"

#include <string>

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
void SetDbLogInsertCallback(db_log_insert_func f);

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
void Jmsg(JobControlRecord* jcr, int type, utime_t mtime, const char* fmt, ...);
void DispatchMessage(JobControlRecord* jcr, int type, utime_t mtime, char* buf);
void InitConsoleMsg(const char* wd);
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
