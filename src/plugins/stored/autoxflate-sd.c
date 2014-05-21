/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
 * Storage Daemon plugin that handles automatic deflation/inflation of data.
 *
 * Marco van Wieringen, June 2013
 */
#include "bareos.h"
#include "stored.h"

#if defined(HAVE_LIBZ)
#include <zlib.h>
#endif

#if defined(HAVE_FASTLZ)
#include <fastlzlib.h>
#endif

#define PLUGIN_LICENSE      "Bareos AGPLv3"
#define PLUGIN_AUTHOR       "Marco van Wieringen"
#define PLUGIN_DATE         "June 2013"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Auto Xflation Storage Daemon Plugin"
#define PLUGIN_USAGE        "(No usage yet)"

/*
 * Forward referenced functions
 */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, psdVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, psdVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bsdEvent *event, void *value);
static bRC setup_record_translation(void *value);
static bRC handle_read_translation(void *value);
static bRC handle_write_translation(void *value);

static bool setup_auto_deflation(DCR *dcr);
static bool setup_auto_inflation(DCR *dcr);
static bool auto_deflate_record(DCR *dcr);
static bool auto_inflate_record(DCR *dcr);

/*
 * Is the SD in compatible mode or not.
 */
static bool sd_enabled_compatible = false;

/*
 * Pointers to Bareos functions
 */
static bsdFuncs *bfuncs = NULL;
static bsdInfo *binfo = NULL;

static genpInfo pluginInfo = {
   sizeof(pluginInfo),
   SD_PLUGIN_INTERFACE_VERSION,
   SD_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION,
   PLUGIN_USAGE
};

static psdFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   SD_PLUGIN_INTERFACE_VERSION,

   /*
    * Entry points into plugin
    */
   newPlugin,        /* new plugin instance */
   freePlugin,       /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent
};

static int const dbglvl = 100;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bareos can directly call these two entry points
 *  they are common to all Bareos plugins.
 *
 * External entry point called by Bareos to "load the plugin
 */
bRC DLL_IMP_EXP loadPlugin(bsdInfo *lbinfo,
                           bsdFuncs *lbfuncs,
                           genpInfo **pinfo,
                           psdFuncs **pfuncs)
{
   bfuncs = lbfuncs;       /* set Bareos funct pointers */
   binfo  = lbinfo;
   Dmsg2(dbglvl, "autoxflate-sd: Loaded: size=%d version=%d\n", bfuncs->size, bfuncs->version);
   *pinfo  = &pluginInfo;  /* return pointer to our info */
   *pfuncs = &pluginFuncs; /* return pointer to our functions */

   /*
    * Get the current setting of the compatible flag.
    */
   bfuncs->getBareosValue(NULL, bsdVarCompatible, (void *)&sd_enabled_compatible);

   return bRC_OK;
}

/*
 * External entry point to unload the plugin
 */
bRC DLL_IMP_EXP unloadPlugin()
{
   return bRC_OK;
}

#ifdef __cplusplus
}
#endif

/*
 * The following entry points are accessed through the function
 * pointers we supplied to Bareos. Each plugin type (dir, fd, sd)
 * has its own set of entry points that the plugin must define.
 *
 * Create a new instance of the plugin i.e. allocate our private storage
 */
static bRC newPlugin(bpContext *ctx)
{
   int JobId = 0;

   bfuncs->getBareosValue(ctx, bsdVarJobId, (void *)&JobId);
   Dmsg1(dbglvl, "autoxflate-sd: newPlugin JobId=%d\n", JobId);

   /*
    * Only register plugin events we are interested in.
    *
    * bsdEventSetupRecordTranslation - Setup the buffers for doing record translation.
    * bsdEventReadRecordTranslation - Perform read-side record translation.
    * bsdEventWriteRecordTranslation - Perform write-side record translantion.
    */
   bfuncs->registerBareosEvents(ctx,
                                3,
                                bsdEventSetupRecordTranslation,
                                bsdEventReadRecordTranslation,
                                bsdEventWriteRecordTranslation);

   return bRC_OK;
}

/*
 * Free a plugin instance, i.e. release our private storage
 */
