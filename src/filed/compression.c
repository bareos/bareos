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
 * Extracted from other source files by Marco van Wieringen, June 2011
 */

#include "bareos.h"
#include "filed.h"

#if defined(HAVE_LZO) || defined(HAVE_LIBZ)

#ifdef HAVE_LZO
#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>
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

void adjust_compression_buffers(JCR *jcr)
{
   uint32_t wanted_compress_buf_size;
   uint32_t compress_buf_size = 0;

   if (jcr->ff->fileset) {
      int i, j;
      findFILESET *fileset = jcr->ff->fileset;

      for (i = 0; i < fileset->include_list.size(); i++) {
         findINCEXE *incexe = (findINCEXE *)fileset->include_list.get(i);
         for (j = 0; j < incexe->opts_list.size(); j++) {
            findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);
            switch (fo->Compress_algo) {
            case 0:
               /*
                * No compression requested.
                */
               break;
#ifdef HAVE_LIBZ
            case COMPRESS_GZIP: {
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
               compress_buf_size = wanted_compress_buf_size;

               z_stream *pZlibStream = (z_stream*)malloc(sizeof(z_stream));
               if (pZlibStream) {
                  pZlibStream->zalloc = Z_NULL;
                  pZlibStream->zfree = Z_NULL;
                  pZlibStream->opaque = Z_NULL;
                  pZlibStream->state = Z_NULL;

                  if (deflateInit(pZlibStream, Z_DEFAULT_COMPRESSION) == Z_OK) {
                     jcr->pZLIB_compress_workset = pZlibStream;
                  } else {
                     free(pZlibStream);
                  }
               }
               break;
            }
#endif
#ifdef HAVE_LZO
            case COMPRESS_LZO1X: {
               /**
                * For LZO1X compression the recommended value is :
                *    output_block_size = input_block_size + (input_block_size / 16) + 64 + 3 + sizeof(comp_stream_header)
                *
                * The LZO compression workset is initialized here to minimize
                * the "per file" load. The jcr member is only set, if the init
                * was successful.
                */
               wanted_compress_buf_size = jcr->buf_size + (jcr->buf_size / 16) + 64 + 3 + (int)sizeof(comp_stream_header);
               if (wanted_compress_buf_size > compress_buf_size) {
                  compress_buf_size = wanted_compress_buf_size;
               }

               lzo_voidp pLzoMem = (lzo_voidp) malloc(LZO1X_1_MEM_COMPRESS);
               if (pLzoMem) {
                  if (lzo_init() == LZO_E_OK) {
                     jcr->LZO_compress_workset = pLzoMem;
                  } else {
                     free(pLzoMem);
                  }
               }
               break;
            }
#endif
            default:
               Jmsg(jcr, M_FATAL, 0, _("Unknown compression algoritm specified %d\n"), fo->Compress_algo);
               break;
            }
         }
      }
   }

   if (compress_buf_size > 0) {
      jcr->compress_buf_size = compress_buf_size;
      jcr->compress_buf = get_memory(jcr->compress_buf_size);
   }
}

bool adjust_decompression_buffers(JCR *jcr)
{
   uint32_t compress_buf_size;

   /*
    * Use the same buffer size to decompress both GZIP and LZO.
    */
   compress_buf_size = jcr->buf_size + 12 + ((jcr->buf_size + 999) / 1000) + 100;
   jcr->compress_buf = get_memory(compress_buf_size);
   jcr->compress_buf_size = compress_buf_size;

#ifdef HAVE_LZO
   if (lzo_init() != LZO_E_OK) {
      Jmsg(jcr, M_FATAL, 0, _("LZO init failed\n"));
      return false;
   }
#endif

   return true;
}

