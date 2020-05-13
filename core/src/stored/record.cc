/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2020 Bareos GmbH & Co. KG

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
 * Kern Sibbald, April MMI
 * added BB02 format October MMII
 */
/**
 * @file
 * Volume (tape/disk) record handling functions
 */

#include "include/bareos.h"
#include "stored/jcr_private.h"
#include "stored/stored.h"
#include "stored/device_control_record.h"
#include "lib/attribs.h"
#include "lib/util.h"
#include "include/jcr.h"

namespace storagedaemon {

/**
 * Convert a FileIndex into a printable
 * ASCII string.  Not reentrant.
 *
 * If the FileIndex is negative, it flags the
 * record as a Label, otherwise it is simply
 * the FileIndex of the current file.
 */
const char* FI_to_ascii(char* buf, int fi)
{
  if (fi >= 0) {
    sprintf(buf, "%d", fi);
    return buf;
  }
  switch (fi) {
    case PRE_LABEL:
      return "PRE_LABEL";
    case VOL_LABEL:
      return "VOL_LABEL";
    case EOM_LABEL:
      return "EOM_LABEL";
    case SOS_LABEL:
      return "SOS_LABEL";
    case EOS_LABEL:
      return "EOS_LABEL";
    case EOT_LABEL:
      return "EOT_LABEL";
      break;
    case SOB_LABEL:
      return "SOB_LABEL";
      break;
    case EOB_LABEL:
      return "EOB_LABEL";
      break;
    default:
      sprintf(buf, _("unknown: %d"), fi);
      return buf;
  }
}


static const char* compression_to_str(PoolMem& resultbuffer,
                                      const char* compression_algorithm,
                                      uint32_t data_length,
                                      uint16_t compression_level,
                                      uint16_t compression_algorithm_version)
{
  PoolMem tmp(PM_MESSAGE);
  tmp.bsprintf("%s, level=%u, version=%u, length=%u", compression_algorithm,
               compression_level, compression_algorithm_version, data_length);
  resultbuffer.strcat(tmp);
  return resultbuffer.c_str();
}

static const char* record_compression_to_str(PoolMem& resultbuffer,
                                             const DeviceRecord* rec)
{
  int32_t maskedStream = rec->maskedStream;
  POOLMEM* buf = rec->data;
  PoolMem tmp(PM_MESSAGE);
  unser_declare;

  if (maskedStream == STREAM_SPARSE_GZIP_DATA ||
      maskedStream == STREAM_SPARSE_COMPRESSED_DATA) {
    uint64_t faddr = 0;

    SerBegin(buf, sizeof(uint64_t));
    unser_uint64(faddr);
    SerEnd(buf, sizeof(uint64_t));

    buf += sizeof(uint64_t);

    Dmsg1(400, "Sparse data stream found: start address=%llu\n", faddr);
    tmp.bsprintf("Sparse: StartAddress=%llu. ", faddr);
    resultbuffer.strcat(tmp);
  }

  Dmsg1(400, "Stream found in DecompressData(): %d\n", maskedStream);
  switch (maskedStream) {
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
      UnserBegin(buf, sizeof(comp_stream_header));
      unser_uint32(comp_magic);
      unser_uint32(comp_len);
      unser_uint16(comp_level);
      unser_uint16(comp_version);
      UnserEnd(buf, sizeof(comp_stream_header));

      Dmsg4(400,
            "Compressed data stream found: magic=0x%x, len=%d, level=%d, "
            "ver=0x%x\n",
            comp_magic, comp_len, comp_level, comp_version);

      switch (comp_magic) {
        case COMPRESS_GZIP:
          compression_to_str(resultbuffer, "GZIP", comp_len, comp_level,
                             comp_version);
          break;
        case COMPRESS_LZO1X:
          compression_to_str(resultbuffer, "LZO1X", comp_len, comp_level,
                             comp_version);
          break;
        case COMPRESS_FZFZ:
          compression_to_str(resultbuffer, "FZFZ", comp_len, comp_level,
                             comp_version);
          break;
        case COMPRESS_FZ4L:
          compression_to_str(resultbuffer, "FZ4L", comp_len, comp_level,
                             comp_version);
          break;
        case COMPRESS_FZ4H:
          compression_to_str(resultbuffer, "FZ4H", comp_len, comp_level,
                             comp_version);
          break;
        default:
          tmp.bsprintf(
              _("Compression algorithm 0x%x found, but not supported!\n"),
              comp_magic);
          resultbuffer.strcat(tmp);
          break;
      }
      break;
    }
    case STREAM_GZIP_DATA:
    case STREAM_SPARSE_GZIP_DATA:
      /* deprecated */
      compression_to_str(resultbuffer, "GZIP", 0, 0, 0);
      break;
    default:
      break;
  }

