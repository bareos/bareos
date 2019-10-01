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

/*
  Build:

  GCC:
  gcc -c -fPIC -O3 -g
    -W -Wall -Wextra -Werror
    -D_REENTRANT
    fastlzlib.c -o fastlzlib.o
  gcc -shared -fPIC -O3 -Wl,-O1 -Wl,--no-undefined
    -rdynamic -shared -Wl,-soname=fastlzlib.so
    fastlzlib.o -o fastlzlib.so.1.0.0
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "fastlz/fastlzlib.h"

/* use LZ4 */
#include "fastlz/lz4.h"
#include "fastlz/lz4hc.h"

/* use fastLZ */
#include "fastlz/fastlz.h"

/* undefined because we use the internal one */
#undef fastlzlibReset

/* note: the 5% ratio (/20) is not sufficient - add 66 bytes too */
/* for LZ4, the expansion ratio is smaller, so we keep the biggest one */
#define EXPANSION_RATIO         10
#define EXPANSION_SECURITY      66
#define HEADER_SIZE             16

#define MIN_BLOCK_SIZE          64
#define DEFAULT_BLOCK_SIZE  262144

/* size of blocks to be compressed */
#define BLOCK_SIZE(S) ( (S)->state->block_size )

/* block size, given the power base (0..15 => 1K..32M) */
#define POWER_BASE 10
#define POWER_TO_BLOCK_SIZE(P) ( 1 << ( P + POWER_BASE ) )

/* estimated upper boundary of compressed size */
#define BUFFER_BLOCK_SIZE(S)                                            \
  ( BLOCK_SIZE(S) + BLOCK_SIZE(S) / EXPANSION_RATIO + HEADER_SIZE*2)

/* block types (base ; the lower four bits are used for block size) */
#define BLOCK_TYPE_RAW         (0x10)
#define BLOCK_TYPE_COMPRESSED  (0xc0)
#define BLOCK_TYPE_BAD_MAGIC   (0xffff)

/* fake level for decompression */
#define ZFAST_LEVEL_DECOMPRESS (-2)

/* macros */

/* the stream is used for compressing */
#define ZFAST_IS_COMPRESSING(S) ( (S)->state->level != ZFAST_LEVEL_DECOMPRESS )

/* the stream is used for decompressing */
#define ZFAST_IS_DECOMPRESSING(S) ( !ZFAST_IS_COMPRESSING(S) )

/* no input bytes available */
#define ZFAST_INPUT_IS_EMPTY(S) ( (S)->avail_in == 0 )

/* no output space available */
#define ZFAST_OUTPUT_IS_FULL(S) ( (S)->avail_out == 0 )

/* the stream has pending output available for the client */
#define ZFAST_HAS_BUFFERED_OUTPUT(S)                    \
  ( s->state->outBuffOffs < s->state->dec_size )

/* compress stream */
#define ZFAST_COMPRESS s->state->compress

/* decompress stream */
#define ZFAST_DECOMPRESS s->state->decompress

/* inlining */
#ifndef ZFASTINLINE
#define ZFASTINLINE FASTLZ_INLINE
#endif

/* tools */
#define READ_8(adr)  (*(adr))
#define READ_16(adr) ( READ_8(adr) | (READ_8((adr)+1) << 8) )
#define READ_32(adr) ( READ_16(adr) | (READ_16((adr)+2) << 16) )
#define WRITE_8(buff, n) do {                          \
    *((buff))     = (unsigned char) ((n) & 0xff);      \
  } while(0)
#define WRITE_16(buff, n) do {                          \
    *((buff))     = (unsigned char) ((n) & 0xff);       \
    *((buff) + 1) = (unsigned char) ((n) >> 8);         \
  } while(0)
#define WRITE_32(buff, n) do {                  \
    WRITE_16((buff), (n) & 0xffff);             \
    WRITE_16((buff) + 2, (n) >> 16);            \
  } while(0)

/* magic for opaque "state" structure */
static const char MAGIC[8] = {'F', 'a', 's', 't', 'L', 'Z', 0x01, 0};

/* magic for stream (7 bytes with terminating \0) */
static const char* BLOCK_MAGIC = "FastLZ";

