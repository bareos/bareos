/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, March MM
 * Extracted from other source files by Marco van Wieringen, June 2011
 */
/**
 * @file
 * Functions to handle compression/decompression of data.
 */

#include "include/bareos.h"
#include "filed.h"

#if defined(HAVE_LZO) || defined(HAVE_LIBZ) || defined(HAVE_FASTLZ)

#if defined(HAVE_LIBZ)
#include <zlib.h>
#endif

#if defined(HAVE_FASTLZ)
#include <fastlzlib.h>
#endif

/**
 * For compression we enable all used compressors in the fileset.
 */
bool adjust_compression_buffers(JobControlRecord *jcr)
{
   findFILESET *fileset = jcr->ff->fileset;
   uint32_t compress_buf_size = 0;

   if (fileset) {
      int i, j;

      for (i = 0; i < fileset->include_list.size(); i++) {
         findIncludeExcludeItem *incexe = (findIncludeExcludeItem *)fileset->include_list.get(i);
         for (j = 0; j < incexe->opts_list.size(); j++) {
            findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);

            if (!setup_compression_buffers(jcr, me->compatible, fo->Compress_algo,
                                           &compress_buf_size)) {
               return false;
            }
         }
      }

      if (compress_buf_size > 0) {
         jcr->compress.deflate_buffer = get_memory(compress_buf_size);
         jcr->compress.deflate_buffer_size = compress_buf_size;
      }
   }

   return true;
}

/**
 * For decompression we use the same decompression buffer for each algorithm.
 */
bool adjust_decompression_buffers(JobControlRecord *jcr)
{
   uint32_t decompress_buf_size;

   setup_decompression_buffers(jcr, &decompress_buf_size);

   if (decompress_buf_size > 0) {
      jcr->compress.inflate_buffer = get_memory(decompress_buf_size);
      jcr->compress.inflate_buffer_size = decompress_buf_size;
   }

   return true;
}

bool setup_compression_context(b_ctx &bctx)
{
   bool retval = false;

   if (bit_is_set(FO_COMPRESS, bctx.ff_pkt->flags)) {
      /*
       * See if we need to be compatible with the old GZIP stream encoding.
       */
      if (!me->compatible || bctx.ff_pkt->Compress_algo != COMPRESS_GZIP) {
         memset(&bctx.ch, 0, sizeof(comp_stream_header));

         /*
          * Calculate buffer offsets.
          */
         if (bit_is_set(FO_SPARSE, bctx.ff_pkt->flags) ||
             bit_is_set(FO_OFFSETS, bctx.ff_pkt->flags)) {
            bctx.chead = (uint8_t *)bctx.jcr->compress.deflate_buffer + OFFSET_FADDR_SIZE;
            bctx.cbuf = (uint8_t *)bctx.jcr->compress.deflate_buffer + OFFSET_FADDR_SIZE + sizeof(comp_stream_header);
            bctx.max_compress_len = bctx.jcr->compress.deflate_buffer_size - (sizeof(comp_stream_header) + OFFSET_FADDR_SIZE);
         } else {
            bctx.chead = (uint8_t *)bctx.jcr->compress.deflate_buffer;
            bctx.cbuf = (uint8_t *)bctx.jcr->compress.deflate_buffer + sizeof(comp_stream_header);
            bctx.max_compress_len = bctx.jcr->compress.deflate_buffer_size - sizeof(comp_stream_header);
         }

         bctx.wbuf = bctx.jcr->compress.deflate_buffer; /* compressed output here */
         bctx.cipher_input = (uint8_t *)bctx.jcr->compress.deflate_buffer; /* encrypt compressed data */
         bctx.ch.magic = bctx.ff_pkt->Compress_algo;
         bctx.ch.version = COMP_HEAD_VERSION;
      } else {
         /*
          * Calculate buffer offsets.
          */
         bctx.chead = NULL;
         if (bit_is_set(FO_SPARSE, bctx.ff_pkt->flags) ||
             bit_is_set(FO_OFFSETS, bctx.ff_pkt->flags)) {
            bctx.cbuf = (uint8_t *)bctx.jcr->compress.deflate_buffer + OFFSET_FADDR_SIZE;
            bctx.max_compress_len = bctx.jcr->compress.deflate_buffer_size - OFFSET_FADDR_SIZE;
         } else {
            bctx.cbuf = (uint8_t *)bctx.jcr->compress.deflate_buffer;
            bctx.max_compress_len = bctx.jcr->compress.deflate_buffer_size;
         }
         bctx.wbuf = bctx.jcr->compress.deflate_buffer; /* compressed output here */
         bctx.cipher_input = (uint8_t *)bctx.jcr->compress.deflate_buffer; /* encrypt compressed data */
      }

      /*
       * Do compression specific actions and set the magic, header version and compression level.
       */
      switch (bctx.ff_pkt->Compress_algo) {
#if defined(HAVE_LIBZ)
      case COMPRESS_GZIP: {
         z_stream *pZlibStream;

         /**
          * Only change zlib parameters if there is no pending operation.
          * This should never happen as deflateReset is called after each
          * deflate.
          */
         pZlibStream = (z_stream *)bctx.jcr->compress.workset.pZLIB;
         if (pZlibStream->total_in == 0) {
            int zstat;

            /*
             * Set gzip compression level - must be done per file
             */
            if ((zstat = deflateParams(pZlibStream, bctx.ff_pkt->Compress_level, Z_DEFAULT_STRATEGY)) != Z_OK) {
               Jmsg(bctx.jcr, M_FATAL, 0, _("Compression deflateParams error: %d\n"), zstat);
               bctx.jcr->setJobStatus(JS_ErrorTerminated);
               goto bail_out;
            }
         }
         bctx.ch.level = bctx.ff_pkt->Compress_level;
         break;
     }
#endif
#if defined(HAVE_LZO)
      case COMPRESS_LZO1X:
         break;
#endif
#if defined(HAVE_FASTLZ)
      case COMPRESS_FZFZ:
      case COMPRESS_FZ4L:
      case COMPRESS_FZ4H: {
         int zstat;
         zfast_stream *pZfastStream;
         zfast_stream_compressor compressor = COMPRESSOR_FASTLZ;

         /**
          * Only change fastlz parameters if there is no pending operation.
          * This should never happen as fastlzlibCompressReset is called after each
          * fastlzlibCompress.
          */
         pZfastStream = (zfast_stream *)bctx.jcr->compress.workset.pZFAST;
         if (pZfastStream->total_in == 0) {

            switch (bctx.ff_pkt->Compress_algo) {
            case COMPRESS_FZ4L:
            case COMPRESS_FZ4H:
               compressor = COMPRESSOR_LZ4;
               break;
            }

            if ((zstat = fastlzlibSetCompressor(pZfastStream, compressor)) != Z_OK) {
               Jmsg(bctx.jcr, M_FATAL, 0, _("Compression fastlzlibSetCompressor error: %d\n"), zstat);
               bctx.jcr->setJobStatus(JS_ErrorTerminated);
               goto bail_out;
            }
         }
         bctx.ch.level = bctx.ff_pkt->Compress_level;
         break;
      }
#endif
      default:
         break;
      }
   }

   retval = true;

bail_out:
   return retval;
}

#else

bool adjust_compression_buffers(JobControlRecord *jcr)
{
   return true;
}

bool setup_compression_context(b_ctx &bctx)
{
   return true;
}
#endif /* defined(HAVE_LZO) || defined(HAVE_LIBZ) || defined(HAVE_FASTLZ) */
