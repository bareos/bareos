/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

DLL_IMP_EXP const char *cmprs_algo_to_text(uint32_t compression_algorithm);
DLL_IMP_EXP bool SetupCompressionBuffers(JobControlRecord *jcr, bool compatible,
                               uint32_t compression_algorithm,
                               uint32_t *compress_buf_size);
DLL_IMP_EXP bool SetupDecompressionBuffers(JobControlRecord *jcr, uint32_t *decompress_buf_size);
DLL_IMP_EXP bool CompressData(JobControlRecord *jcr, uint32_t compression_algorithm, char *rbuf,
                   uint32_t rsize, unsigned char *cbuf,
                   uint32_t max_compress_len, uint32_t *compress_len);
DLL_IMP_EXP bool DecompressData(JobControlRecord *jcr, const char *last_fname, int32_t stream,
                     char **data, uint32_t *length, bool want_data_stream);
DLL_IMP_EXP void CleanupCompression(JobControlRecord *jcr);

#endif // BAREOS_LIB_COMPRESSION_H_
