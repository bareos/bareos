/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "include/jcr.h"
#include "include/ch.h"
#include "include/streams.h"
#include "lib/edit.h"
#include "lib/serial.h"
#include "lib/compression.h"

#include <zlib.h>

#ifdef HAVE_LZO
#  include <lzo/lzoconf.h>
#  include <lzo/lzo1x.h>
#endif

#include "fastlz/fastlzlib.h"

static const std::string kCompressorNameUnknown = "Unknown";
static const std::string kCompressorNameGZIP = "GZIP";
static const std::string kCompressorNameLZO = "LZO";
static const std::string kCompressorNameFZLZ = "FASTLZ";
static const std::string kCompressorNameFZ4L = "LZ4";
static const std::string kCompressorNameFZ4H = "LZ4HC";
const std::string& CompressorName(uint32_t compression_algorithm)
{
  switch (compression_algorithm) {
    case COMPRESS_GZIP:
      return kCompressorNameGZIP;
    case COMPRESS_LZO1X:
      return kCompressorNameLZO;
    case COMPRESS_FZFZ:
      return kCompressorNameFZLZ;
    case COMPRESS_FZ4L:
      return kCompressorNameFZ4L;
    case COMPRESS_FZ4H:
      return kCompressorNameFZ4H;
    default:
      return kCompressorNameUnknown;
  }
}

// Convert ZLIB error code into an ASCII message
// These are also used by fastlzlib, so this always needs to be
// available
static const char* zlib_strerror(int stat)
{
  if (stat >= 0) { return T_("None"); }
  switch (stat) {
    case Z_ERRNO:
      return T_("Zlib errno");
    case Z_STREAM_ERROR:
      return T_("Zlib stream error");
    case Z_DATA_ERROR:
      return T_("Zlib data error");
    case Z_MEM_ERROR:
      return T_("Zlib memory error");
    case Z_BUF_ERROR:
      return T_("Zlib buffer error");
    case Z_VERSION_ERROR:
      return T_("Zlib version error");
    default:
      return T_("*None*");
  }
}

static inline void UnknownCompressionAlgorithm(JobControlRecord* jcr,
                                               uint32_t compression_algorithm)
{
  Jmsg(jcr, M_FATAL, 0, T_("%s compression not supported on this platform\n"),
       CompressorName(compression_algorithm).c_str());
}

std::size_t RequiredCompressionOutputBufferSize(uint32_t algo,
                                                std::size_t max_input_size)
{
  switch (algo) {
    case COMPRESS_GZIP: {
      return compressBound(max_input_size) + 18 + sizeof(comp_stream_header);
    }
#ifdef HAVE_LZO
    case COMPRESS_LZO1X:
      return max_input_size + (max_input_size / 16) + 64 + 3
             + sizeof(comp_stream_header);
      break;
#endif
    case COMPRESS_FZFZ:
    case COMPRESS_FZ4L:
    case COMPRESS_FZ4H:
      return max_input_size + (max_input_size / 10 + 16 * 2)
             + sizeof(comp_stream_header);
      break;
  }

  return max_input_size + sizeof(comp_stream_header);
}

class z4_compressor {
  zfast_stream pZfastStream{};
  bool init_error{false};
  std::optional<PoolMem> error{};

 public:
  z4_compressor(int type, zfast_stream_compressor compressor)
  {
    pZfastStream.zalloc = Z_NULL;
    pZfastStream.zfree = Z_NULL;
    pZfastStream.opaque = Z_NULL;
    pZfastStream.state = Z_NULL;

    if (auto zstat = fastlzlibCompressInit(&pZfastStream, type);
        zstat != Z_OK) {
      init_error = true;
      error.emplace("Failed to initialize FASTLZ compression");
    }

    if (auto zstat = fastlzlibSetCompressor(&pZfastStream, compressor);
        zstat != Z_OK) {
      error.emplace("Failed to set FASTLZ compressor");
    }
  }

  result<std::size_t> compress(char const* input,
                               std::size_t size,
                               char* output,
                               std::size_t capacity)
  {
    if (error) { return PoolMem{error->c_str()}; }

    pZfastStream.next_in = (Bytef*)input;
    pZfastStream.avail_in = size;
    pZfastStream.next_out = (Bytef*)output;
    pZfastStream.avail_out = capacity;

    if (auto zstat = fastlzlibCompress(&pZfastStream, Z_FINISH);
        zstat != Z_STREAM_END) {
      PoolMem errmsg;
      Mmsg(errmsg, "Compression fastlzlibCompress error: %d\n", zstat);
      return errmsg;
    }

    std::size_t out = pZfastStream.total_out;
    if (fastlzlibCompressReset(&pZfastStream)) {
      error.emplace("Failed to reset fastzlib");
    }

    ASSERT(out <= capacity);
    return out;
  }

