/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2025 Bareos GmbH & Co. KG

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
#ifndef BAREOS_LIB_BSYS_H_
#define BAREOS_LIB_BSYS_H_

#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "mem_pool.h"
#include "bsnprintf.h"

#include "include/compiler_macro.h"

class JobControlRecord;

char* bstrinlinecpy(char* dest, const char* src);
char* bstrncpy(char* dest, const char* src, int maxlen);
char* bstrncpy(char* dest, PoolMem& src, int maxlen);
char* bstrncat(char* dest, const char* src, int maxlen);
char* bstrncat(char* dest, PoolMem& src, int maxlen);
bool bstrcmp(const char* s1, const char* s2);
bool bstrncmp(const char* s1, const char* s2, int n);
bool Bstrcasecmp(const char* s1, const char* s2);
bool bstrncasecmp(const char* s1, const char* s2, int n);
int cstrlen(const char* str);
int PoolSprintf(char* pool_buf, const char* fmt, ...) PRINTF_LIKE(2, 3);
int CreatePidFile(const char* progname, const char* pidfile_path);
void WritePidFile(int pidfile_fd,
                  const char* pidfile_path,
                  const char* progname);
int DeletePidFile(std::string pidfile_path);
int Bmicrosleep(int32_t sec, int32_t usec);
char* bfgets(char* s, int size, FILE* fd);
char* bfgets(POOLMEM*& s, FILE* fd);
void MakeUniqueFilename(POOLMEM*& name, int Id, char* what);
void ReadStateFile(const char* dir, const char* progname, int port);
void WriteStateFile(const char* dir, const char* progname, int port);
int b_strerror(int errnum, char* buf, size_t bufsiz);
char* escape_filename(const char* file_path);
int Zdeflate(char* in, int in_len, char* out, int& out_len);
int Zinflate(char* in, int in_len, char* out, int& out_len);
void stack_trace();
int SaferUnlink(const char* pathname, const char* regex);
int SecureErase(JobControlRecord* jcr, const char* pathname);
void SetSecureEraseCmdline(const char* cmdline);
bool PathExists(const char* path);
bool PathExists(PoolMem& path);
bool PathIsDirectory(const char* path);
bool PathIsDirectory(PoolMem& path);
bool PathContainsDirectory(const char* path);
bool PathContainsDirectory(PoolMem& path);
bool PathIsAbsolute(const char* path);
bool PathIsAbsolute(PoolMem& path);
bool PathGetDirectory(PoolMem& directory, PoolMem& path);
bool PathAppend(char* path, const char* extra, unsigned int max_path);
bool PathAppend(PoolMem& path, const char* extra);
bool PathAppend(PoolMem& path, PoolMem& extra);
bool PathCreate(const char* path, mode_t mode = 0750);
bool PathCreate(PoolMem& path, mode_t mode = 0750);

struct StateFileHeader {
  char id[14];
  int32_t version;
  uint64_t last_jobs_addr;
  uint64_t end_of_recent_job_results_list;
  uint64_t reserved[19];
};

static_assert(offsetof(StateFileHeader, version) == 16,
              "StateFileHeader.version offset");
static_assert(offsetof(StateFileHeader, last_jobs_addr) == 20
                  || offsetof(StateFileHeader, last_jobs_addr) == 24,
              "StageFileHeader.last_jobs_addr offset");
static_assert(offsetof(StateFileHeader, end_of_recent_job_results_list) == 28
                  || offsetof(StateFileHeader, end_of_recent_job_results_list)
                         == 32,
              "StateFileHeader.end_of_recent_job_results_list offset");
static_assert(offsetof(StateFileHeader, reserved) == 36
                  || offsetof(StateFileHeader, reserved) == 40,
              "StateFileHeader.reserved offset");
#endif  // BAREOS_LIB_BSYS_H_
