/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2014 Planets Communications B.V.
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

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
// Marco van Wieringen, June 2013
/**
 * @file
 * Storage Daemon plugin that handles automatic deflation/inflation of data.
 */
#include "include/bareos.h"
#include "include/protocol_types.h"
#include "stored/stored.h"
#include "stored/device_control_record.h"

#if defined(HAVE_LIBZ)
#  include <zlib.h>
#endif

#include "fastlz/fastlzlib.h"

using namespace storagedaemon;

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Marco van Wieringen"
#define PLUGIN_DATE "June 2013"
#define PLUGIN_VERSION "1"
#define PLUGIN_DESCRIPTION "Auto Xflation Storage Daemon Plugin"
#define PLUGIN_USAGE "(No usage yet)"

#define Dmsg(context, level, ...)                                         \
  bareos_core_functions->DebugMessage(context, __FILE__, __LINE__, level, \
                                      __VA_ARGS__)
#define Jmsg(context, type, ...)                                          \
  bareos_core_functions->JobMessage(context, __FILE__, __LINE__, type, 0, \
                                    __VA_ARGS__)

#define SETTING_YES (char*)"yes"
#define SETTING_NO (char*)"no"
#define SETTING_UNSET (char*)"unknown"

#define COMPRESSOR_NAME_GZIP (char*)"GZIP"
#define COMPRESSOR_NAME_LZO (char*)"LZO"
#define COMPRESSOR_NAME_FZLZ (char*)"FASTLZ"
#define COMPRESSOR_NAME_FZ4L (char*)"LZ4"
#define COMPRESSOR_NAME_FZ4H (char*)"LZ4HC"
#define COMPRESSOR_NAME_UNSET (char*)"unknown"

// Forward referenced functions
static bRC newPlugin(PluginContext* ctx);
static bRC freePlugin(PluginContext* ctx);
static bRC getPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC setPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC handlePluginEvent(PluginContext* ctx, bSdEvent* event, void* value);
static bRC handleJobEnd(PluginContext* ctx);
static bRC setup_record_translation(PluginContext* ctx, void* value);
static bRC handle_read_translation(PluginContext* ctx, void* value);
static bRC handle_write_translation(PluginContext* ctx, void* value);

static bool SetupAutoDeflation(PluginContext* ctx, DeviceControlRecord* dcr);
static bool SetupAutoInflation(PluginContext* ctx, DeviceControlRecord* dcr);
static bool AutoDeflateRecord(PluginContext* ctx, DeviceControlRecord* dcr);
static bool AutoInflateRecord(PluginContext* ctx, DeviceControlRecord* dcr);

// Is the SD in compatible mode or not.
static bool sd_enabled_compatible = false;

// Pointers to Bareos functions
static CoreFunctions* bareos_core_functions = NULL;
static PluginApiDefinition* bareos_plugin_interface_version = NULL;

static PluginInformation pluginInfo
    = {sizeof(pluginInfo), SD_PLUGIN_INTERFACE_VERSION,
       SD_PLUGIN_MAGIC,    PLUGIN_LICENSE,
       PLUGIN_AUTHOR,      PLUGIN_DATE,
       PLUGIN_VERSION,     PLUGIN_DESCRIPTION,
       PLUGIN_USAGE};

static PluginFunctions pluginFuncs
    = {sizeof(pluginFuncs), SD_PLUGIN_INTERFACE_VERSION,

       // Entry points into plugin
       newPlugin,  /* new plugin instance */
       freePlugin, /* free plugin instance */
       getPluginValue, setPluginValue, handlePluginEvent};

// Plugin private context
struct plugin_ctx {
  // Counters for compression/decompression ratio
  uint64_t deflate_bytes_in;
  uint64_t deflate_bytes_out;
  uint64_t inflate_bytes_in;
  uint64_t inflate_bytes_out;
};

static int const debuglevel = 200;


