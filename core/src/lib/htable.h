/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2022 Bareos GmbH & Co. KG

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
// Written by Kern Sibbald, MMIV
/**
 * @file
 * Hash table class -- htable
 */

#ifndef BAREOS_LIB_HTABLE_H_
#define BAREOS_LIB_HTABLE_H_

#define foreach_htable(var, tbl) \
  for ((var) = (tbl)->first(); (var); (var) = (tbl)->next())

#include <memory>

#include "include/config.h"
#include "monotonic_buffer.h"

typedef enum
{
  KEY_TYPE_CHAR = 1,
  KEY_TYPE_UINT32 = 2,
  KEY_TYPE_UINT64 = 3,
  KEY_TYPE_BINARY = 4
} key_type_t;

union hlink_key {
  char* char_key;
  uint32_t uint32_key;
  uint64_t uint64_key;
  uint8_t* binary_key;
};

struct hlink {
  void* next;          /* Next hash item */
  key_type_t key_type; /* Type of key used to hash */
  union hlink_key key; /* Key for this item */
  uint32_t key_len;    /* Length of key for this item */
  uint64_t hash;       /* Hash for this key */
};

class htableImpl {
  hlink** table = nullptr;      /* Hash table */
  int loffset = 0;              /* Link offset in item */
  hlink* walkptr = nullptr;     /* Table walk pointer */
  uint64_t hash = 0;            /* Temp storage */
  uint32_t walk_index = 0;      /* Table walk index */
  uint32_t num_items = 0;       /* Current number of items */
  uint32_t max_items = 0;       /* Maximum items before growing */
  uint32_t buckets = 0;         /* Size of hash table */
  uint32_t index = 0;           /* Temp storage */
  uint32_t mask = 0;            /* "Remainder" mask */
  uint32_t rshift = 0;          /* Amount to shift down */
  void HashIndex(char* key);    /* Produce hash key,index */
  void HashIndex(uint32_t key); /* Produce hash key,index */
  void HashIndex(uint64_t key); /* Produce hash key,index */
  void HashIndex(uint8_t* key, uint32_t key_len); /* Produce hash key,index */
  void grow_table();                              /* Grow the table */

 public:
  htableImpl() = default;
  htableImpl(int t_loffset, int tsize = 31);
  ~htableImpl() { destroy(); }
  void init(int tsize = 31);
  bool insert(char* key, void* item);
  bool insert(uint32_t key, void* item);
  bool insert(uint64_t key, void* item);
  bool insert(uint8_t* key, uint32_t key_len, void* item);
  void* lookup(char* key);
  void* lookup(uint32_t key);
  void* lookup(uint64_t key);
  void* lookup(uint8_t* key, uint32_t key_len);
  void* first(); /* Get first item in table */
  void* next();  /* Get next item in table */
  void destroy();
  void stats();    /* Print stats about the table */
  uint32_t size(); /* Return size of table */
};

struct htable_binary_key {
  uint8_t* ptr;
  uint32_t len;
};

template <typename Key,
          typename T,
          enum MonotonicBuffer::Size BufferSize = MonotonicBuffer::Size::Large>
class htable {
  std::unique_ptr<htableImpl> pimpl;
  MonotonicBuffer monobuf{BufferSize};

 public:
  htable(int tsize = 31)
  {
    if constexpr (std::is_same<T, hlink>::value) {
      pimpl = std::make_unique<htableImpl>(0, tsize);
    } else {
      pimpl = std::make_unique<htableImpl>(offsetof(T, link), tsize);
    }
  }
  T* lookup(Key key)
  {
    if constexpr (std::is_same<Key, htable_binary_key>::value) {
      return static_cast<T*>(pimpl->lookup(key.ptr, key.len));
    } else {
      return static_cast<T*>(pimpl->lookup(key));
    }
  }
  bool insert(Key key, T* item)
  {
    if constexpr (std::is_same<Key, htable_binary_key>::value) {
      return pimpl->insert(key.ptr, key.len, item);
    } else {
      return pimpl->insert(key, item);
    }
  }

  char* hash_malloc(int size)
  {
    return static_cast<char*>(monobuf.allocate(size));
  }
  T* first() { return static_cast<T*>(pimpl->first()); }
  T* next() { return static_cast<T*>(pimpl->next()); }
  uint32_t size() { return pimpl->size(); }
};

#endif  // BAREOS_LIB_HTABLE_H_
