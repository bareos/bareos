/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG
   Copyright (C) 2009 Free Software Foundation Europe e.V.

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

#ifndef BAREOS_FILED_RESTORE_H_
#define BAREOS_FILED_RESTORE_H_

struct DelayedDataStream {
   int32_t stream;                     /* stream less new bits */
   char *content;                      /* stream data */
   uint32_t content_length;            /* stream length */
};

struct RestoreCipherContext {
   CIPHER_CONTEXT *cipher;
   uint32_t block_size;

   POOLMEM *buf;                       /* Pointer to descryption buffer */
   int32_t buf_len;                    /* Count of bytes currently in buf */
   int32_t packet_len;                 /* Total bytes in packet */
};

struct r_ctx {
   JobControlRecord *jcr;
   int32_t stream;                     /* stream less new bits */
   int32_t prev_stream;                /* previous stream */
   int32_t full_stream;                /* full stream including new bits */
   int32_t comp_stream;                /* last compressed stream found. needed only to restore encrypted compressed backup */
   BareosWinFilePacket bfd;                          /* File content */
   uint64_t fileAddr;                  /* file write address */
   uint32_t size;                      /* Size of file */
   char flags[FOPTS_BYTES];            /* Options for ExtractData() */
   BareosWinFilePacket forkbfd;                      /* Alternative data stream */
   uint64_t fork_addr;                 /* Write address for alternative stream */
   int64_t fork_size;                  /* Size of alternate stream */
   char fork_flags[FOPTS_BYTES];       /* Options for ExtractData() */
   int32_t type;                       /* file type FT_ */
   Attributes *attr;                         /* Pointer to attributes */
   bool extract;                       /* set when extracting */
   alist *delayed_streams;             /* streams that should be restored as last */

   SIGNATURE *sig;                     /* Cryptographic signature (if any) for file */
   CRYPTO_SESSION *cs;                 /* Cryptographic session data (if any) for file */
   RestoreCipherContext cipher_ctx;      /* Cryptographic restore context (if any) for file */
   RestoreCipherContext fork_cipher_ctx; /* Cryptographic restore context (if any) for alternative stream */
};

void DoRestore(JobControlRecord *jcr);
void FreeSession(r_ctx &rctx);
int DoFileDigest(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool top_level);
bool SparseData(JobControlRecord *jcr, BareosWinFilePacket *bfd, uint64_t *addr, char **data, uint32_t *length);
bool StoreData(JobControlRecord *jcr, BareosWinFilePacket *bfd, char *data, const int32_t length, bool win32_decomp);

#endif