  ~z4_compressor()
  {
    if (!init_error && fastlzlibCompressEnd(&pZfastStream) != Z_OK) {
      Dmsg0(100, "Could not properly destroy compress stream.\n");
    }
  }
};

class gzip_compressor {
  z_stream zlibStream{};
  bool init_error{false};
  std::optional<PoolMem> error{};

 public:
  gzip_compressor()
  {
    zlibStream.zalloc = Z_NULL;
    zlibStream.zfree = Z_NULL;
    zlibStream.opaque = Z_NULL;
    zlibStream.state = Z_NULL;

    if (deflateInit(&zlibStream, Z_DEFAULT_COMPRESSION) != Z_OK) {
      init_error = true;
      error.emplace("Failed to initialize zlib.");
    }
  }

  bool set_level(int level)
  {
    if (error) return false;

    if (auto zstat = deflateParams(&zlibStream, level, Z_DEFAULT_STRATEGY);
        zstat != Z_OK) {
      error.emplace("Failed to set zlib params.");
    }

    return !error;
  }

  result<std::size_t> compress(char const* input,
                               std::size_t size,
                               char* output,
                               std::size_t capacity)
  {
    zlibStream.next_in = (Bytef*)input;
    zlibStream.avail_in = size;
    zlibStream.next_out = (Bytef*)output;
    zlibStream.avail_out = capacity;

    if (auto zstat = deflate(&zlibStream, Z_FINISH); zstat != Z_STREAM_END) {
      Mmsg(error.emplace(), "Compression deflate error: %d\n", zstat);
      return PoolMem{error->c_str()};
    }

    std::size_t compress_len = zlibStream.total_out;

    // Reset zlib stream to be able to begin from scratch again
    if (auto zstat = deflateReset(&zlibStream); zstat != Z_OK) {
      Mmsg(error.emplace(), "Compression deflateReset error: %d\n", zstat);
      return PoolMem{error->c_str()};
    }

    Dmsg2(400, "GZIP compressed len=%" PRIuz " uncompressed len=%" PRIuz "\n",
          compress_len, size);

    return compress_len;
  }

  ~gzip_compressor()
  {
    if (!init_error) { deflateEnd(&zlibStream); }
  }
};

#ifdef HAVE_LZO
class lzo_compressor {
  lzo_voidp lzoMem{nullptr};
  bool init_error{false};
  std::optional<PoolMem> error{};

 public:
  lzo_compressor()
  {
    if (lzo_init() != LZO_E_OK) {
      init_error = true;
      error.emplace("Failed to init lzo.");
    } else {
      lzoMem = static_cast<lzo_voidp>(malloc(LZO1X_1_MEM_COMPRESS));
    }
  }

  result<std::size_t> compress(char const* input,
                               std::size_t size,
                               char* output,
                               std::size_t capacity)
  {
    if (error) return PoolMem{error->c_str()};
    memset(lzoMem, 0, LZO1X_1_MEM_COMPRESS);
    lzo_uint len = 0;

    Dmsg3(400, "cbuf=%p rbuf=%p len=%" PRIuz "\n", output, input, size);

    int lzores = lzo1x_1_compress(
        reinterpret_cast<const unsigned char*>(input), size,
        reinterpret_cast<unsigned char*>(output), &len, lzoMem);

    if (lzores != LZO_E_OK || len > capacity) {
      // This should NEVER happen
      Mmsg(error.emplace(), "Compression LZO error: %d\n", lzores);
      return PoolMem{error->c_str()};
    }

    Dmsg2(400, "LZO compressed len=%llu uncompressed len=%" PRIuz "\n",
          static_cast<long long unsigned>(len), size);

    return len;
  }

  ~lzo_compressor()
  {
    if (!init_error) { free(lzoMem); }
  }
};
#endif

struct compressors {
  std::unique_ptr<gzip_compressor> gzip{nullptr};
#ifdef HAVE_LZO
  std::unique_ptr<lzo_compressor> lzo{nullptr};
#endif
  std::unique_ptr<z4_compressor> lz_fast{nullptr};
  std::unique_ptr<z4_compressor> lz_default{nullptr};
  std::unique_ptr<z4_compressor> lz_best{nullptr};
};

template <typename T> struct tls_manager {
  pthread_key_t key;

  tls_manager()
  {
    pthread_key_create(
        &key, +[](void* arg) {
          /* destructor is only called if arg != NULL */
          auto* val = reinterpret_cast<T*>(arg);
          delete val;
        });
  }
  ~tls_manager() { pthread_key_delete(key); }
  T* thread_local_value()
  {
    auto* local = reinterpret_cast<T*>(pthread_getspecific(key));
    if (!local) {
      local = new T{};
      ASSERT(pthread_setspecific(key, local) == 0);
    }
    return local;
  }
};

