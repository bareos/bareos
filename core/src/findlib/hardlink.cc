/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2014 Bareos GmbH & Co. KG

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
 * Written by Marco van Wieringen, May 2014
 */
/**
 * @file
 * Hardlinked files handling. The hardlinks are tracked using a htable.
 */

#include "bareos.h"
#include "find.h"

/**
 * Lookup a inode/dev in the list of hardlinked files.
 */
CurLink *lookup_hardlink(JobControlRecord *jcr, FindFilesPacket *ff_pkt, ino_t ino, dev_t dev)
{
   CurLink *hl;
   uint64_t binary_search_key[2];

   if (!ff_pkt->linkhash) {
      return NULL;
   }

   /*
    * Search link list of hard linked files
    */
   memset(binary_search_key, 0, sizeof(binary_search_key));
   binary_search_key[0] = dev;
   binary_search_key[1] = ino;

   hl = (CurLink *)ff_pkt->linkhash->lookup((uint8_t *)binary_search_key, sizeof(binary_search_key));

   return hl;
}

CurLink *new_hardlink(JobControlRecord *jcr, FindFilesPacket *ff_pkt, char *fname, ino_t ino, dev_t dev)
{
   int len;
   CurLink *hl = NULL;
   uint64_t binary_search_key[2];
   uint8_t *new_key;

   if (!ff_pkt->linkhash) {
      ff_pkt->linkhash = (htable *)malloc(sizeof(htable));
      ff_pkt->linkhash->init(hl, &hl->link, 10000, 480);
   }

   len = strlen(fname) + 1;
   hl = (CurLink *)ff_pkt->linkhash->hash_malloc(sizeof(CurLink) + len);
   hl->digest = NULL;                /* Set later */
   hl->digest_stream = 0;            /* Set later */
   hl->digest_len = 0;               /* Set later */
   hl->ino = ino;
   hl->dev = dev;
   hl->FileIndex = 0;                /* Set later */
   bstrncpy(hl->name, fname, len);

   memset(binary_search_key, 0, sizeof(binary_search_key));
   binary_search_key[0] = dev;
   binary_search_key[1] = ino;

   new_key = (uint8_t *)ff_pkt->linkhash->hash_malloc(sizeof(binary_search_key));
   memcpy(new_key, binary_search_key, sizeof(binary_search_key));

   ff_pkt->linkhash->insert(new_key, sizeof(binary_search_key), hl);

   return hl;
}

/**
 * When the current file is a hardlink, the backup code can compute
 * the checksum and store it into the CurLink structure.
 */
void ff_pkt_set_link_digest(FindFilesPacket *ff_pkt, int32_t digest_stream,
                            const char *digest, uint32_t len)
{
   if (ff_pkt->linked && !ff_pkt->linked->digest) {     /* is a hardlink */
      ff_pkt->linked->digest = (char *) ff_pkt->linkhash->hash_malloc(len);
      memcpy(ff_pkt->linked->digest, digest, len);
      ff_pkt->linked->digest_len = len;
      ff_pkt->linked->digest_stream = digest_stream;
   }
}
