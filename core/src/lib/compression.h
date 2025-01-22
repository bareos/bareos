/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2023 Bareos GmbH & Co. KG

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
#ifndef BAREOS_LIB_COMPRESSION_H_
#define BAREOS_LIB_COMPRESSION_H_

#include "lib/util.h"

std::string_view CompressorName(uint32_t compression_algorithm);

bool SetupCompressionBuffers(JobControlRecord* jcr,
                             uint32_t compression_algorithm,
                             uint32_t* compress_buf_size);
bool SetupDecompressionBuffers(JobControlRecord* jcr,
                               uint32_t* decompress_buf_size);
bool SetupSpecificCompressionContext(JobControlRecord& jcr,
                                    uint32_t algo,
                                    uint32_t compression_level);

// return the number of bytes written to the output on success
// or std::nullopt on error
result<std::size_t> ThreadlocalCompress(uint32_t algo,
                                        uint32_t level,
                                        char const* input,
                                        std::size_t size,
                                        char* output,
                                        std::size_t capacity);

std::size_t RequiredCompressionOutputBufferSize(uint32_t algo,
                                                std::size_t max_input_size);

bool CompressData(JobControlRecord* jcr,
                  uint32_t compression_algorithm,
                  char* rbuf,
                  uint32_t rsize,
                  unsigned char* cbuf,
                  uint32_t max_compress_len,
                  uint32_t* compress_len);
bool DecompressData(JobControlRecord* jcr,
                    const char* last_fname,
                    int32_t stream,
                    char** data,
                    uint32_t* length,
                    bool want_data_stream);
void CleanupCompression(JobControlRecord* jcr);

#endif  // BAREOS_LIB_COMPRESSION_H_