// Does the mode contain OUT
static bool AutoxflateModeContainsOut(AutoXflateMode mode)
{
  return (mode == AutoXflateMode::IO_DIRECTION_OUT
          || mode == AutoXflateMode::IO_DIRECTION_INOUT);
}

// Does the mode contain IN
static bool AutoxflateModeContainsIn(AutoXflateMode mode)
{
  return (mode == AutoXflateMode::IO_DIRECTION_IN
          || mode == AutoXflateMode::IO_DIRECTION_INOUT);
}

// what streams can be decompressed by the plugin
static bool IsCompressedStreamWeHandle(int streamtype)
{
  return (streamtype == STREAM_COMPRESSED_DATA
          || streamtype == STREAM_WIN32_COMPRESSED_DATA
          || streamtype == STREAM_SPARSE_COMPRESSED_DATA);
}

// what streams can be compressed by the plugin
static bool IsUncompressedStreamWeHandle(int streamtype)
{
  return (streamtype == STREAM_FILE_DATA || streamtype == STREAM_WIN32_DATA
          || streamtype == STREAM_SPARSE_DATA);
}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bareos can directly call these two entry points
 *  they are common to all Bareos plugins.
 *
 * External entry point called by Bareos to "load the plugin
 */
bRC loadPlugin(PluginApiDefinition* lbareos_plugin_interface_version,
               CoreFunctions* lbareos_core_functions,
               PluginInformation** plugin_information,
               PluginFunctions** plugin_functions)
{
  bareos_core_functions
      = lbareos_core_functions; /* set Bareos funct pointers */
  bareos_plugin_interface_version = lbareos_plugin_interface_version;
  *plugin_information = &pluginInfo; /* return pointer to our info */
  *plugin_functions = &pluginFuncs;  /* return pointer to our functions */

  // Get the current setting of the compatible flag.
  bareos_core_functions->getBareosValue(NULL, bsdVarCompatible,
                                        (void*)&sd_enabled_compatible);

  return bRC_OK;
}

// External entry point to unload the plugin
bRC unloadPlugin() { return bRC_OK; }

#ifdef __cplusplus
}
#endif

/**
 * The following entry points are accessed through the function
 * pointers we supplied to Bareos. Each plugin type (dir, fd, sd)
 * has its own set of entry points that the plugin must define.
 *
 * Create a new instance of the plugin i.e. allocate our private storage
 */
static bRC newPlugin(PluginContext* ctx)
{
  int JobId = 0;
  struct plugin_ctx* p_ctx;

  bareos_core_functions->getBareosValue(ctx, bsdVarJobId, (void*)&JobId);
  Dmsg(ctx, debuglevel, "autoxflate-sd: newPlugin JobId=%d\n", JobId);

  p_ctx = (struct plugin_ctx*)malloc(sizeof(struct plugin_ctx));
  if (!p_ctx) { return bRC_Error; }

  memset(p_ctx, 0, sizeof(struct plugin_ctx));
  ctx->plugin_private_context = (void*)p_ctx; /* set our context pointer */

  /*
   * Only register plugin events we are interested in.
   *
   * bSdEventJobEnd - SD Job finished.
   * bSdEventSetupRecordTranslation - Setup the buffers for doing record
   * translation. bSdEventReadRecordTranslation - Perform read-side record
   * translation. bSdEventWriteRecordTranslation - Perform write-side record
   * translation.
   */
  bareos_core_functions->registerBareosEvents(
      ctx, 4, bSdEventJobEnd, bSdEventSetupRecordTranslation,
      bSdEventReadRecordTranslation, bSdEventWriteRecordTranslation);

  return bRC_OK;
}

