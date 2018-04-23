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

DLL_IMP_EXP void escape_string(PoolMem &snew, char *old, int len);
DLL_IMP_EXP bool is_buf_zero(char *buf, int len);
DLL_IMP_EXP void lcase(char *str);
DLL_IMP_EXP void bash_spaces(char *str);
DLL_IMP_EXP void bash_spaces(PoolMem &pm);
DLL_IMP_EXP void unbash_spaces(char *str);
DLL_IMP_EXP void unbash_spaces(PoolMem &pm);
DLL_IMP_EXP const char* indent_multiline_string(PoolMem &resultbuffer, const char *multilinestring, const char *separator);
DLL_IMP_EXP char *encode_time(utime_t time, char *buf);
DLL_IMP_EXP bool convert_timeout_to_timespec(timespec &timeout, int timeout_in_seconds);
DLL_IMP_EXP char *encode_mode(mode_t mode, char *buf);
DLL_IMP_EXP int do_shell_expansion(char *name, int name_len);
DLL_IMP_EXP void jobstatus_to_ascii(int JobStatus, char *msg, int maxlen);
DLL_IMP_EXP void jobstatus_to_ascii_gui(int JobStatus, char *msg, int maxlen);
DLL_IMP_EXP int run_program(char *prog, int wait, POOLMEM *&results);
DLL_IMP_EXP int run_program_full_output(char *prog, int wait, POOLMEM *&results);
DLL_IMP_EXP char *action_on_purge_to_string(int aop, PoolMem &ret);
DLL_IMP_EXP const char *job_type_to_str(int type);
DLL_IMP_EXP const char *job_status_to_str(int stat);
DLL_IMP_EXP const char *job_level_to_str(int level);
DLL_IMP_EXP const char *volume_status_to_str(const char *status);
DLL_IMP_EXP void make_session_key(char *key, char *seed, int mode);
DLL_IMP_EXP void encode_session_key(char *encode, char *session, char *key, int maxlen);
DLL_IMP_EXP void decode_session_key(char *decode, char *session, char *key, int maxlen);
DLL_IMP_EXP POOLMEM *edit_job_codes(JobControlRecord *jcr, char *omsg, char *imsg, const char *to, job_code_callback_t job_code_callback = NULL);
DLL_IMP_EXP void set_working_directory(char *wd);
DLL_IMP_EXP const char *last_path_separator(const char *str);

#endif // BAREOS_LIB_UTIL_H_
