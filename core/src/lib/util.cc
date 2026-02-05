/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2026 Bareos GmbH & Co. KG

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
 * util.c  miscellaneous utility subroutines for BAREOS
 *
 * Kern Sibbald, MM
 */
#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif
#include <openssl/md5.h>

#include "include/bareos.h"
#include "lib/util.h"
#include "include/jcr.h"
#include "lib/edit.h"
#include "lib/ascii_control_characters.h"
#include "lib/bstringlist.h"
#include "lib/qualified_resource_name_type_converter.h"
#include "include/version_numbers.h"
#include "lib/bpipe.h"
#include "lib/btime.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#include <openssl/rand.h>
#include <openssl/err.h>

// Various BAREOS Utility subroutines

/*
 * Escape special characters in bareos configuration strings
 * needed for dumping config strings
 */
void EscapeString(PoolMem& snew, const char* old, int len)
{
  char* n;
  const char* o;

  snew.check_size(len * 2);
  n = snew.c_str();
  o = old;
  while (len--) {
    switch (*o) {
      case '\'':
        *n++ = '\'';
        *n++ = '\'';
        o++;
        break;
      case '\\':
        *n++ = '\\';
        *n++ = '\\';
        o++;
        break;
      case 0:
        *n++ = '\\';
        *n++ = 0;
        o++;
        break;
      case '(':
      case ')':
      case '<':
      case '>':
      case '"':
        *n++ = '\\';
        *n++ = *o++;
        break;
      default:
        *n++ = *o++;
        break;
    }
  }
  *n = 0;
}


std::string EscapeString(const char* old)
{
  PoolMem snew;
  EscapeString(snew, old, strlen(old));
  return std::string(snew.c_str());
}


// Return true of buffer has all zero bytes
bool IsBufZero(char* buf, int len)
{
  uint64_t* ip;
  char* p;
  int i, len64, done, rem;

  if (buf[0] != 0) { return false; }
  ip = (uint64_t*)buf;

  // Optimize by checking uint64_t for zero
  len64 = len / sizeof(uint64_t);
  for (i = 0; i < len64; i++) {
    if (ip[i] != 0) { return false; }
  }
  done = len64 * sizeof(uint64_t); /* bytes already checked */
  p = buf + done;
  rem = len - done;
  for (i = 0; i < rem; i++) {
    if (p[i] != 0) { return false; }
  }
  return true;
}


// Convert a string in place to lower case
void lcase(char* str)
{
  while (*str) {
    if (B_ISUPPER(*str)) { *str = tolower((int)(*str)); }
    str++;
  }
}

/*
 * Convert spaces to non-space character.
 * This makes scanf of fields containing spaces easier.
 */
void BashSpaces(char* str)
{
  while (*str) {
    if (*str == ' ') *str = 0x1;
    str++;
  }
}

void BashSpaces(std::string& str)
{
  std::replace(str.begin(), str.end(), ' ', static_cast<char>(0x1));
}

void BashSpaces(PoolMem& pm)
{
  char* str = pm.c_str();
  while (*str) {
    if (*str == ' ') *str = 0x1;
    str++;
  }
}

// Convert non-space characters (0x1) back into spaces
void UnbashSpaces(char* str)
{
  while (*str) {
    if (*str == 0x1) *str = ' ';
    str++;
  }
}

// Convert non-space characters (0x1) back into spaces
void UnbashSpaces(PoolMem& pm)
{
  char* str = pm.c_str();
  while (*str) {
    if (*str == 0x1) *str = ' ';
    str++;
  }
}

struct HelloInformation {
  std::string hello_string;
  std::string resource_type_string;
  uint32_t position_of_name;
  int32_t position_of_version;
};

using SizeTypeOfHelloList = std::vector<std::string>::size_type;

static std::list<HelloInformation> hello_list{
    /* this order is important */
    {"Hello Storage calling Start Job", "R_JOB", 5, -1},
    {"Hello Start Storage Job", "R_JOB", 4, -1},
    {"Hello Start Job", "R_JOB", 3, -1},
    {"Hello Director", "R_DIRECTOR", 2, -1},
    {"Hello Storage", "R_STORAGE", 2, -1},
    {"Hello Client", "R_CLIENT", 2, -1},
    {"Hello", "R_CONSOLE", 1, 4} /* "Hello %s calling version %s" */
};

