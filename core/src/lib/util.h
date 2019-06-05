/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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
#ifndef BAREOS_LIB_UTIL_H_
#define BAREOS_LIB_UTIL_H_

#include "lib/ascii_control_characters.h"
#include "lib/message.h"

class BareosSocket;
class ConfigurationParser;
class QualifiedResourceNameTypeConverter;
enum class BareosVersionNumber : uint32_t;

void EscapeString(PoolMem& snew, const char* old, int len);
bool IsBufZero(char* buf, int len);
void lcase(char* str);
void BashSpaces(char* str);
void BashSpaces(PoolMem& pm);
void UnbashSpaces(char* str);
void UnbashSpaces(PoolMem& pm);
bool GetNameAndResourceTypeAndVersionFromHello(const std::string& input,
                                               std::string& name,
                                               std::string& r_type_str,
                                               BareosVersionNumber& version);
void FillBSockWithConnectedDaemonInformation(
    const ConfigurationParser& my_config,
    BareosSocket* bs);
const char* IndentMultilineString(PoolMem& resultbuffer,
                                  const char* multilinestring,
                                  const char* separator);
char* encode_time(utime_t time, char* buf);
bool ConvertTimeoutToTimespec(timespec& timeout, int timeout_in_seconds);
char* encode_mode(mode_t mode, char* buf);
int DoShellExpansion(char* name, int name_len);
void JobstatusToAscii(int JobStatus, char* msg, int maxlen);
void JobstatusToAsciiGui(int JobStatus, char* msg, int maxlen);
int RunProgram(char* prog, int wait, POOLMEM*& results);
int RunProgramFullOutput(char* prog, int wait, POOLMEM*& results);
char* action_on_purge_to_string(int aop, PoolMem& ret);
const char* job_type_to_str(int type);
const char* job_status_to_str(int stat);
const char* job_level_to_str(int level);
const char* volume_status_to_str(const char* status);
void MakeSessionKey(char* key, char* seed, int mode);
void EncodeSessionKey(char* encode, char* session, char* key, int maxlen);
void DecodeSessionKey(char* decode, char* session, char* key, int maxlen);
POOLMEM* edit_job_codes(JobControlRecord* jcr,
                        char* omsg,
                        const char* imsg,
                        const char* to,
                        job_code_callback_t job_code_callback = NULL);
void SetWorkingDirectory(char* wd);
const char* last_path_separator(const char* str);

#endif  // BAREOS_LIB_UTIL_H_
