/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, MM
 */
/**
 * @file
 * Memory Pool prototypes
 */

#ifndef BAREOS_LIB_MEM_POOL_H_
#define BAREOS_LIB_MEM_POOL_H_

#if 0  // def SMARTALLOC Ueb

#define GetPoolMemory(pool) sm_get_pool_memory(__FILE__, __LINE__, pool)
POOLMEM* sm_get_pool_memory(const char* file, int line, int pool);

#define GetMemory(size) sm_get_memory(__FILE__, __LINE__, size)
POOLMEM* sm_get_memory(const char* fname, int line, int32_t size);

#define SizeofPoolMemory(buf) sm_sizeof_pool_memory(__FILE__, __LINE__, buf)
int32_t sm_sizeof_pool_memory(const char* fname, int line, POOLMEM* buf);

#define ReallocPoolMemory(buf, size) \
  sm_realloc_pool_memory(__FILE__, __LINE__, buf, size)
POOLMEM* sm_realloc_pool_memory(const char* fname,
                                int line,
                                POOLMEM* buf,
                                int32_t size);

#define CheckPoolMemorySize(buf, size) \
  sm_check_pool_memory_size(__FILE__, __LINE__, buf, size)
POOLMEM* sm_check_pool_memory_size(const char* fname,
                                   int line,
                                   POOLMEM* buf,
                                   int32_t size);

#define FreePoolMemory(x) SmFreePoolMemory(__FILE__, __LINE__, x)
#define FreeMemory(x) SmFreePoolMemory(__FILE__, __LINE__, x)
void SmFreePoolMemory(const char* fname, int line, POOLMEM* buf);

#else

POOLMEM* GetPoolMemory(int pool);
POOLMEM* GetMemory(int32_t size);
int32_t SizeofPoolMemory(POOLMEM* buf);
POOLMEM* ReallocPoolMemory(POOLMEM* buf, int32_t size);
POOLMEM* CheckPoolMemorySize(POOLMEM* buf, int32_t size);
#define FreeMemory(x) FreePoolMemory(x)
void FreePoolMemory(POOLMEM* buf);

#endif

/**
 * Macro to simplify free/reset pointers
 */
#define FreeAndNullPoolMemory(a) \
  do {                           \
    if (a) {                     \
      FreePoolMemory(a);         \
      (a) = NULL;                \
    }                            \
  } while (0)

void GarbageCollectMemoryPool();
void CloseMemoryPool();
void PrintMemoryPoolStats();

void GarbageCollectMemory();

enum
{
  PM_NOPOOL = 0,  /* Nonpooled memory */
  PM_NAME = 1,    /* BAREOS name */
  PM_FNAME = 2,   /* File name buffer */
  PM_MESSAGE = 3, /* Daemon message */
  PM_EMSG = 4,    /* Error message */
  PM_BSOCK = 5,   /* BareosSocket buffer */
  PM_RECORD = 6   /* DeviceRecord buffer */
};

#define PM_MAX PM_RECORD /* Number of types */

class PoolMem {
  char* mem;

 public:
  PoolMem()
  {
    mem = GetPoolMemory(PM_NAME);
    *mem = 0;
  }
  PoolMem(int pool)
  {
    mem = GetPoolMemory(pool);
    *mem = 0;
  }
  PoolMem(const char* str)
  {
    mem = GetPoolMemory(PM_NAME);
    *mem = 0;
    strcpy(str);
  }
  ~PoolMem()
  {
    FreePoolMemory(mem);
    mem = NULL;
  }
  char* c_str() const { return mem; }
  POOLMEM*& addr() { return mem; }
  int size() const { return SizeofPoolMemory(mem); }
  char* check_size(int32_t size)
  {
    mem = CheckPoolMemorySize(mem, size);
    return mem;
  }
  int32_t MaxSize();
  void ReallocPm(int32_t size);
  int strcpy(PoolMem& str);
  int strcpy(const char* str);
  int strcat(PoolMem& str);
  int strcat(const char* str);
  void toLower();
  size_t strlen() { return ::strlen(mem); }
  int bsprintf(const char* fmt, ...);
  int Bvsprintf(const char* fmt, va_list arg_ptr);
};

int PmStrcat(POOLMEM*& pm, const char* str);
int PmStrcat(POOLMEM*& pm, PoolMem& str);
int PmStrcat(PoolMem& pm, const char* str);
int PmStrcat(PoolMem*& pm, const char* str);

int PmStrcpy(POOLMEM*& pm, const char* str);
int PmStrcpy(POOLMEM*& pm, PoolMem& str);
int PmStrcpy(PoolMem& pm, const char* str);
int PmStrcpy(PoolMem*& pm, const char* str);

int PmMemcpy(POOLMEM*& pm, const char* data, int32_t n);
int PmMemcpy(POOLMEM*& pm, PoolMem& data, int32_t n);
int PmMemcpy(PoolMem& pm, const char* data, int32_t n);
int PmMemcpy(PoolMem*& pm, const char* data, int32_t n);
#endif