/* opaque structure for "state" zlib structure member */
struct internal_state {
  /* magic ; must be BLOCK_MAGIC */
  char magic[8];

  /* compression level or decompression mode (ZFAST_LEVEL_*) */
  int level;

  /* buffered header and data read so far (if inHdrOffs != 0) */
  Bytef inHdr[HEADER_SIZE];
  uInt inHdrOffs;

  /* block size ; must be 2**(POWER_BASE+n) with n < 16 */
  uInt block_size;
  /* block type (BLOCK_TYPE_*) */
  uInt block_type;
  /* current block stream size (input data block except header) */
  uInt str_size;
  /* current output stream size (output data block) */
  uInt dec_size;

  /* buffered data input */
  Bytef *inBuff;
  /* buffered data output */
  Bytef *outBuff;
  /* buffered data offset in inBuff (iff inBuffOffs < str_size)*/
  uInt inBuffOffs;
  /* buffered data offset in outBuff (iff outBuffOffs < dec_size)*/
  uInt outBuffOffs;
  
  /* block compression backend function */
  int (*compress)(int level, const void* input, int length, void* output);

  /* block decompression backend function */
  int (*decompress)(const void* input, int length, void* output, int maxout); 
};

/* our typed internal state */
typedef struct internal_state zfast_stream_internal;

/* code version */
const char * fastlzlibVersion() {
  return FASTLZ_VERSION_STRING;
}

/* get the block size */
int fastlzlibGetBlockSize(zfast_stream *s) {
  if (s != NULL && s->state != NULL) {
    assert(strcmp(s->state->magic, MAGIC) == 0);
    return BLOCK_SIZE(s);
  }
  return 0;
}

const char* fastlzlibGetLastErrorMessage(zfast_stream *s) {
  if (s != NULL && s->msg != NULL) {
    return s->msg;
  } else {
    return NULL;
  }
}

int fastlzlibGetHeaderSize() {
  return HEADER_SIZE;
}

static voidpf default_zalloc(uInt items, uInt size) {
  return malloc(items*size);
}

static void default_zfree(voidpf address) {
  free(address);
}

static voidpf zalloc(zfast_stream *s, uInt items, uInt size) {
  if (s->zalloc != NULL) {
    return s->zalloc(s->opaque, items, size);
  } else {
    return default_zalloc(items, size);
  }
}

static void zfree(zfast_stream *s, voidpf address) {
  if (s->zfree != NULL) {
    s->zfree(s->opaque, address);
  } else {
    default_zfree(address);
  }
}

/* free private fields */
static void fastlzlibFree(zfast_stream *s) {
  if (s != NULL) {
    if (s->state != NULL) {
      assert(strcmp(s->state->magic, MAGIC) == 0);
      if (s->state->inBuff != NULL) {
        zfree(s, s->state->inBuff);
        s->state->inBuff = NULL;
      }
      if (s->state->outBuff != NULL) {
        zfree(s, s->state->outBuff);
        s->state->outBuff = NULL;
      }
      zfree(s, s->state);
      s->state = NULL;
    }
  }
}

/* reset internal state */
static void fastlzlibReset(zfast_stream *s) {
  assert(strcmp(s->state->magic, MAGIC) == 0);
  s->msg = NULL;
  s->state->inHdrOffs = 0;
  s->state->block_type = 0;
  s->state->str_size = 0;
  s->state->dec_size = 0;
  s->state->inBuffOffs = 0;
  s->state->outBuffOffs = 0;
  s->total_in = 0;
  s->total_out = 0;
}

static ZFASTINLINE int fastlzlibGetBlockSizeLevel(int block_size) {
  int i;
  for(i = 0 ; block_size > 1 && ( ( block_size & 1 ) == 0 )
        ; block_size >>= 1, i++);
  if (block_size == 1 && i >= POWER_BASE && i < POWER_BASE + 0xf) {
    return i - POWER_BASE;
  }
  return -1;
}

/* compression backend for LZ4 */
static int lz4_backend_compress(int level, const void* input, int length,
                                void* output) {
  if (level == Z_BEST_COMPRESSION) {
    return LZ4_compressHC(input, output, length);
  }
  else {
    return LZ4_compress(input, output, length);
  }
}

