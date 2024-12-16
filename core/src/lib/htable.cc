/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2024 Bareos GmbH & Co. KG

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
 * BAREOS hash table routines
 *
 * htable is a hash table of items (pointers). This code is
 * adapted and enhanced from code I wrote in 1982 for a
 * relocatable linker.  At that time, the hash table size
 * was fixed and a primary number, which essentially provides
 * the randomness. In this program, the hash table can grow when
 * it gets too full, so the table size here is a binary number. The
 * hashing is provided using an idea from Tcl where the initial
 * hash code is "randomized" using a simple calculation from
 * a random number generator that multiplies by a big number
 * (I multiply by a prime number, while Tcl did not)
 * then shifts the result down and does the binary division
 * by masking.  Increasing the size of the hash table is simple.
 * Just create a new larger table, walk the old table and
 * re-hash insert each entry into the new table.
 *
 * Kern Sibbald, July MMIII
 */

#include <cinttypes>
#include "include/config.h"

#include "include/bareos.h"
#include "lib/htable.h"

static const int debuglevel = 500;

// htable (Hash Table) class.

static inline long long unsigned cast(std::uint64_t x)
{
  return static_cast<long long unsigned>(x);
}

/*
 * Create hash of key, stored in hash then
 * create and return the pseudo random bucket index
 */
void htableImpl::HashIndex(char* key)
{
  hash = 0;
  for (char* p = key; *p; p++) {
    hash += ((hash << 5) | (hash >> (sizeof(hash) * 8 - 5))) + (uint32_t)*p;
  }

  // Multiply by large prime number, take top bits, mask for remainder.
  index = ((hash * 1103515249) >> rshift) & mask;
  Dmsg2(debuglevel, "Leave HashIndex hash=0x%llx index=%d\n", cast(hash),
        index);
}

void htableImpl::HashIndex(uint32_t key)
{
  hash = key;

  // Multiply by large prime number, take top bits, mask for remainder.
  index = ((hash * 1103515249) >> rshift) & mask;
  Dmsg2(debuglevel, "Leave HashIndex hash=0x%llx index=%d\n", cast(hash),
        index);
}

void htableImpl::HashIndex(uint64_t key)
{
  hash = key;

  // Multiply by large prime number, take top bits, mask for remainder.
  index = ((hash * 1103515249) >> rshift) & mask;
  Dmsg2(debuglevel, "Leave HashIndex hash=0x%llx index=%d\n", cast(hash),
        index);
}

void htableImpl::HashIndex(uint8_t* key, uint32_t keylen)
{
  hash = 0;
  for (uint8_t* p = key; keylen--; p++) {
    hash += ((hash << 5) | (hash >> (sizeof(hash) * 8 - 5))) + (uint32_t)*p;
  }

  // Multiply by large prime number, take top bits, mask for remainder.
  index = ((hash * 1103515249) >> rshift) & mask;
  Dmsg2(debuglevel, "Leave HashIndex hash=0x%llx index=%d\n", cast(hash),
        index);
}

// tsize is the estimated number of entries in the hash table
htableImpl::htableImpl(size_t t_loffset, int tsize)
{
  init(tsize);
  loffset = t_loffset;
}

void htableImpl::init(int tsize)
{
  memset(this, 0, sizeof(htableImpl));
  if (tsize < 31) { tsize = 31; }
  tsize >>= 2;

  int pwr;
  for (pwr = 0; tsize; pwr++) { tsize >>= 1; }
  rshift = 30 - pwr;  /* Start using bits 28, 29, 30 */
  buckets = 1 << pwr; /* Hash table size -- power of two */

  mask = buckets - 1;      /* 3 bits => table size = 8 */
  max_items = buckets * 4; /* Allow average nr_entries entries per chain */
  table = (hlink**)malloc(buckets * sizeof(hlink*));
  memset(table, 0, buckets * sizeof(hlink*));
}

uint32_t htableImpl::size() { return num_items; }