  return resultbuffer.c_str();
}

static const char* record_digest_to_str(PoolMem& resultbuffer,
                                        const DeviceRecord* rec)
{
  char digest[BASE64_SIZE(CRYPTO_DIGEST_MAX_SIZE)];

  switch (rec->maskedStream) {
    case STREAM_MD5_DIGEST:
      BinToBase64(digest, sizeof(digest), (char*)rec->data,
                  CRYPTO_DIGEST_MD5_SIZE, true);
      break;
    case STREAM_SHA1_DIGEST:
      BinToBase64(digest, sizeof(digest), (char*)rec->data,
                  CRYPTO_DIGEST_SHA1_SIZE, true);
      break;
    case STREAM_SHA256_DIGEST:
      BinToBase64(digest, sizeof(digest), (char*)rec->data,
                  CRYPTO_DIGEST_SHA256_SIZE, true);
      break;
    case STREAM_SHA512_DIGEST:
      BinToBase64(digest, sizeof(digest), (char*)rec->data,
                  CRYPTO_DIGEST_SHA512_SIZE, true);
      break;
    default:
      return "";
  }

  resultbuffer.bsprintf("%s (base64)", digest);

  return resultbuffer.c_str();
}

/**
 * Convert a Stream ID into a printable
 * ASCII string. Not reentrant.
 *
 * A negative stream number represents
 * stream data that is continued from a
 * record in the previous block.
 *
 * If the FileIndex is negative, we are
 * dealing with a Label, hence the
 * stream is the JobId.
 */