/* decompression backend for LZ4 */
static int lz4_backend_decompress(const void* input, int length, void* output,
                                  int maxout) {
  return LZ4_decompress_safe(input, output, length, maxout);
  /* return LZ4_uncompress(input, output, maxout); */
}

/* compression backend for FastLZ (level adjustment) */
static int fastlz_backend_compress(int level, const void* input, int length,
                                   void* output) {
  /* Level 1 is the fastest compression and generally useful for short data.
     Level 2 is slightly slower but it gives better compression ratio. */
  const int l = level <= Z_BEST_SPEED ? 1 : 2;
  return fastlz_compress_level(l, input, length, output);
}

#define fastlz_backend_decompress fastlz_decompress

/* initialize private fields */
static int fastlzlibInit(zfast_stream *s, int block_size) {
  if (s != NULL) {
    int code;
    if (fastlzlibGetBlockSizeLevel(block_size) == -1) {
      s->msg = "block size is invalid";
      return Z_STREAM_ERROR;
    }
    s->state = (zfast_stream_internal*)
      zalloc(s, sizeof(zfast_stream_internal), 1);
    strcpy(s->state->magic, MAGIC);
    s->state->compress = NULL;
    s->state->decompress = NULL;
    if ( ( code = fastlzlibSetCompressor(s, COMPRESSOR_DEFAULT) ) != Z_OK) {
      fastlzlibFree(s);
      return code;
    }
    s->state->block_size = (uInt) block_size;
    s->state->inBuff = zalloc(s, BUFFER_BLOCK_SIZE(s), 1);
    s->state->outBuff = zalloc(s, BUFFER_BLOCK_SIZE(s), 1);
    if (s->state->inBuff != NULL && s->state->outBuff != NULL) {
      fastlzlibReset(s);
      return Z_OK;
    }
    s->msg = "memory exhausted";
    fastlzlibFree(s);
  } else {
    return Z_STREAM_ERROR;
  }
  return Z_MEM_ERROR;
}

int fastlzlibCompressInit2(zfast_stream *s, int level, int block_size) {
  const int success = fastlzlibInit(s, block_size);
  if (success == Z_OK) {
    /* default or unrecognized compression level */
    if (level < Z_NO_COMPRESSION || level > Z_BEST_COMPRESSION) {
      level = Z_BEST_COMPRESSION;
    }
    s->state->level = level;
  }
  return success;
}

int fastlzlibCompressInit(zfast_stream *s, int level) {
  return fastlzlibCompressInit2(s, level, DEFAULT_BLOCK_SIZE);
}

int fastlzlibDecompressInit2(zfast_stream *s, int block_size) {
  const int success = fastlzlibInit(s, block_size);
  if (success == Z_OK) {
    s->state->level = ZFAST_LEVEL_DECOMPRESS;
  }
  return success;
}

int fastlzlibDecompressInit(zfast_stream *s) {
  return fastlzlibDecompressInit2(s, DEFAULT_BLOCK_SIZE);
}

void fastlzlibSetCompress(zfast_stream *s,
                          int (*compress)(int level, const void* input,
                                          int length, void* output)) {
  s->state->compress = compress;
}

void fastlzlibSetDecompress(zfast_stream *s,
                            int (*decompress)(const void* input, int length,
                                              void* output, int maxout)) {
  s->state->decompress = decompress;
}

int fastlzlibSetCompressor(zfast_stream *s,
                           zfast_stream_compressor compressor) {
  if (compressor == COMPRESSOR_LZ4) {
    fastlzlibSetCompress(s, lz4_backend_compress);
    fastlzlibSetDecompress(s, lz4_backend_decompress);
    return Z_OK;
  }
  if (compressor == COMPRESSOR_FASTLZ) {
    fastlzlibSetCompress(s, fastlz_backend_compress);
    fastlzlibSetDecompress(s, fastlz_backend_decompress);
    return Z_OK;
  }
  return Z_VERSION_ERROR;
}

int fastlzlibCompressEnd(zfast_stream *s) {
  if (s == NULL) {
    return Z_STREAM_ERROR;
  }
  fastlzlibFree(s);
  return Z_OK;
}

