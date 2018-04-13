/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Kern Sibbald, November MM
 * Extracted from other source files by Marco van Wieringen, May 2013
 */
/**
 * @file
 * Functions to handle compression/decompression of data.
 */

#include "bareos.h"
#include "jcr.h"
#include "ch.h"
#include "streams.h"

#if defined(HAVE_LZO) || defined(HAVE_LIBZ) || defined(HAVE_FASTLZ)

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#ifdef HAVE_LZO
#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>
#endif

#ifdef HAVE_FASTLZ
#include <fastlzlib.h>
#endif

#ifdef HAVE_LIBZ

#ifndef HAVE_COMPRESS_BOUND
#define compressBound(sourceLen) (sourceLen + (sourceLen >> 12) + (sourceLen >> 14) + (sourceLen >> 25) + 13)
#endif

const char *cmprs_algo_to_text(uint32_t compression_algorithm)
{
   switch (compression_algorithm) {
   case COMPRESS_GZIP:
      return "GZIP";
   case COMPRESS_LZO1X:
      return "LZO2";
   case COMPRESS_FZFZ:
      return "LZFZ";
   case COMPRESS_FZ4L:
      return "LZ4";
   case COMPRESS_FZ4H:
      return "LZ4HC";
   default:
      return "Unknown";
   }
}

/**
 * Convert ZLIB error code into an ASCII message
 */
static const char *zlib_strerror(int stat)
{
   if (stat >= 0) {
      return _("None");
   }
   switch (stat) {
   case Z_ERRNO:
      return _("Zlib errno");
   case Z_STREAM_ERROR:
      return _("Zlib stream error");
   case Z_DATA_ERROR:
      return _("Zlib data error");
   case Z_MEM_ERROR:
      return _("Zlib memory error");
   case Z_BUF_ERROR:
      return _("Zlib buffer error");
   case Z_VERSION_ERROR:
      return _("Zlib version error");
   default:
      return _("*None*");
   }
}
#endif

static inline void unknown_compression_algorithm(JobControlRecord *jcr, uint32_t compression_algorithm)
{
   Jmsg(jcr, M_FATAL, 0, _("%s compression not supported on this platform\n"),
        cmprs_algo_to_text(compression_algorithm));
}

static inline void non_compatible_compression_algorithm(JobControlRecord *jcr, uint32_t compression_algorithm)
{
   Jmsg(jcr, M_FATAL, 0, _("Illegal compression algorithm %s for compatible mode\n"),
        cmprs_algo_to_text(compression_algorithm));
}