/*
 * Take each hash link and walk down the chain of items
 *  that hash there counting them (i.e. the hits),
 *  then report that number.
 * Obiously, the more hits in a chain, the more time
 *  it takes to reference them. Empty chains are not so
 *  hot either -- as it means unused or wasted space.
 */
#define MAX_COUNT 20
void htableImpl::stats()
{
  int hits[MAX_COUNT];
  int max = 0;
  int i, j;
  hlink* p;
  printf("\n\nNumItems=%d\nTotal buckets=%d\n", num_items, buckets);
  printf("Hits/bucket: buckets\n");
  for (i = 0; i < MAX_COUNT; i++) { hits[i] = 0; }
  for (i = 0; i < (int)buckets; i++) {
    p = table[i];
    j = 0;
    while (p) {
      p = (hlink*)(p->next);
      j++;
    }
    if (j > max) { max = j; }
    if (j < MAX_COUNT) { hits[j]++; }
  }
  for (i = 0; i < MAX_COUNT; i++) { printf("%2d:           %d\n", i, hits[i]); }
  printf("buckets=%d num_items=%d max_items=%d\n", buckets, num_items,
         max_items);
  printf("max hits in a bucket = %d\n", max);
}

void htableImpl::grow_table()
{
  htableImpl* big;
  hlink* cur;
  void* next_item;

  Dmsg1(100, "Grow called old size = %d\n", buckets);

  // Setup a bigger table.
  big = (htableImpl*)malloc(sizeof(htableImpl));
  big->hash = hash;
  big->index = index;
  big->loffset = loffset;
  big->mask = mask << 1 | 1;
  big->rshift = rshift - 1;
  big->num_items = 0;
  big->buckets = buckets * 2;
  big->max_items = big->buckets * 4;

  // Create a bigger hash table.
  big->table = (hlink**)malloc(big->buckets * sizeof(hlink*));
  memset(big->table, 0, big->buckets * sizeof(hlink*));
  big->walkptr = NULL;
  big->walk_index = 0;

  // Insert all the items in the new hash table
  Dmsg1(100, "Before copy num_items=%d\n", num_items);

  /* We walk through the old smaller tree getting items,
   * but since we are overwriting the colision links, we must
   * explicitly save the item->next pointer and walk each
   * colision chain ourselves. We do use next() for getting
   * to the next bucket. */
  for (void* item = first(); item;) {
    cur = (hlink*)((char*)item + loffset);
    next_item = cur->next; /* Save link overwritten by insert */
    switch (cur->key_type) {
      case KEY_TYPE_CHAR:
        Dmsg1(100, "Grow insert: %s\n", cur->key.char_key);
        big->insert(cur->key.char_key, item);
        break;
      case KEY_TYPE_UINT32:
        Dmsg1(100, "Grow insert: %" PRIu32 "\n", cur->key.uint32_key);
        big->insert(cur->key.uint32_key, item);
        break;
      case KEY_TYPE_UINT64:
        Dmsg1(100, "Grow insert: %" PRId64 "\n", cur->key.uint64_key);
        big->insert(cur->key.uint64_key, item);
        break;
      case KEY_TYPE_BINARY:
        big->insert(cur->key.binary_key, cur->key_len, item);
        break;
    }
    if (next_item) {
      item = (void*)((char*)next_item - loffset);
    } else {
      walkptr = NULL;
      item = next();
    }
  }

  Dmsg1(100, "After copy new num_items=%d\n", big->num_items);
  if (num_items != big->num_items) {
    Dmsg0(000, "****** Big problems num_items mismatch ******\n");
  }

  free(table);
  memcpy(this, big, sizeof(htableImpl)); /* Move everything across */
  free(big);

  Dmsg0(100, "Exit grow.\n");
}

