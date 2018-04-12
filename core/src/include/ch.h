/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
/**
 * Laurent Papier
 */
/**
 * @file
 * Compressed stream header struct
 */

#ifndef __CH_H
#define __CH_H 1

/**
 * Compression algorithm signature. 4 letters as a 32bits integer
 */
#define COMPRESS_NONE  0x4e4f4e45  /* used for incompressible block */
#define COMPRESS_GZIP  0x475a4950
#define COMPRESS_LZO1X 0x4c5a4f58
#define COMPRESS_FZFZ  0x465A465A
#define COMPRESS_FZ4L  0x465A344C
#define COMPRESS_FZ4H  0x465A3448

/**
 * Compression header version
 */
#define COMP_HEAD_VERSION 0x1

/* Compressed data stream header */
typedef struct {
   uint32_t magic;      /* compression algo used in this compressed data stream */
   uint16_t level;      /* compression level used */
   uint16_t version;    /* for futur evolution */
   uint32_t size;       /* compressed size of the original data */
} comp_stream_header;

#endif /* __CH_H */