static bRC freePlugin(bpContext *ctx)
{
   int JobId = 0;

   bfuncs->getBareosValue(ctx, bsdVarJobId, (void *)&JobId);
   Dmsg1(dbglvl, "autoxflate-sd: freePlugin JobId=%d\n", JobId);

   return bRC_OK;
}

/*
 * Return some plugin value (none defined)
 */
static bRC getPluginValue(bpContext *ctx, psdVariable var, void *value)
{
   Dmsg1(dbglvl, "autoxflate-sd: getPluginValue var=%d\n", var);

   return bRC_OK;
}

/*
 * Set a plugin value (none defined)
 */
static bRC setPluginValue(bpContext *ctx, psdVariable var, void *value)
{
   Dmsg1(dbglvl, "autoxflate-sd: setPluginValue var=%d\n", var);

   return bRC_OK;
}

/*
 * Handle an event that was generated in Bareos
 */
static bRC handlePluginEvent(bpContext *ctx, bsdEvent *event, void *value)
{
   Dmsg1(dbglvl, "autoxflate-sd: handlePluginEvent event %d\n", event->eventType);
   switch (event->eventType) {
   case bsdEventSetupRecordTranslation:
      return setup_record_translation(value);
   case bsdEventReadRecordTranslation:
      return handle_read_translation(value);
   case bsdEventWriteRecordTranslation:
      return handle_write_translation(value);
   default:
      Dmsg1(dbglvl, "autoxflate-sd: Unknown event %d\n", event->eventType);
      return bRC_Error;
   }

   return bRC_OK;
}

static bRC setup_record_translation(void *value)
{
   DCR *dcr;

   /*
    * Unpack the arguments passed in.
    */
   dcr = (DCR *)value;
   if (!dcr) {
      return bRC_Error;
   }

   /*
    * Setup auto deflation/inflation of streams when enabled for this device.
    */
   switch (dcr->autodeflate) {
   case IO_DIRECTION_OUT:
   case IO_DIRECTION_INOUT:
      if (!setup_auto_deflation(dcr)) {
         return bRC_Error;
      }
      break;
   default:
      break;
   }

   switch (dcr->autoinflate) {
   case IO_DIRECTION_OUT:
   case IO_DIRECTION_INOUT:
      if (!setup_auto_inflation(dcr)) {
         return bRC_Error;
      }
      break;
   default:
      break;
   }

   return bRC_OK;
}

static bRC handle_read_translation(void *value)
{
   DCR *dcr;
   bool swap_record = false;

   /*
    * Unpack the arguments passed in.
    */
   dcr = (DCR *)value;
   if (!dcr) {
      return bRC_Error;
   }

   /*
    * See if we need to perform auto deflation/inflation of streams.
    */
   switch (dcr->autoinflate) {
   case IO_DIRECTION_IN:
   case IO_DIRECTION_INOUT:
      swap_record = auto_inflate_record(dcr);
      break;
   default:
      break;
   }

   if (!swap_record) {
      switch (dcr->autodeflate) {
      case IO_DIRECTION_IN:
      case IO_DIRECTION_INOUT:
         swap_record = auto_deflate_record(dcr);
         break;
      default:
         break;
      }
   }

   return bRC_OK;
}

static bRC handle_write_translation(void *value)
{
   DCR *dcr;
   bool swap_record = false;

   /*
    * Unpack the arguments passed in.
    */
   dcr = (DCR *)value;
   if (!dcr) {
      return bRC_Error;
   }

   /*
    * See if we need to perform auto deflation/inflation of streams.
    */
   switch (dcr->autoinflate) {
   case IO_DIRECTION_OUT:
   case IO_DIRECTION_INOUT:
      swap_record = auto_inflate_record(dcr);
      break;
   default:
      break;
   }

   if (!swap_record) {
      switch (dcr->autodeflate) {
      case IO_DIRECTION_OUT:
      case IO_DIRECTION_INOUT:
         swap_record = auto_deflate_record(dcr);
         break;
      default:
         break;
      }
   }

   return bRC_OK;
}

#if defined(HAVE_LZO) || defined(HAVE_LIBZ) || defined(HAVE_FASTLZ)
/*
 * Setup deflate for auto deflate of data streams.
 */