bool setup_compression_context(b_ctx &bctx)
{
   bool retval = false;

   if ((bctx.ff_pkt->flags & FO_COMPRESS)) {
      /*
       * See if we need to be compatible with the old GZIP stream encoding.
       */
      if (!me->compatible || bctx.ff_pkt->Compress_algo != COMPRESS_GZIP) {
         memset(&bctx.ch, 0, sizeof(comp_stream_header));

         /*
          * Calculate buffer offsets.
          */
         if ((bctx.ff_pkt->flags & FO_SPARSE) ||
             (bctx.ff_pkt->flags & FO_OFFSETS)) {
            bctx.chead = (Bytef *)bctx.jcr->compress_buf + OFFSET_FADDR_SIZE;
            bctx.cbuf = (Bytef *)bctx.jcr->compress_buf + OFFSET_FADDR_SIZE + sizeof(comp_stream_header);
            bctx.max_compress_len = bctx.jcr->compress_buf_size - (sizeof(comp_stream_header) + OFFSET_FADDR_SIZE);
         } else {
            bctx.chead = (Bytef *)bctx.jcr->compress_buf;
            bctx.cbuf = (Bytef *)bctx.jcr->compress_buf + sizeof(comp_stream_header);
            bctx.max_compress_len = bctx.jcr->compress_buf_size - sizeof(comp_stream_header);
         }

         bctx.wbuf = bctx.jcr->compress_buf; /* compressed output here */
         bctx.cipher_input = (uint8_t *)bctx.jcr->compress_buf; /* encrypt compressed data */
         bctx.ch.magic = bctx.ff_pkt->Compress_algo;
         bctx.ch.version = COMP_HEAD_VERSION;
      } else {
         /*
          * Calculate buffer offsets.
          */
         bctx.chead = NULL;
         if ((bctx.ff_pkt->flags & FO_SPARSE) ||
             (bctx.ff_pkt->flags & FO_OFFSETS)) {
            bctx.cbuf = (Bytef *)bctx.jcr->compress_buf + OFFSET_FADDR_SIZE;
            bctx.max_compress_len = bctx.jcr->compress_buf_size - OFFSET_FADDR_SIZE;
         } else {
            bctx.cbuf = (Bytef *)bctx.jcr->compress_buf;
            bctx.max_compress_len = bctx.jcr->compress_buf_size;
         }
         bctx.wbuf = bctx.jcr->compress_buf; /* compressed output here */
      }

      /*
       * Do compression specific actions and set the magic, header version and compression level.
       */
      switch (bctx.ff_pkt->Compress_algo) {
#ifdef HAVE_LIBZ
      case COMPRESS_GZIP:
         /**
          * Only change zlib parameters if there is no pending operation.
          * This should never happen as deflatereset is called after each
          * deflate.
          */
         if (((z_stream *)bctx.jcr->pZLIB_compress_workset)->total_in == 0) {
            int zstat;

            /*
             * Set gzip compression level - must be done per file
             */
            if ((zstat = deflateParams((z_stream*)bctx.jcr->pZLIB_compress_workset,
                                        bctx.ff_pkt->Compress_level, Z_DEFAULT_STRATEGY)) != Z_OK) {
               Jmsg(bctx.jcr, M_FATAL, 0, _("Compression deflateParams error: %d\n"), zstat);
               bctx.jcr->setJobStatus(JS_ErrorTerminated);
               goto bail_out;
            }
         }
         bctx.ch.level = bctx.ff_pkt->Compress_level;
         break;
#endif
#ifdef HAVE_LZO
      case COMPRESS_LZO1X:
         break;
#endif
      default:
         break;
      }
   }

   retval = true;

bail_out:
   return retval;
}

