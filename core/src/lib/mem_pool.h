/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2022 Bareos GmbH & Co. KG

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
// Kern Sibbald, MM
/**
 * @file
 * Memory Pool prototypes
 */

#ifndef BAREOS_LIB_MEM_POOL_H_
#define BAREOS_LIB_MEM_POOL_H_

#include <stdarg.h>
#include <string.h>
#include <string>
#include "include/bc_types.h"

POOLMEM* GetPoolMemory(int pool) noexcept;
POOLMEM* GetMemory(int32_t size) noexcept;
int32_t SizeofPoolMemory(POOLMEM* buf) noexcept;
POOLMEM* ReallocPoolMemory(POOLMEM* buf, int32_t size) noexcept;
POOLMEM* CheckPoolMemorySize(POOLMEM* buf, int32_t size) noexcept;
void FreePoolMemory(POOLMEM* buf) noexcept;
inline void FreeMemory(POOLMEM* buf) noexcept { FreePoolMemory(buf); }

// Macro to simplify free/reset pointers
#define FreeAndNullPoolMemory(a) \
  do {                           \
    if (a) {                     \
      FreePoolMemory(a);         \
      (a) = NULL;                \
    }                            \
  } while (0)

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
  explicit PoolMem(int pool)
  {
    mem = GetPoolMemory(pool);
    *mem = 0;
  }
  explicit PoolMem(const char* str)
  {
    mem = GetPoolMemory(PM_NAME);
    *mem = 0;
    strcpy(str);
  }
  explicit PoolMem(const std::string& str) : PoolMem(str.c_str()) {}

  ~PoolMem()
  {
	  // handle the moved out case!
	  if (mem) {
		  FreePoolMemory(mem);
		  mem = NULL;
	  }
  }
	// needed since we have a custom destructor
	PoolMem(PoolMem&& moved) : mem{nullptr} {
		std::swap(moved.mem, mem);
	};
	PoolMem& operator=(PoolMem&& moved) {
		FreePoolMemory(mem);
		mem = moved.mem;
		moved.mem = nullptr;
		return *this;
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

int PmStrcat(POOLMEM*& dest_pm, const char* str);
int PmStrcat(POOLMEM*& dest_pm, PoolMem& str);
int PmStrcat(PoolMem& dest_pm, const char* str);
int PmStrcat(PoolMem*& dest_pm, const char* str);

int PmStrcpy(POOLMEM*& dest_pm, const char* str);
int PmStrcpy(POOLMEM*& dest_pm, PoolMem& str);
int PmStrcpy(PoolMem& dest_pm, const char* src_str);
int PmStrcpy(PoolMem*& dest_pm, const char* str);

int PmMemcpy(POOLMEM*& dest_pm, const char* data, int32_t n);
int PmMemcpy(POOLMEM*& dest_pm, PoolMem& data, int32_t n);
int PmMemcpy(PoolMem& dest_pm, const char* data, int32_t n);
int PmMemcpy(PoolMem*& dest_pm, const char* data, int32_t n);

#endif  // BAREOS_LIB_MEM_POOL_H_
