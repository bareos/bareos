/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

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
 * Memory Pool prototypes
 *
 * Kern Sibbald, MM
 */

#ifndef __MEM_POOL_H_
#define __MEM_POOL_H_

#ifdef SMARTALLOC

#define get_pool_memory(pool) sm_get_pool_memory(__FILE__, __LINE__, pool)
POOLMEM *sm_get_pool_memory(const char *file, int line, int pool);

#define get_memory(size) sm_get_memory(__FILE__, __LINE__, size)
POOLMEM *sm_get_memory(const char *fname, int line, int32_t size);

#define sizeof_pool_memory(buf) sm_sizeof_pool_memory(__FILE__, __LINE__, buf)
int32_t sm_sizeof_pool_memory(const char *fname, int line, POOLMEM *buf);

#define realloc_pool_memory(buf,size) sm_realloc_pool_memory(__FILE__, __LINE__, buf, size)
POOLMEM *sm_realloc_pool_memory(const char *fname, int line, POOLMEM *buf, int32_t size);

#define check_pool_memory_size(buf,size) sm_check_pool_memory_size(__FILE__, __LINE__, buf, size)
POOLMEM *sm_check_pool_memory_size(const char *fname, int line, POOLMEM *buf, int32_t size);

#define free_pool_memory(x) sm_free_pool_memory(__FILE__, __LINE__, x)
#define free_memory(x) sm_free_pool_memory(__FILE__, __LINE__, x)
void sm_free_pool_memory(const char *fname, int line, POOLMEM *buf);

#else

POOLMEM *get_pool_memory(int pool);
POOLMEM *get_memory(int32_t size);
int32_t sizeof_pool_memory(POOLMEM *buf);
POOLMEM *realloc_pool_memory(POOLMEM *buf, int32_t size);
POOLMEM *check_pool_memory_size(POOLMEM *buf, int32_t size);
#define free_memory(x) free_pool_memory(x)
void free_pool_memory(POOLMEM *buf);

#endif

/*
 * Macro to simplify free/reset pointers
 */
#define free_and_null_pool_memory(a) do { if (a) { free_pool_memory(a); (a) = NULL;} } while (0)

void garbage_collect_memory_pool();
void close_memory_pool();
void print_memory_pool_stats();

void garbage_collect_memory();

enum {
   PM_NOPOOL = 0,                     /* Nonpooled memory */
   PM_NAME = 1,                       /* BAREOS name */
   PM_FNAME = 2,                      /* File name buffer */
   PM_MESSAGE = 3,                    /* Daemon message */
   PM_EMSG = 4,                       /* Error message */
   PM_BSOCK = 5,                      /* BSOCK buffer */
   PM_RECORD = 6                      /* DEV_RECORD buffer */
};

#define PM_MAX PM_RECORD              /* Number of types */

class POOL_MEM {
   char *mem;
public:
   POOL_MEM() { mem = get_pool_memory(PM_NAME); *mem = 0; }
   POOL_MEM(int pool) { mem = get_pool_memory(pool); *mem = 0; }
   POOL_MEM(const char *str) { mem = get_pool_memory(PM_NAME); *mem = 0; strcpy(str); }
   ~POOL_MEM() { free_pool_memory(mem); mem = NULL; }
   char *c_str() const { return mem; }
   POOLMEM *&addr() { return mem; }
   int size() const { return sizeof_pool_memory(mem); }
   char *check_size(int32_t size) {
      mem = check_pool_memory_size(mem, size);
      return mem;
   }
   int32_t max_size();
   void realloc_pm(int32_t size);
   int strcpy(POOL_MEM &str);
   int strcpy(const char *str);
   int strcat(POOL_MEM &str);
   int strcat(const char *str);
   void toLower();
   size_t strlen() { return ::strlen(mem); };
   int bsprintf(const char *fmt, ...);
   int bvsprintf(const char *fmt, va_list arg_ptr);
};

int pm_strcat(POOLMEM *&pm, const char *str);
int pm_strcat(POOLMEM *&pm, POOL_MEM &str);
int pm_strcat(POOL_MEM &pm, const char *str);
int pm_strcat(POOL_MEM *&pm, const char *str);

int pm_strcpy(POOLMEM *&pm, const char *str);
int pm_strcpy(POOLMEM *&pm, POOL_MEM &str);
int pm_strcpy(POOL_MEM &pm, const char *str);
int pm_strcpy(POOL_MEM *&pm, const char *str);

int pm_memcpy(POOLMEM *&pm, const char *data, int32_t n);
int pm_memcpy(POOLMEM *&pm, POOL_MEM &data, int32_t n);
int pm_memcpy(POOL_MEM &pm, const char *data, int32_t n);
int pm_memcpy(POOL_MEM *&pm, const char *data, int32_t n);
#endif