int fastlzlibDecompressEnd(zfast_stream *s) {
  return fastlzlibCompressEnd(s);
}

int fastlzlibCompressReset(zfast_stream *s) {
  if (s == NULL) {
    return Z_STREAM_ERROR;
  }
  fastlzlibReset(s);
  return Z_OK;
}

int fastlzlibDecompressReset(zfast_stream *s) {
  return fastlzlibCompressReset(s);
}

int fastlzlibCompressMemory(zfast_stream *s) {
  if (s == NULL || s->state == NULL) {
    return -1;
  }
  return (int) ( sizeof(zfast_stream_internal) + BUFFER_BLOCK_SIZE(s) * 2 );
}

int fastlzlibDecompressMemory(zfast_stream *s) {
  return fastlzlibCompressMemory(s);
}

static ZFASTINLINE void inSeek(zfast_stream *s, uInt offs) {
  assert(s->avail_in >= offs);
  s->next_in += offs;
  s->avail_in -= offs;
  s->total_in += offs;
}

static ZFASTINLINE void outSeek(zfast_stream *s, uInt offs) {
  assert(s->avail_out >= offs);
  s->next_out += offs;
  s->avail_out -= offs;
  s->total_out += offs;
}

/* write an header to "dest" */
static ZFASTINLINE int fastlz_write_header(Bytef* dest,
                                           uInt type,
                                           uInt block_size,
                                           uInt compressed,
                                           uInt original) {
  const int bs = fastlzlibGetBlockSizeLevel(block_size);
  const int packed_type = type + bs;
  assert(bs >= 0x0 && bs <= 0xf);
  assert( ( type & 0x0f ) == 0);
  
  memcpy(&dest[0], BLOCK_MAGIC, 7);
  WRITE_8(&dest[7], packed_type);
  WRITE_32(&dest[8], compressed);
  WRITE_32(&dest[12], original);
  return HEADER_SIZE;
}

/* read an header from "source" */
static ZFASTINLINE void fastlz_read_header(const Bytef* source,
                                           uInt *type,
                                           uInt *block_size,
                                           uInt *compressed,
                                           uInt *original) {
  if (memcmp(&source[0], BLOCK_MAGIC, 7) == 0) {
    const int packed_type = READ_8(&source[7]);
    const int block_pow = packed_type & 0x0f;
    *type = packed_type & 0xf0;
    *compressed = READ_32(&source[8]);
    *original = READ_32(&source[12]);
    *block_size = POWER_TO_BLOCK_SIZE(block_pow);
  } else {
    *type = BLOCK_TYPE_BAD_MAGIC;
    *compressed =  0;
    *original =  0;
    *block_size = 0;
  }
}

int fastlzlibGetStreamBlockSize(const void* input, int length) {
  uInt block_size = 0;
  if (length >= HEADER_SIZE) {
    uInt block_type;
    uInt str_size;
    uInt dec_size;
    fastlz_read_header((const Bytef*) input, &block_type, &block_size,
                       &str_size, &dec_size);
  }
  return block_size;
}

int fastlzlibGetStreamInfo(const void* input, int length,
                           uInt *compressed_size,
                           uInt *uncompressed_size) {
  if (input != NULL && compressed_size != NULL && uncompressed_size != NULL) {
    if (length >= HEADER_SIZE) {
      uInt block_size = 0;
      uInt block_type;
      fastlz_read_header((const Bytef*) input, &block_type, &block_size,
                         compressed_size, uncompressed_size);
      if (block_size != 0) {
        return Z_OK;
      } else {
        return Z_DATA_ERROR;
      }
    } else {
      return Z_BUF_ERROR;
    }
  } else {
    return Z_STREAM_ERROR;
  }
}