const char* stream_to_ascii(char* buf, int stream, int fi)
{
  if (fi < 0) {
    sprintf(buf, "%d", stream);
    return buf;
  }

  if (stream < 0) {
    stream = -stream;
    stream &= STREAMMASK_TYPE;
    /*
     * Stream was negative => all are continuation items
     */
    switch (stream) {
      case STREAM_UNIX_ATTRIBUTES:
        return "contUATTR";
      case STREAM_FILE_DATA:
        return "contDATA";
      case STREAM_WIN32_DATA:
        return "contWIN32-DATA";
      case STREAM_WIN32_GZIP_DATA:
        return "contWIN32-GZIP";
      case STREAM_WIN32_COMPRESSED_DATA:
        return "contWIN32-COMPRESSED";
      case STREAM_MD5_DIGEST:
        return "contMD5";
      case STREAM_SHA1_DIGEST:
        return "contSHA1";
      case STREAM_GZIP_DATA:
        return "contGZIP";
      case STREAM_COMPRESSED_DATA:
        return "contCOMPRESSED";
      case STREAM_UNIX_ATTRIBUTES_EX:
        return "contUNIX-Attributes-EX";
      case STREAM_RESTORE_OBJECT:
        return "contRESTORE-OBJECT";
      case STREAM_SPARSE_DATA:
        return "contSPARSE-DATA";
      case STREAM_SPARSE_GZIP_DATA:
        return "contSPARSE-GZIP";
      case STREAM_SPARSE_COMPRESSED_DATA:
        return "contSPARSE-COMPRESSED";
      case STREAM_PROGRAM_NAMES:
        return "contPROG-NAMES";
      case STREAM_PROGRAM_DATA:
        return "contPROG-DATA";
      case STREAM_MACOS_FORK_DATA:
        return "contMACOS-RSRC";
      case STREAM_HFSPLUS_ATTRIBUTES:
        return "contHFSPLUS-Attributes";
      case STREAM_SHA256_DIGEST:
        return "contSHA256";
      case STREAM_SHA512_DIGEST:
        return "contSHA512";
      case STREAM_SIGNED_DIGEST:
        return "contSIGNED-DIGEST";
      case STREAM_ENCRYPTED_SESSION_DATA:
        return "contENCRYPTED-SESSION-DATA";
      case STREAM_ENCRYPTED_FILE_DATA:
        return "contENCRYPTED-FILE";
      case STREAM_ENCRYPTED_FILE_GZIP_DATA:
        return "contENCRYPTED-GZIP";
      case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
        return "contENCRYPTED-COMPRESSED";
      case STREAM_ENCRYPTED_WIN32_DATA:
        return "contENCRYPTED-WIN32-DATA";
      case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
        return "contENCRYPTED-WIN32-GZIP";
      case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
        return "contENCRYPTED-WIN32-COMPRESSED";
      case STREAM_ENCRYPTED_MACOS_FORK_DATA:
        return "contENCRYPTED-MACOS-RSRC";
      case STREAM_PLUGIN_NAME:
        return "contPLUGIN-NAME";
      default:
        sprintf(buf, "%d", -stream);
        return buf;
    }
  }

  switch (stream & STREAMMASK_TYPE) {
    case STREAM_UNIX_ATTRIBUTES:
      return "UATTR";
    case STREAM_FILE_DATA:
      return "DATA";
    case STREAM_WIN32_DATA:
      return "WIN32-DATA";
    case STREAM_WIN32_GZIP_DATA:
      return "WIN32-GZIP";
    case STREAM_WIN32_COMPRESSED_DATA:
      return "WIN32-COMPRESSED";
    case STREAM_MD5_DIGEST:
      return "MD5";
    case STREAM_SHA1_DIGEST:
      return "SHA1";
    case STREAM_GZIP_DATA:
      return "GZIP";
    case STREAM_COMPRESSED_DATA:
      return "COMPRESSED";
    case STREAM_UNIX_ATTRIBUTES_EX:
      return "UNIX-Attributes-EX";
    case STREAM_RESTORE_OBJECT:
      return "RESTORE-OBJECT";
    case STREAM_SPARSE_DATA:
      return "SPARSE-DATA";
    case STREAM_SPARSE_GZIP_DATA:
      return "SPARSE-GZIP";
    case STREAM_SPARSE_COMPRESSED_DATA:
      return "SPARSE-COMPRESSED";
    case STREAM_PROGRAM_NAMES:
      return "PROG-NAMES";
    case STREAM_PROGRAM_DATA:
      return "PROG-DATA";
    case STREAM_PLUGIN_NAME:
      return "PLUGIN-NAME";
    case STREAM_MACOS_FORK_DATA:
      return "MACOS-RSRC";
    case STREAM_HFSPLUS_ATTRIBUTES:
      return "HFSPLUS-Attributes";
    case STREAM_SHA256_DIGEST:
      return "SHA256";
    case STREAM_SHA512_DIGEST:
      return "SHA512";
    case STREAM_SIGNED_DIGEST:
      return "SIGNED-DIGEST";
    case STREAM_ENCRYPTED_SESSION_DATA:
      return "ENCRYPTED-SESSION-DATA";
    case STREAM_ENCRYPTED_FILE_DATA:
      return "ENCRYPTED-FILE";
    case STREAM_ENCRYPTED_FILE_GZIP_DATA:
      return "ENCRYPTED-GZIP";
    case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
      return "ENCRYPTED-COMPRESSED";
    case STREAM_ENCRYPTED_WIN32_DATA:
      return "ENCRYPTED-WIN32-DATA";
    case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
      return "ENCRYPTED-WIN32-GZIP";
    case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
      return "ENCRYPTED-WIN32-COMPRESSED";
    case STREAM_ENCRYPTED_MACOS_FORK_DATA:
      return "ENCRYPTED-MACOS-RSRC";
    default:
      sprintf(buf, "%d", stream);
      return buf;
  }
}

static const char* record_state_to_ascii(rec_state state)
{
  switch (state) {
    case st_none:
      return "st_none";
    case st_header:
      return "st_header";
    case st_header_cont:
      return "st_header_cont";
    case st_data:
      return "st_data";
    default:
      return "<unknown>";
  }
}

static const char* findex_to_str(int32_t index, char* buf, size_t bufsz)
{
  if (index >= 0) {
    Bsnprintf(buf, bufsz, "<User> %d", index);
    return buf;
  }

  FI_to_ascii(buf, index);

  return buf;
}


static const char* record_unix_attributes_to_str(PoolMem& resultbuffer,
                                                 JobControlRecord* jcr,
                                                 const DeviceRecord* rec)
{
  Attributes* attr = new_attr(NULL);

  if (!UnpackAttributesRecord(jcr, rec->Stream, rec->data, rec->data_len,
                              attr)) {
    resultbuffer.bsprintf("ERROR");
    return NULL;
  }

  attr->data_stream =
      DecodeStat(attr->attr, &attr->statp, sizeof(attr->statp), &attr->LinkFI);
  BuildAttrOutputFnames(jcr, attr);
  attr_to_str(resultbuffer, jcr, attr);

  FreeAttr(attr);

  return resultbuffer.c_str();
}


