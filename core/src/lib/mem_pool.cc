/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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
 * Historically, the following was may have been true, but nowadays operating
 * systems are a lot better at handling memory than we are, so the pooling has
 * been removed.
 *
 * Andreas Rogge
 */
/*
 * BAREOS memory pool routines.
 *
 * The idea behind these routines is that there will be pools of memory that
 * are pre-allocated for quick access. The pools will have a fixed memory size
 * on allocation but if need be, the size can be increased. This is particularly
 * useful for filename buffers where 256 bytes should be sufficient in 99.99%
 * of the cases, but when it isn't we want to be able to increase the size.
 *
 * A major advantage of the pool memory aside from the speed is that the buffer
 * carries around its size, so to ensure that there is enough memory, simply
 * call the CheckPoolMemorySize() with the desired size and it will adjust only
 * if necessary.
 *
 * Kern E. Sibbald
 */
#if defined(HAVE_WIN32)
#  include "compat.h"
#endif
#include "lib/mem_pool.h"

#include <stdarg.h>
#include <string.h>

#include "lib/util.h"
#include "include/baconfig.h"

// Memory allocation control structures and storage.
struct abufhead {
  int32_t ablen;     /* Buffer length in bytes */
  int32_t bnet_size; /* dummy for BnetSend() */
};

constexpr int32_t HEAD_SIZE{BALIGN(sizeof(struct abufhead))};

/*
 * Special version of error reporting using a static buffer so we don't use
 * the normal error reporting which uses dynamic memory e.g. recursivly calls
 * these routines again leading to deadlocks.
 */
[[noreturn]] PRINTF_LIKE(3, 4) static void MemPoolErrorMessage(const char* file,
                                                               int line,
                                                               const char* fmt,
                                                               ...)
{
  char buf[256];
  va_list arg_ptr;
  int len;

  len = Bsnprintf(buf, sizeof(buf), T_("%s: ABORTING due to ERROR in %s:%d\n"),
                  my_name, get_basename(file), line);

  va_start(arg_ptr, fmt);
  Bvsnprintf(buf + len, sizeof(buf) - len, (char*)fmt, arg_ptr);
  va_end(arg_ptr);

  DispatchMessage(NULL, M_ABORT, 0, buf);
  abort();
}

static abufhead* GetPmHeader(POOLMEM* pm_ptr) noexcept
{
  ASSERT(pm_ptr);
  return static_cast<abufhead*>(static_cast<void*>(pm_ptr - HEAD_SIZE));
}

POOLMEM* GetPoolMemory(int pool) noexcept
{
  static constexpr int32_t pool_init_size[] = {
      256,                 /* PM_NOPOOL no pooling */
      MAX_NAME_LENGTH + 2, /* PM_NAME Bareos name */
      256,                 /* PM_FNAME filename buffers */
      512,                 /* PM_MESSAGE message buffer */
      1024,                /* PM_EMSG error message buffer */
      4096,                /* PM_BSOCK message buffer */
      128                  /* PM_RECORD message buffer */
  };
  return GetMemory(pool_init_size[pool]);
}

POOLMEM* GetMemory(int32_t size) noexcept
{
  char* buf = static_cast<char*>(malloc(size + HEAD_SIZE));
  if (buf == NULL) {
    MemPoolErrorMessage(__FILE__, __LINE__,
                        T_("Out of memory requesting %d bytes\n"), size);
  }
  POOLMEM* pm_ptr = static_cast<POOLMEM*>(buf + HEAD_SIZE);
  GetPmHeader(pm_ptr)->ablen = size;
  return pm_ptr;
}

int32_t SizeofPoolMemory(POOLMEM* obuf) noexcept
{
  return GetPmHeader(obuf)->ablen;
}

POOLMEM* ReallocPoolMemory(POOLMEM* obuf, int32_t size) noexcept
{
  if (size < 0) { return obuf; }

  struct abufhead* old_abuf_ptr = GetPmHeader(obuf);
  char* buf = static_cast<char*>(realloc(old_abuf_ptr, size + HEAD_SIZE));
  if (buf == NULL) {
    MemPoolErrorMessage(__FILE__, __LINE__,
                        T_("Out of memory requesting %d bytes\n"), size);
  }
  POOLMEM* new_pm_ptr = static_cast<POOLMEM*>(buf + HEAD_SIZE);
  GetPmHeader(new_pm_ptr)->ablen = size;
  return new_pm_ptr;
}

POOLMEM* CheckPoolMemorySize(POOLMEM* obuf, int32_t size) noexcept
{
  if (size <= SizeofPoolMemory(obuf)) { return obuf; }
  return ReallocPoolMemory(obuf, size);
}

void FreePoolMemory(POOLMEM* obuf) noexcept { free(GetPmHeader(obuf)); }