/* helper for fastlz_compress */
static ZFASTINLINE int fastlz_compress_hdr(const zfast_stream *const s,
                                           const void* input, uInt length,
                                           void* output, uInt output_length,
                                           int block_size, int level,
                                           int flush) {
  uInt done = 0;
  Bytef*const output_start = (Bytef*) output;
  if (length > 0) {
    void*const output_data_start = &output_start[HEADER_SIZE];
    uInt type;
    /* compress and fill header after */
    if (length > MIN_BLOCK_SIZE) {
      done = ZFAST_COMPRESS(level, input, length, output_data_start);
      assert(done + HEADER_SIZE*2 <= output_length);
      if (done < length) {
        type = BLOCK_TYPE_COMPRESSED;
      }
      /* compressed version is greater ; use raw data */
      else {
        memcpy(output_data_start, input, length);
        done = length;
        type = BLOCK_TYPE_RAW;
      }
    }
    /* store small chunk as raw data */
    else {
      assert(length + HEADER_SIZE*2 <= output_length);
      memcpy(output_data_start, input, length);
      done = length;
      type = BLOCK_TYPE_RAW;
    }
    /* write back header */
    done += fastlz_write_header(output_start, type, block_size, done, length);
  }
  /* write an EOF marker (empty block with compressed=uncompressed=0) */
  if (flush == Z_FINISH) {
    Bytef*const output_end = &output_start[done];
    done += fastlz_write_header(output_end, BLOCK_TYPE_COMPRESSED, block_size,
                                0, 0);
  }
  assert(done <= output_length);
  return done;
}

/*
 * Compression and decompression processing routine.
 * The only difference with compression is that the input and output are
 * variables (may change with flush)
 */
