/*  
  zlib-like interface to fast block compression (LZ4 or FastLZ) libraries
  Copyright (C) 2010-2013 Exalead SA. (http://www.exalead.com/)
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

  Remarks/Bugs:
  LZ4 compression library by Yann Collet (yann.collet.73@gmail.com)
  FastLZ compression library by Ariya Hidayat (ariya@kde.org)
  Library encapsulation by Xavier Roche (fastlz@exalead.com)
*/

#ifndef FASTLZ_FASTLZLIB_H
#define FASTLZ_FASTLZLIB_H

/* extracted from fastlz.h */
#define FASTLZ_VERSION_STRING "0.1.0"

/* optional conf.h file if build with -DFASTLZ_INCLUDE_CONFIG_H */
#ifdef FASTLZ_INCLUDE_CONFIG_H
#include "config.h"
#endif
#ifndef ZFASTEXTERN
#ifdef _WIN32
#if defined(FASTLZ_DLL) || defined(DLL_EXPORT)
#define ZFASTEXTERN __declspec(dllexport)
#else
#define ZFASTEXTERN __declspec(dllimport)
#endif
#else
#define ZFASTEXTERN extern
#endif
#endif
#ifndef ZFASTINLINE
#define ZFASTINLINE
#endif

/* we are using only zlib types and defines, including z_stream_s */
#define NO_DUMMY_DECL
#include "zlib.h"

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * The zfast structure is identical to zlib one, except for the "state" opaque
 * member.
 * Do not use a stream initialized by fastlzlibDecompressInit() or
 * fastlzlibCompressInit() with zlib functions, or you will experience very
 * annoying crashes.
 **/
typedef z_stream zfast_stream;

/**
 * Backend compressor type.
 **/
typedef enum zfast_stream_compressor {
  COMPRESSOR_FASTLZ,
  COMPRESSOR_LZ4,
  COMPRESSOR_DEFAULT = COMPRESSOR_FASTLZ
} zfast_stream_compressor;

/**
 * Return the fastlz library version.
 * (zlib equivalent: zlibVersion)
 **/
ZFASTEXTERN const char * fastlzlibVersion(void);

/**
 * Initialize a compressing stream.
 * Returns Z_OK upon success, Z_MEM_ERROR upon memory allocation error.
 * (zlib equivalent: deflateInit)
 **/
ZFASTEXTERN int fastlzlibCompressInit(zfast_stream *s, int level);

/**
 * Initialize a compressing stream, and set the block size to "block_size".
 * The block size MUST be a power of two, and be within the
 * [1024 .. 33554432] interval
 * Returns Z_OK upon success, Z_MEM_ERROR upon memory allocation error.
 **/
ZFASTEXTERN int fastlzlibCompressInit2(zfast_stream *s, int level,
                                       int block_size);

/**
 * Initialize a decompressing stream.
 * Returns Z_OK upon success, Z_MEM_ERROR upon memory allocation error.
 * (zlib equivalent: inflateInit)
 **/
ZFASTEXTERN int fastlzlibDecompressInit(zfast_stream *s);

/**
 * Initialize a decompressing stream, and set the block size to "block_size".
 * The block size MUST be a power of two, and be within the
 * [1024 .. 33554432] interval
 * Returns Z_OK upon success, Z_MEM_ERROR upon memory allocation error, and
 * Z_DATA_ERROR if the block size is invalid.
 * (zlib equivalent: inflateInit)
 **/
ZFASTEXTERN int fastlzlibDecompressInit2(zfast_stream *s, int block_size);

/**
 * Set the block compressor type.
 * Returns Z_OK upon success, Z_VERSION_ERROR upon if the compressor is not
 * supported.
 **/
ZFASTEXTERN int fastlzlibSetCompressor(zfast_stream *s,
                                       zfast_stream_compressor compressor);

/**
 * Set the block compressor function.
 * The corresponding decompressor should be set using fastlzlibSetDecompress()
 **/
ZFASTEXTERN void fastlzlibSetCompress(zfast_stream *s,
                                      int (*compress)(int level,
                                                      const void* input,
                                                      int length,
                                                      void* output));

/**
 * Set the block decompressor function.
 * The corresponding compressor should be set using fastlzlibSetCompress()
 **/
ZFASTEXTERN void fastlzlibSetDecompress(zfast_stream *s,
                                        int (*decompress)(const void* input,
                                                          int length,
                                                          void* output,
                                                          int maxout));

/**
 * Free allocated data.
 * Returns Z_OK upon success.
 * (zlib equivalent: deflateEnd)
 **/
ZFASTEXTERN int fastlzlibCompressEnd(zfast_stream *s);

/**
 * Free allocated data.
 * Returns Z_OK upon success.
 * (zlib equivalent: inflateEnd)
 **/
ZFASTEXTERN int fastlzlibDecompressEnd(zfast_stream *s);

/**
 * Free allocated data by a compressing or decompressing stream.
 * Returns Z_OK upon success.
 **/
#define fastlzlibEnd fastlzlibCompressEnd

/**
 * Reset.
 * Returns Z_OK upon success.
 * (zlib equivalent: deflateReset)
 **/