result<std::size_t> ThreadlocalCompress(uint32_t algo,
                                        uint32_t level,
                                        char const* input,
                                        std::size_t size,
                                        char* output,
                                        std::size_t capacity)
{
  /* NOTE: this is done because thread_local is broken on the crosscompiler.
   * it may free the underlying thread local storage before destroying the
   * objects therein. */
  static tls_manager<compressors> manager;

  auto* comps = manager.thread_local_value();
  switch (algo) {
    case COMPRESS_GZIP: {
      if (!comps->gzip) comps->gzip.reset(new gzip_compressor);
      comps->gzip->set_level(level);
      return comps->gzip->compress(input, size, output, capacity);
    } break;
#ifdef HAVE_LZO
    case COMPRESS_LZO1X: {
      if (!comps->lzo) comps->lzo.reset(new lzo_compressor);
      return comps->lzo->compress(input, size, output, capacity);
    } break;
#endif
    case COMPRESS_FZFZ: {
      if (!comps->lz_fast)
        comps->lz_fast.reset(
            new z4_compressor{Z_BEST_SPEED, COMPRESSOR_FASTLZ});
      return comps->lz_fast->compress(input, size, output, capacity);
    }
    case COMPRESS_FZ4L: {
      if (!comps->lz_default)
        comps->lz_default.reset(
            new z4_compressor{Z_BEST_SPEED, COMPRESSOR_LZ4});
      return comps->lz_default->compress(input, size, output, capacity);
    }
    case COMPRESS_FZ4H: {
      if (!comps->lz_best)
        comps->lz_best.reset(
            new z4_compressor{Z_BEST_COMPRESSION, COMPRESSOR_LZ4});
      return comps->lz_best->compress(input, size, output, capacity);
    } break;
  }

  PoolMem errmsg;
  Mmsg(errmsg, "Unknown compression algorithm: %d", algo);
  return errmsg;
}

bool SetupCompressionBuffers(JobControlRecord* jcr,
                             uint32_t compression_algorithm,
                             uint32_t* compress_buf_size)
{
  uint32_t wanted_compress_buf_size;

  switch (compression_algorithm) {
    case 0:
      // No compression requested.
      break;
    case COMPRESS_GZIP: {
      z_stream* pZlibStream;

      /* Use compressBound() to get an idea what zlib thinks
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
       * was successful. */
      wanted_compress_buf_size
          = compressBound(jcr->buf_size) + 18 + (int)sizeof(comp_stream_header);
      if (wanted_compress_buf_size > *compress_buf_size) {
        *compress_buf_size = wanted_compress_buf_size;
      }

      // See if this compression algorithm is already setup.
      if (jcr->compress.workset.pZLIB) { return true; }

      pZlibStream = (z_stream*)malloc(sizeof(z_stream));
      memset(pZlibStream, 0, sizeof(z_stream));
      pZlibStream->zalloc = Z_NULL;
      pZlibStream->zfree = Z_NULL;
      pZlibStream->opaque = Z_NULL;
      pZlibStream->state = Z_NULL;

      if (deflateInit(pZlibStream, Z_DEFAULT_COMPRESSION) == Z_OK) {
        jcr->compress.workset.pZLIB = pZlibStream;
      } else {
        Jmsg(jcr, M_FATAL, 0, T_("Failed to initialize ZLIB compression\n"));
        free(pZlibStream);
        return false;
      }
      break;
    }
#ifdef HAVE_LZO
    case COMPRESS_LZO1X: {
      lzo_voidp pLzoMem;

      /* For LZO1X compression the recommended value is:
       *    output_block_size = input_block_size + (input_block_size / 16) + 64
       * + 3 + sizeof(comp_stream_header)
       *
       * The LZO compression workset is initialized here to minimize
       * the "per file" load. The jcr member is only set, if the init
       * was successful. */
      wanted_compress_buf_size = jcr->buf_size + (jcr->buf_size / 16) + 64 + 3
                                 + (int)sizeof(comp_stream_header);
      if (wanted_compress_buf_size > *compress_buf_size) {
        *compress_buf_size = wanted_compress_buf_size;
      }

      // See if this compression algorithm is already setup.
      if (jcr->compress.workset.pLZO) { return true; }

      pLzoMem = (lzo_voidp)malloc(LZO1X_1_MEM_COMPRESS);
      memset(pLzoMem, 0, LZO1X_1_MEM_COMPRESS);

      if (lzo_init() == LZO_E_OK) {
        jcr->compress.workset.pLZO = pLzoMem;
      } else {
        Jmsg(jcr, M_FATAL, 0, T_("Failed to initialize LZO compression\n"));
        free(pLzoMem);
        return false;
      }
      break;
    }
#endif
    case COMPRESS_FZFZ:
    case COMPRESS_FZ4L:
    case COMPRESS_FZ4H: {
      int level, zstat;
      zfast_stream* pZfastStream;

      if (compression_algorithm == COMPRESS_FZ4H) {
        level = Z_BEST_COMPRESSION;
      } else {
        level = Z_BEST_SPEED;
      }

      /* For FASTLZ compression the recommended value is:
       *    output_block_size = input_block_size + (input_block_size / 10 + 16 *
       * 2) + sizeof(comp_stream_header)
       *
       * The FASTLZ compression workset is initialized here to minimize
       * the "per file" load. The jcr member is only set, if the init
       * was successful. */
      wanted_compress_buf_size = jcr->buf_size + (jcr->buf_size / 10 + 16 * 2)
                                 + (int)sizeof(comp_stream_header);
      if (wanted_compress_buf_size > *compress_buf_size) {
        *compress_buf_size = wanted_compress_buf_size;
      }

      // See if this compression algorithm is already setup.
      if (jcr->compress.workset.pZFAST) { return true; }

      pZfastStream = (zfast_stream*)malloc(sizeof(zfast_stream));
      memset(pZfastStream, 0, sizeof(zfast_stream));
      pZfastStream->zalloc = Z_NULL;
      pZfastStream->zfree = Z_NULL;
      pZfastStream->opaque = Z_NULL;
      pZfastStream->state = Z_NULL;

      if ((zstat = fastlzlibCompressInit(pZfastStream, level)) == Z_OK) {
        jcr->compress.workset.pZFAST = pZfastStream;
      } else {
        Jmsg(jcr, M_FATAL, 0, T_("Failed to initialize FASTLZ compression\n"));
        free(pZfastStream);
        return false;
      }
      break;
    }
    default:
      UnknownCompressionAlgorithm(jcr, compression_algorithm);
      return false;
  }

  return true;
}