static const char* get_record_short_info(PoolMem& resultbuffer,
                                         JobControlRecord* jcr,
                                         const DeviceRecord* rec)
{
  switch (rec->maskedStream) {
    case STREAM_UNIX_ATTRIBUTES:
    case STREAM_UNIX_ATTRIBUTES_EX:
      record_unix_attributes_to_str(resultbuffer, jcr, rec);
      break;
    case STREAM_MD5_DIGEST:
    case STREAM_SHA1_DIGEST:
    case STREAM_SHA256_DIGEST:
    case STREAM_SHA512_DIGEST:
      record_digest_to_str(resultbuffer, rec);
      break;
    case STREAM_PLUGIN_NAME: {
      char data[100];
      int len = MIN(rec->data_len + 1, sizeof(data));
      bstrncpy(data, rec->data, len);
      resultbuffer.bsprintf("data: %s\n", data);
      break;
    }
    case STREAM_RESTORE_OBJECT:
      resultbuffer.bsprintf("Restore Object record");
      break;
    case STREAM_COMPRESSED_DATA:
    case STREAM_SPARSE_COMPRESSED_DATA:
    case STREAM_WIN32_COMPRESSED_DATA:
    case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
    case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
    case STREAM_GZIP_DATA:        /* deprecated */
    case STREAM_SPARSE_GZIP_DATA: /* deprecated */
      record_compression_to_str(resultbuffer, rec);
      break;
    default:
      break;
  }
  return resultbuffer.c_str();
}

const char* record_to_str(PoolMem& resultbuffer,
                          JobControlRecord* jcr,
                          const DeviceRecord* rec)
{
  PoolMem record_info_buf(PM_MESSAGE);
  char stream_buf[100];

  resultbuffer.bsprintf(
      "FileIndex=%-5d Stream=%-2d %-25s DataLen=%-5d", rec->FileIndex,
      rec->Stream, stream_to_ascii(stream_buf, rec->Stream, rec->FileIndex),
      rec->data_len);
  IndentMultilineString(
      resultbuffer, get_record_short_info(record_info_buf, jcr, rec), " | ");

  return resultbuffer.c_str();
}

void DumpRecord(const char* tag, const DeviceRecord* rec)
{
  char stream[128];
  char findex[128];

  Dmsg2(100, "%s: rec %p\n", tag, rec);

  Dmsg3(100, "%-14s next %p prev %p\n", "link", rec->link.next, rec->link.prev);
  Dmsg2(100, "%-14s %u\n", "File", rec->File);
  Dmsg2(100, "%-14s %u\n", "Block", rec->Block);
  Dmsg2(100, "%-14s %u\n", "VolSessionId", rec->VolSessionId);
  Dmsg2(100, "%-14s %u\n", "VolSessionTime", rec->VolSessionTime);
  Dmsg2(100, "%-14s %s\n", "FileIndex",
        findex_to_str(rec->FileIndex, findex, sizeof(findex)));
  Dmsg2(100, "%-14s %s\n", "Stream",
        stream_to_ascii(stream, rec->Stream, rec->FileIndex));
  Dmsg2(100, "%-14s %d\n", "maskedStream", rec->maskedStream);
  Dmsg2(100, "%-14s %u\n", "data_len", rec->data_len);
  Dmsg2(100, "%-14s %u\n", "remainder", rec->remainder);
  for (unsigned int i = 0;
       i < (sizeof(rec->state_bits) / sizeof(rec->state_bits[0])); i++) {
    Dmsg3(100, "%-11s[%d]        %2.2x\n", "state_bits", i,
          (uint8_t)rec->state_bits[i]);
  }
  Dmsg3(100, "%-14s %u (%s)\n", "state", rec->state,
        record_state_to_ascii(rec->state));
  Dmsg2(100, "%-14s %p\n", "bsr", rec->bsr);
  Dmsg2(100, "%-14s %p\n", "data", rec->data);
  Dmsg2(100, "%-14s %d\n", "match_stat", rec->match_stat);
  Dmsg2(100, "%-14s %u\n", "last_VolSessionId", rec->last_VolSessionId);
  Dmsg2(100, "%-14s %u\n", "last_VolSessionTime", rec->last_VolSessionTime);
  Dmsg2(100, "%-14s %d\n", "last_FileIndex", rec->last_FileIndex);
  Dmsg2(100, "%-14s %d\n", "last_Stream", rec->last_Stream);
  Dmsg2(100, "%-14s %s\n", "own_mempool", rec->own_mempool ? "true" : "false");
}