// Free a plugin instance, i.e. release our private storage
static bRC freePlugin(PluginContext* ctx)
{
  int JobId = 0;
  struct plugin_ctx* p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;

  bareos_core_functions->getBareosValue(ctx, bsdVarJobId, (void*)&JobId);
  Dmsg(ctx, debuglevel, "autoxflate-sd: freePlugin JobId=%d\n", JobId);

  if (!p_ctx) {
    Dmsg(ctx, debuglevel, "autoxflate-sd: freePlugin JobId=%d\n", JobId);
    return bRC_Error;
  }

  if (p_ctx) { free(p_ctx); }
  ctx->plugin_private_context = NULL;

  return bRC_OK;
}

// Return some plugin value (none defined)
static bRC getPluginValue(PluginContext* ctx, pVariable var, void* value)
{
  Dmsg(ctx, debuglevel, "autoxflate-sd: getPluginValue var=%d\n", var);

  return bRC_OK;
}

// Set a plugin value (none defined)
static bRC setPluginValue(PluginContext* ctx, pVariable var, void* value)
{
  Dmsg(ctx, debuglevel, "autoxflate-sd: setPluginValue var=%d\n", var);

  return bRC_OK;
}

// Handle an event that was generated in Bareos
static bRC handlePluginEvent(PluginContext* ctx, bSdEvent* event, void* value)
{
  switch (event->eventType) {
    case bSdEventSetupRecordTranslation:
      return setup_record_translation(ctx, value);
    case bSdEventReadRecordTranslation:
      return handle_read_translation(ctx, value);
    case bSdEventWriteRecordTranslation:
      return handle_write_translation(ctx, value);
    case bSdEventJobEnd:
      return handleJobEnd(ctx);
    default:
      Dmsg(ctx, debuglevel, "autoxflate-sd: Unknown event %d\n",
           event->eventType);
      return bRC_Error;
  }

  return bRC_OK;
}

// At end of job report how inflate/deflate ratio was.
static bRC handleJobEnd(PluginContext* ctx)
{
  struct plugin_ctx* p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx) { goto bail_out; }

  if (p_ctx->inflate_bytes_in) {
    Dmsg(ctx, debuglevel, "autoxflate-sd: inflate ratio: %lld/%lld = %0.2f%%\n",
         p_ctx->inflate_bytes_out, p_ctx->inflate_bytes_in,
         (p_ctx->inflate_bytes_out * 100.0 / p_ctx->inflate_bytes_in));
    Jmsg(ctx, M_INFO, _("autoxflate-sd: inflate ratio: %0.2f%%\n"),
         (p_ctx->inflate_bytes_out * 100.0 / p_ctx->inflate_bytes_in));
  }

  if (p_ctx->deflate_bytes_in) {
    Dmsg(ctx, debuglevel,
         "autoxflate-sd: deflate ratio: %lld/%lld =  %0.2f%%\n",
         p_ctx->deflate_bytes_out, p_ctx->deflate_bytes_in,
         (p_ctx->deflate_bytes_out * 100.0 / p_ctx->deflate_bytes_in));
    Jmsg(ctx, M_INFO, _("autoxflate-sd: deflate ratio: %0.2f%%\n"),
         (p_ctx->deflate_bytes_out * 100.0 / p_ctx->deflate_bytes_in));
  }

bail_out:
  return bRC_OK;
}