bool GetNameAndResourceTypeAndVersionFromHello(
    const std::string& input,
    std::string& name,
    std::string& r_type_str,
    BareosVersionNumber& bareos_version)
{
  std::list<HelloInformation>::const_iterator hello = hello_list.cbegin();

  bool found = false;
  while (hello != hello_list.cend()) {
    uint32_t size = hello->hello_string.size();
    uint32_t input_size = input.size();
    if (input_size >= size) {
      if (!input.compare(0, size, hello->hello_string)) {
        found = true;
        break;
      }
    }
    hello++;
  }

  if (!found) {
    Dmsg1(100, "Client information not found: %s\n", input.c_str());
    return false;
  }

  BStringList arguments_of_hello_string(input, ' '); /* split at blanks */

  bool ok = false;
  if (arguments_of_hello_string.size() > hello->position_of_name) {
    name = arguments_of_hello_string[hello->position_of_name];
    std::replace(name.begin(), name.end(), (char)0x1, ' ');
    r_type_str = hello->resource_type_string;
    ok = true;
  } else {
    Dmsg0(100, "Failed to retrieve the name from hello message\n");
  }

  if (ok) {
    bareos_version = BareosVersionNumber::kUndefined;
    if (hello->position_of_version >= 0) {
      if (arguments_of_hello_string.size()
          > static_cast<SizeTypeOfHelloList>(hello->position_of_version)) {
        std::string version_str
            = arguments_of_hello_string[hello->position_of_version];
        if (!version_str.empty()) {
          ok = false;
          BStringList splittet_version(version_str, '.');
          if (splittet_version.size() > 1) {
            uint32_t v;
            try {
              v = std::stoul(splittet_version[0]) * 100;
              v += std::stoul(splittet_version[1]);
              bareos_version = static_cast<BareosVersionNumber>(v);
              ok = true;
            } catch (const std::exception& e) {
              Dmsg0(100,
                    "Could not read out any version from hello message: %s\n",
                    e.what());
            }
          }
        }
      }
    }
  }

  return ok;
}

/*
 * Parameter:
 *   resultbuffer: one line string
 *   mutlilinestring: multiline string (separated by "\n")
 *   separator: separator string
 *
 * multilinestring should be indented according to resultbuffer.
 *
 * return:
 *   resultbuffer: multilinestring will be added to resultbuffer.
 *
 * Example:
 *   resultbuffer="initial string"
 *   mutlilinestring="line1\nline2\nline3"
 *   separator="->"
 * Result:
 *   initial string->line1
 *                 ->line2
 *                 ->line3
 */
const char* IndentMultilineString(PoolMem& resultbuffer,
                                  const char* multilinestring,
                                  const char* separator)
{
  PoolMem multiline(multilinestring);
  PoolMem indent(PM_MESSAGE);
  char* p1 = multiline.c_str();
  char* p2 = NULL;
  bool line1 = true;
  size_t len;

  /* create indentation string */
  for (len = resultbuffer.strlen(); len > 0; len--) { indent.strcat(" "); }
  indent.strcat(separator);

  resultbuffer.strcat(separator);

  while ((p2 = strchr(p1, '\n')) != NULL) {
    *p2 = 0;
    if (!line1) { resultbuffer.strcat(indent); }
    resultbuffer.strcat(p1);
    resultbuffer.strcat("\n");
    p1 = p2 + 1;
    line1 = false;
  }

  if (!line1) { resultbuffer.strcat(indent); }
  resultbuffer.strcat(p1);

  return resultbuffer.c_str();
}

char* encode_time(utime_t utime, char* buf)
{
  struct tm tm;
  int n = 0;
  time_t time = utime;

#if defined(HAVE_WIN32)
  /* Avoid a seg fault in Microsoft's CRT localtime_r(),
   * which incorrectly references a NULL returned from gmtime() if
   * time is negative before or after the timezone adjustment. */
  struct tm* gtm;

  if ((gtm = gmtime(&time)) == NULL) { return buf; }

  if (gtm->tm_year == 1970 && gtm->tm_mon == 1 && gtm->tm_mday < 3) {
    return buf;
  }
#endif

  Blocaltime(&time, &tm);
  // FIXME: this is unsafe, because we don't know the size of the buffer
  //        we're writing to
  n = snprintf(buf, 24, "%04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900,
               tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

  return buf + n;
}

bool ConvertTimeoutToTimespec(timespec& timeout, int timeout_in_seconds)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);
  timeout.tv_nsec = tv.tv_usec * 1000;
  timeout.tv_sec = tv.tv_sec + timeout_in_seconds;

  return true;
}