bool setup_compression_buffers(JobControlRecord *jcr,
                               bool compatible,
                               uint32_t compression_algorithm,
                               uint32_t *compress_buf_size)
{
   uint32_t wanted_compress_buf_size;

   switch (compression_algorithm) {
   case 0:
      /*
       * No compression requested.
       */
      break;
#ifdef HAVE_LIBZ
   case COMPRESS_GZIP: {
      z_stream *pZlibStream;

      /**
       * Use compressBound() to get an idea what zlib thinks
       * what the upper limit is of what it needs to compress
       * a buffer of x bytes. To that we add 18 bytes and the size
       * of an compression header.
       *
       * This gives a bit extra plus room for the sparse addr if any.
       * Note, we adjust the read size to be smaller so that the
       * same output buffer can be used without growing it.
       *
       * The zlib compression workset is initialized here to minimize
       * the "per file" load. The jcr member is only set, if the init
       * was successful.
       */
      wanted_compress_buf_size = compressBound(jcr->buf_size) + 18 + (int)sizeof(comp_stream_header);
      if (wanted_compress_buf_size > *compress_buf_size) {
         *compress_buf_size = wanted_compress_buf_size;
      }

      /*
       * See if this compression algorithm is already setup.
       */
      if (jcr->compress.workset.pZLIB) {
         return true;
      }

      pZlibStream = (z_stream *)malloc(sizeof(z_stream));
      memset(pZlibStream, 0, sizeof(z_stream));
      pZlibStream->zalloc = Z_NULL;
      pZlibStream->zfree = Z_NULL;
      pZlibStream->opaque = Z_NULL;
      pZlibStream->state = Z_NULL;

      if (deflateInit(pZlibStream, Z_DEFAULT_COMPRESSION) == Z_OK) {
         jcr->compress.workset.pZLIB = pZlibStream;
      } else {
         Jmsg(jcr, M_FATAL, 0, _("Failed to initialize ZLIB compression\n"));
         free(pZlibStream);
         return false;
      }
      break;
   }
#endif
#ifdef HAVE_LZO
   case COMPRESS_LZO1X: {
      lzo_voidp pLzoMem;

      /**
       * For LZO1X compression the recommended value is:
       *    output_block_size = input_block_size + (input_block_size / 16) + 64 + 3 + sizeof(comp_stream_header)
       *
       * The LZO compression workset is initialized here to minimize
       * the "per file" load. The jcr member is only set, if the init
       * was successful.
       */
      wanted_compress_buf_size = jcr->buf_size + (jcr->buf_size / 16) + 64 + 3 + (int)sizeof(comp_stream_header);
      if (wanted_compress_buf_size > *compress_buf_size) {
         *compress_buf_size = wanted_compress_buf_size;
      }

      /*
       * See if this compression algorithm is already setup.
       */
      if (jcr->compress.workset.pLZO) {
         return true;
      }

      pLzoMem = (lzo_voidp) malloc(LZO1X_1_MEM_COMPRESS);
      memset(pLzoMem, 0, LZO1X_1_MEM_COMPRESS);

      if (lzo_init() == LZO_E_OK) {
         jcr->compress.workset.pLZO = pLzoMem;
      } else {
         Jmsg(jcr, M_FATAL, 0, _("Failed to initialize LZO compression\n"));
         free(pLzoMem);
         return false;
      }
      break;
   }
#endif
#ifdef HAVE_FASTLZ
   case COMPRESS_FZFZ:
   case COMPRESS_FZ4L:
   case COMPRESS_FZ4H: {
      int level, zstat;
      zfast_stream *pZfastStream;

      if (compatible) {
         non_compatible_compression_algorithm(jcr, compression_algorithm);
         return false;
      }

      if (compression_algorithm == COMPRESS_FZ4H) {
         level = Z_BEST_COMPRESSION;
      } else {
         level = Z_BEST_SPEED;
      }

      /*
       * For FASTLZ compression the recommended value is:
       *    output_block_size = input_block_size + (input_block_size / 10 + 16 * 2) + sizeof(comp_stream_header)
       *
       * The FASTLZ compression workset is initialized here to minimize
       * the "per file" load. The jcr member is only set, if the init
       * was successful.
       */
      wanted_compress_buf_size = jcr->buf_size + (jcr->buf_size / 10 + 16 * 2) + (int)sizeof(comp_stream_header);
      if (wanted_compress_buf_size > *compress_buf_size) {
         *compress_buf_size = wanted_compress_buf_size;
      }

      /*
       * See if this compression algorithm is already setup.
       */
      if (jcr->compress.workset.pZFAST) {
         return true;
      }

      pZfastStream = (zfast_stream *)malloc(sizeof(zfast_stream));
      memset(pZfastStream, 0, sizeof(zfast_stream));
      pZfastStream->zalloc = Z_NULL;
      pZfastStream->zfree = Z_NULL;
      pZfastStream->opaque = Z_NULL;
      pZfastStream->state = Z_NULL;

      if ((zstat = fastlzlibCompressInit(pZfastStream, level)) == Z_OK) {
         jcr->compress.workset.pZFAST = pZfastStream;
      } else {
         Jmsg(jcr, M_FATAL, 0, _("Failed to initialize FASTLZ compression\n"));
         free(pZfastStream);
         return false;
      }
      break;
   }
#endif
   default:
      unknown_compression_algorithm(jcr, compression_algorithm);
      return false;
   }

   return true;
}

bool setup_decompression_buffers(JobControlRecord *jcr, uint32_t *decompress_buf_size)
{
   uint32_t compress_buf_size;

   /*
    * Use the same buffer size to decompress all data.
    */
   compress_buf_size = jcr->buf_size;
   if (compress_buf_size < DEFAULT_NETWORK_BUFFER_SIZE) {
      compress_buf_size = DEFAULT_NETWORK_BUFFER_SIZE;
   }
   *decompress_buf_size = compress_buf_size + 12 + ((compress_buf_size + 999) / 1000) + 100;

#ifdef HAVE_LZO
   if (!jcr->compress.inflate_buffer && lzo_init() != LZO_E_OK) {
      Jmsg(jcr, M_FATAL, 0, _("LZO init failed\n"));
      return false;
   }
#endif

   return true;
}