static bRC setup_record_translation(PluginContext* ctx, void* value)
{
  DeviceControlRecord* dcr;
  bool did_setup = false;
  const char* inflate_in = SETTING_UNSET;
  const char* inflate_out = SETTING_UNSET;
  const char* deflate_in = SETTING_UNSET;
  const char* deflate_out = SETTING_UNSET;

  // Unpack the arguments passed in.
  dcr = (DeviceControlRecord*)value;
  if (!dcr || !dcr->jcr) { return bRC_Error; }

  // When doing NDMP restore we must inflate the data before sending it to the
  // client. Thus, if configured differently, we enforce the required settings.
  if (dcr->jcr->getJobProtocol() == PT_NDMP_BAREOS
      && dcr->jcr->is_JobType(JT_RESTORE)
      && (AutoxflateModeContainsIn(dcr->autodeflate)
          || !AutoxflateModeContainsIn(dcr->autoinflate))) {
    dcr->autoinflate = AutoXflateMode::IO_DIRECTION_IN;
    dcr->autodeflate = AutoXflateMode::IO_DIRECTION_NONE;
    Jmsg(ctx, M_INFO,
         _("autoxflate-sd: overriding settings on %s for NDMP restore\n"),
         dcr->dev_name);
  }

  // Give jobmessage info what is configured
  switch (dcr->autodeflate) {
    case AutoXflateMode::IO_DIRECTION_NONE:
      deflate_in = SETTING_NO;
      deflate_out = SETTING_NO;
      break;
    case AutoXflateMode::IO_DIRECTION_IN:
      deflate_in = SETTING_YES;
      deflate_out = SETTING_NO;
      break;
    case AutoXflateMode::IO_DIRECTION_OUT:
      deflate_in = SETTING_NO;
      deflate_out = SETTING_YES;
      break;
    case AutoXflateMode::IO_DIRECTION_INOUT:
      deflate_in = SETTING_YES;
      deflate_out = SETTING_YES;
      break;
    default:
      Jmsg(ctx, M_ERROR,
           _("autoxflate-sd: Unexpected autodeflate setting on %s"),
           dcr->dev_name);
      break;
  }

  switch (dcr->autoinflate) {
    case AutoXflateMode::IO_DIRECTION_NONE:
      inflate_in = SETTING_NO;
      inflate_out = SETTING_NO;
      break;
    case AutoXflateMode::IO_DIRECTION_IN:
      inflate_in = SETTING_YES;
      inflate_out = SETTING_NO;
      break;
    case AutoXflateMode::IO_DIRECTION_OUT:
      inflate_in = SETTING_NO;
      inflate_out = SETTING_YES;
      break;
    case AutoXflateMode::IO_DIRECTION_INOUT:
      inflate_in = SETTING_YES;
      inflate_out = SETTING_YES;
      break;
    default:
      Jmsg(ctx, M_ERROR,
           _("autoxflate-sd: Unexpected autoinflate setting on %s"),
           dcr->dev_name);
      break;
  }

  if (AutoxflateModeContainsOut(dcr->autodeflate)) {
    if (!SetupAutoDeflation(ctx, dcr)) { return bRC_Error; }
    did_setup = true;
  }

  if (AutoxflateModeContainsIn(dcr->autoinflate)) {
    if (!SetupAutoInflation(ctx, dcr)) { return bRC_Error; }
    did_setup = true;
  }

  if (did_setup) {
    Jmsg(ctx, M_INFO,
         _("autoxflate-sd: %s OUT:[SD->inflate=%s->deflate=%s->DEV] "
           "IN:[DEV->inflate=%s->deflate=%s->SD]\n"),
         dcr->dev_name, inflate_out, deflate_out, inflate_in, deflate_in);
  }

  return bRC_OK;
}

static bRC handle_read_translation(PluginContext* ctx, void* value)
{
  DeviceControlRecord* dcr;
  bool record_was_swapped = false;

  // Unpack the arguments passed in.
  dcr = (DeviceControlRecord*)value;
  if (!dcr) { return bRC_Error; }

  // See if we need to perform auto deflation/inflation of streams.
  if (AutoxflateModeContainsIn(dcr->autoinflate)) {
    record_was_swapped = AutoInflateRecord(ctx, dcr);
  }

  if (!record_was_swapped) {
    if (AutoxflateModeContainsOut(dcr->autodeflate)) {
      record_was_swapped = AutoDeflateRecord(ctx, dcr);
    }
  }

  return bRC_OK;
}

