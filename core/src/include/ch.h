/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2025 Bareos GmbH & Co. KG

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
// Laurent Papier
/**
 * @file
 * Compressed stream header struct
 */

#ifndef BAREOS_INCLUDE_CH_H_
#define BAREOS_INCLUDE_CH_H_

#include <cstdint>

namespace {
constexpr std::uint32_t compression_constant(const char (&txt)[5])
{
  std::uint32_t a32 = txt[0];
  std::uint32_t b32 = txt[1];
  std::uint32_t c32 = txt[2];
  std::uint32_t d32 = txt[3];

  return (a32 << 24) | (b32 << 16) | (c32 << 8) | d32;
}
};  // namespace

// Compression algorithm signature. 4 letters as a 32bits integer
enum compression_type : std::uint32_t
{
  COMPRESS_NONE
  = compression_constant("NONE"), /* used for incompressible block */
  COMPRESS_GZIP = compression_constant("GZIP"),
  COMPRESS_LZO1X = compression_constant("LZOX"),
  COMPRESS_FZFZ = compression_constant("FZFZ"),
  COMPRESS_FZ4L = compression_constant("FZ4L"),
  COMPRESS_FZ4H = compression_constant("FZ4H"),
};

// Compression header version
#define COMP_HEAD_VERSION 0x1

/* Compressed data stream header */
typedef struct {
  uint32_t magic;   /* compression algo used in this compressed data stream */
  uint16_t level;   /* compression level used */
  uint16_t version; /* for futur evolution */
  uint32_t size;    /* compressed size of the original data */
} comp_stream_header;

#endif  // BAREOS_INCLUDE_CH_H_
