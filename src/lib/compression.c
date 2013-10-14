/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2011 Free Software Foundation Europe e.V.
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
/**
 * Functions to handle compression/decompression of data.
 *
 * Extracted from other source files by Marco van Wieringen, May 2013
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
/*
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
      return _("*none*");
   }
}
#endif

static inline void unknown_compression_algorithm(JCR *jcr, uint32_t compression_algorithm)
{
   switch (compression_algorithm) {
   case COMPRESS_GZIP:
      Jmsg(jcr, M_FATAL, 0, _("GZIP compression not supported on this platform\n"));
      break;
   case COMPRESS_LZO1X:
      Jmsg(jcr, M_FATAL, 0, _("LZO2 compression not supported on this platform\n"));
      break;
   case COMPRESS_FZFZ:
      Jmsg(jcr, M_FATAL, 0, _("LZFZ compression not supported on this platform\n"));
      break;
   case COMPRESS_FZ4L:
      Jmsg(jcr, M_FATAL, 0, _("LZ4 compression not supported on this platform\n"));
      break;
   case COMPRESS_FZ4H:
      Jmsg(jcr, M_FATAL, 0, _("LZ4HC compression not supported on this platform\n"));
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Unknown compression algorithm specified %d\n"), compression_algorithm);
      break;
   }
}

static inline void non_compatible_compression_algorithm(JCR *jcr, uint32_t compression_algorithm)
{
   switch (compression_algorithm) {
   case COMPRESS_FZFZ:
      Jmsg(jcr, M_FATAL, 0, _("Illegal compression algorithm LZFZ for compatible mode\n"));
      break;
   case COMPRESS_FZ4L:
      Jmsg(jcr, M_FATAL, 0, _("Illegal compression algorithm LZ4 for compatible mode\n"));
      break;
   case COMPRESS_FZ4H:
      Jmsg(jcr, M_FATAL, 0, _("Illegal compression algorithm LZ4HC for compatible mode\n"));
      break;
   default:
      break;
   }
}

bool setup_compression_buffers(JCR *jcr,
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
      /*
       * See if this compression algorithm is already setup.
       */
      if (jcr->compress.workset.pZLIB) {
         return true;
      }

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

      z_stream *pZlibStream = (z_stream *)malloc(sizeof(z_stream));
      memset(pZlibStream, 0, sizeof(z_stream));
      if (pZlibStream) {
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
      }
      break;
   }
#endif
#ifdef HAVE_LZO
   case COMPRESS_LZO1X: {
      /*
       * See if this compression algorithm is already setup.
       */
      if (jcr->compress.workset.pLZO) {
         return true;
      }

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

      lzo_voidp pLzoMem = (lzo_voidp) malloc(LZO1X_1_MEM_COMPRESS);
      memset(pLzoMem, 0, LZO1X_1_MEM_COMPRESS);
      if (pLzoMem) {
         if (lzo_init() == LZO_E_OK) {
            jcr->compress.workset.pLZO = pLzoMem;
         } else {
            Jmsg(jcr, M_FATAL, 0, _("Failed to initialize LZO compression\n"));
            free(pLzoMem);
            return false;
         }
      }
      break;
   }