// Convert a JobStatus code into a human readable form
std::string JobstatusToAscii(int JobStatus)
{
  switch (JobStatus) {
    case JS_Created:
      return "Created";

    case JS_Running:
      return "Running";

    case JS_Blocked:
      return "Blocked";

    case JS_Terminated:
      return "OK";

    case JS_Incomplete:
      return "Error: incomplete job";

    case JS_FatalError:
      return "Fatal Error";

    case JS_ErrorTerminated:
      return "Error";

    case JS_Error:
      return "Non-fatal error";

    case JS_Warnings:
      return "OK -- with warnings";

    case JS_Canceled:
      return "Canceled";

    case JS_Differences:
      return "Verify differences";

    case JS_WaitFD:
      return "Waiting on FD";

    case JS_WaitSD:
      return "Wait on SD";

    case JS_WaitMedia:
      return "Wait for new Volume";

    case JS_WaitMount:
      return "Waiting for mount";

    case JS_WaitStoreRes:
      return "Waiting for Storage resource";

    case JS_WaitJobRes:
      return "Waiting for Job resource";

    case JS_WaitClientRes:
      return "Waiting for Client resource";

    case JS_WaitMaxJobs:
      return "Waiting on Max Jobs";

    case JS_WaitStartTime:
      return "Waiting for Start Time";

    case JS_WaitPriority:
      return "Waiting on Priority";

    case JS_DataCommitting:
      return "SD committing Data";

    case JS_DataDespooling:
      return "SD despooling Data";

    case JS_AttrDespooling:
      return "SD despooling Attributes";

    case JS_AttrInserting:
      return "Dir inserting Attributes";
    case 0:
      return "";
    default:
      return "Unknown Job termination status=" + std::to_string(JobStatus);
  }
}

// Convert Job Termination Status into a string
const char* job_status_to_str(int stat)
{
  const char* str;

  switch (stat) {
    case JS_Terminated:
      str = T_("OK");
      break;
    case JS_Warnings:
      str = T_("OK -- with warnings");
      break;
    case JS_ErrorTerminated:
    case JS_Error:
      str = T_("Error");
      break;
    case JS_FatalError:
      str = T_("Fatal Error");
      break;
    case JS_Canceled:
      str = T_("Canceled");
      break;
    case JS_Differences:
      str = T_("Differences");
      break;
    default:
      str = T_("Unknown term code");
      break;
  }
  return str;
}

// Convert Job Type into a string
const char* job_type_to_str(int type)
{
  const char* str = NULL;

  switch (type) {
    case JT_BACKUP:
      str = T_("Backup");
      break;
    case JT_MIGRATED_JOB:
      str = T_("Migrated Job");
      break;
    case JT_VERIFY:
      str = T_("Verify");
      break;
    case JT_RESTORE:
      str = T_("Restore");
      break;
    case JT_CONSOLE:
      str = T_("Console");
      break;
    case JT_SYSTEM:
      str = T_("System or Console");
      break;
    case JT_ADMIN:
      str = T_("Admin");
      break;
    case JT_ARCHIVE:
      str = T_("Archive");
      break;
    case JT_JOB_COPY:
      str = T_("Job Copy");
      break;
    case JT_COPY:
      str = T_("Copy");
      break;
    case JT_MIGRATE:
      str = T_("Migrate");
      break;
    case JT_SCAN:
      str = T_("Scan");
      break;
    case JT_CONSOLIDATE:
      str = T_("Consolidate");
      break;
  }
  if (!str) { str = T_("Unknown Type"); }
  return str;
}

const char* job_replace_to_str(int replace)
{
  const char* str = NULL;
  switch (replace) {
    case REPLACE_ALWAYS:
      str = T_("always");
      break;
    case REPLACE_IFNEWER:
      str = T_("ifnewer");
      break;
    case REPLACE_IFOLDER:
      str = T_("ifolder");
      break;
    case REPLACE_NEVER:
      str = T_("never");
      break;
    default:
      str = T_("Unknown Replace");
      break;
  }
  return str;
}

// Convert ActionOnPurge to string (Truncate, Erase, Destroy)
char* action_on_purge_to_string(int aop, PoolMem& ret)
{
  if (aop & ON_PURGE_TRUNCATE) { PmStrcpy(ret, T_("Truncate")); }
  if (!aop) { PmStrcpy(ret, T_("None")); }
  return ret.c_str();
}