static bool setup_auto_deflation(DCR *dcr)
{
   JCR *jcr = dcr->jcr;
   bool retval = false;
   uint32_t compress_buf_size = 0;

   if (jcr->buf_size == 0) {
      jcr->buf_size = DEFAULT_NETWORK_BUFFER_SIZE;
   }

   if (!setup_compression_buffers(jcr, sd_enabled_compatible,
                                  dcr->device->autodeflate_algorithm,
                                  &compress_buf_size)) {
      goto bail_out;
   }

   /*
    * See if we need to create a new compression buffer or make sure the existing is big enough.
    */
   if (!jcr->compress.deflate_buffer) {
      jcr->compress.deflate_buffer = get_memory(compress_buf_size);
      jcr->compress.deflate_buffer_size = compress_buf_size;
   } else {
      if (compress_buf_size > jcr->compress.deflate_buffer_size) {
         jcr->compress.deflate_buffer = realloc_pool_memory(jcr->compress.deflate_buffer, compress_buf_size);
         jcr->compress.deflate_buffer_size = compress_buf_size;
      }
   }

   switch (dcr->device->autodeflate_algorithm) {
#if defined(HAVE_LIBZ)
   case COMPRESS_GZIP: {
      int zstat;
      z_stream *pZlibStream;

      pZlibStream = (z_stream *)jcr->compress.workset.pZLIB;
      if ((zstat = deflateParams(pZlibStream, dcr->device->autodeflate_level, Z_DEFAULT_STRATEGY)) != Z_OK) {
         Jmsg(jcr, M_FATAL, 0, _("Compression deflateParams error: %d\n"), zstat);
         jcr->setJobStatus(JS_ErrorTerminated);
         goto bail_out;
      }
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

      switch (dcr->device->autodeflate_algorithm) {
      case COMPRESS_FZ4L:
      case COMPRESS_FZ4H:
         compressor = COMPRESSOR_LZ4;
         break;
      }

      pZfastStream = (zfast_stream *)jcr->compress.workset.pZFAST;
      if ((zstat = fastlzlibSetCompressor(pZfastStream, compressor)) != Z_OK) {
         Jmsg(jcr, M_FATAL, 0, _("Compression fastlzlibSetCompressor error: %d\n"), zstat);
         jcr->setJobStatus(JS_ErrorTerminated);
         goto bail_out;
      }
      break;
   }
#endif
   default:
      break;
   }

   retval = true;

bail_out:
   return retval;
}

/*
 * Setup inflation for auto inflation of data streams.
 */
static bool setup_auto_inflation(DCR *dcr)
{
   JCR *jcr = dcr->jcr;
   uint32_t decompress_buf_size;

   if (jcr->buf_size == 0) {
      jcr->buf_size = DEFAULT_NETWORK_BUFFER_SIZE;
   }

   setup_decompression_buffers(jcr, &decompress_buf_size);
   if (decompress_buf_size > 0) {
      /*
       * See if we need to create a new compression buffer or make sure the existing is big enough.
       */
      if (!jcr->compress.inflate_buffer) {
         jcr->compress.inflate_buffer = get_memory(decompress_buf_size);
         jcr->compress.inflate_buffer_size = decompress_buf_size;
      } else {
         if (decompress_buf_size > jcr->compress.inflate_buffer_size) {
            jcr->compress.inflate_buffer = realloc_pool_memory(jcr->compress.inflate_buffer, decompress_buf_size);
            jcr->compress.inflate_buffer_size = decompress_buf_size;
         }
      } else {
         return false;
      }
   }

   return true;
}

/*
 * Perform automatic compression of certain stream types when enabled in the config.
 */
