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
#ifndef LIB_BSYS_H_
#define LIB_BSYS_H_

DLL_IMP_EXP char *bstrinlinecpy(char *dest, const char *src);
DLL_IMP_EXP char *bstrncpy(char *dest, const char *src, int maxlen);
DLL_IMP_EXP char *bstrncpy(char *dest, PoolMem &src, int maxlen);
DLL_IMP_EXP char *bstrncat(char *dest, const char *src, int maxlen);
DLL_IMP_EXP char *bstrncat(char *dest, PoolMem &src, int maxlen);
DLL_IMP_EXP bool bstrcmp(const char *s1, const char *s2);
DLL_IMP_EXP bool bstrncmp(const char *s1, const char *s2, int n);
DLL_IMP_EXP bool bstrcasecmp(const char *s1, const char *s2);
DLL_IMP_EXP bool bstrncasecmp(const char *s1, const char *s2, int n);
DLL_IMP_EXP int cstrlen(const char *str);
DLL_IMP_EXP void *b_malloc(const char *file, int line, size_t size);
#ifndef bmalloc
DLL_IMP_EXP void *bmalloc(size_t size);
#endif
DLL_IMP_EXP void bfree(void *buf);
DLL_IMP_EXP void *brealloc(void *buf, size_t size);
DLL_IMP_EXP void *bcalloc(size_t size1, size_t size2);
DLL_IMP_EXP int bsnprintf(char *str, int32_t size, const char *format, ...);
DLL_IMP_EXP int bvsnprintf(char *str, int32_t size, const char *format, va_list ap);
DLL_IMP_EXP int pool_sprintf(char *pool_buf, const char *fmt, ...);
DLL_IMP_EXP void create_pid_file(char *dir, const char *progname, int port);
DLL_IMP_EXP int delete_pid_file(char *dir, const char *progname, int port);
DLL_IMP_EXP void drop(char *uid, char *gid, bool keep_readall_caps);
DLL_IMP_EXP int bmicrosleep(int32_t sec, int32_t usec);
DLL_IMP_EXP char *bfgets(char *s, int size, FILE *fd);
DLL_IMP_EXP char *bfgets(POOLMEM *&s, FILE *fd);
DLL_IMP_EXP void make_unique_filename(POOLMEM *&name, int Id, char *what);
#ifndef HAVE_STRTOLL
DLL_IMP_EXP long long int strtoll(const char *ptr, char **endptr, int base);
#endif
DLL_IMP_EXP void read_state_file(char *dir, const char *progname, int port);
DLL_IMP_EXP int b_strerror(int errnum, char *buf, size_t bufsiz);
DLL_IMP_EXP char *escape_filename(const char *file_path);
DLL_IMP_EXP int Zdeflate(char *in, int in_len, char *out, int &out_len);
DLL_IMP_EXP int Zinflate(char *in, int in_len, char *out, int &out_len);
DLL_IMP_EXP void stack_trace();
DLL_IMP_EXP int safer_unlink(const char *pathname, const char *regex);
DLL_IMP_EXP int secure_erase(JobControlRecord *jcr, const char *pathname);
DLL_IMP_EXP void set_secure_erase_cmdline(const char *cmdline);
DLL_IMP_EXP bool path_exists(const char *path);
DLL_IMP_EXP bool path_exists(PoolMem &path);
DLL_IMP_EXP bool path_is_directory(const char *path);
DLL_IMP_EXP bool path_is_directory(PoolMem &path);
DLL_IMP_EXP bool path_contains_directory(const char *path);
DLL_IMP_EXP bool path_contains_directory(PoolMem &path);
DLL_IMP_EXP bool path_is_absolute(const char *path);
DLL_IMP_EXP bool path_is_absolute(PoolMem &path);
DLL_IMP_EXP bool path_get_directory(PoolMem &directory, PoolMem &path);
DLL_IMP_EXP bool path_append(char *path, const char *extra, unsigned int max_path);
DLL_IMP_EXP bool path_append(PoolMem &path, const char *extra);
DLL_IMP_EXP bool path_append(PoolMem &path, PoolMem &extra);
DLL_IMP_EXP bool path_create(const char *path, mode_t mode = 0750);
DLL_IMP_EXP bool path_create(PoolMem &path, mode_t mode = 0750);

#endif // LIB_BSYS_H_