static ZFASTINLINE int fastlzlibProcess(zfast_stream *const s, const int flush,
                                        const int may_buffer) {
  const Bytef *in = NULL;
  const uInt prev_avail_in = s->avail_in;
  const uInt prev_avail_out = s->avail_out;

  /* returns Z_OK if something was processed, Z_BUF_ERROR otherwise */
#define PROGRESS_OK() ( ( s->avail_in != prev_avail_in                \
                          || s->avail_out != prev_avail_out )         \
                        ? Z_OK : Z_BUF_ERROR )
  
  /* sanity check for next_in/next_out */
  if (s->next_in == NULL && !ZFAST_INPUT_IS_EMPTY(s)) {
    s->msg = "invalid input";
    return Z_STREAM_ERROR;
  }
  else if (s->next_out == NULL && !ZFAST_OUTPUT_IS_FULL(s)) {
    s->msg = "invalid output";
    return Z_STREAM_ERROR;
  }
  
  /* output buffer data to be processed */
  if (ZFAST_HAS_BUFFERED_OUTPUT(s)) {
    /* maximum size that can be copied */
    uInt size = s->state->dec_size - s->state->outBuffOffs;
    if (size > s->avail_out) {
      size = s->avail_out;
    }
    /* copy and seek */
    if (size > 0) {
      memcpy(s->next_out, &s->state->outBuff[s->state->outBuffOffs], size);
      s->state->outBuffOffs += size;
      outSeek(s, size);
    }
    /* and return chunk */
    return PROGRESS_OK();
  }

  /* read next block (note: output buffer is empty here) */
  else if (s->state->str_size == 0) {
    /* for error reporting only */
    uInt block_size = 0;

    /* decompressing: header is present */
    if (ZFAST_IS_DECOMPRESSING(s)) {

      /* sync to a block */
      if (flush == Z_SYNC_FLUSH && s->state->inHdrOffs == 0) {
        return Z_NEED_DICT;
      }
      
      /* if header read in progress or will be in multiple passes (sheesh) */
      if (s->state->inHdrOffs != 0 || s->avail_in < HEADER_SIZE) {
        /* we are to go buffered for the header - check if this is allowed */
        if (s->state->inHdrOffs == 0 && !may_buffer) {
          s->msg = "need more data on input";
          return Z_BUF_ERROR;
        }
        /* copy up to HEADER_SIZE bytes */
        for( ; s->avail_in > 0 && s->state->inHdrOffs < HEADER_SIZE
               ; s->state->inHdrOffs++, inSeek(s, 1)) {
          s->state->inHdr[s->state->inHdrOffs] = *s->next_in;
        }
      }
      /* process header if completed */

      /* header on client region */
      if (s->state->inHdrOffs == 0 && s->avail_in >= HEADER_SIZE) {
        /* peek header */
        uInt block_type;
        uInt str_size;
        uInt dec_size;
        fastlz_read_header(s->next_in, &block_type, &block_size,
                           &str_size, &dec_size);

        /* not buffered: check if we can do the job at once */
        if (!may_buffer) {
          /* input buffer too small */
          if (s->avail_in < str_size) {
            s->msg = "need more data on input";
            return Z_BUF_ERROR;
          }
          /* output buffer too small */
          else if (s->avail_out < dec_size) {
            s->msg = "need more room on output";
            return Z_BUF_ERROR;
          }
        }

        /* apply/eat the header and continue */
        s->state->block_type = block_type;
        s->state->str_size =  str_size;
        s->state->dec_size =  dec_size;
        inSeek(s, HEADER_SIZE);
      }
      /* header in inHdrOffs buffer */
      else if (s->state->inHdrOffs == HEADER_SIZE) {
        /* peek header */
        uInt block_type;
        uInt str_size;
        uInt dec_size;
        assert(may_buffer);  /* impossible at this point */
        fastlz_read_header(s->state->inHdr, &block_type, &block_size,
                           &str_size, &dec_size);
        s->state->block_type = block_type;
        s->state->str_size = str_size;
        s->state->dec_size = dec_size;
        s->state->inHdrOffs = 0;
      }
      /* otherwise, please come back later (header not fully processed) */
      else {
        assert(may_buffer);  /* impossible at this point */
        return PROGRESS_OK();
      }

      /* compressed and uncompressed == 0 : EOF marker */
      if (s->state->str_size == 0 && s->state->dec_size == 0) {
        return Z_STREAM_END;
      }
    }
    /* compressing: fixed input size (unless flush) */
    else {
      uInt block_type = BLOCK_TYPE_COMPRESSED;
      uInt str_size = BLOCK_SIZE(s);

      /* not enough room on input */
      if (str_size > s->avail_in) {
        if (flush > Z_NO_FLUSH) {
          str_size = s->avail_in;
        } else if (!may_buffer) {
          s->msg = "need more data on input";
          return Z_BUF_ERROR;
        }
      }
      
      /* apply/eat the header and continue */
      s->state->block_type = block_type;
      s->state->str_size = str_size;
      s->state->dec_size = 0;  /* yet unknown */
    }
    
    /* output not buffered yet */
    s->state->outBuffOffs = s->state->dec_size;

    /* sanity check */
    if (s->state->block_type == BLOCK_TYPE_BAD_MAGIC) {
      s->msg = "corrupted compressed stream (bad magic)";
      return Z_DATA_ERROR;
    }
    else if (s->state->block_type != BLOCK_TYPE_RAW
             && s->state->block_type != BLOCK_TYPE_COMPRESSED) {
      s->msg = "corrupted compressed stream (illegal block type)";
      return Z_VERSION_ERROR;
    }
    else if (block_size > BLOCK_SIZE(s)) {
      s->msg = "block size too large";
      return Z_VERSION_ERROR;
    }
    else if (s->state->dec_size > BUFFER_BLOCK_SIZE(s)) {
      s->msg = "corrupted compressed stream (illegal decompressed size)";
      return Z_VERSION_ERROR;
    }
    else if (s->state->str_size > BUFFER_BLOCK_SIZE(s)) {
      s->msg = "corrupted compressed stream (illegal stream size)";
      return Z_VERSION_ERROR;
    }
    
    /* direct data fully available (ie. complete compressed block) ? */
    if (s->avail_in >= s->state->str_size) {
      in = s->next_in;
      inSeek(s, s->state->str_size);
    }
    /* otherwise, buffered */
    else {
      s->state->inBuffOffs = 0;
    }
  }

  /* notes: */
  /* - header always processed at this point */
  /* - no output buffer data to be processed (outBuffOffs == 0) */
 
  /* buffered data: copy as much as possible to inBuff until we have the
     block data size */
  if (in == NULL) {
    /* remaining data to copy in input buffer */
    if (s->state->inBuffOffs < s->state->str_size) {
      uInt size = s->state->str_size - s->state->inBuffOffs;
      if (size > s->avail_in) {
        size = s->avail_in;
      }
      if (size > 0) {
        memcpy(&s->state->inBuff[s->state->inBuffOffs], s->next_in, size);
        s->state->inBuffOffs += size;
        inSeek(s, size);
      }
    }
    /* block stream size (ie. compressed one) reached */
    if (s->state->inBuffOffs == s->state->str_size) {
      in = s->state->inBuff;
      /* we are about to eat buffered data */
      s->state->inBuffOffs = 0;
    }
    /* forced flush: adjust str_size */
    else if (ZFAST_IS_COMPRESSING(s) && flush != Z_NO_FLUSH) {
      in = s->state->inBuff;
      s->state->str_size = s->state->inBuffOffs;
      /* we are about to eat buffered data, reset it (now empty) */
      s->state->inBuffOffs = 0;
    }
  }

  /* we have a complete compressed block (str_size) : where to uncompress ? */
  if (in != NULL) {
    Bytef *out = NULL;
    const uInt in_size = s->state->str_size;

    int flush_now = flush;
    /* we are supposed to finish, but we did not eat all data: ignore for now */
    if (flush_now == Z_FINISH && !ZFAST_INPUT_IS_EMPTY(s)) {
      flush_now = Z_NO_FLUSH;
    }

    /* decompressing */
    if (ZFAST_IS_DECOMPRESSING(s)) {
      int done = 0;
      const uInt out_size = s->state->dec_size;

      /* can decompress directly on client memory */
      if (s->avail_out >= s->state->dec_size) {
        out = s->next_out;
        outSeek(s, s->state->dec_size);
        /* no buffer */
        s->state->outBuffOffs = s->state->dec_size;
      }
      /* otherwise in output buffer */
      else {
        out = s->state->outBuff;
        s->state->outBuffOffs = 0;
      }

      /* input eaten */
      s->state->str_size = 0;

      /* rock'in */
      switch(s->state->block_type) {
      case BLOCK_TYPE_COMPRESSED:
        done = ZFAST_DECOMPRESS(in, in_size, out, out_size);
        break;
      case BLOCK_TYPE_RAW:
        if (out_size >= in_size) {
          memcpy(out, in, in_size);
          done = in_size;
        } else {
          done = 0;
        }
        break;
      default:
        assert(0);
        break;
      }
      if (done != (int) s->state->dec_size) {
        s->msg = "unable to decompress block stream";
        return Z_STREAM_ERROR;
      }
    }
    /* compressing */
    else {
      /* note: if < MIN_BLOCK_SIZE, fastlz_compress_hdr will not compress */
      const uInt estimated_dec_size = in_size + in_size / EXPANSION_RATIO
        + EXPANSION_SECURITY;

      /* can compress directly on client memory */
      if (s->avail_out >= estimated_dec_size) {
        const int done = fastlz_compress_hdr(s, in, in_size,
                                             s->next_out, estimated_dec_size,
                                             BLOCK_SIZE(s),
                                             s->state->level,
                                             flush_now);
        /* seek output */
        outSeek(s, done);
        /* no buffer */
        s->state->outBuffOffs = s->state->dec_size;
      }
      /* otherwise in output buffer */
      else {
        const int done = fastlz_compress_hdr(s, in, in_size,
                                             s->state->outBuff,
                                             BUFFER_BLOCK_SIZE(s),
                                             BLOCK_SIZE(s),
                                             s->state->level,
                                             flush_now);
        /* produced size (in outBuff) */
        s->state->dec_size = (uInt) done;
        /* buffered */
        s->state->outBuffOffs = 0;
      }

      /* input eaten */
      s->state->str_size = 0;
    }
  }

  /* new output buffer data to be processed ; same logic as begining */
  if (ZFAST_HAS_BUFFERED_OUTPUT(s)) {
    /* maximum size that can be copied */
    uInt size = s->state->dec_size - s->state->outBuffOffs;
    if (size > s->avail_out) {
      size = s->avail_out;
    }
    /* copy and seek */
    if (size > 0) {
      memcpy(s->next_out, &s->state->outBuff[s->state->outBuffOffs], size);
      s->state->outBuffOffs += size;
      outSeek(s, size);
    }
  }

  /* so far so good */

  /* success and EOF */
  if (flush == Z_FINISH
      && ZFAST_INPUT_IS_EMPTY(s)
      && !ZFAST_HAS_BUFFERED_OUTPUT(s)) {
    if (!ZFAST_IS_DECOMPRESSING(s)) {
      return Z_STREAM_END;
    }
    /* we are supposed to be done in decompressing but did not see any EOF */
    else {
      s->msg = "unexpected EOF";
      return Z_BUF_ERROR;
    }
  }
  /* success */
  else {
    return PROGRESS_OK();
  }