static bool auto_deflate_record(DCR *dcr)
{
   ser_declare;
   comp_stream_header ch;
   DEV_RECORD *rec, *nrec;
   bool retval = false;
   bool intermediate_value = false;
   unsigned int max_compression_length = 0;
   unsigned char *data = NULL;

   /*
    * See what our starting point is. When dcr->after_rec is set we already have
    * a translated record by an other SD plugin. Then we use that translated record
    * as the starting point otherwise we start at dcr->before_rec. When an earlier
    * translation already happened we can free that record when we have a success
    * full translation here as that record is of no use anymore.
    */
   if (dcr->after_rec) {
      rec = dcr->after_rec;
      intermediate_value = true;
   } else {
      rec = dcr->before_rec;
   }

   /*
    * We only do autocompression for the following stream types:
    *
    * - STREAM_FILE_DATA
    * - STREAM_WIN32_DATA
    * - STREAM_SPARSE_DATA
    */
   switch (rec->maskedStream) {
   case STREAM_FILE_DATA:
   case STREAM_WIN32_DATA:
   case STREAM_SPARSE_DATA:
      break;
   default:
      goto bail_out;
   }

   /*
    * Clone the data from the original DEV_RECORD to the converted one.
    * As we use the compression buffers for the data we need a new
    * DEV_RECORD without a new memory buffer so we call new_record here
    * with the with_data boolean set explicitly to false.
    */
   nrec = bfuncs->new_record(false);
   bfuncs->copy_record_state(nrec, rec);

   /*
    * Setup the converted DEV_RECORD to point with its data buffer to the compression buffer.
    */
   nrec->data = dcr->jcr->compress.deflate_buffer;
   switch (rec->maskedStream) {
   case STREAM_FILE_DATA:
   case STREAM_WIN32_DATA:
      data = (unsigned char *)nrec->data + sizeof(comp_stream_header);
      max_compression_length = dcr->jcr->compress.deflate_buffer_size - sizeof(comp_stream_header);
      break;
   case STREAM_SPARSE_DATA:
      data = (unsigned char *)nrec->data + OFFSET_FADDR_SIZE + sizeof(comp_stream_header);
      max_compression_length = dcr->jcr->compress.deflate_buffer_size - OFFSET_FADDR_SIZE - sizeof(comp_stream_header);
      break;
   }

   /*
    * Compress the data using the configured compression algorithm.
    */
   if (!compress_data(dcr->jcr, dcr->device->autodeflate_algorithm, rec->data, rec->data_len,
                      data, max_compression_length, &nrec->data_len)) {
      bfuncs->free_record(nrec);
      goto bail_out;
   }

   /*
    * Map the streams.
    */
   switch (rec->maskedStream) {
   case STREAM_FILE_DATA:
      nrec->Stream = STREAM_COMPRESSED_DATA;
      nrec->maskedStream = STREAM_COMPRESSED_DATA;
      break;
   case STREAM_WIN32_DATA:
      nrec->Stream = STREAM_WIN32_COMPRESSED_DATA;
      nrec->maskedStream = STREAM_WIN32_COMPRESSED_DATA;
      break;
   case STREAM_SPARSE_DATA:
      nrec->Stream = STREAM_SPARSE_COMPRESSED_DATA;
      nrec->maskedStream = STREAM_SPARSE_COMPRESSED_DATA;
      break;
   default:
      break;
   }

   /*
    * Generate a compression header.
    */
   ch.magic = dcr->device->autodeflate_algorithm;
   ch.level = dcr->device->autodeflate_level;
   ch.version = COMP_HEAD_VERSION;
   ch.size = nrec->data_len;

   switch (nrec->maskedStream) {
   case STREAM_COMPRESSED_DATA:
   case STREAM_WIN32_COMPRESSED_DATA:
      ser_begin(nrec->data, sizeof(comp_stream_header));
      ser_uint32(ch.magic);
      ser_uint32(ch.size);
      ser_uint16(ch.level);
      ser_uint16(ch.version);
      ser_end(nrec->data, sizeof(comp_stream_header));
      nrec->data_len += sizeof(comp_stream_header);
      break;
   case STREAM_SPARSE_COMPRESSED_DATA:
      /*
       * Copy the sparse offset from the original.
       */
      memcpy(nrec->data, rec->data, OFFSET_FADDR_SIZE);
      ser_begin(nrec->data + OFFSET_FADDR_SIZE, sizeof(comp_stream_header));
      ser_uint32(ch.magic);
      ser_uint32(ch.size);
      ser_uint16(ch.level);
      ser_uint16(ch.version);
      ser_end(nrec->data + OFFSET_FADDR_SIZE, sizeof(comp_stream_header));
      nrec->data_len += OFFSET_FADDR_SIZE + sizeof(comp_stream_header);
      break;
   }

   Dmsg4(400, "auto_deflate_record: From datastream %d to %d from original size %ld to %ld\n",
         rec->maskedStream, nrec->maskedStream, rec->data_len, nrec->data_len);

   /*
    * If the input is just an intermediate value free it now.
    */
   if (intermediate_value) {
      bfuncs->free_record(dcr->after_rec);
   }
   dcr->after_rec = nrec;
   retval = true;

bail_out:
   return retval;
}