/*
 * Concatenate a string (str) onto a pool memory buffer pm
 * Returns: length of concatenated string
 */
int PmStrcat(POOLMEM*& dest_pm, const char* str)
{
  int pmlen = strlen(dest_pm);
  int len;

  if (!str) str = "";

  len = strlen(str) + 1;
  dest_pm = CheckPoolMemorySize(dest_pm, pmlen + len);
  memcpy(dest_pm + pmlen, str, len);
  return pmlen + len - 1;
}

int PmStrcat(POOLMEM*& dest_pm, PoolMem& str)
{
  return PmStrcat(dest_pm, str.c_str());
}

int PmStrcat(PoolMem& dest_pm, const char* str)
{
  int pmlen = strlen(dest_pm.c_str());
  int len;

  if (!str) str = "";

  len = strlen(str) + 1;
  dest_pm.check_size(pmlen + len);
  memcpy(dest_pm.c_str() + pmlen, str, len);
  return pmlen + len - 1;
}

int PmStrcat(PoolMem*& dest_pm, const char* str)
{
  return PmStrcat(*dest_pm, str);
}

/*
 * Copy a string (str) into a pool memory buffer pm
 * Returns: length of string copied
 */
int PmStrcpy(POOLMEM*& pm, const char* str)
{
  int len;

  if (!str) str = "";

  len = strlen(str) + 1;
  pm = CheckPoolMemorySize(pm, len);
  memcpy(pm, str, len);
  return len - 1;
}

int PmStrcpy(POOLMEM*& dest_pm, PoolMem& src_str)
{
  return PmStrcpy(dest_pm, src_str.c_str());
}

int PmStrcpy(PoolMem& dest_pm, const char* src_str)
{
  int len;

  if (!src_str) src_str = "";

  len = strlen(src_str) + 1;
  dest_pm.check_size(len);
  memcpy(dest_pm.c_str(), src_str, len);
  return len - 1;
}

int PmStrcpy(PoolMem*& dest_pm, const char* src_str)
{
  return PmStrcpy(*dest_pm, src_str);
}

/*
 * Copy data into a pool memory buffer pm
 * Returns: length of data copied
 */
int PmMemcpy(POOLMEM*& dest_pm, const char* data, int32_t n)
{
  dest_pm = CheckPoolMemorySize(dest_pm, n);
  memcpy(dest_pm, data, n);
  return n;
}

int PmMemcpy(POOLMEM*& dest_pm, PoolMem& data, int32_t n)
{
  return PmMemcpy(dest_pm, data.c_str(), n);
}

int PmMemcpy(PoolMem& dest_pm, const char* data, int32_t n)
{
  dest_pm.check_size(n);
  memcpy(dest_pm.c_str(), data, n);
  return n;
}

int PmMemcpy(PoolMem*& pm, const char* data, int32_t n)
{
  return PmMemcpy(*pm, data, n);
}

/* ==============  CLASS PoolMem   ============== */

// Return the size of a memory buffer
int32_t PoolMem::MaxSize() { return SizeofPoolMemory(mem); }

void PoolMem::ReallocPm(int32_t size) { mem = ReallocPoolMemory(mem, size); }

int PoolMem::strcat(PoolMem& str) { return PmStrcat(*this, str.c_str()); }

int PoolMem::strcat(const char* str) { return PmStrcat(*this, str); }

int PoolMem::strcpy(PoolMem& str) { return PmStrcpy(*this, str.c_str()); }

int PoolMem::strcpy(const char* str) { return PmStrcpy(*this, str); }

void PoolMem::toLower() { lcase(mem); }

int PoolMem::bsprintf(const char* fmt, ...)
{
  int len;
  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  len = Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);
  return len;
}

int PoolMem::Bvsprintf(const char* fmt, va_list arg_ptr)
{
  return PmVFormat(mem, fmt, arg_ptr);
}

int PmFormat(POOLMEM*& dest_pm, const char* fmt, ...)
{
  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  int len = PmVFormat(dest_pm, fmt, arg_ptr);
  va_end(arg_ptr);
  return len;
}

int PmVFormat(POOLMEM*& dest_pm, const char* fmt, va_list arg_ptr)
{
  va_list ap;

  ASSERT(SizeofPoolMemory(dest_pm) >= 0);

  for (;;) {
    int maxlen = SizeofPoolMemory(dest_pm);

    va_copy(ap, arg_ptr);
    int len = Bvsnprintf(dest_pm, maxlen, fmt, ap);
    va_end(ap);

    // if len == maxlen, then we might have been truncated, so we need to
    // try again
    if (len < maxlen) { return len; }

    // add 1 to make sure we always grow (think of maxlen = 0, 1)
    auto* mem = ReallocPoolMemory(dest_pm, maxlen + 1 + maxlen / 2);
    if (!mem) { return -1; }
    dest_pm = mem;
  }
}