#endif
#ifdef HAVE_FASTLZ
   case COMPRESS_FZFZ:
   case COMPRESS_FZ4L:
   case COMPRESS_FZ4H: {
      int level, zstat;

      /*
       * See if this compression algorithm is already setup.
       */
      if (jcr->compress.workset.pZFAST) {
         return true;
      }

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

      zfast_stream *pZfastStream = (zfast_stream *)malloc(sizeof(zfast_stream));
      memset(pZfastStream, 0, sizeof(zfast_stream));
      if (pZfastStream) {
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

bool setup_decompression_buffers(JCR *jcr)
{
   uint32_t compress_buf_size;

   /*
    * Use the same buffer size to decompress all data.
    */
   memset(&jcr->compress, 0, sizeof(CMPRS_CTX));
   compress_buf_size = jcr->buf_size;
   if (compress_buf_size < DEFAULT_NETWORK_BUFFER_SIZE) {
      compress_buf_size = DEFAULT_NETWORK_BUFFER_SIZE;
   }
   compress_buf_size = compress_buf_size + 12 + ((compress_buf_size + 999) / 1000) + 100;
   jcr->compress.buffer = get_memory(compress_buf_size);
   jcr->compress.buffer_size = compress_buf_size;

#ifdef HAVE_LZO
   if (lzo_init() != LZO_E_OK) {
      Jmsg(jcr, M_FATAL, 0, _("LZO init failed\n"));
      return false;
   }
#endif

   return true;
}

#ifdef HAVE_LIBZ
static bool compress_with_zlib(JCR *jcr,
                               char *rbuf,
                               uint32_t rsize,
                               unsigned char *cbuf,
                               uint32_t max_compress_len,
                               uint32_t *compress_len)
{
   int zstat;

   Dmsg3(400, "cbuf=0x%x rbuf=0x%x len=%u\n", cbuf, rbuf, rsize);

   ((z_stream *)jcr->compress.workset.pZLIB)->next_in = (Bytef *)rbuf;
   ((z_stream *)jcr->compress.workset.pZLIB)->avail_in = rsize;
   ((z_stream *)jcr->compress.workset.pZLIB)->next_out = (Bytef *)cbuf;
   ((z_stream *)jcr->compress.workset.pZLIB)->avail_out = max_compress_len;

   if ((zstat = deflate((z_stream *)jcr->compress.workset.pZLIB, Z_FINISH)) != Z_STREAM_END) {
      Jmsg(jcr, M_FATAL, 0, _("Compression deflate error: %d\n"), zstat);
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   *compress_len = ((z_stream *)jcr->compress.workset.pZLIB)->total_out;

   /*
    * Reset zlib stream to be able to begin from scratch again
    */
   if ((zstat = deflateReset((z_stream *)jcr->compress.workset.pZLIB)) != Z_OK) {
      Jmsg(jcr, M_FATAL, 0, _("Compression deflateReset error: %d\n"), zstat);
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   Dmsg2(400, "GZIP compressed len=%d uncompressed len=%d\n", *compress_len, rsize);

   return true;
}
#endif

#ifdef HAVE_LZO
static bool compress_with_lzo(JCR *jcr,
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
static bool compress_with_fastlz(JCR *jcr,
                                 char *rbuf,
                                 uint32_t rsize,
                                 unsigned char *cbuf,
                                 uint32_t max_compress_len,
                                 uint32_t *compress_len)
{
   int zstat;

   Dmsg3(400, "cbuf=0x%x rbuf=0x%x len=%u\n", cbuf, rbuf, rsize);

   ((zfast_stream *)jcr->compress.workset.pZFAST)->next_in = (Bytef *)rbuf;
   ((zfast_stream *)jcr->compress.workset.pZFAST)->avail_in = rsize;
   ((zfast_stream *)jcr->compress.workset.pZFAST)->next_out = (Bytef *)cbuf;
   ((zfast_stream *)jcr->compress.workset.pZFAST)->avail_out = max_compress_len;

   if ((zstat = fastlzlibCompress((zfast_stream *)jcr->compress.workset.pZFAST, Z_FINISH)) != Z_STREAM_END) {
      Jmsg(jcr, M_FATAL, 0, _("Compression fastlzlibCompress error: %d\n"), zstat);
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   *compress_len = ((zfast_stream *)jcr->compress.workset.pZFAST)->total_out;

   /*
    * Reset fastlz stream to be able to begin from scratch again
    */
   if ((zstat = fastlzlibCompressReset((zfast_stream *)jcr->compress.workset.pZFAST)) != Z_OK) {
      Jmsg(jcr, M_FATAL, 0, _("Compression fastlzlibCompressReset error: %d\n"), zstat);
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   Dmsg2(400, "FASTLZ compressed len=%d uncompressed len=%d\n", *compress_len, rsize);

   return true;
}
#endif

bool compress_data(JCR *jcr,
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
static bool decompress_with_zlib(JCR *jcr, const char *last_fname, char **data, uint32_t *length, bool with_header)
{
   char ec1[50]; /* Buffer printing huge values */
   uLong compress_len;
   const unsigned char *cbuf;
   int status, real_compress_len;

   /*
    * NOTE! We only use uLong and Byte because they are
    * needed by the zlib routines, they should not otherwise
    * be used in Bareos.
    */
   compress_len = jcr->compress.buffer_size;

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

   Dmsg2(200, "Comp_len=%d msglen=%d\n", compress_len, *length);

   while ((status = uncompress((Byte *)jcr->compress.buffer, &compress_len,
                               (const Byte *)cbuf, (uLong)real_compress_len)) == Z_BUF_ERROR) {
      /*
       * The buffer size is too small, try with a bigger one
       */
      compress_len = jcr->compress.buffer_size = jcr->compress.buffer_size + (jcr->compress.buffer_size >> 1);
      Dmsg2(200, "Comp_len=%d msglen=%d\n", compress_len, *length);
      jcr->compress.buffer = check_pool_memory_size(jcr->compress.buffer, compress_len);
   }

   if (status != Z_OK) {
      Qmsg(jcr, M_ERROR, 0, _("Uncompression error on file %s. ERR=%s\n"), last_fname, zlib_strerror(status));
      return false;
   }

   *data = jcr->compress.buffer;
   *length = compress_len;

   Dmsg2(200, "Write uncompressed %d bytes, total before write=%s\n", compress_len, edit_uint64(jcr->JobBytes, ec1));

   return true;
}
#endif
#ifdef HAVE_LZO
static bool decompress_with_lzo(JCR *jcr, const char *last_fname, char **data, uint32_t *length)
{
   char ec1[50]; /* Buffer printing huge values */
   lzo_uint compress_len;
   const unsigned char *cbuf;
   int status, real_compress_len;

   compress_len = jcr->compress.buffer_size;
   cbuf = (const unsigned char *)*data + sizeof(comp_stream_header);
   real_compress_len = *length - sizeof(comp_stream_header);
   Dmsg2(200, "Comp_len=%d msglen=%d\n", compress_len, *length);
   while ((status = lzo1x_decompress_safe(cbuf, real_compress_len,
                                          (unsigned char *)jcr->compress.buffer,
                                          &compress_len, NULL)) == LZO_E_OUTPUT_OVERRUN) {
      /*
       * The buffer size is too small, try with a bigger one
       */
      compress_len = jcr->compress.buffer_size = jcr->compress.buffer_size + (jcr->compress.buffer_size >> 1);
      Dmsg2(200, "Comp_len=%d msglen=%d\n", compress_len, *length);
      jcr->compress.buffer = check_pool_memory_size(jcr->compress.buffer, compress_len);
   }

   if (status != LZO_E_OK) {
      Qmsg(jcr, M_ERROR, 0, _("LZO uncompression error on file %s. ERR=%d\n"), last_fname, status);
      return false;
   }

   *data = jcr->compress.buffer;
   *length = compress_len;

   Dmsg2(200, "Write uncompressed %d bytes, total before write=%s\n", compress_len, edit_uint64(jcr->JobBytes, ec1));

   return true;
}
#endif

#ifdef HAVE_FASTLZ
static bool decompress_with_fastlz(JCR *jcr, const char *last_fname, char **data, uint32_t *length, uint32_t comp_magic)
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
   stream.next_out = (Bytef *)jcr->compress.buffer;
   stream.avail_out = (uInt)jcr->compress.buffer_size;

   Dmsg2(200, "Comp_len=%d msglen=%d\n", stream.avail_in, *length);

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
         jcr->compress.buffer_size = jcr->compress.buffer_size + (jcr->compress.buffer_size >> 1);
         jcr->compress.buffer = check_pool_memory_size(jcr->compress.buffer, jcr->compress.buffer_size);
         stream.next_out = (Bytef *)jcr->compress.buffer;
         stream.avail_out = (uInt)jcr->compress.buffer_size;
         continue;
      case Z_OK:
      case Z_STREAM_END:
         break;
      default:
         goto cleanup;
      }
      break;
   }

   *data = jcr->compress.buffer;
   *length = stream.total_out;
   Dmsg2(200, "Write uncompressed %d bytes, total before write=%s\n", *length, edit_uint64(jcr->JobBytes, ec1));
   fastlzlibDecompressEnd(&stream);

   return true;

cleanup:
   Qmsg(jcr, M_ERROR, 0, _("Uncompression error on file %s. ERR=%s\n"), last_fname, zlib_strerror(zstat));
   fastlzlibDecompressEnd(&stream);

   return false;
}
#endif

bool decompress_data(JCR *jcr, const char *last_fname, int32_t stream, char **data, uint32_t *length)
{

   Dmsg1(200, "Stream found in decompress_data(): %d\n", stream);
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
      Dmsg4(200, "Compressed data stream found: magic=0x%x, len=%d, level=%d, ver=0x%x\n",
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
            return decompress_with_zlib(jcr, last_fname, data, length, true);
#endif
#ifdef HAVE_LZO
         case COMPRESS_LZO1X:
            return decompress_with_lzo(jcr, last_fname, data, length);
#endif
#ifdef HAVE_FASTLZ
         case COMPRESS_FZFZ:
         case COMPRESS_FZ4L:
         case COMPRESS_FZ4H:
            return decompress_with_fastlz(jcr, last_fname, data, length, comp_magic);
#endif
         default:
            Qmsg(jcr, M_ERROR, 0, _("Compression algorithm 0x%x found, but not supported!\n"), comp_magic);
            return false;
      }
      break;
   }
   default:
      return decompress_with_zlib(jcr, last_fname, data, length, false);
   }
}

void cleanup_compression(JCR *jcr)
{
   if (jcr->compress.buffer) {
      free_pool_memory(jcr->compress.buffer);
      jcr->compress.buffer = NULL;
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
bool setup_compression_buffers(JCR *jcr,
                               bool compatible,
                               uint32_t compression_algorithm,
                               uint32_t *compress_buf_size)
{
   return true;
}

bool setup_decompression_buffers(JCR *jcr)
{
   return true;
}

bool compress_data(JCR *jcr,
                   uint32_t compression_algorithm,
                   char *rbuf,
                   uint32_t rsize,
                   unsigned char *cbuf,
                   uint32_t max_compress_len,
                   uint32_t *compress_len)
{
   return true;
}

bool decompress_data(JCR *jcr, int32_t stream, char **data, uint32_t *length)
{
   Qmsg(jcr, M_ERROR, 0, _("Compressed data stream found, but compression not configured!\n"));
   return false;
}

void cleanup_compression(JCR *jcr)
{
}
#endif /* defined(HAVE_LZO) || defined(HAVE_LIBZ) || defined(HAVE_FASTLZ) */