#undef PROGRESS_OK
}

/* same as fastlzlibProcess(), but retry once if first call did not produce any
   data (ie. only modified the state machine) */
static ZFASTINLINE int fastlzlibProcess2(zfast_stream *const s, const int flush,
                                         const int may_buffer) {
  const uInt prev_avail_in = s->avail_in;
  const uInt prev_avail_out = s->avail_out;
  const int success = fastlzlibProcess(s, flush, may_buffer);
  const uInt avail_in = s->avail_in;
  const uInt avail_out = s->avail_out;
  /* successful, ate input, no data on output ? */
  if (success == 0
      && avail_out == prev_avail_out && avail_in != prev_avail_in
      && flush != Z_NO_FLUSH) {
    return fastlzlibProcess(s, flush, may_buffer);
  } else {
    return success;
  }
}

int fastlzlibDecompress2(zfast_stream *s, int flush, const int may_buffer) {
  if (ZFAST_IS_DECOMPRESSING(s)) {
    return fastlzlibProcess2(s, flush, may_buffer);
  } else {
    s->msg = "decompressing function used with a compressing stream";
    return Z_STREAM_ERROR;
  }
}

int fastlzlibDecompress(zfast_stream *s) {
  return fastlzlibDecompress2(s, Z_NO_FLUSH, 1);
}

