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

DLL_IMP_EXP void EscapeString(PoolMem &snew, char *old, int len);
DLL_IMP_EXP bool IsBufZero(char *buf, int len);
DLL_IMP_EXP void lcase(char *str);
DLL_IMP_EXP void BashSpaces(char *str);
DLL_IMP_EXP void BashSpaces(PoolMem &pm);
DLL_IMP_EXP void UnbashSpaces(char *str);
DLL_IMP_EXP void UnbashSpaces(PoolMem &pm);
DLL_IMP_EXP const char* IndentMultilineString(PoolMem &resultbuffer, const char *multilinestring, const char *separator);
DLL_IMP_EXP char *encode_time(utime_t time, char *buf);
DLL_IMP_EXP bool ConvertTimeoutToTimespec(timespec &timeout, int timeout_in_seconds);
DLL_IMP_EXP char *encode_mode(mode_t mode, char *buf);
DLL_IMP_EXP int DoShellExpansion(char *name, int name_len);
DLL_IMP_EXP void JobstatusToAscii(int JobStatus, char *msg, int maxlen);
DLL_IMP_EXP void JobstatusToAsciiGui(int JobStatus, char *msg, int maxlen);
DLL_IMP_EXP int RunProgram(char *prog, int wait, POOLMEM *&results);
DLL_IMP_EXP int RunProgramFullOutput(char *prog, int wait, POOLMEM *&results);
DLL_IMP_EXP char *action_on_purge_to_string(int aop, PoolMem &ret);
DLL_IMP_EXP const char *job_type_to_str(int type);
DLL_IMP_EXP const char *job_status_to_str(int stat);
DLL_IMP_EXP const char *job_level_to_str(int level);
DLL_IMP_EXP const char *volume_status_to_str(const char *status);
DLL_IMP_EXP void MakeSessionKey(char *key, char *seed, int mode);
DLL_IMP_EXP void EncodeSessionKey(char *encode, char *session, char *key, int maxlen);
DLL_IMP_EXP void DecodeSessionKey(char *decode, char *session, char *key, int maxlen);
DLL_IMP_EXP POOLMEM *edit_job_codes(JobControlRecord *jcr, char *omsg, char *imsg, const char *to, job_code_callback_t job_code_callback = NULL);
DLL_IMP_EXP void SetWorkingDirectory(char *wd);
DLL_IMP_EXP const char *last_path_separator(const char *str);

#endif // BAREOS_LIB_UTIL_H_