// Convert Job Level into a string
const char* job_level_to_str(int level)
{
  const char* str;

  switch (level) {
    case L_BASE:
      str = T_("Base");
      break;
    case L_FULL:
      str = T_("Full");
      break;
    case L_INCREMENTAL:
      str = T_("Incremental");
      break;
    case L_DIFFERENTIAL:
      str = T_("Differential");
      break;
    case L_SINCE:
      str = T_("Since");
      break;
    case L_VERIFY_CATALOG:
      str = T_("Verify Catalog");
      break;
    case L_VERIFY_INIT:
      str = T_("Verify Init Catalog");
      break;
    case L_VERIFY_VOLUME_TO_CATALOG:
      str = T_("Verify Volume to Catalog");
      break;
    case L_VERIFY_DISK_TO_CATALOG:
      str = T_("Verify Disk to Catalog");
      break;
    case L_VERIFY_DATA:
      str = T_("Verify Data");
      break;
    case L_VIRTUAL_FULL:
      str = T_("Virtual Full");
      break;
    case L_NONE:
      str = " ";
      break;
    default:
      str = T_("Unknown Job Level");
      break;
  }
  return str;
}

// Encode the mode bits into a 10 character string like LS does
char* encode_mode(mode_t mode, char* buf)
{
  char* cp = buf;
  // clang-format off
  *cp++ = S_ISDIR(mode)    ? 'd'
          : S_ISBLK(mode)  ? 'b'
          : S_ISCHR(mode)  ? 'c'
          : S_ISLNK(mode)  ? 'l'
          : S_ISFIFO(mode) ? 'f'
          : S_ISSOCK(mode) ? 's'
                           : '-';
  // clang-format on
  *cp++ = mode & S_IRUSR ? 'r' : '-';
  *cp++ = mode & S_IWUSR ? 'w' : '-';
  *cp++ = (mode & S_ISUID ? (mode & S_IXUSR ? 's' : 'S')
                          : (mode & S_IXUSR ? 'x' : '-'));
  *cp++ = mode & S_IRGRP ? 'r' : '-';
  *cp++ = mode & S_IWGRP ? 'w' : '-';
  *cp++ = (mode & S_ISGID ? (mode & S_IXGRP ? 's' : 'S')
                          : (mode & S_IXGRP ? 'x' : '-'));
  *cp++ = mode & S_IROTH ? 'r' : '-';
  *cp++ = mode & S_IWOTH ? 'w' : '-';
  *cp++ = (mode & S_ISVTX ? (mode & S_IXOTH ? 't' : 'T')
                          : (mode & S_IXOTH ? 'x' : '-'));
  *cp = '\0';
  return cp;
}

#if defined(HAVE_WIN32)
char* DoShellExpansion(const char* name)
{
  // this should return chars needed + 1 (NUL) + 1 (for some ascii reason)
  DWORD space_required = ExpandEnvironmentStringsA(name, nullptr, 0);

  if (space_required == 0) { return strdup(name); }

  char* expanded = static_cast<char*>(malloc(space_required + 1));

  // since this fits, it should return chars needed + 1 (for some reason)
  DWORD space_used = ExpandEnvironmentStringsA(name, expanded, space_required);

  Dmsg3(2000,
        "ShellExpansion(%s) => %s needed %u bytes and we wrote %u bytes.  The "
        "last char "
        "is '%c' (%d)\n",
        name, space_used > 0 ? expanded : "(error)", space_required, space_used,
        space_used > 0 ? expanded[space_used - 1] : 0,
        space_used > 0 ? expanded[space_used - 1] : -1);

  // we expect space_used to be exactly one smaller than space_required
  // (see above)
  if (space_used + 1 != space_required) {
    free(expanded);
    return strdup(name);
  }

  // ExpandEnvironmentStrings should null terminate the string already
  // but just to be 100% sure, we add a null byte at the end of the buffer.
  expanded[space_required] = '\0';

  return expanded;
}
#else
char* DoShellExpansion(const char* name) { return strdup(name); }
#endif

/* Create a new session key. key needs to be able to hold at least
 * 120 bytes (keys are 40 bytes long, but errors might be longer).
 * If successful, key contains the generated key, otherwise key will
 * contain an error message. */