/**
 * Return a new record entity
 */
DeviceRecord* new_record(bool with_data)
{
  DeviceRecord* rec;

  rec = (DeviceRecord*)GetPoolMemory(PM_RECORD);
  *rec = DeviceRecord{};
  if (with_data) {
    rec->data = GetPoolMemory(PM_MESSAGE);
    rec->own_mempool = true;
  }
  rec->state = st_none;

  return rec;
}

void EmptyRecord(DeviceRecord* rec)
{
  rec->File = rec->Block = 0;
  rec->VolSessionId = rec->VolSessionTime = 0;
  rec->FileIndex = rec->Stream = 0;
  rec->data_len = rec->remainder = 0;

  ClearBit(REC_PARTIAL_RECORD, rec->state_bits);
  ClearBit(REC_BLOCK_EMPTY, rec->state_bits);
  ClearBit(REC_NO_MATCH, rec->state_bits);
  ClearBit(REC_CONTINUATION, rec->state_bits);

  rec->state = st_none;
}

void CopyRecordState(DeviceRecord* dst, DeviceRecord* src)
{
  bool own_mempool;
  int32_t Stream, maskedStream;
  uint32_t data_len;
  POOLMEM* data;

  /*
   * Preserve some important fields all other can be overwritten.
   */
  Stream = dst->Stream;
  maskedStream = dst->maskedStream;
  data = dst->data;
  data_len = dst->data_len;
  own_mempool = dst->own_mempool;

  memcpy(dst, src, sizeof(DeviceRecord));

  dst->Stream = Stream;
  dst->maskedStream = maskedStream;
  dst->data = data;
  dst->data_len = data_len;
  dst->own_mempool = own_mempool;
}

/**
 * Free the record entity
 */
void FreeRecord(DeviceRecord* rec)
{
  Dmsg0(950, "Enter FreeRecord.\n");
  if (rec->data && rec->own_mempool) { FreePoolMemory(rec->data); }
  Dmsg0(950, "Data buf is freed.\n");
  FreePoolMemory((POOLMEM*)rec);
  Dmsg0(950, "Leave FreeRecord.\n");
}

static inline ssize_t WriteHeaderToBlock(DeviceBlock* block,
                                         const DeviceRecord* rec,
                                         int32_t Stream)
{
  ser_declare;

  /*
   * Require enough room to write a full header
   */
  if (BlockWriteNavail(block) < WRITE_RECHDR_LENGTH) return -1;

  SerBegin(block->bufp, WRITE_RECHDR_LENGTH);

  if (BLOCK_VER == 1) {
    ser_uint32(rec->VolSessionId);
    ser_uint32(rec->VolSessionTime);
  } else {
    block->VolSessionId = rec->VolSessionId;
    block->VolSessionTime = rec->VolSessionTime;
  }

  ser_int32(rec->FileIndex);
  ser_int32(Stream);

  ser_uint32(
      rec->remainder); /* each header tracks remaining user bytes to write */

  block->bufp += WRITE_RECHDR_LENGTH;
  block->binbuf += WRITE_RECHDR_LENGTH;

  if (rec->FileIndex > 0) {
    /*
     * If data record, update what we have in this block
     */
    if (block->FirstIndex == 0) { block->FirstIndex = rec->FileIndex; }
    block->LastIndex = rec->FileIndex;
  }

  return WRITE_RECHDR_LENGTH;
}

static inline ssize_t WriteDataToBlock(DeviceBlock* block,
                                       const DeviceRecord* rec)
{
  uint32_t len;

  len = MIN(rec->remainder, BlockWriteNavail(block));
  memcpy(block->bufp,
         ((unsigned char*)rec->data) + (rec->data_len - rec->remainder), len);
  block->bufp += len;
  block->binbuf += len;
  return len;
}

/**
 * Write a Record to the block
 *
 * Returns: false means the block could not be written to tape/disk.
 *          true on success (all bytes written to the block).
 */