static bRC handle_write_translation(PluginContext* ctx, void* value)
{
  DeviceControlRecord* dcr;
  bool record_was_swapped = false;

  // Unpack the arguments passed in.
  dcr = (DeviceControlRecord*)value;
  if (!dcr) { return bRC_Error; }

  // See if we need to perform auto deflation/inflation of streams.
  if (AutoxflateModeContainsOut(dcr->autoinflate)) {
    record_was_swapped = AutoInflateRecord(ctx, dcr);
  }
  if (!record_was_swapped) {
    if (AutoxflateModeContainsOut(dcr->autodeflate)) {
      record_was_swapped = AutoDeflateRecord(ctx, dcr);
    }
  }
  return bRC_OK;
}

// Setup deflate for auto deflate of data streams.
static bool SetupAutoDeflation(PluginContext* ctx, DeviceControlRecord* dcr)
{
  JobControlRecord* jcr = dcr->jcr;
  bool retval = false;
  uint32_t compress_buf_size = 0;
  const char* compressorname = COMPRESSOR_NAME_UNSET;

  if (jcr->buf_size == 0) { jcr->buf_size = DEFAULT_NETWORK_BUFFER_SIZE; }

  if (!SetupCompressionBuffers(jcr, sd_enabled_compatible,
                               dcr->device_resource->autodeflate_algorithm,
                               &compress_buf_size)) {
    goto bail_out;
  }

  // See if we need to create a new compression buffer or make sure the
  // existing is big enough.
  if (!jcr->compress.deflate_buffer) {
    jcr->compress.deflate_buffer = GetMemory(compress_buf_size);
    jcr->compress.deflate_buffer_size = compress_buf_size;
  } else {
    if (compress_buf_size > jcr->compress.deflate_buffer_size) {
      jcr->compress.deflate_buffer
          = ReallocPoolMemory(jcr->compress.deflate_buffer, compress_buf_size);
      jcr->compress.deflate_buffer_size = compress_buf_size;
    }
  }

  switch (dcr->device_resource->autodeflate_algorithm) {
#if defined(HAVE_LIBZ)
    case COMPRESS_GZIP: {
      compressorname = COMPRESSOR_NAME_GZIP;
      int zstat;
      z_stream* pZlibStream;

      pZlibStream = (z_stream*)jcr->compress.workset.pZLIB;
      if ((zstat
           = deflateParams(pZlibStream, dcr->device_resource->autodeflate_level,
                           Z_DEFAULT_STRATEGY))
          != Z_OK) {
        Jmsg(ctx, M_FATAL,
             _("autoxflate-sd: Compression deflateParams error: %d\n"), zstat);
        jcr->setJobStatus(JS_ErrorTerminated);
        goto bail_out;
      }
      break;
    }
#endif
#if defined(HAVE_LZO)
    case COMPRESS_LZO1X:
      compressorname = COMPRESSOR_NAME_LZO;
      break;
#endif
    case COMPRESS_FZFZ:
      compressorname = COMPRESSOR_NAME_FZLZ;
      [[fallthrough]];
    case COMPRESS_FZ4L:
      compressorname = COMPRESSOR_NAME_FZ4L;
      [[fallthrough]];
    case COMPRESS_FZ4H: {
      compressorname = COMPRESSOR_NAME_FZ4H;
      int zstat;
      zfast_stream* pZfastStream;
      zfast_stream_compressor compressor = COMPRESSOR_FASTLZ;

      switch (dcr->device_resource->autodeflate_algorithm) {
        case COMPRESS_FZ4L:
        case COMPRESS_FZ4H:
          compressor = COMPRESSOR_LZ4;
          break;
      }

      pZfastStream = (zfast_stream*)jcr->compress.workset.pZFAST;
      if ((zstat = fastlzlibSetCompressor(pZfastStream, compressor)) != Z_OK) {
        Jmsg(ctx, M_FATAL,
             _("autoxflate-sd: Compression fastlzlibSetCompressor error: "
               "%d\n"),
             zstat);
        jcr->setJobStatus(JS_ErrorTerminated);
        goto bail_out;
      }
      break;
    }
    default:
      break;
  }

  Jmsg(ctx, M_INFO, _("autoxflate-sd: Compressor on device %s is %s\n"),
       dcr->dev_name, compressorname);
  retval = true;

bail_out:
  return retval;
}