bool MakeSessionKey(char key[120])
{
  unsigned char s[16];

  if (RAND_bytes(s, sizeof(s)) != 1) {
    auto err = ERR_get_error();
    ERR_error_string(err, key);
    return false;
  }

  for (int j = 0; j < 16; j++) {
    char low = (s[j] & 0x0F);
    char high = (s[j] & 0xF0) >> 4;

    *key++ = 'A' + low;
    *key++ = 'A' + high;

    if (j & 1) { *key++ = '-'; }
  }
  *--key = 0;

  return true;
}

/*
 * Edit job codes into main command line
 *  %% = %
 *  %B = Job Bytes in human readable format
 *  %F = Job Files
 *  %P = Pid of daemon
 *  %b = Job Bytes
 *  %c = Client's name
 *  %d = Director's name
 *  %e = Job Exit code
 *  %i = JobId
 *  %j = Unique Job id
 *  %l = job level
 *  %n = Unadorned Job name
 *  %r = Recipients
 *  %s = Since time
 *  %t = Job type (Backup, ...)
 *  %v = Volume name(s)
 *
 *  omsg = edited output message
 *  imsg = input string containing edit codes (%x)
 *  to = recipients list
 */
POOLMEM* edit_job_codes(JobControlRecord* jcr,
                        char* omsg,
                        const char* imsg,
                        const char* to,
                        job_code_callback_t callback)
{
  const char* p;
  char* q;
  const char* str;
  char ed1[50];
  char add[50];
  char name[MAX_NAME_LENGTH];
  int i;

  *omsg = 0;
  Dmsg1(200, "edit_job_codes: %s\n", imsg);
  for (p = imsg; *p; p++) {
    if (*p == '%') {
      switch (*++p) {
        case '%':
          str = "%";
          break;
        case 'B': /* Job Bytes in human readable format */
          if (jcr) {
            Bsnprintf(add, sizeof(add), "%sB",
                      edit_uint64_with_suffix(jcr->JobBytes, ed1));
            str = add;
          } else {
            str = T_("*None*");
          }
          break;
        case 'F': /* Job Files */
          if (jcr) {
            str = edit_uint64(jcr->JobFiles, add);
          } else {
            str = T_("*None*");
          }
          break;
        case 'P': /* Process Id */
          Bsnprintf(add, sizeof(add), "%llu",
                    static_cast<long long unsigned>(getpid()));
          str = add;
          break;
        case 'b': /* Job Bytes */
          if (jcr) {
            str = edit_uint64(jcr->JobBytes, add);
          } else {
            str = T_("*None*");
          }
          break;
        case 'c': /* Client's name */
          if (jcr && jcr->client_name) {
            str = jcr->client_name;
          } else {
            str = T_("*None*");
          }
          break;
        case 'd': /* Director's name */
          str = my_name;
          break;
        case 'e': /* Job Exit code */
          if (jcr) {
            str = job_status_to_str(jcr->getJobStatus());
          } else {
            str = T_("*None*");
          }
          break;
        case 'i': /* JobId */
          if (jcr) {
            Bsnprintf(add, sizeof(add), "%d", jcr->JobId);
            str = add;
          } else {
            str = T_("*None*");
          }
          break;
        case 'j': /* Job name */
          if (jcr) {
            str = jcr->Job;
          } else {
            str = T_("*None*");
          }
          break;
        case 'l': /* Job level */
          if (jcr) {
            str = job_level_to_str(jcr->getJobLevel());
          } else {
            str = T_("*None*");
          }
          break;
        case 'n': /* Unadorned Job name */
          if (jcr) {
            bstrncpy(name, jcr->Job, sizeof(name));
            // There are three periods after the Job name
            for (i = 0; i < 3; i++) {
              if ((q = strrchr(name, '.')) != NULL) { *q = 0; }
            }
            str = name;
          } else {
            str = T_("*None*");
          }
          break;
        case 'r': /* Recipients */
          str = to;
          break;
        case 's': /* Since time */
          if (jcr && jcr->starttime_string) {
            str = jcr->starttime_string;
          } else {
            str = T_("*None*");
          }
          break;
        case 't': /* Job type */
          if (jcr) {
            str = job_type_to_str(jcr->getJobType());
          } else {
            str = T_("*None*");
          }
          break;
        case 'v': /* Volume name(s) */
          if (jcr) {
            if (jcr->VolumeName) {
              str = jcr->VolumeName;
            } else {
              str = T_("*None*");
            }
          } else {
            str = T_("*None*");
          }
          break;
        default:
          str = NULL;
          if (callback != NULL) {
            auto callback_result = callback(jcr, p);
            if (callback_result) {
              snprintf(add, sizeof(add), "%s", callback_result->c_str());
              str = add;
            }
          }

          if (!str) {
            add[0] = '%';
            add[1] = *p;
            add[2] = 0;
            str = add;
          }
          break;
      }
    } else {
      add[0] = *p;
      add[1] = 0;
      str = add;
    }
    Dmsg1(1200, "add_str %s\n", str);
    PmStrcat(omsg, str);
    Dmsg1(1200, "omsg=%s\n", omsg);
  }

  return omsg;
}