bool htableImpl::insert(char* key, void* item)
{
  hlink* hp;

  if (lookup(key)) { return false; /* Already exists */ }

  ASSERT(index < buckets);
  Dmsg2(debuglevel, "Insert: hash=0x%llx index=%d\n", cast(hash), index);
  hp = (hlink*)(((char*)item) + loffset);

  Dmsg4(debuglevel, "Insert hp=%p index=%d item=%p offset=%" PRIuz "\n", hp,
        index, item, loffset);

  hp->next = table[index];
  hp->hash = hash;
  hp->key_type = KEY_TYPE_CHAR;
  hp->key.char_key = key;
  hp->key_len = 0;
  table[index] = hp;

  Dmsg3(debuglevel, "Insert hp->next=%p hp->hash=0x%llx hp->key=%s\n", hp->next,
        cast(hp->hash), hp->key.char_key);

  if (++num_items >= max_items) {
    Dmsg2(debuglevel, "num_items=%d max_items=%d\n", num_items, max_items);
    grow_table();
  }

  Dmsg3(debuglevel, "Leave insert index=%d num_items=%d key=%s\n", index,
        num_items, key);

  return true;
}

bool htableImpl::insert(uint32_t key, void* item)
{
  hlink* hp;

  if (lookup(key)) { return false; /* Already exists */ }

  ASSERT(index < buckets);
  Dmsg2(debuglevel, "Insert: hash=0x%llx index=%d\n", cast(hash), index);
  hp = (hlink*)(((char*)item) + loffset);

  Dmsg4(debuglevel, "Insert hp=%p index=%d item=%p offset=%" PRIuz "\n", hp,
        index, item, loffset);

  hp->next = table[index];
  hp->hash = hash;
  hp->key_type = KEY_TYPE_UINT32;
  hp->key.uint32_key = key;
  hp->key_len = 0;
  table[index] = hp;

  Dmsg3(debuglevel, "Insert hp->next=%p hp->hash=0x%llx hp->key=%" PRIu32 "\n",
        hp->next, cast(hp->hash), hp->key.uint32_key);

  if (++num_items >= max_items) {
    Dmsg2(debuglevel, "num_items=%d max_items=%d\n", num_items, max_items);
    grow_table();
  }

  Dmsg3(debuglevel, "Leave insert index=%d num_items=%d key=%" PRIu32 "\n",
        index, num_items, key);

  return true;
}

bool htableImpl::insert(uint64_t key, void* item)
{
  hlink* hp;

  if (lookup(key)) { return false; /* Already exists */ }

  ASSERT(index < buckets);
  Dmsg2(debuglevel, "Insert: hash=0x%llx index=%d\n", cast(hash), index);
  hp = (hlink*)(((char*)item) + loffset);

  Dmsg4(debuglevel, "Insert hp=%p index=%d item=%p offset=%" PRIuz "\n", hp,
        index, item, loffset);

  hp->next = table[index];
  hp->hash = hash;
  hp->key_type = KEY_TYPE_UINT64;
  hp->key.uint64_key = key;
  hp->key_len = 0;
  table[index] = hp;

  Dmsg3(debuglevel, "Insert hp->next=%p hp->hash=0x%llx hp->key=%" PRIu64 "\n",
        hp->next, cast(hp->hash), hp->key.uint64_key);

  if (++num_items >= max_items) {
    Dmsg2(debuglevel, "num_items=%d max_items=%d\n", num_items, max_items);
    grow_table();
  }

  Dmsg3(debuglevel, "Leave insert index=%d num_items=%d key=%" PRIu64 "\n",
        index, num_items, key);

  return true;
}

bool htableImpl::insert(uint8_t* key, uint32_t key_len, void* item)
{
  hlink* hp;

  if (lookup(key, key_len)) { return false; /* Already exists */ }

  ASSERT(index < buckets);
  Dmsg2(debuglevel, "Insert: hash=0x%llx index=%d\n", cast(hash), index);
  hp = (hlink*)(((char*)item) + loffset);

  Dmsg4(debuglevel, "Insert hp=%p index=%d item=%p offset=%" PRIuz "\n", hp,
        index, item, loffset);

  hp->next = table[index];
  hp->hash = hash;
  hp->key_type = KEY_TYPE_BINARY;
  hp->key.binary_key = key;
  hp->key_len = key_len;
  table[index] = hp;

  Dmsg2(debuglevel, "Insert hp->next=%p hp->hash=0x%llx\n", hp->next,
        cast(hp->hash));

  if (++num_items >= max_items) {
    Dmsg2(debuglevel, "num_items=%d max_items=%d\n", num_items, max_items);
    grow_table();
  }

  Dmsg2(debuglevel, "Leave insert index=%d num_items=%d\n", index, num_items);

  return true;
}

