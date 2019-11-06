/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_SRC_INCLUDE_COMPRESSION_CONTEXT_H_
#define BAREOS_SRC_INCLUDE_COMPRESSION_CONTEXT_H_

/* clang-format off */
struct CompressionContext {
  POOLMEM* deflate_buffer{nullptr}; /**< Buffer used for deflation (compression) */
  POOLMEM* inflate_buffer{nullptr}; /**< Buffer used for inflation (decompression) */
  uint32_t deflate_buffer_size{}; /**< Length of deflation buffer */
  uint32_t inflate_buffer_size{}; /**< Length of inflation buffer */
  struct {
#ifdef HAVE_LIBZ
    void* pZLIB{nullptr}; /**< ZLIB compression session data */
#endif
#ifdef HAVE_LZO
    void* pLZO{nullptr}; /**< LZO compression session data */
#endif
    void* pZFAST{nullptr}; /**< FASTLZ compression session data */
  } workset;
};
/* clang-format on */

#endif  // BAREOS_SRC_INCLUDE_COMPRESSION_CONTEXT_H_