// Setup inflation for auto inflation of data streams.
static bool SetupAutoInflation(PluginContext* ctx, DeviceControlRecord* dcr)
{
  JobControlRecord* jcr = dcr->jcr;
  uint32_t decompress_buf_size;

  if (jcr->buf_size == 0) { jcr->buf_size = DEFAULT_NETWORK_BUFFER_SIZE; }

  SetupDecompressionBuffers(jcr, &decompress_buf_size);
  if (decompress_buf_size > 0) {
    // See if we need to create a new compression buffer or make sure the
    // existing is big enough.
    if (!jcr->compress.inflate_buffer) {
      jcr->compress.inflate_buffer = GetMemory(decompress_buf_size);
      jcr->compress.inflate_buffer_size = decompress_buf_size;
    } else {
      if (decompress_buf_size > jcr->compress.inflate_buffer_size) {
        jcr->compress.inflate_buffer = ReallocPoolMemory(
            jcr->compress.inflate_buffer, decompress_buf_size);
        jcr->compress.inflate_buffer_size = decompress_buf_size;
      }
    }
  } else {
    return false;
  }

  return true;
}

/**
 * Perform automatic compression of certain stream types when enabled in the
 * config.
 */
static bool AutoDeflateRecord(PluginContext* ctx, DeviceControlRecord* dcr)
{
  ser_declare;
  bool retval = false;
  comp_stream_header ch;
  DeviceRecord *rec, *nrec;
  struct plugin_ctx* p_ctx;
  unsigned char* data = NULL;
  bool intermediate_value = false;
  unsigned int max_compression_length = 0;

  p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;
  if (!p_ctx) { goto bail_out; }

  // When dcr->after_rec is set we already have a translated record by another
  // SD plugin which we need to translate and to free after successful
  // translation.
  // Otherwise we start from dcr->before_rec.
  if (dcr->after_rec) {
    rec = dcr->after_rec;
    intermediate_value = true;
  } else {
    rec = dcr->before_rec;
  }

  if (!IsUncompressedStreamWeHandle(rec->maskedStream)) { goto bail_out; }

  // Clone the data from the original DeviceRecord to the converted one.
  nrec = bareos_core_functions->new_record(/* with_data = */ false);
  bareos_core_functions->CopyRecordState(nrec, rec);

  // Setup the converted DeviceRecord to point with its data buffer to the
  // compression buffer.
  nrec->data = dcr->jcr->compress.deflate_buffer;
  switch (rec->maskedStream) {
    case STREAM_FILE_DATA:
    case STREAM_WIN32_DATA:
      data = (unsigned char*)nrec->data + sizeof(comp_stream_header);
      max_compression_length
          = dcr->jcr->compress.deflate_buffer_size - sizeof(comp_stream_header);
      break;
    case STREAM_SPARSE_DATA:
      data = (unsigned char*)nrec->data + OFFSET_FADDR_SIZE
             + sizeof(comp_stream_header);
      max_compression_length = dcr->jcr->compress.deflate_buffer_size
                               - OFFSET_FADDR_SIZE - sizeof(comp_stream_header);
      break;
  }

  // Compress the data using the configured compression algorithm.
  if (!CompressData(dcr->jcr, dcr->device_resource->autodeflate_algorithm,
                    rec->data, rec->data_len, data, max_compression_length,
                    &nrec->data_len)) {
    bareos_core_functions->FreeRecord(nrec);
    goto bail_out;
  }

  // Map the streams.
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

  // Generate a compression header.
  ch.magic = dcr->device_resource->autodeflate_algorithm;
  ch.level = dcr->device_resource->autodeflate_level;
  ch.version = COMP_HEAD_VERSION;
  ch.size = nrec->data_len;

  switch (nrec->maskedStream) {
    case STREAM_COMPRESSED_DATA:
    case STREAM_WIN32_COMPRESSED_DATA:
      SerBegin(nrec->data, sizeof(comp_stream_header));
      ser_uint32(ch.magic);
      ser_uint32(ch.size);
      ser_uint16(ch.level);
      ser_uint16(ch.version);
      SerEnd(nrec->data, sizeof(comp_stream_header));
      nrec->data_len += sizeof(comp_stream_header);
      break;
    case STREAM_SPARSE_COMPRESSED_DATA:
      // Copy the sparse offset from the original.
      memcpy(nrec->data, rec->data, OFFSET_FADDR_SIZE);
      SerBegin(nrec->data + OFFSET_FADDR_SIZE, sizeof(comp_stream_header));
      ser_uint32(ch.magic);
      ser_uint32(ch.size);
      ser_uint16(ch.level);
      ser_uint16(ch.version);
      SerEnd(nrec->data + OFFSET_FADDR_SIZE, sizeof(comp_stream_header));
      nrec->data_len += OFFSET_FADDR_SIZE + sizeof(comp_stream_header);
      break;
  }

  Dmsg(ctx, 400,
       "AutoDeflateRecord: From datastream %d to %d from original size %ld to "
       "%ld\n",
       rec->maskedStream, nrec->maskedStream, rec->data_len, nrec->data_len);

  p_ctx->deflate_bytes_in += rec->data_len;
  p_ctx->deflate_bytes_out += nrec->data_len;

  // If the input is just an intermediate value free it now.
  if (intermediate_value) { bareos_core_functions->FreeRecord(dcr->after_rec); }
  dcr->after_rec = nrec;
  retval = true;

bail_out:
  return retval;
}