void* htableImpl::lookup(char* key)
{
  HashIndex(key);
  for (hlink* hp = table[index]; hp; hp = (hlink*)hp->next) {
    ASSERT(hp->key_type == KEY_TYPE_CHAR);
    if (hash == hp->hash && bstrcmp(key, hp->key.char_key)) {
      Dmsg1(debuglevel, "lookup return %p\n", ((char*)hp) - loffset);
      return ((char*)hp) - loffset;
    }
  }

  return NULL;
}

void* htableImpl::lookup(uint32_t key)
{
  HashIndex(key);
  for (hlink* hp = table[index]; hp; hp = (hlink*)hp->next) {
    ASSERT(hp->key_type == KEY_TYPE_UINT32);
    if (hash == hp->hash && key == hp->key.uint32_key) {
      Dmsg1(debuglevel, "lookup return %p\n", ((char*)hp) - loffset);
      return ((char*)hp) - loffset;
    }
  }

  return NULL;
}

void* htableImpl::lookup(uint64_t key)
{
  HashIndex(key);
  for (hlink* hp = table[index]; hp; hp = (hlink*)hp->next) {
    ASSERT(hp->key_type == KEY_TYPE_UINT64);
    if (hash == hp->hash && key == hp->key.uint64_key) {
      Dmsg1(debuglevel, "lookup return %p\n", ((char*)hp) - loffset);
      return ((char*)hp) - loffset;
    }
  }

  return NULL;
}

void* htableImpl::lookup(uint8_t* key, uint32_t key_len)
{
  HashIndex(key, key_len);
  for (hlink* hp = table[index]; hp; hp = (hlink*)hp->next) {
    ASSERT(hp->key_type == KEY_TYPE_BINARY);
    if (hash == hp->hash && memcmp(key, hp->key.binary_key, hp->key_len) == 0) {
      Dmsg1(debuglevel, "lookup return %p\n", ((char*)hp) - loffset);
      return ((char*)hp) - loffset;
    }
  }

  return NULL;
}

void* htableImpl::next()
{
  Dmsg1(debuglevel, "Enter next: walkptr=%p\n", walkptr);
  if (walkptr) { walkptr = (hlink*)(walkptr->next); }

  while (!walkptr && walk_index < buckets) {
    walkptr = table[walk_index++];
    if (walkptr) {
      Dmsg3(debuglevel, "new walkptr=%p next=%p inx=%d\n", walkptr,
            walkptr->next, walk_index - 1);
    }
  }

  if (walkptr) {
    Dmsg2(debuglevel, "next: rtn %p walk_index=%d\n",
          ((char*)walkptr) - loffset, walk_index);
    return ((char*)walkptr) - loffset;
  }
  Dmsg0(debuglevel, "next: return NULL\n");

  return NULL;
}

void* htableImpl::first()
{
  Dmsg0(debuglevel, "Enter first\n");
  walkptr = table[0]; /* get first bucket */
  walk_index = 1;     /* Point to next index */

  while (!walkptr && walk_index < buckets) {
    walkptr = table[walk_index++]; /* go to next bucket */
    if (walkptr) {
      Dmsg3(debuglevel, "first new walkptr=%p next=%p inx=%d\n", walkptr,
            walkptr->next, walk_index - 1);
    }
  }

  if (walkptr) {
    Dmsg1(debuglevel, "Leave first walkptr=%p\n", walkptr);
    return ((char*)walkptr) - loffset;
  }
  Dmsg0(debuglevel, "Leave first walkptr=NULL\n");

  return NULL;
}

/* Destroy the table and its contents */
void htableImpl::destroy()
{
  free(table);
  table = NULL;
  Dmsg0(100, "Done destroy.\n");
}