bool DeviceControlRecord::WriteRecord()
{
  bool retval = false;
  bool translated_record = false;
  char buf1[100], buf2[100];

  /*
   * Perform record translations.
   */
  before_rec = rec;
  after_rec = NULL;
  if (GeneratePluginEvent(jcr, bsdEventWriteRecordTranslation, this) !=
      bRC_OK) {
    goto bail_out;
  }

  /*
   * The record got translated when we got an after_rec pointer after calling
   * the bsdEventWriteRecordTranslation plugin event. If no translation has
   * taken place we just point the after_rec pointer to same DeviceRecord as in
   * the before_rec pointer.
   */
  if (!after_rec) {
    after_rec = before_rec;
  } else {
    translated_record = true;
  }

  while (!WriteRecordToBlock(this, after_rec)) {
    Dmsg2(850, "!WriteRecordToBlock data_len=%d rem=%d\n", after_rec->data_len,
          after_rec->remainder);
    if (!WriteBlockToDevice()) {
      Dmsg2(90, "Got WriteBlockToDev error on device %s. %s\n",
            dev->print_name(), dev->bstrerror());
      goto bail_out;
    }
  }

  jcr->JobBytes += after_rec->data_len; /* increment bytes this job */
  if (jcr->impl->RemainingQuota && jcr->JobBytes > jcr->impl->RemainingQuota) {
    Jmsg0(jcr, M_FATAL, 0, _("Quota Exceeded. Job Terminated.\n"));
    goto bail_out;
  }

  Dmsg4(850, "WriteRecord FI=%s SessId=%d Strm=%s len=%d\n",
        FI_to_ascii(buf1, after_rec->FileIndex), after_rec->VolSessionId,
        stream_to_ascii(buf2, after_rec->Stream, after_rec->FileIndex),
        after_rec->data_len);

  retval = true;

bail_out:
  if (translated_record) {
    CopyRecordState(before_rec, after_rec);
    FreeRecord(after_rec);
    after_rec = NULL;
  }

  return retval;
}

/**
 * Write a Record to the block
 *
 *  Returns: false on failure (none or partially written)
 *           true  on success (all bytes written)
 *
 *  and remainder returned in packet.
 *
 *  We require enough room for the header, and we deal with
 *  two special cases. 1. Only part of the record may have
 *  been transferred the last time (when remainder is
 *  non-zero), and 2. The remaining bytes to write may not
 *  all fit into the block.
 */
bool WriteRecordToBlock(DeviceControlRecord* dcr, DeviceRecord* rec)
{
  ssize_t n;
  bool retval = false;
  char buf1[100], buf2[100];
  DeviceBlock* block = dcr->block;

  /*
   * After this point the record is in nrec not rec e.g. its either converted
   * or is just a pointer to the same as the rec pointer being passed in.
   */

  while (1) {
    ASSERT(block->binbuf == (uint32_t)(block->bufp - block->buf));
    ASSERT(block->buf_len >= block->binbuf);

    Dmsg9(890,
          "%s() state=%d (%s) FI=%s SessId=%d Strm=%s len=%d "
          "block_navail=%d remainder=%d\n",
          __PRETTY_FUNCTION__, rec->state, record_state_to_ascii(rec->state),
          FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
          stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len,
          BlockWriteNavail(block), rec->remainder);

    switch (rec->state) {
      case st_none:
        /*
         * Figure out what to do
         */
        rec->state = st_header;
        rec->remainder = rec->data_len; /* length of data remaining to write */
        continue;                       /* goto st_header */

      case st_header:
        /*
         * Write header
         */
        n = WriteHeaderToBlock(block, rec, rec->Stream);
        if (n < 0) {
          /*
           * the header did not fit into the block, so flush the current
           * block and come back to st_header and try again on the next block.
           */
          goto bail_out;
        }

        if (BlockWriteNavail(block) == 0) {
          /*
           * The header fit, but no bytes of data will fit,
           * so flush this block and start the next block with a
           * continuation header.
           */
          rec->state = st_header_cont;
          goto bail_out;
        }

        /*
         * The header fit, and at least one byte of data will fit,
         * so move to the st_data state and start filling the block
         * with data bytes
         */
        rec->state = st_data;
        continue;

      case st_header_cont:
        /*
         * Write continuation header
         */
        n = WriteHeaderToBlock(block, rec, -rec->Stream);
        if (n < 0) {
          /*
           * The continuation header wouldn't fit, which is impossible
           * unless something is broken
           */
          Emsg0(M_ABORT, 0, _("couldn't write continuation header\n"));
        }

        /*
         * After successfully writing a continuation header, we always start
         * writing data, even if none will fit into this block.
         */
        rec->state = st_data;

        if (BlockWriteNavail(block) == 0) {
          /*
           * The header fit, but no bytes of data will fit,
           * so flush the block and start the next block with
           * data bytes
           */
          goto bail_out; /* Partial transfer */
        }

        continue;

      case st_data:
        /*
         * Write normal data
         *
         * Part of it may have already been transferred, and we
         * may not have enough room to transfer the whole this time.
         */
        if (rec->remainder > 0) {
          n = WriteDataToBlock(block, rec);
          if (n < 0) {
            /*
             * error appending data to block should be impossible
             * unless something is broken
             */
            Emsg0(M_ABORT, 0, _("data write error\n"));
          }

          rec->remainder -= n;

          if (rec->remainder > 0) {
            /*
             * Could not fit all of the data bytes into this block, so
             * flush the current block, and start the next block with a
             * continuation header
             */
            rec->state = st_header_cont;
            goto bail_out;
          }
        }

        rec->remainder = 0; /* did whole transfer */
        rec->state = st_none;
        retval = true;
        goto bail_out;

      default:
        Emsg1(M_ABORT, 0, _("Something went wrong. Unknown state %d.\n"),
              rec->state);
        rec->state = st_none;
        retval = true;
        goto bail_out;
    }
  }

bail_out:
  return retval;
}

