/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2016 Bareos GmbH & Co. KG

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
 * Written by Kern Sibbald, MMIV
 */
/**
 * @file
 * Hash table class -- htable
 */

#ifndef BAREOS_LIB_HTABLE_H_
#define BAREOS_LIB_HTABLE_H_

/**
 * Loop var through each member of table
 */
#ifdef HAVE_TYPEOF
#define foreach_htable(var, tbl) \
        for((var)=(typeof(var))((tbl)->first()); \
           (var); \
           (var)=(typeof(var))((tbl)->next()))
#else
#define foreach_htable(var, tbl) \
        for((*((void **)&(var))=(void *)((tbl)->first())); \
            (var); \
            (*((void **)&(var))=(void *)((tbl)->next())))
#endif


#include "hostconfig.h"

#ifdef HAVE_HPUX_OS
#pragma pack(push,4)
#endif

typedef enum {
   KEY_TYPE_CHAR = 1,
   KEY_TYPE_UINT32 = 2,
   KEY_TYPE_UINT64 = 3,
   KEY_TYPE_BINARY = 4
} key_type_t;

union hlink_key {
   char *char_key;
   uint32_t uint32_key;
   uint64_t uint64_key;
   uint8_t *binary_key;
};

struct hlink {
   void *next;                        /* Next hash item */
   key_type_t key_type;               /* Type of key used to hash */
   union hlink_key key;               /* Key for this item */
   uint32_t key_len;                  /* Length of key for this item */
   uint64_t hash;                     /* Hash for this key */
};

struct h_mem {
   struct h_mem *next;                /* Next buffer */
   int32_t rem;                       /* Remaining bytes in big_buffer */
   char *mem;                         /* Memory pointer */
   char first[1];                     /* First byte */
};

#ifdef HAVE_HPUX_OS
#pragma pack(pop)
#endif

class DLL_IMP_EXP htable : public SmartAlloc {
   hlink **table;                     /* Hash table */
   int loffset;                       /* Link offset in item */
   hlink *walkptr;                    /* Table walk pointer */
   uint64_t hash;                     /* Temp storage */
   uint64_t total_size;               /* Total bytes malloced */
   uint32_t extend_length;            /* Number of bytes to allocate when extending buffer */
   uint32_t walk_index;               /* Table walk index */
   uint32_t num_items;                /* Current number of items */
   uint32_t max_items;                /* Maximum items before growing */
   uint32_t buckets;                  /* Size of hash table */
   uint32_t index;                    /* Temp storage */
   uint32_t mask;                     /* "Remainder" mask */
   uint32_t rshift;                   /* Amount to shift down */
   uint32_t blocks;                   /* Blocks malloced */
   struct h_mem *mem_block;           /* Malloc'ed memory block chain */
   void MallocBigBuf(int size);     /* Get a big buffer */
   void HashIndex(char *key);        /* Produce hash key,index */
   void HashIndex(uint32_t key);     /* Produce hash key,index */
   void HashIndex(uint64_t key);     /* Produce hash key,index */
   void HashIndex(uint8_t *key, uint32_t key_len); /* Produce hash key,index */
   void grow_table();                 /* Grow the table */

public:
   htable(void *item, void *link, int tsize = 31,
          int nr_pages = 0, int nr_entries = 4);
   ~htable() { destroy(); }
   void init(void *item, void *link, int tsize = 31,
             int nr_pages = 0, int nr_entries = 4);
   bool insert(char *key, void *item);
   bool insert(uint32_t key, void *item);
   bool insert(uint64_t key, void *item);
   bool insert(uint8_t *key, uint32_t key_len, void *item);
   void *lookup(char *key);
   void *lookup(uint32_t key);
   void *lookup(uint64_t key);
   void *lookup(uint8_t *key, uint32_t key_len);
   void *first();                     /* Get first item in table */
   void *next();                      /* Get next item in table */
   void destroy();
   void stats();                      /* Print stats about the table */
   uint32_t size();                   /* Return size of table */
   char *hash_malloc(int size);       /* Malloc bytes for a hash entry */
   void HashBigFree();              /* Free all hash allocated big buffers */
};
#endif  /* BAREOS_LIB_HTABLE_H_ */