#ifdef HAVE_LIBZ
static bool compress_with_zlib(JobControlRecord *jcr,
                               char *rbuf,
                               uint32_t rsize,
                               unsigned char *cbuf,
                               uint32_t max_compress_len,
                               uint32_t *compress_len)
{
   int zstat;
   z_stream *pZlibStream;

   Dmsg3(400, "cbuf=0x%x rbuf=0x%x len=%u\n", cbuf, rbuf, rsize);

   pZlibStream = (z_stream *)jcr->compress.workset.pZLIB;
   pZlibStream->next_in = (Bytef *)rbuf;
   pZlibStream->avail_in = rsize;
   pZlibStream->next_out = (Bytef *)cbuf;
   pZlibStream->avail_out = max_compress_len;

   if ((zstat = deflate(pZlibStream, Z_FINISH)) != Z_STREAM_END) {
      Jmsg(jcr, M_FATAL, 0, _("Compression deflate error: %d\n"), zstat);
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   *compress_len = pZlibStream->total_out;

   /*
    * Reset zlib stream to be able to begin from scratch again
    */
   if ((zstat = deflateReset(pZlibStream)) != Z_OK) {
      Jmsg(jcr, M_FATAL, 0, _("Compression deflateReset error: %d\n"), zstat);
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   Dmsg2(400, "GZIP compressed len=%d uncompressed len=%d\n", *compress_len, rsize);

   return true;
}
#endif

#ifdef HAVE_LZO
static bool compress_with_lzo(JobControlRecord *jcr,
                              char *rbuf,
                              uint32_t rsize,
                              unsigned char *cbuf,
                              uint32_t max_compress_len,
                              uint32_t *compress_len)
{
   int lzores;
   lzo_uint len = 0;

   Dmsg3(400, "cbuf=0x%x rbuf=0x%x len=%u\n", cbuf, rbuf, rsize);

   lzores = lzo1x_1_compress((const unsigned char *)rbuf, rsize,
                             cbuf, &len, jcr->compress.workset.pLZO);
   *compress_len = len;

   if (lzores != LZO_E_OK || *compress_len > max_compress_len) {
      /*
       * This should NEVER happen
       */
      Jmsg(jcr, M_FATAL, 0, _("Compression LZO error: %d\n"), lzores);
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   Dmsg2(400, "LZO compressed len=%d uncompressed len=%d\n", *compress_len, rsize);

   return true;
}
#endif

#ifdef HAVE_FASTLZ
static bool compress_with_fastlz(JobControlRecord *jcr,
                                 char *rbuf,
                                 uint32_t rsize,
                                 unsigned char *cbuf,
                                 uint32_t max_compress_len,
                                 uint32_t *compress_len)
{
   int zstat;
   zfast_stream *pZfastStream;

   Dmsg3(400, "cbuf=0x%x rbuf=0x%x len=%u\n", cbuf, rbuf, rsize);

   pZfastStream = (zfast_stream *)jcr->compress.workset.pZFAST;
   pZfastStream->next_in = (Bytef *)rbuf;
   pZfastStream->avail_in = rsize;
   pZfastStream->next_out = (Bytef *)cbuf;
   pZfastStream->avail_out = max_compress_len;

   if ((zstat = fastlzlibCompress(pZfastStream, Z_FINISH)) != Z_STREAM_END) {
      Jmsg(jcr, M_FATAL, 0, _("Compression fastlzlibCompress error: %d\n"), zstat);
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   *compress_len = pZfastStream->total_out;

   /*
    * Reset fastlz stream to be able to begin from scratch again
    */
   if ((zstat = fastlzlibCompressReset(pZfastStream)) != Z_OK) {
      Jmsg(jcr, M_FATAL, 0, _("Compression fastlzlibCompressReset error: %d\n"), zstat);
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   Dmsg2(400, "FASTLZ compressed len=%d uncompressed len=%d\n", *compress_len, rsize);

   return true;
}
#endif

bool compress_data(JobControlRecord *jcr,
                   uint32_t compression_algorithm,
                   char *rbuf,
                   uint32_t rsize,
                   unsigned char *cbuf,
                   uint32_t max_compress_len,
                   uint32_t *compress_len)
{
   *compress_len = 0;
   switch (compression_algorithm) {
#ifdef HAVE_LIBZ
   case COMPRESS_GZIP:
      if (jcr->compress.workset.pZLIB) {
         if (!compress_with_zlib(jcr, rbuf, rsize, cbuf, max_compress_len, compress_len)) {
            return false;
         }
      }
      break;
#endif
#ifdef HAVE_LZO
   case COMPRESS_LZO1X:
      if (jcr->compress.workset.pLZO) {
         if (!compress_with_lzo(jcr, rbuf, rsize, cbuf, max_compress_len, compress_len)) {
            return false;
         }
      }
      break;
#endif
#ifdef HAVE_FASTLZ
   case COMPRESS_FZFZ:
   case COMPRESS_FZ4L:
   case COMPRESS_FZ4H:
      if (jcr->compress.workset.pZFAST) {
         if (!compress_with_fastlz(jcr, rbuf, rsize, cbuf, max_compress_len, compress_len)) {
            return false;
         }
      }
      break;
#endif
   default:
      break;
   }

   return true;
}

#ifdef HAVE_LIBZ
static bool decompress_with_zlib(JobControlRecord *jcr,
                                 const char *last_fname,
                                 char **data,
                                 uint32_t *length,
                                 bool sparse,
                                 bool with_header,
                                 bool want_data_stream)
{
   char ec1[50]; /* Buffer printing huge values */
   uLong compress_len;
   const unsigned char *cbuf;
   char *wbuf;
   int status, real_compress_len;

   /*
    * NOTE! We only use uLong and Byte because they are
    * needed by the zlib routines, they should not otherwise
    * be used in Bareos.
    */
   if (sparse && want_data_stream) {
      wbuf = jcr->compress.inflate_buffer + OFFSET_FADDR_SIZE;
      compress_len = jcr->compress.inflate_buffer_size - OFFSET_FADDR_SIZE;
   } else {
      wbuf = jcr->compress.inflate_buffer;
      compress_len = jcr->compress.inflate_buffer_size;
   }

   /*
    * See if this is a compressed stream with the new compression header or an old one.
    */
   if (with_header) {
      cbuf = (const unsigned char*)*data + sizeof(comp_stream_header);
      real_compress_len = *length - sizeof(comp_stream_header);
   } else {
      cbuf = (const unsigned char*)*data;
      real_compress_len = *length;
   }

   Dmsg2(400, "Comp_len=%d msglen=%d\n", compress_len, *length);

   while ((status = uncompress((Byte *)wbuf, &compress_len, (const Byte *)cbuf, (uLong)real_compress_len)) == Z_BUF_ERROR) {
      /*
       * The buffer size is too small, try with a bigger one
       */
      jcr->compress.inflate_buffer_size = jcr->compress.inflate_buffer_size + (jcr->compress.inflate_buffer_size >> 1);
      jcr->compress.inflate_buffer = check_pool_memory_size(jcr->compress.inflate_buffer, jcr->compress.inflate_buffer_size);

      if (sparse && want_data_stream) {
         wbuf = jcr->compress.inflate_buffer + OFFSET_FADDR_SIZE;
         compress_len = jcr->compress.inflate_buffer_size - OFFSET_FADDR_SIZE;
      } else {
         wbuf = jcr->compress.inflate_buffer;
         compress_len = jcr->compress.inflate_buffer_size;
      }
      Dmsg2(400, "Comp_len=%d msglen=%d\n", compress_len, *length);
   }

   if (status != Z_OK) {
      Qmsg(jcr, M_ERROR, 0, _("Uncompression error on file %s. ERR=%s\n"), last_fname, zlib_strerror(status));
      return false;
   }

   /*
    * We return a decompressed data stream with the fileoffset encoded when this was a sparse stream.
    */
   if (sparse && want_data_stream) {
      memcpy(jcr->compress.inflate_buffer, *data, OFFSET_FADDR_SIZE);
   }

   *data = jcr->compress.inflate_buffer;
   *length = compress_len;

   Dmsg2(400, "Write uncompressed %d bytes, total before write=%s\n", compress_len, edit_uint64(jcr->JobBytes, ec1));

   return true;
}
#endif
#ifdef HAVE_LZO
static bool decompress_with_lzo(JobControlRecord *jcr,
                                const char *last_fname,
                                char **data,
                                uint32_t *length,
                                bool sparse,
                                bool want_data_stream)
{
   char ec1[50]; /* Buffer printing huge values */
   lzo_uint compress_len;
   const unsigned char *cbuf;
   unsigned char *wbuf;
   int status, real_compress_len;

   if (sparse && want_data_stream) {
      compress_len = jcr->compress.inflate_buffer_size - OFFSET_FADDR_SIZE;
      cbuf = (const unsigned char *)*data + OFFSET_FADDR_SIZE + sizeof(comp_stream_header);
      wbuf = (unsigned char *)jcr->compress.inflate_buffer + OFFSET_FADDR_SIZE;
   } else {
      compress_len = jcr->compress.inflate_buffer_size;
      cbuf = (const unsigned char *)*data + sizeof(comp_stream_header);
      wbuf = (unsigned char *)jcr->compress.inflate_buffer;
   }

   real_compress_len = *length - sizeof(comp_stream_header);
   Dmsg2(400, "Comp_len=%d msglen=%d\n", compress_len, *length);
   while ((status = lzo1x_decompress_safe(cbuf, real_compress_len, wbuf, &compress_len, NULL)) == LZO_E_OUTPUT_OVERRUN) {
      /*
       * The buffer size is too small, try with a bigger one
       */
      jcr->compress.inflate_buffer_size = jcr->compress.inflate_buffer_size + (jcr->compress.inflate_buffer_size >> 1);
      jcr->compress.inflate_buffer = check_pool_memory_size(jcr->compress.inflate_buffer, jcr->compress.inflate_buffer_size);

      if (sparse && want_data_stream) {
         compress_len = jcr->compress.inflate_buffer_size - OFFSET_FADDR_SIZE;
         wbuf = (unsigned char *)jcr->compress.inflate_buffer + OFFSET_FADDR_SIZE;
      } else {
         compress_len = jcr->compress.inflate_buffer_size;
         wbuf = (unsigned char *)jcr->compress.inflate_buffer;
      }
      Dmsg2(400, "Comp_len=%d msglen=%d\n", compress_len, *length);
   }

   if (status != LZO_E_OK) {
      Qmsg(jcr, M_ERROR, 0, _("LZO uncompression error on file %s. ERR=%d\n"), last_fname, status);
      return false;
   }

   /*
    * We return a decompressed data stream with the fileoffset encoded when this was a sparse stream.
    */
   if (sparse && want_data_stream) {
      memcpy(jcr->compress.inflate_buffer, *data, OFFSET_FADDR_SIZE);
   }

   *data = jcr->compress.inflate_buffer;
   *length = compress_len;

   Dmsg2(400, "Write uncompressed %d bytes, total before write=%s\n", compress_len, edit_uint64(jcr->JobBytes, ec1));

   return true;
}
#endif

#ifdef HAVE_FASTLZ
static bool decompress_with_fastlz(JobControlRecord *jcr,
                                   const char *last_fname,
                                   char **data,
                                   uint32_t *length,
                                   uint32_t comp_magic,
                                   bool sparse,
                                   bool want_data_stream)
{
   int zstat;
   zfast_stream stream;
   zfast_stream_compressor compressor = COMPRESSOR_FASTLZ;
   char ec1[50]; /* Buffer printing huge values */

   switch (comp_magic) {
   case COMPRESS_FZ4L:
   case COMPRESS_FZ4H:
      compressor = COMPRESSOR_LZ4;
      break;
   }

   /*
    * NOTE! We only use uInt and Bytef because they are
    * needed by the fastlz routines, they should not otherwise
    * be used in Bareos.
    */
   memset(&stream, 0, sizeof(stream));
   stream.next_in = (Bytef *)*data + sizeof(comp_stream_header);
   stream.avail_in = (uInt)*length - sizeof(comp_stream_header);
   if (sparse && want_data_stream) {
      stream.next_out = (Bytef *)jcr->compress.inflate_buffer + OFFSET_FADDR_SIZE;
      stream.avail_out = (uInt)jcr->compress.inflate_buffer_size - OFFSET_FADDR_SIZE;
   } else {
      stream.next_out = (Bytef *)jcr->compress.inflate_buffer;
      stream.avail_out = (uInt)jcr->compress.inflate_buffer_size;
   }

   Dmsg2(400, "Comp_len=%d msglen=%d\n", stream.avail_in, *length);

   if ((zstat = fastlzlibDecompressInit(&stream)) != Z_OK) {
      goto cleanup;
   }

   if ((zstat = fastlzlibSetCompressor(&stream, compressor)) != Z_OK) {
      goto cleanup;
   }

   while (1) {
      zstat = fastlzlibDecompress(&stream);
      switch (zstat) {
      case Z_BUF_ERROR:
         /*
          * The buffer size is too small, try with a bigger one
          */
         jcr->compress.inflate_buffer_size = jcr->compress.inflate_buffer_size + (jcr->compress.inflate_buffer_size >> 1);
         jcr->compress.inflate_buffer = check_pool_memory_size(jcr->compress.inflate_buffer, jcr->compress.inflate_buffer_size);
         if (sparse && want_data_stream) {
            stream.next_out = (Bytef *)jcr->compress.inflate_buffer + OFFSET_FADDR_SIZE;
            stream.avail_out = (uInt)jcr->compress.inflate_buffer_size - OFFSET_FADDR_SIZE;
         } else {
            stream.next_out = (Bytef *)jcr->compress.inflate_buffer;
            stream.avail_out = (uInt)jcr->compress.inflate_buffer_size;
         }
         continue;
      case Z_OK:
      case Z_STREAM_END:
         break;
      default:
         goto cleanup;
      }
      break;
   }

   /*
    * We return a decompressed data stream with the fileoffset encoded when this was a sparse stream.
    */
   if (sparse && want_data_stream) {
      memcpy(jcr->compress.inflate_buffer, *data, OFFSET_FADDR_SIZE);
   }

   *data = jcr->compress.inflate_buffer;
   *length = stream.total_out;
   Dmsg2(400, "Write uncompressed %d bytes, total before write=%s\n", *length, edit_uint64(jcr->JobBytes, ec1));
   fastlzlibDecompressEnd(&stream);

   return true;

cleanup:
   Qmsg(jcr, M_ERROR, 0, _("Uncompression error on file %s. ERR=%s\n"), last_fname, zlib_strerror(zstat));
   fastlzlibDecompressEnd(&stream);

   return false;
}
#endif

bool decompress_data(JobControlRecord *jcr,
                     const char *last_fname,
                     int32_t stream,
                     char **data,
                     uint32_t *length,
                     bool want_data_stream)
{
   Dmsg1(400, "Stream found in decompress_data(): %d\n", stream);
   switch (stream) {
   case STREAM_COMPRESSED_DATA:
   case STREAM_SPARSE_COMPRESSED_DATA:
   case STREAM_WIN32_COMPRESSED_DATA:
   case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
   case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA: {
      uint32_t comp_magic, comp_len;
      uint16_t comp_level, comp_version;

      /*
       * Read compress header
       */
      unser_declare;
      unser_begin(*data, sizeof(comp_stream_header));
      unser_uint32(comp_magic);
      unser_uint32(comp_len);
      unser_uint16(comp_level);
      unser_uint16(comp_version);
      unser_end(*data, sizeof(comp_stream_header));
      Dmsg4(400, "Compressed data stream found: magic=0x%x, len=%d, level=%d, ver=0x%x\n",
            comp_magic, comp_len, comp_level, comp_version);

      /*
       * Version check
       */
      if (comp_version != COMP_HEAD_VERSION) {
         Qmsg(jcr, M_ERROR, 0, _("Compressed header version error. version=0x%x\n"), comp_version);
         return false;
      }

      /*
       * Size check
       */
      if (comp_len + sizeof(comp_stream_header) != *length) {
         Qmsg(jcr, M_ERROR, 0, _("Compressed header size error. comp_len=%d, msglen=%d\n"),
              comp_len, *length);
         return false;
      }

      /*
       * Based on the compression used perform the actual decompression of the data.
       */
      switch (comp_magic) {
#ifdef HAVE_LIBZ
         case COMPRESS_GZIP:
            switch (stream) {
            case STREAM_SPARSE_COMPRESSED_DATA:
               return decompress_with_zlib(jcr, last_fname, data, length, true, true, want_data_stream);
            default:
               return decompress_with_zlib(jcr, last_fname, data, length, false, true, want_data_stream);
            }
#endif
#ifdef HAVE_LZO
         case COMPRESS_LZO1X:
            switch (stream) {
            case STREAM_SPARSE_COMPRESSED_DATA:
               return decompress_with_lzo(jcr, last_fname, data, length, true, want_data_stream);
            default:
               return decompress_with_lzo(jcr, last_fname, data, length, false, want_data_stream);
            }
#endif
#ifdef HAVE_FASTLZ
         case COMPRESS_FZFZ:
         case COMPRESS_FZ4L:
         case COMPRESS_FZ4H:
            switch (stream) {
            case STREAM_SPARSE_COMPRESSED_DATA:
               return decompress_with_fastlz(jcr, last_fname, data, length, comp_magic, true, want_data_stream);
            default:
               return decompress_with_fastlz(jcr, last_fname, data, length, comp_magic, false, want_data_stream);
            }
#endif
         default:
            Qmsg(jcr, M_ERROR, 0, _("Compression algorithm 0x%x found, but not supported!\n"), comp_magic);
            return false;
      }
      break;
   }
   default:
#ifdef HAVE_LIBZ
      switch (stream) {
      case STREAM_SPARSE_GZIP_DATA:
         return decompress_with_zlib(jcr, last_fname, data, length, true, false, want_data_stream);
      default:
         return decompress_with_zlib(jcr, last_fname, data, length, false, false, want_data_stream);
      }
#else
      Qmsg(jcr, M_ERROR, 0, _("Compression algorithm GZIP found, but not supported!\n"));
      return false;
#endif
   }
}

void cleanup_compression(JobControlRecord *jcr)
{
   if (jcr->compress.deflate_buffer) {
      free_pool_memory(jcr->compress.deflate_buffer);
      jcr->compress.deflate_buffer = NULL;
   }

   if (jcr->compress.inflate_buffer) {
      free_pool_memory(jcr->compress.inflate_buffer);
      jcr->compress.inflate_buffer = NULL;
   }

#ifdef HAVE_LIBZ
   if (jcr->compress.workset.pZLIB) {
      /*
       * Free the zlib stream
       */
      deflateEnd((z_stream *)jcr->compress.workset.pZLIB);
      free(jcr->compress.workset.pZLIB);
      jcr->compress.workset.pZLIB = NULL;
   }
#endif

#ifdef HAVE_LZO
   if (jcr->compress.workset.pLZO) {
      free(jcr->compress.workset.pLZO);
      jcr->compress.workset.pLZO = NULL;
   }
#endif

#ifdef HAVE_FASTLZ
   if (jcr->compress.workset.pZFAST) {
      free(jcr->compress.workset.pZFAST);
      jcr->compress.workset.pZFAST = NULL;
   }
#endif

}
#else
const char *cmprs_algo_to_text(uint32_t compression_algorithm)
{
   return "Unknown";
}

bool setup_compression_buffers(JobControlRecord *jcr,
                               bool compatible,
                               uint32_t compression_algorithm,
                               uint32_t *compress_buf_size)
{
   return true;
}

bool setup_decompression_buffers(JobControlRecord *jcr, uint32_t *decompress_buf_size)
{
   *decompress_buf_size = 0;
   return true;
}

bool compress_data(JobControlRecord *jcr,
                   uint32_t compression_algorithm,
                   char *rbuf,
                   uint32_t rsize,
                   unsigned char *cbuf,
                   uint32_t max_compress_len,
                   uint32_t *compress_len)
{
   return true;
}

bool decompress_data(JobControlRecord *jcr,
                     const char *last_fname,
                     int32_t stream,
                     char **data,
                     uint32_t *length,
                     bool want_data_stream)
{
   Qmsg(jcr, M_ERROR, 0, _("Compressed data stream found, but compression not configured!\n"));
   return false;
}

void cleanup_compression(JobControlRecord *jcr)
{
}
#endif /* defined(HAVE_LZO) || defined(HAVE_LIBZ) || defined(HAVE_FASTLZ) */