/**
 * Test if we can write whole record to the block
 *
 *  Returns: false on failure
 *           true  on success (all bytes can be written)
 */
bool CanWriteRecordToBlock(DeviceBlock* block, const DeviceRecord* rec)
{
  return BlockWriteNavail(block) >= WRITE_RECHDR_LENGTH + rec->remainder;
}

uint64_t GetRecordAddress(const DeviceRecord* rec)
{
  return ((uint64_t)rec->File) << 32 | rec->Block;
}

/**
 * Read a Record from the block
 *
 * Returns: false if nothing read or if the continuation record does not match.
 *                In both of these cases, a block read must be done.
 *          true  if at least the record header was read, this
 *                routine may have to be called again with a new
 *                block if the entire record was not read.
 */
bool ReadRecordFromBlock(DeviceControlRecord* dcr, DeviceRecord* rec)
{
  ser_declare;
  uint32_t remlen;
  uint32_t VolSessionId;
  uint32_t VolSessionTime;
  int32_t FileIndex;
  int32_t Stream;
  uint32_t data_bytes;
  uint32_t rhl;
  char buf1[100], buf2[100];

  remlen = dcr->block->binbuf;

  /*
   * Clear state flags
   */
  ClearAllBits(REC_STATE_MAX, rec->state_bits);
  if (dcr->block->dev->IsTape()) { SetBit(REC_ISTAPE, rec->state_bits); }
  rec->Block = ((Device*)(dcr->block->dev))->EndBlock;
  rec->File = ((Device*)(dcr->block->dev))->EndFile;

  /*
   * Get the header. There is always a full header, otherwise we find it in the
   * next block.
   */
  Dmsg3(450, "Block=%d Ver=%d size=%u\n", dcr->block->BlockNumber,
        dcr->block->BlockVer, dcr->block->block_len);
  if (dcr->block->BlockVer == 1) {
    rhl = RECHDR1_LENGTH;
  } else {
    rhl = RECHDR2_LENGTH;
  }
  if (remlen >= rhl) {
    Dmsg4(450,
          "Enter read_record_block: remlen=%d data_len=%d rem=%d blkver=%d\n",
          remlen, rec->data_len, rec->remainder, dcr->block->BlockVer);

    UnserBegin(dcr->block->bufp, WRITE_RECHDR_LENGTH);
    if (dcr->block->BlockVer == 1) {
      unser_uint32(VolSessionId);
      unser_uint32(VolSessionTime);
    } else {
      VolSessionId = dcr->block->VolSessionId;
      VolSessionTime = dcr->block->VolSessionTime;
    }
    unser_int32(FileIndex);
    unser_int32(Stream);
    unser_uint32(data_bytes);

    dcr->block->bufp += rhl;
    dcr->block->binbuf -= rhl;
    remlen -= rhl;

    /*
     * If we are looking for more (remainder!=0), we reject anything
     * where the VolSessionId and VolSessionTime don't agree
     */
    if (rec->remainder && (rec->VolSessionId != VolSessionId ||
                           rec->VolSessionTime != VolSessionTime)) {
      SetBit(REC_NO_MATCH, rec->state_bits);
      Dmsg0(450, "remainder and VolSession doesn't match\n");
      return false; /* This is from some other Session */
    }

    /*
     * If Stream is negative, it means that this is a continuation
     * of a previous partially written record.
     */
    if (Stream < 0) { /* continuation record? */
      Dmsg1(500, "Got negative Stream => continuation. remainder=%d\n",
            rec->remainder);
      SetBit(REC_CONTINUATION, rec->state_bits);
      if (!rec->remainder) { /* if we didn't read previously */
        rec->data_len = 0;   /* return data as if no continuation */
      } else if (rec->Stream != -Stream) {
        SetBit(REC_NO_MATCH, rec->state_bits);
        return false; /* This is from some other Session */
      }
      rec->Stream = -Stream; /* set correct Stream */
      rec->maskedStream = rec->Stream & STREAMMASK_TYPE;
    } else { /* Regular record */
      rec->Stream = Stream;
      rec->maskedStream = rec->Stream & STREAMMASK_TYPE;
      rec->data_len = 0; /* transfer to beginning of data */
    }
    rec->VolSessionId = VolSessionId;
    rec->VolSessionTime = VolSessionTime;
    rec->FileIndex = FileIndex;
    if (FileIndex > 0) {
      if (dcr->block->FirstIndex == 0) { dcr->block->FirstIndex = FileIndex; }
      dcr->block->LastIndex = FileIndex;
    }

    Dmsg6(450,
          "rd_rec_blk() got FI=%s SessId=%d Strm=%s len=%u\n"
          "remlen=%d data_len=%d\n",
          FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
          stream_to_ascii(buf2, rec->Stream, rec->FileIndex), data_bytes,
          remlen, rec->data_len);
  } else {
    /*
     * No more records in this block because the number
     * of remaining bytes are less than a record header
     * length, so return empty handed, but indicate that
     * he must read again. By returning, we allow the
     * higher level routine to fetch the next block and
     * then reread.
     */
    Dmsg0(450, "read_record_block: nothing\n");
    SetBit(REC_NO_HEADER, rec->state_bits);
    SetBit(REC_BLOCK_EMPTY, rec->state_bits);
    EmptyBlock(dcr->block); /* mark block empty */
    return false;
  }

  /* Sanity check */
  if (data_bytes >= MAX_BLOCK_LENGTH) {
    /*
     * Something is wrong, force read of next block, abort
     *   continuing with this block.
     */
    SetBit(REC_NO_HEADER, rec->state_bits);
    SetBit(REC_BLOCK_EMPTY, rec->state_bits);
    EmptyBlock(dcr->block);
    Jmsg2(dcr->jcr, M_WARNING, 0,
          _("Sanity check failed. maxlen=%d datalen=%d. Block discarded.\n"),
          MAX_BLOCK_LENGTH, data_bytes);
    return false;
  }

  rec->data = CheckPoolMemorySize(rec->data, rec->data_len + data_bytes);

  /*
   * At this point, we have read the header, now we
   * must transfer as much of the data record as
   * possible taking into account: 1. A partial
   * data record may have previously been transferred,
   * 2. The current block may not contain the whole data
   * record.
   */
  if (remlen >= data_bytes) {
    /*
     * Got whole record
     */
    memcpy(rec->data + rec->data_len, dcr->block->bufp, data_bytes);
    dcr->block->bufp += data_bytes;
    dcr->block->binbuf -= data_bytes;
    rec->data_len += data_bytes;
  } else {
    /*
     * Partial record
     */
    memcpy(rec->data + rec->data_len, dcr->block->bufp, remlen);
    dcr->block->bufp += remlen;
    dcr->block->binbuf -= remlen;
    rec->data_len += remlen;
    rec->remainder = 1; /* partial record transferred */
    Dmsg1(450, "read_record_block: partial xfered=%d\n", rec->data_len);
    SetBit(REC_PARTIAL_RECORD, rec->state_bits);
    SetBit(REC_BLOCK_EMPTY, rec->state_bits);
    return true;
  }
  rec->remainder = 0;

  Dmsg4(450, "Rtn full rd_rec_blk FI=%s SessId=%d Strm=%s len=%d\n",
        FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
        stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);

  return true; /* transferred full record */
}

} /* namespace storagedaemon */