void SetWorkingDirectory(const char* wd)
{
  struct stat stat_buf;

  if (wd == NULL) {
    Emsg0(M_ERROR_TERM, 0,
          T_("Working directory not defined. Cannot continue.\n"));
  }
  if (stat(wd, &stat_buf) != 0) {
    Emsg1(M_ERROR_TERM, 0,
          T_("Working Directory: \"%s\" not found. Cannot continue.\n"), wd);
  }
  if (!S_ISDIR(stat_buf.st_mode)) {
    Emsg1(
        M_ERROR_TERM, 0,
        T_("Working Directory: \"%s\" is not a directory. Cannot continue.\n"),
        wd);
  }
  working_directory = wd; /* set global */
}

const char* last_path_separator(const char* str)
{
  if (*str != '\0') {
    for (const char* p = &str[strlen(str) - 1]; p >= str; p--) {
      if (IsPathSeparator(*p)) { return p; }
    }
  }
  return NULL;
}

void StringToLowerCase(std::string& s)
{
  for (auto& c : s) { c = std::tolower(c); }
}

void StringToLowerCase(std::string& out, const std::string& in)
{
  out.clear();
  for (const auto& c : in) { out += std::tolower(c); }
}

void SortCaseInsensitive(std::vector<std::string>& v)
{
  if (v.empty()) { return; }

  std::sort(v.begin(), v.end(), [](const std::string& a, const std::string& b) {
    std::string x{a}, y{b};
    StringToLowerCase(x);
    StringToLowerCase(y);
    return x < y;
  });
}

std::string getenv_std_string(std::string env_var)
{
  const char* v = (std::getenv(env_var.c_str()));
  return v ? std::string(v) : std::string();
}


bool pm_append(void* pm_string, const char* fmt, ...)
{
  PoolMem additionalstring;
  PoolMem* pm = static_cast<PoolMem*>(pm_string);

  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  additionalstring.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);

  pm->strcat(additionalstring);

  return true;
}

std::vector<std::string> split_string(const std::string& str, char delim)
{
  std::istringstream ss(str);
  std::vector<std::string> parts;
  std::string part;
  while (std::getline(ss, part, delim)) { parts.push_back(part); }
  return parts;
}

std::string CreateDelimitedStringForSqlQueries(
    const std::vector<char>& elements,
    char delim)
{
  std::string empty_list{"''"};
  std::string result{};
  if (!elements.empty()) {
    for (const auto& element : elements) {
      result += "'";
      result += element;
      result += "'";
      result += delim;
    }
    result.pop_back();
    return result;
  }
  return empty_list;
}

std::string TPAsString(const std::chrono::system_clock::time_point& tp)
{
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  char str[100];
  if (!std::strftime(str, sizeof(str), "%Y-%m-%d_%H:%M:%S",
                     std::localtime(&t))) {
    return std::string("strftime error");
  }
  std::string ts = str;
  return ts;
}

void to_lower(std::string& s)
{
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
}

void timer::reset_and_start()
{
  start = std::chrono::steady_clock::now();
  end.reset();
}

void timer::stop()
{
  ASSERT(!end);
  end = std::chrono::steady_clock::now();
}

const char* timer::format_human_readable()
{
  using namespace std::chrono;

  auto dur = end.value_or(steady_clock::now()) - start;

  auto h = duration_cast<hours>(dur);
  dur -= h;
  auto m = duration_cast<minutes>(dur);
  dur -= m;
  auto s = duration_cast<seconds>(dur);

  for (;;) {
    auto ssize = std::snprintf(formatted.data(), formatted.size(),
                               "%02llu:%02llu:%02llu",
                               static_cast<long long unsigned>(h.count()),
                               static_cast<long long unsigned>(m.count()),
                               static_cast<long long unsigned>(s.count()));

    if (ssize < 0) { return "<format error>"; }

    auto size = static_cast<std::size_t>(ssize);

    if (size < formatted.size()) { break; }

    formatted.resize(size + 1);
  }

  return formatted.c_str();
}