/*
 * Inflate (uncompress) the content of a read record and return the data as an alternative datastream.
 */
static bool auto_inflate_record(DCR *dcr)
{
   DEV_RECORD *rec, *nrec;
   bool retval = false;
   bool intermediate_value = false;

   /*
    * See what our starting point is. When dcr->after_rec is set we already have
    * a translated record by an other SD plugin. Then we use that translated record
    * as the starting point otherwise we start at dcr->before_rec. When an earlier
    * translation already happened we can free that record when we have a success
    * full translation here as that record is of no use anymore.
    */
   if (dcr->after_rec) {
      rec = dcr->after_rec;
      intermediate_value = true;
   } else {
      rec = dcr->before_rec;
   }

   /*
    * We only do auto inflation for the following stream types:
    *
    * - STREAM_COMPRESSED_DATA
    * - STREAM_WIN32_COMPRESSED_DATA
    * - STREAM_SPARSE_COMPRESSED_DATA
    */
   switch (rec->maskedStream) {
   case STREAM_COMPRESSED_DATA:
   case STREAM_WIN32_COMPRESSED_DATA:
   case STREAM_SPARSE_COMPRESSED_DATA:
      break;
   default:
      goto bail_out;
   }

   /*
    * Clone the data from the original DEV_RECORD to the converted one.
    * As we use the compression buffers for the data we need a new
    * DEV_RECORD without a new memory buffer so we call new_record here
    * with the with_data boolean set explicitly to false.
    */
   nrec = bfuncs->new_record(false);
   bfuncs->copy_record_state(nrec, rec);

   /*
    * Setup the converted record to point to the original data.
    * The decompress_data function will decompress that data and
    * then update the pointers with the data in the compression buffer
    * and with the length of the decompressed data.
    */
   nrec->data = rec->data;
   nrec->data_len = rec->data_len;

   if (!decompress_data(dcr->jcr, "Unknown", rec->maskedStream,
                        &nrec->data, &nrec->data_len, true)) {
      bfuncs->free_record(nrec);
      goto bail_out;
   }

   /*
    * If we succeeded in decompressing the data update the stream type.
    */
   switch (rec->maskedStream) {
   case STREAM_COMPRESSED_DATA:
      nrec->Stream = STREAM_FILE_DATA;
      nrec->maskedStream = STREAM_FILE_DATA;
      break;
   case STREAM_WIN32_COMPRESSED_DATA:
      nrec->Stream = STREAM_WIN32_DATA;
      nrec->maskedStream = STREAM_WIN32_DATA;
      break;
   case STREAM_SPARSE_COMPRESSED_DATA:
      nrec->Stream = STREAM_SPARSE_DATA;
      nrec->maskedStream = STREAM_SPARSE_DATA;
      break;
   default:
      break;
   }

   Dmsg4(400, "auto_inflate_record: From datastream %d to %d from original size %ld to %ld\n",
         rec->maskedStream, nrec->maskedStream, rec->data_len, nrec->data_len);

   /*
    * If the input is just an intermediate value free it now.
    */
   if (intermediate_value) {
      bfuncs->free_record(dcr->after_rec);
   }
   dcr->after_rec = nrec;
   retval = true;

bail_out:
   return retval;
}
#else
/*
 * Setup deflate for auto deflate of data streams.
 */
static bool setup_auto_deflation(DCR *dcr)
{
   return true;
}

/*
 * Setup inflation for auto inflation of data streams.
 */
static bool setup_auto_inflation(DCR *dcr)
{
   return true;
}

/*
 * Perform automatic compression of certain stream types when enabled in the config.
 */
static bool auto_deflate_record(DCR *dcr)
{
   return false;
}

/*
 * Inflate (uncompress) the content of a read record and return
 * the data as an alternative datastream.
 */
static bool auto_inflate_record(DCR *dcr)
{
   return false;
}
#endif /* defined(HAVE_LZO) || defined(HAVE_LIBZ) || defined(HAVE_FASTLZ) */