bool SetupSpecificCompressionContext(JobControlRecord& jcr,
                                     uint32_t algo,
                                     uint32_t compression_level)
{
  if (algo == COMPRESS_GZIP) {
    auto* pZlibStream = reinterpret_cast<z_stream*>(jcr.compress.workset.pZLIB);
    int zstatus
        = deflateParams(pZlibStream, compression_level, Z_DEFAULT_STRATEGY);
    if (pZlibStream->total_in == 0) {
      if (zstatus != Z_OK) {
        Jmsg(&jcr, M_FATAL, 0, T_("Compression deflateParams error: %d\n"),
             zstatus);
        jcr.setJobStatusWithPriorityCheck(JS_ErrorTerminated);
        return false;
      }
    } else {
      Jmsg(&jcr, M_FATAL, 0,
           T_("Cannot set up compression context while the buffer still "
              "contains data."));
      return false;
    }
  }
  if (algo == COMPRESS_FZFZ || algo == COMPRESS_FZ4L || algo == COMPRESS_FZ4H) {
    zfast_stream_compressor compressor = COMPRESSOR_DEFAULT;
    switch (algo) {
      case COMPRESS_FZFZ:
        compressor = COMPRESSOR_FASTLZ;
        break;
      case COMPRESS_FZ4L:
        compressor = COMPRESSOR_LZ4;
        break;
      case COMPRESS_FZ4H:
        compressor = COMPRESSOR_LZ4;
        break;
      default:
        break;
    }

    auto* pZFastStream
        = reinterpret_cast<zfast_stream*>(jcr.compress.workset.pZFAST);
    if (pZFastStream->total_in == 0) {
      int zstat = fastlzlibSetCompressor(pZFastStream, compressor);
      if (zstat != Z_OK) {
        Jmsg(&jcr, M_FATAL, 0,
             T_("Compression fastlzlibSetCompressor error: %d\n"), zstat);
        jcr.setJobStatusWithPriorityCheck(JS_ErrorTerminated);
        return false;
      }
    } else {
      Jmsg(&jcr, M_FATAL, 0,
           T_("Cannot set up compression context while the buffer still "
              "contains data."));
      return false;
    }
  }
  return true;
}

bool SetupDecompressionBuffers(JobControlRecord* jcr,
                               uint32_t* decompress_buf_size)
{
  uint32_t compress_buf_size;

  // Use the same buffer size to decompress all data.
  compress_buf_size = jcr->buf_size;
  if (compress_buf_size < DEFAULT_NETWORK_BUFFER_SIZE) {
    compress_buf_size = DEFAULT_NETWORK_BUFFER_SIZE;
  }
  *decompress_buf_size
      = compress_buf_size + 12 + ((compress_buf_size + 999) / 1000) + 100;

#ifdef HAVE_LZO
  if (!jcr->compress.inflate_buffer && lzo_init() != LZO_E_OK) {
    Jmsg(jcr, M_FATAL, 0, T_("LZO init failed\n"));
    return false;
  }
#endif

  return true;
}