int fastlzlibCompress2(zfast_stream *s, int flush, const int may_buffer) {
  if (ZFAST_IS_COMPRESSING(s)) {
    return fastlzlibProcess2(s, flush, may_buffer);
  } else {
    s->msg = "compressing function used with a decompressing stream";
    return Z_STREAM_ERROR;
  }
}

int fastlzlibCompress(zfast_stream *s, int flush) {
  return fastlzlibCompress2(s, flush, 1);
}

int fastlzlibIsCompressedStream(const void* input, int length) {
  if (length >= HEADER_SIZE) {
    const Bytef*const in = (const Bytef*) input;
    return fastlzlibGetStreamBlockSize(in, length) != 0 ? Z_OK : Z_DATA_ERROR;
  } else {
    return Z_BUF_ERROR;
  }
}

int fastlzlibDecompressSync(zfast_stream *s) {
  if (ZFAST_IS_DECOMPRESSING(s)) {
    if (ZFAST_HAS_BUFFERED_OUTPUT(s)) {
      /* not in an error state: uncompressed data available in buffer */
      return Z_OK;
    }
    else {
      /* Note: if s->state->str_size == 0, we are not in an error state: the
         next chunk is to be read; However, we check the chunk anyway. */

      /* at least HEADER_SIZE data */
      if (s->avail_in < HEADER_SIZE) {
        s->msg = "need more data on input";
        return Z_BUF_ERROR;
      }
        
      /* in buffered read of the header.. reset to 0 */
      if (s->state->inHdrOffs != 0) {
        s->state->inHdrOffs = 0;
      }
      
      /* seek */
      for( ; s->avail_in >= HEADER_SIZE
             ; s->state->inHdrOffs++, inSeek(s, 1)) {
        const Bytef *const in = s->next_in;
        if (in[0] == BLOCK_MAGIC[0]
            && in[1] == BLOCK_MAGIC[1]
            && in[2] == BLOCK_MAGIC[2]
            && in[3] == BLOCK_MAGIC[3]
            && in[4] == BLOCK_MAGIC[4]
            && in[5] == BLOCK_MAGIC[5]
            && in[6] == BLOCK_MAGIC[6]
            ) {
          const int block_size = fastlzlibGetStreamBlockSize(in, HEADER_SIZE);
          if (block_size != 0) {
            /* successful seek */
            return Z_OK;
          }
        }
      }
      s->msg = "no flush point found";
      return Z_DATA_ERROR;
    }
  } else {
    s->msg = "decompressing function used with a compressing stream";
    return Z_STREAM_ERROR;
  }
}