ZFASTEXTERN int fastlzlibCompressReset(zfast_stream *s);

/**
 * Reset.
 * Returns Z_OK upon success.
 * (zlib equivalent: inflateReset)
 **/
ZFASTEXTERN int fastlzlibDecompressReset(zfast_stream *s);

/**
 * Reset a compressing or decompressing stream.
 * Returns Z_OK upon success.
 **/
#define fastlzlibReset fastlzlibCompressReset

/**
 * Decompress.
 * (zlib equivalent: inflate)
 **/
ZFASTEXTERN int fastlzlibDecompress(zfast_stream *s);

/**
 * Compress.
 * (zlib equivalent: deflate)
 **/
ZFASTEXTERN int fastlzlibCompress(zfast_stream *s, int flush);

/**
 * Decompress.
 * @arg may_buffer if non zero, accept to process partially a stream by using
 * internal buffers. if zero, input data shortage or output buffer room shortage
 * will return Z_BUF_ERROR. in this case, the client should ensure that the
 * input data provided and the output buffer are large enough before calling
 * again the function. (the output buffer should be validated before getting
 * this code, to ensure that Z_BUF_ERROR implies a need to read
 * additional input data)
 * @arg flush if set to Z_SYNC_FLUSH, process until the next block is reached,
 * and, if reached, return Z_NEED_DICT (a code currently unused outside this
 * function). this flag can be used to synchronize an input compressed stream
 * to a block, and seek to a desired position without the need of decompressing
 * or reading the stream, by skipping each compressed block.
 * see also s->total_out to get the current stream position, and
 * fastlzlibGetStreamInfo() to get information on compressed blocks
 **/
ZFASTEXTERN int fastlzlibDecompress2(zfast_stream *s, int flush,
                                     const int may_buffer);

/**
 * Compress.
 * @arg may_buffer if non zero, accept to process partially a stream by using
 * internal buffers. if zero, input data shortage or output buffer room shortage
 * will return Z_BUF_ERROR. in this case, the client should ensure that the
 * input data provided and the output buffer are large enough before calling
 * again the function. (the output buffer should be validated before getting
 * this code, to ensure that Z_BUF_ERROR implies a need to read additional
 * input data)
 **/
ZFASTEXTERN int fastlzlibCompress2(zfast_stream *s, int flush,
                                   const int may_buffer);

/**
 * Skip invalid data until a valid marker is found in the stream. All skipped
 * data will be lost, and associated uncompressed data too.
 * Call this function after fastlzlibDecompress() returned Z_DATA_ERROR to
 * locate the next valid compressed block.
 * Returns Z_OK upon success.
 * (zlib equivalent: inflateSync)
 **/
ZFASTEXTERN int fastlzlibDecompressSync(zfast_stream *s);

/**
 * Return the header size, that is, the fixed size of data at the begining of
 * a stream which contains details on the compression type..
 **/
ZFASTEXTERN int fastlzlibGetHeaderSize(void);

/**
 * Return the block size, that is, a size hint which can be used as a lower
 * bound for output buffer allocation and input buffer reads.
 **/
ZFASTEXTERN int fastlzlibGetBlockSize(zfast_stream *s);

/**
 * Return the block size of a compressed stream begining with "input".
 * Returns 0 if the stream is invalid or too short.
 * You may use fastlzlibGetHeaderSize() to know how many bytes needs to be
 * read for identifying a stream.
 **/
ZFASTEXTERN int fastlzlibGetStreamBlockSize(const void* input, int length);

/**
 * Return the last error message, if any.
 * Returns NULL if no specific error message was stored.
 **/
ZFASTEXTERN const char* fastlzlibGetLastErrorMessage(zfast_stream *s);

/**
 * Return the block size of a compressed stream begining with "input".
 * Returns Z_OK if the two members were successfully filles, Z_DATA_ERROR if
 * the stream is not a valid start of block, Z_BUF_ERROR if the buffer is too
 * small, and Z_STREAM_ERROR if arguments are invalid (NULL pointer).
 * You may use fastlzlibGetHeaderSize() to know how many bytes needs to be
 * read for identifying a stream.
 **/
ZFASTEXTERN int fastlzlibGetStreamInfo(const void* input, int length,
                                       uInt *compressed_size,
                                       uInt *uncompressed_size);

/**
 * Check if the given data is a fastlz compressed stream.
 * Returns Z_OK is the stream is a fastlz compressed stream, Z_BUF_ERROR is the
 * input data size is too small, and Z_DATA_ERROR is the stream is not a
 * fastlz stream.
 **/
ZFASTEXTERN int fastlzlibIsCompressedStream(const void* input, int length);

/**
 * Return the internal memory buffers size.
 * Returns -1 upon error.
 **/
ZFASTEXTERN int fastlzlibCompressMemory(zfast_stream *s);

/**
 * Return the internal memory buffers size.
 * Returns -1 upon error.
 **/
ZFASTEXTERN int fastlzlibDecompressMemory(zfast_stream *s);

#if defined (__cplusplus)
}
#endif

#endif
