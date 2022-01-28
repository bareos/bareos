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

// Loop var through each member of table
#ifdef HAVE_TYPEOF
#  define foreach_htable(var, tbl)                     \
    for ((var) = (typeof(var))((tbl)->first()); (var); \
         (var) = (typeof(var))((tbl)->next()))
#else
#  define foreach_htable(var, tbl)                             \
    for ((*((void**)&(var)) = (void*)((tbl)->first())); (var); \
         (*((void**)&(var)) = (void*)((tbl)->next())))
#endif


#include "include/config.h"

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

struct h_mem {
  struct h_mem* next; /* Next buffer */
  int32_t rem;        /* Remaining bytes in big_buffer */
  char* mem;          /* Memory pointer */
  char first[1];      /* First byte */
};

class htableImpl {
  hlink** table = nullptr;  /* Hash table */
  int loffset = 0;          /* Link offset in item */
  hlink* walkptr = nullptr; /* Table walk pointer */
  uint64_t hash = 0;        /* Temp storage */
  uint64_t total_size = 0;  /* Total bytes malloced */
  uint32_t extend_length
      = 0; /* Number of bytes to allocate when extending buffer */
  uint32_t walk_index = 0;           /* Table walk index */
  uint32_t num_items = 0;            /* Current number of items */
  uint32_t max_items = 0;            /* Maximum items before growing */
  uint32_t buckets = 0;              /* Size of hash table */
  uint32_t index = 0;                /* Temp storage */
  uint32_t mask = 0;                 /* "Remainder" mask */
  uint32_t rshift = 0;               /* Amount to shift down */
  uint32_t blocks = 0;               /* Blocks malloced */
  struct h_mem* mem_block = nullptr; /* Malloc'ed memory block chain */
  void MallocBigBuf(int size);       /* Get a big buffer */
  void HashIndex(char* key);         /* Produce hash key,index */
  void HashIndex(uint32_t key);      /* Produce hash key,index */
  void HashIndex(uint64_t key);      /* Produce hash key,index */
  void HashIndex(uint8_t* key, uint32_t key_len); /* Produce hash key,index */
  void grow_table();                              /* Grow the table */

 public:
  htableImpl() = default;
  htableImpl(void* item, void* link, int tsize = 31, int nr_pages = 0);
  ~htableImpl() { destroy(); }
  void init(int tsize = 31, int nr_pages = 0);
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

 private:
  void stats(); /* Print stats about the table */
 public:
  uint32_t size();             /* Return size of table */
  char* hash_malloc(int size); /* Malloc bytes for a hash entry */
 private:
  void HashBigFree(); /* Free all hash allocated big buffers */
};

struct htable_binary_key {
  uint8_t* ptr;
  uint32_t len;
};

enum class htableBufferSize
{
  small,
  medium,
  large
};

template <typename Key,
          typename T,
          enum htableBufferSize BufferSize = htableBufferSize::large>
class htable {
  std::unique_ptr<htableImpl> pimpl;

 public:
  htable() { pimpl = std::make_unique<htableImpl>(); }
  htable(T* item, hlink* link, int tsize = 31)
  {
    int nr_pages = 0;
    if constexpr (BufferSize == htableBufferSize::large) {
      nr_pages = 0;
    } else if constexpr (BufferSize == htableBufferSize::medium) {
      nr_pages = 512;
    } else if constexpr (BufferSize == htableBufferSize::small) {
      nr_pages = 1;
    }

    pimpl = std::make_unique<htableImpl>(item, link, tsize, nr_pages);
  }
  T* lookup(Key key)
  {
    if constexpr (std::is_same<Key, htable_binary_key>::value) {
      return (T*)pimpl->lookup(key.ptr, key.len);
    } else {
      return (T*)pimpl->lookup(key);
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

  char* hash_malloc(int size) { return pimpl->hash_malloc(size); }
  T* first() { return (T*)pimpl->first(); }
  T* next() { return (T*)pimpl->next(); }
  uint32_t size() { return pimpl->size(); }
};

#endif  // BAREOS_LIB_HTABLE_H_