static bool compress_with_zlib(JobControlRecord* jcr,
                               char* rbuf,
                               uint32_t rsize,
                               unsigned char* cbuf,
                               uint32_t max_compress_len,
                               uint32_t* compress_len)
{
  int zstat;
  z_stream* pZlibStream;

  Dmsg3(400, "cbuf=%p rbuf=%p len=%" PRIu32 "\n", cbuf, rbuf, rsize);

  pZlibStream = (z_stream*)jcr->compress.workset.pZLIB;
  pZlibStream->next_in = (Bytef*)rbuf;
  pZlibStream->avail_in = rsize;
  pZlibStream->next_out = (Bytef*)cbuf;
  pZlibStream->avail_out = max_compress_len;

  if ((zstat = deflate(pZlibStream, Z_FINISH)) != Z_STREAM_END) {
    Jmsg(jcr, M_FATAL, 0, T_("Compression deflate error: %d\n"), zstat);
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    return false;
  }

  *compress_len = pZlibStream->total_out;

  // Reset zlib stream to be able to begin from scratch again
  if ((zstat = deflateReset(pZlibStream)) != Z_OK) {
    Jmsg(jcr, M_FATAL, 0, T_("Compression deflateReset error: %d\n"), zstat);
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    return false;
  }

  Dmsg2(400, "GZIP compressed len=%d uncompressed len=%d\n", *compress_len,
        rsize);

  return true;
}

#ifdef HAVE_LZO
static bool compress_with_lzo(JobControlRecord* jcr,
                              char* rbuf,
                              uint32_t rsize,
                              unsigned char* cbuf,
                              uint32_t max_compress_len,
                              uint32_t* compress_len)
{
  int lzores;
  lzo_uint len = 0;

  Dmsg3(400, "cbuf=%p rbuf=%p len=%" PRIu32 "\n", cbuf, rbuf, rsize);

  lzores = lzo1x_1_compress((const unsigned char*)rbuf, rsize, cbuf, &len,
                            jcr->compress.workset.pLZO);
  *compress_len = len;

  if (lzores != LZO_E_OK || *compress_len > max_compress_len) {
    // This should NEVER happen
    Jmsg(jcr, M_FATAL, 0, T_("Compression LZO error: %d\n"), lzores);
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    return false;
  }

  Dmsg2(400, "LZO compressed len=%d uncompressed len=%d\n", *compress_len,
        rsize);

  return true;
}
#endif

static bool compress_with_fastlz(JobControlRecord* jcr,
                                 char* rbuf,
                                 uint32_t rsize,
                                 unsigned char* cbuf,
                                 uint32_t max_compress_len,
                                 uint32_t* compress_len)
{
  int zstat;
  zfast_stream* pZfastStream;

  Dmsg3(400, "cbuf=%p rbuf=%p len=%" PRIu32 "\n", cbuf, rbuf, rsize);

  pZfastStream = (zfast_stream*)jcr->compress.workset.pZFAST;
  pZfastStream->next_in = (Bytef*)rbuf;
  pZfastStream->avail_in = rsize;
  pZfastStream->next_out = (Bytef*)cbuf;
  pZfastStream->avail_out = max_compress_len;

  if ((zstat = fastlzlibCompress(pZfastStream, Z_FINISH)) != Z_STREAM_END) {
    Jmsg(jcr, M_FATAL, 0, T_("Compression fastlzlibCompress error: %d\n"),
         zstat);
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    return false;
  }

  *compress_len = pZfastStream->total_out;

  // Reset fastlz stream to be able to begin from scratch again
  if ((zstat = fastlzlibCompressReset(pZfastStream)) != Z_OK) {
    Jmsg(jcr, M_FATAL, 0, T_("Compression fastlzlibCompressReset error: %d\n"),
         zstat);
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    return false;
  }

  Dmsg2(400, "FASTLZ compressed len=%d uncompressed len=%d\n", *compress_len,
        rsize);

  return true;
}

bool CompressData(JobControlRecord* jcr,
                  uint32_t compression_algorithm,
                  char* rbuf,
                  uint32_t rsize,
                  unsigned char* cbuf,
                  uint32_t max_compress_len,
                  uint32_t* compress_len)
{
  *compress_len = 0;
  switch (compression_algorithm) {
    case COMPRESS_GZIP:
      if (jcr->compress.workset.pZLIB) {
        if (!compress_with_zlib(jcr, rbuf, rsize, cbuf, max_compress_len,
                                compress_len)) {
          return false;
        }
      }
      break;
#ifdef HAVE_LZO
    case COMPRESS_LZO1X:
      if (jcr->compress.workset.pLZO) {
        if (!compress_with_lzo(jcr, rbuf, rsize, cbuf, max_compress_len,
                               compress_len)) {
          return false;
        }
      }
      break;
#endif
    case COMPRESS_FZFZ:
    case COMPRESS_FZ4L:
    case COMPRESS_FZ4H:
      if (jcr->compress.workset.pZFAST) {
        if (!compress_with_fastlz(jcr, rbuf, rsize, cbuf, max_compress_len,
                                  compress_len)) {
          return false;
        }
      }
      break;
    default:
      break;
  }

  return true;
}

