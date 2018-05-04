/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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

#ifndef BAREOS_FILED_BACKUP_H_
#define BAREOS_FILED_BACKUP_H_

struct b_save_ctx {
   JobControlRecord *jcr;                    /* Current Job Control Record */
   FindFilesPacket *ff_pkt;              /* File being processed */
   DIGEST *digest;              /* Encryption Digest */
   DIGEST *signing_digest;      /* Signing Digest */
   int digest_stream;           /* Type of Signing Digest */
};

struct b_ctx {
   JobControlRecord *jcr;                    /* Current Job Control Record */
   FindFilesPacket *ff_pkt;              /* File being processed */
   POOLMEM *msgsave;            /* Saved sd->msg */
   char *rbuf;                  /* Read buffer */
   char *wbuf;                  /* Write buffer */
   int32_t rsize;               /* Read size */
   uint64_t fileAddr;           /* File address */

   /*
    * Compression data.
    */
   const unsigned char *chead;  /* Compression header */
   unsigned char *cbuf;         /* Compression buffer when using generic comp_stream_header */
   uint32_t compress_len;       /* Actual length after compression */
   uint32_t max_compress_len;   /* Maximum size that will fit into compression buffer */
   comp_stream_header ch;       /* Compression Stream Header with info about compression used */

   /*
    * Encryption data.
    */
   uint32_t cipher_input_len;   /* Actual length of the data to encrypt */
   const uint8_t *cipher_input; /* Data to encrypt */
   uint32_t encrypted_len;      /* Actual length after encryption */
   DIGEST *digest;              /* Encryption Digest */
   DIGEST *signing_digest;      /* Signing Digest */
   CIPHER_CONTEXT *cipher_ctx;  /* Cipher context */
};

bool BlastDataToStorageDaemon(JobControlRecord *jcr, char *addr, crypto_cipher_t cipher);
bool EncodeAndSendAttributes(JobControlRecord *jcr, FindFilesPacket *ff_pkt, int &data_stream);
void StripPath(FindFilesPacket *ff_pkt);
void UnstripPath(FindFilesPacket *ff_pkt);

#endif
