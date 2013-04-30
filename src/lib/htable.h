/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Written by Kern Sibbald, MMIV
 */

#ifndef HTABLE_H
#define HTABLE_H

/* ========================================================================
 *
 *   Hash table class -- htable
 *
 */

/*
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

typedef enum {
   KEY_TYPE_CHAR = 1,
   KEY_TYPE_UINT32 = 2,
   KEY_TYPE_UINT64 = 3
} key_type_t;

union hlink_key {
   char *char_key;
   uint32_t uint32_key;
   uint64_t uint64_key;
};

struct hlink {
   void *next;                        /* next hash item */
   key_type_t key_type;               /* type of key used to hash */
   union hlink_key key;               /* key for this item */
   uint64_t hash;                     /* hash for this key */
};

struct h_mem {
   struct h_mem *next;                /* next buffer */
   int32_t rem;                       /* remaining bytes in big_buffer */
   char *mem;                         /* memory pointer */
   char first[1];                     /* first byte */
};

class htable : public SMARTALLOC {
   hlink **table;                     /* hash table */
   int loffset;                       /* link offset in item */
   hlink *walkptr;                    /* table walk pointer */
   uint64_t hash;                     /* temp storage */
   uint64_t total_size;               /* total bytes malloced */
   uint32_t extend_length;            /* number of bytes to allocate when extending buffer */
   uint32_t walk_index;               /* table walk index */
   uint32_t num_items;                /* current number of items */
   uint32_t max_items;                /* maximum items before growing */
   uint32_t buckets;                  /* size of hash table */
   uint32_t index;                    /* temp storage */
   uint32_t mask;                     /* "remainder" mask */
   uint32_t rshift;                   /* amount to shift down */
   uint32_t blocks;                   /* blocks malloced */
   struct h_mem *mem_block;           /* malloc'ed memory block chain */
   void malloc_big_buf(int size);     /* Get a big buffer */
   void hash_index(char *key);        /* produce hash key,index */
   void hash_index(uint32_t key);     /* produce hash key,index */
   void hash_index(uint64_t key);     /* produce hash key,index */
   void grow_table();                 /* grow the table */

public:
   htable(void *item, void *link, int tsize = 31, int nr_pages = 0);
   ~htable() { destroy(); }
   void init(void *item, void *link, int tsize = 31, int nr_pages = 0);
   bool insert(char *key, void *item);
   bool insert(uint32_t key, void *item);
   bool insert(uint64_t key, void *item);
   void *lookup(char *key);
   void *lookup(uint32_t key);
   void *lookup(uint64_t key);
   void *first();                     /* get first item in table */
   void *next();                      /* get next item in table */
   void destroy();
   void stats();                      /* print stats about the table */
   uint32_t size();                   /* return size of table */
   char *hash_malloc(int size);       /* malloc bytes for a hash entry */
   void hash_big_free();              /* free all hash allocated big buffers */
};

#endif  /* HTABLE_H */