static bool decompress_with_zlib(JobControlRecord* jcr,
                                 const char* last_fname,
                                 char** data,
                                 uint32_t* length,
                                 bool sparse,
                                 bool with_header,
                                 bool want_data_stream)
{
  uLong compress_len;
  const unsigned char* cbuf;
  char* wbuf;
  int status, real_compress_len;

  /* NOTE! We only use uLong and Byte because they are
   * needed by the zlib routines, they should not otherwise
   * be used in Bareos. */
  if (sparse && want_data_stream) {
    wbuf = jcr->compress.inflate_buffer + OFFSET_FADDR_SIZE;
    compress_len = jcr->compress.inflate_buffer_size - OFFSET_FADDR_SIZE;
  } else {
    wbuf = jcr->compress.inflate_buffer;
    compress_len = jcr->compress.inflate_buffer_size;
  }

  // See if this is a compressed stream with the new compression header or an
  // old one.
  if (with_header) {
    cbuf = (const unsigned char*)*data + sizeof(comp_stream_header);
    real_compress_len = *length - sizeof(comp_stream_header);
  } else {
    cbuf = (const unsigned char*)*data;
    real_compress_len = *length;
  }

  Dmsg2(400, "Comp_len=%llu message_length=%d\n",
        static_cast<long long unsigned>(compress_len), *length);

  while ((status = uncompress((Byte*)wbuf, &compress_len, (const Byte*)cbuf,
                              (uLong)real_compress_len))
         == Z_BUF_ERROR) {
    // The buffer size is too small, try with a bigger one
    jcr->compress.inflate_buffer_size
        = jcr->compress.inflate_buffer_size
          + (jcr->compress.inflate_buffer_size >> 1);
    jcr->compress.inflate_buffer = CheckPoolMemorySize(
        jcr->compress.inflate_buffer, jcr->compress.inflate_buffer_size);

    if (sparse && want_data_stream) {
      wbuf = jcr->compress.inflate_buffer + OFFSET_FADDR_SIZE;
      compress_len = jcr->compress.inflate_buffer_size - OFFSET_FADDR_SIZE;
    } else {
      wbuf = jcr->compress.inflate_buffer;
      compress_len = jcr->compress.inflate_buffer_size;
    }
    Dmsg2(400, "Comp_len=%llu message_length=%d\n",
          static_cast<long long unsigned>(compress_len), *length);
  }

  if (status != Z_OK) {
    Qmsg(jcr, M_ERROR, 0, T_("Uncompression error on file %s. ERR=%s\n"),
         last_fname, zlib_strerror(status));
    return false;
  }

  /* We return a decompressed data stream with the fileoffset encoded when this
   * was a sparse stream. */
  if (sparse && want_data_stream) {
    memcpy(jcr->compress.inflate_buffer, *data, OFFSET_FADDR_SIZE);
  }

  *data = jcr->compress.inflate_buffer;
  *length = compress_len;

  Dmsg2(400, "Write uncompressed %llu bytes, total before write=%" PRIu64 "\n",
        static_cast<long long unsigned>(compress_len), jcr->JobBytes);

  return true;
}
#ifdef HAVE_LZO
static bool decompress_with_lzo(JobControlRecord* jcr,
                                const char* last_fname,
                                char** data,
                                uint32_t* length,
                                bool sparse,
                                bool want_data_stream)
{
  lzo_uint compress_len;
  const unsigned char* cbuf;
  unsigned char* wbuf;
  int status, real_compress_len;

  if (sparse && want_data_stream) {
    compress_len = jcr->compress.inflate_buffer_size - OFFSET_FADDR_SIZE;
    cbuf = (const unsigned char*)*data + OFFSET_FADDR_SIZE
           + sizeof(comp_stream_header);
    wbuf = (unsigned char*)jcr->compress.inflate_buffer + OFFSET_FADDR_SIZE;
  } else {
    compress_len = jcr->compress.inflate_buffer_size;
    cbuf = (const unsigned char*)*data + sizeof(comp_stream_header);
    wbuf = (unsigned char*)jcr->compress.inflate_buffer;
  }

  real_compress_len = *length - sizeof(comp_stream_header);
  Dmsg2(400, "Comp_len=%llu message_length=%" PRIu32 "\n",
        static_cast<long long unsigned>(compress_len), *length);
  while ((status = lzo1x_decompress_safe(cbuf, real_compress_len, wbuf,
                                         &compress_len, NULL))
         == LZO_E_OUTPUT_OVERRUN) {
    // The buffer size is too small, try with a bigger one
    jcr->compress.inflate_buffer_size
        = jcr->compress.inflate_buffer_size
          + (jcr->compress.inflate_buffer_size >> 1);
    jcr->compress.inflate_buffer = CheckPoolMemorySize(
        jcr->compress.inflate_buffer, jcr->compress.inflate_buffer_size);

    if (sparse && want_data_stream) {
      compress_len = jcr->compress.inflate_buffer_size - OFFSET_FADDR_SIZE;
      wbuf = (unsigned char*)jcr->compress.inflate_buffer + OFFSET_FADDR_SIZE;
    } else {
      compress_len = jcr->compress.inflate_buffer_size;
      wbuf = (unsigned char*)jcr->compress.inflate_buffer;
    }
    Dmsg2(400, "Comp_len=%llu message_length=%" PRIu32 "\n",
          static_cast<long long unsigned>(compress_len), *length);
  }

  if (status != LZO_E_OK) {
    Qmsg(jcr, M_ERROR, 0, T_("LZO uncompression error on file %s. ERR=%d\n"),
         last_fname, status);
    return false;
  }

  /* We return a decompressed data stream with the fileoffset encoded when this
   * was a sparse stream. */
  if (sparse && want_data_stream) {
    memcpy(jcr->compress.inflate_buffer, *data, OFFSET_FADDR_SIZE);
  }

  *data = jcr->compress.inflate_buffer;
  *length = compress_len;

  Dmsg2(400, "Write uncompressed %llu bytes, total before write=%" PRIu64 "\n",
        static_cast<long long unsigned>(compress_len), jcr->JobBytes);

  return true;
}
#endif