// Inflate (uncompress) the content of a read record and return the data as an
// alternative datastream.
static bool AutoInflateRecord(PluginContext* ctx, DeviceControlRecord* dcr)
{
  DeviceRecord *rec, *nrec;
  bool retval = false;
  struct plugin_ctx* p_ctx;
  bool intermediate_value = false;

  p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;
  if (!p_ctx) { goto bail_out; }

  // When dcr->after_rec is set we already have a translated record by another
  // SD plugin which we need to translate and to free after successful
  // translation.
  // Otherwise we start from dcr->before_rec.
  if (dcr->after_rec) {
    rec = dcr->after_rec;
    intermediate_value = true;
  } else {
    rec = dcr->before_rec;
  }
  if (!IsCompressedStreamWeHandle(rec->maskedStream)) { goto bail_out; }

  // Clone the data from the original DeviceRecord to the converted one.
  nrec = bareos_core_functions->new_record(/* with_data = */ false);
  bareos_core_functions->CopyRecordState(nrec, rec);

  // Setup the converted record to point to the original data. The
  // DecompressData function will decompress the data in the
  // compression buffer and set the length of the decompressed data.
  nrec->data = rec->data;
  nrec->data_len = rec->data_len;

  if (!DecompressData(dcr->jcr, "Unknown", rec->maskedStream, &nrec->data,
                      &nrec->data_len, true)) {
    bareos_core_functions->FreeRecord(nrec);
    goto bail_out;
  }

  // If we succeeded in decompressing the data update the stream type.
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

  Dmsg(ctx, 400,
       "AutoInflateRecord: From datastream %d to %d from original size %ld to "
       "%ld\n",
       rec->maskedStream, nrec->maskedStream, rec->data_len, nrec->data_len);

  p_ctx->inflate_bytes_in += rec->data_len;
  p_ctx->inflate_bytes_out += nrec->data_len;

  // If the input is just an intermediate value free it now.
  if (intermediate_value) { bareos_core_functions->FreeRecord(dcr->after_rec); }
  dcr->after_rec = nrec;
  retval = true;

bail_out:
  return retval;
}
