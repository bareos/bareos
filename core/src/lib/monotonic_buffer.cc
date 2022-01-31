/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include "monotonic_buffer.h"

struct MonotonicBuffer::Allocation {
  struct Allocation* next; /* Next buffer */
  size_t rem;              /* Remaining bytes in this buffer */
  char* mem;               /* Memory pointer */
  max_align_t first[1];    /* First byte */
};

static size_t get_alloc_size(MonotonicBuffer::Size size)
{
  switch (size) {
    case MonotonicBuffer::Size::Small:
      return 32 * 4096;
    case MonotonicBuffer::Size::Medium:
      return 512 * 4096;
    case MonotonicBuffer::Size::Large:
      return 2400 * 4096;
  }
  throw std::invalid_argument("unsupported MonotonicBuffer::Size");
}

/* round-up size to next multiple of alignof(max_align_t) */
static size_t aligned_size(size_t size)
{
  constexpr auto mod = alignof(max_align_t);

  auto rem = size % mod;
  if (rem == 0) return size;
  return size + mod - rem;
}

MonotonicBuffer::~MonotonicBuffer()
{
  while (mem_block_) {
    Allocation* ptr = mem_block_;
    mem_block_ = mem_block_->next;
    free(ptr);
  }
}

void* MonotonicBuffer::allocate(size_t size)
{
  const auto asize = aligned_size(size);
  assert(asize >= size);

  // allocate a new mem_block if needed
  if (!mem_block_ || mem_block_->rem < asize) {
    size_t alloc_size = get_alloc_size(grow_size_);
    Allocation* hmem = (Allocation*)malloc(alloc_size);
    hmem->next = mem_block_;
    hmem->mem = static_cast<char*>(static_cast<void*>(hmem->first));
    hmem->rem = alloc_size - offsetof(Allocation, first);
    mem_block_ = hmem;
  }
  mem_block_->rem -= asize;
  void* buf = mem_block_->mem;
  mem_block_->mem += asize;
  return buf;
}