static bool decompress_with_fastlz(JobControlRecord* jcr,
                                   const char* last_fname,
                                   char** data,
                                   uint32_t* length,
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

  /* NOTE! We only use uInt and Bytef because they are
   * needed by the fastlz routines, they should not otherwise
   * be used in Bareos. */
  memset(&stream, 0, sizeof(stream));
  stream.next_in = (Bytef*)*data + sizeof(comp_stream_header);
  stream.avail_in = (uInt)*length - sizeof(comp_stream_header);
  if (sparse && want_data_stream) {
    stream.next_out = (Bytef*)jcr->compress.inflate_buffer + OFFSET_FADDR_SIZE;
    stream.avail_out
        = (uInt)jcr->compress.inflate_buffer_size - OFFSET_FADDR_SIZE;
  } else {
    stream.next_out = (Bytef*)jcr->compress.inflate_buffer;
    stream.avail_out = (uInt)jcr->compress.inflate_buffer_size;
  }

  Dmsg2(400, "Comp_len=%d message_length=%d\n", stream.avail_in, *length);

  if ((zstat = fastlzlibDecompressInit(&stream)) != Z_OK) { goto cleanup; }

  if ((zstat = fastlzlibSetCompressor(&stream, compressor)) != Z_OK) {
    goto cleanup;
  }

  while (1) {
    zstat = fastlzlibDecompress(&stream);
    switch (zstat) {
      case Z_BUF_ERROR:
        // The buffer size is too small, try with a bigger one
        jcr->compress.inflate_buffer_size
            = jcr->compress.inflate_buffer_size
              + (jcr->compress.inflate_buffer_size >> 1);
        jcr->compress.inflate_buffer = CheckPoolMemorySize(
            jcr->compress.inflate_buffer, jcr->compress.inflate_buffer_size);
        if (sparse && want_data_stream) {
          stream.next_out
              = (Bytef*)jcr->compress.inflate_buffer + OFFSET_FADDR_SIZE;
          stream.avail_out
              = (uInt)jcr->compress.inflate_buffer_size - OFFSET_FADDR_SIZE;
        } else {
          stream.next_out = (Bytef*)jcr->compress.inflate_buffer;
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

  /* We return a decompressed data stream with the fileoffset encoded when this
   * was a sparse stream. */
  if (sparse && want_data_stream) {
    memcpy(jcr->compress.inflate_buffer, *data, OFFSET_FADDR_SIZE);
  }

  *data = jcr->compress.inflate_buffer;
  *length = stream.total_out;
  Dmsg2(400, "Write uncompressed %d bytes, total before write=%s\n", *length,
        edit_uint64(jcr->JobBytes, ec1));
  fastlzlibDecompressEnd(&stream);

  return true;

cleanup:
  Qmsg(jcr, M_ERROR, 0, T_("Uncompression error on file %s. ERR=%s\n"),
       last_fname, zlib_strerror(zstat));
  fastlzlibDecompressEnd(&stream);

  return false;
}

bool DecompressData(JobControlRecord* jcr,
                    const char* last_fname,
                    int32_t stream,
                    char** data,
                    uint32_t* length,
                    bool want_data_stream)
{
  Dmsg1(400, "Stream found in DecompressData(): %d\n", stream);
  switch (stream) {
    case STREAM_COMPRESSED_DATA:
    case STREAM_SPARSE_COMPRESSED_DATA:
    case STREAM_WIN32_COMPRESSED_DATA:
    case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
    case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA: {
      uint32_t comp_magic, comp_len;
      uint16_t comp_level, comp_version;

      // Read compress header
      unser_declare;
      UnserBegin(*data, sizeof(comp_stream_header));
      unser_uint32(comp_magic);
      unser_uint32(comp_len);
      unser_uint16(comp_level);
      unser_uint16(comp_version);
      UnserEnd(*data, sizeof(comp_stream_header));
      Dmsg4(400,
            "Compressed data stream found: magic=0x%x, len=%d, level=%d, "
            "ver=0x%x\n",
            comp_magic, comp_len, comp_level, comp_version);

      // Version check
      if (comp_version != COMP_HEAD_VERSION) {
        Qmsg(jcr, M_ERROR, 0,
             T_("Compressed header version error. version=0x%x\n"),
             comp_version);
        return false;
      }

      // Size check
      if (comp_len + sizeof(comp_stream_header) != *length) {
        Qmsg(jcr, M_ERROR, 0,
             T_("Compressed header size error. comp_len=%d, "
                "message_length=%d\n"),
             comp_len, *length);
        return false;
      }

      /* Based on the compression used perform the actual decompression of the
       * data. */
      switch (comp_magic) {
        case COMPRESS_GZIP:
          switch (stream) {
            case STREAM_SPARSE_COMPRESSED_DATA:
              return decompress_with_zlib(jcr, last_fname, data, length, true,
                                          true, want_data_stream);
            default:
              return decompress_with_zlib(jcr, last_fname, data, length, false,
                                          true, want_data_stream);
          }
#ifdef HAVE_LZO
        case COMPRESS_LZO1X:
          switch (stream) {
            case STREAM_SPARSE_COMPRESSED_DATA:
              return decompress_with_lzo(jcr, last_fname, data, length, true,
                                         want_data_stream);
            default:
              return decompress_with_lzo(jcr, last_fname, data, length, false,
                                         want_data_stream);
          }
#endif
        case COMPRESS_FZFZ:
        case COMPRESS_FZ4L:
        case COMPRESS_FZ4H:
          switch (stream) {
            case STREAM_SPARSE_COMPRESSED_DATA:
              return decompress_with_fastlz(jcr, last_fname, data, length,
                                            comp_magic, true, want_data_stream);
            default:
              return decompress_with_fastlz(jcr, last_fname, data, length,
                                            comp_magic, false,
                                            want_data_stream);
          }
        default:
          Qmsg(jcr, M_ERROR, 0,
               T_("Compression algorithm 0x%x found, but not supported!\n"),
               comp_magic);
          return false;
      }
      break;
    }
    default:
      switch (stream) {
        case STREAM_SPARSE_GZIP_DATA:
          return decompress_with_zlib(jcr, last_fname, data, length, true,
                                      false, want_data_stream);
        default:
          return decompress_with_zlib(jcr, last_fname, data, length, false,
                                      false, want_data_stream);
      }
  }
}

void CleanupCompression(JobControlRecord* jcr)
{
  if (jcr->compress.deflate_buffer) {
    FreePoolMemory(jcr->compress.deflate_buffer);
    jcr->compress.deflate_buffer = NULL;
  }

  if (jcr->compress.inflate_buffer) {
    FreePoolMemory(jcr->compress.inflate_buffer);
    jcr->compress.inflate_buffer = NULL;
  }

  if (jcr->compress.workset.pZLIB) {
    // Free the zlib stream
    deflateEnd((z_stream*)jcr->compress.workset.pZLIB);
    free(jcr->compress.workset.pZLIB);
    jcr->compress.workset.pZLIB = NULL;
  }

#ifdef HAVE_LZO
  if (jcr->compress.workset.pLZO) {
    free(jcr->compress.workset.pLZO);
    jcr->compress.workset.pLZO = NULL;
  }
#endif

  if (jcr->compress.workset.pZFAST) {
    fastlzlibCompressEnd((zfast_stream*)jcr->compress.workset.pZFAST);
    free(jcr->compress.workset.pZFAST);
    jcr->compress.workset.pZFAST = NULL;
  }
}