#ifdef HAVE_LIBZ
static bool compress_with_zlib(b_ctx &bctx)
{
   int zstat;
   uint32_t input_len = bctx.jcr->store_bsock->msglen;

   Dmsg3(400, "cbuf=0x%x rbuf=0x%x len=%u\n", bctx.cbuf, bctx.rbuf, input_len);

   ((z_stream*)bctx.jcr->pZLIB_compress_workset)->next_in = (Bytef *)bctx.rbuf;
   ((z_stream*)bctx.jcr->pZLIB_compress_workset)->avail_in = input_len;
   ((z_stream*)bctx.jcr->pZLIB_compress_workset)->next_out = (Bytef *)bctx.cbuf;
   ((z_stream*)bctx.jcr->pZLIB_compress_workset)->avail_out = bctx.max_compress_len;

   if ((zstat = deflate((z_stream*)bctx.jcr->pZLIB_compress_workset, Z_FINISH)) != Z_STREAM_END) {
      Jmsg(bctx.jcr, M_FATAL, 0, _("Compression deflate error: %d\n"), zstat);
      bctx.jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   bctx.compress_len = ((z_stream*)bctx.jcr->pZLIB_compress_workset)->total_out;

   /*
    * Reset zlib stream to be able to begin from scratch again
    */
   if ((zstat = deflateReset((z_stream*)bctx.jcr->pZLIB_compress_workset)) != Z_OK) {
      Jmsg(bctx.jcr, M_FATAL, 0, _("Compression deflateReset error: %d\n"), zstat);
      bctx.jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   Dmsg2(400, "GZIP compressed len=%d uncompressed len=%d\n", bctx.compress_len, input_len);

   return true;
}
#endif

#ifdef HAVE_LZO
static bool compress_with_lzo(b_ctx &bctx)
{
   int lzores;
   lzo_uint len;
   uint32_t input_len = bctx.jcr->store_bsock->msglen;

   Dmsg3(400, "cbuf=0x%x rbuf=0x%x len=%u\n", bctx.cbuf, bctx.rbuf, input_len);

   lzores = lzo1x_1_compress((const unsigned char *)bctx.rbuf, input_len,
                                    bctx.cbuf, &len, bctx.jcr->LZO_compress_workset);
   bctx.compress_len = len;

   if (lzores != LZO_E_OK || bctx.compress_len > bctx.max_compress_len) {
      /*
       * This should NEVER happen
       */
      Jmsg(bctx.jcr, M_FATAL, 0, _("Compression LZO error: %d\n"), lzores);
      bctx.jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }

   Dmsg2(400, "LZO compressed len=%d uncompressed len=%d\n", bctx.compress_len, input_len);

   return true;
}
#endif

bool compress_data(b_ctx &bctx)
{
   bool retval = false;

   bctx.compress_len = 0;
   switch (bctx.ff_pkt->Compress_algo) {
#ifdef HAVE_LIBZ
   case COMPRESS_GZIP:
      if (bctx.jcr->pZLIB_compress_workset) {
         if (!compress_with_zlib(bctx)) {
            goto bail_out;
         }
      }
      break;
#endif
#ifdef HAVE_LZO
   case COMPRESS_LZO1X:
      if (bctx.jcr->LZO_compress_workset) {
         if (!compress_with_lzo(bctx)) {
            goto bail_out;
         }
      }
      break;
#endif
   default:
      break;
   }

   if (bctx.chead) {
      ser_declare;

      /*
       * Complete header
       */
      ser_begin(bctx.chead, sizeof(comp_stream_header));
      ser_uint32(bctx.ch.magic);
      ser_uint32(bctx.compress_len);
      ser_uint16(bctx.ch.level);
      ser_uint16(bctx.ch.version);
      ser_end(bctx.chead, sizeof(comp_stream_header));

      bctx.compress_len += sizeof(comp_stream_header); /* add size of header */
   }

   bctx.jcr->store_bsock->msglen = bctx.compress_len; /* set compressed length */
   bctx.cipher_input_len = bctx.compress_len;

   retval = true;

bail_out:
   return retval;
}

#ifdef HAVE_LIBZ
static bool decompress_with_zlib(JCR *jcr, char **data, uint32_t *length, bool with_header)
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
   compress_len = jcr->compress_buf_size;

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

   while ((status = uncompress((Byte *)jcr->compress_buf, &compress_len,
                               (const Byte *)cbuf, (uLong)real_compress_len)) == Z_BUF_ERROR) {
      /*
       * The buffer size is too small, try with a bigger one
       */
      compress_len = jcr->compress_buf_size = jcr->compress_buf_size + (jcr->compress_buf_size >> 1);
      Dmsg2(200, "Comp_len=%d msglen=%d\n", compress_len, *length);
      jcr->compress_buf = check_pool_memory_size(jcr->compress_buf, compress_len);
   }

   if (status != Z_OK) {
      Qmsg(jcr, M_ERROR, 0, _("Uncompression error on file %s. ERR=%s\n"),
           jcr->last_fname, zlib_strerror(status));
      return false;
   }

   *data = jcr->compress_buf;
   *length = compress_len;

   Dmsg2(200, "Write uncompressed %d bytes, total before write=%s\n", compress_len, edit_uint64(jcr->JobBytes, ec1));

   return true;
}
#endif
#ifdef HAVE_LZO
static bool decompress_with_lzo(JCR *jcr, char **data, uint32_t *length)
{
   char ec1[50]; /* Buffer printing huge values */
   lzo_uint compress_len;
   const unsigned char *cbuf;
   int status, real_compress_len;

   compress_len = jcr->compress_buf_size;
   cbuf = (const unsigned char*)*data + sizeof(comp_stream_header);
   real_compress_len = *length - sizeof(comp_stream_header);
   Dmsg2(200, "Comp_len=%d msglen=%d\n", compress_len, *length);
   while ((status = lzo1x_decompress_safe(cbuf, real_compress_len,
                                          (unsigned char *)jcr->compress_buf,
                                          &compress_len, NULL)) == LZO_E_OUTPUT_OVERRUN) {
      /*
       * The buffer size is too small, try with a bigger one
       */
      compress_len = jcr->compress_buf_size = jcr->compress_buf_size + (jcr->compress_buf_size >> 1);
      Dmsg2(200, "Comp_len=%d msglen=%d\n", compress_len, *length);
      jcr->compress_buf = check_pool_memory_size(jcr->compress_buf, compress_len);
   }

   if (status != LZO_E_OK) {
      Qmsg(jcr, M_ERROR, 0, _("LZO uncompression error on file %s. ERR=%d\n"), jcr->last_fname, status);
      return false;
   }

   *data = jcr->compress_buf;
   *length = compress_len;

   Dmsg2(200, "Write uncompressed %d bytes, total before write=%s\n", compress_len, edit_uint64(jcr->JobBytes, ec1));

   return true;
}
#endif

bool decompress_data(JCR *jcr, int32_t stream, char **data, uint32_t *length)
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
            return decompress_with_zlib(jcr, data, length, true);
#endif
#ifdef HAVE_LZO
         case COMPRESS_LZO1X:
            return decompress_with_lzo(jcr, data, length);
#endif
         default:
            Qmsg(jcr, M_ERROR, 0, _("Compression algorithm 0x%x found, but not supported!\n"), comp_magic);
            return false;
      }
      break;
   }
   default:
      return decompress_with_zlib(jcr, data, length, false);
   }
}
#else
void adjust_compression_buffers(JCR *jcr)
{
}

bool setup_compression_context(b_ctx &bctx)
{
   return true;
}

bool compress_data(b_ctx &bctx)
{
   return true;
}

bool decompress_data(JCR *jcr, int32_t stream, char **data, uint32_t *length)
{
   Qmsg(jcr, M_ERROR, 0, _("Compressed data stream found, but compression not configured!\n"));
   return false;
}
#endif /* defined(HAVE_LZO) || defined(HAVE_LIBZ) */
