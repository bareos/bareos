/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *
 *   record.c -- Volume (tape/disk) record handling functions
 *
 *              Kern Sibbald, April MMI
 *                added BB02 format October MMII
 *
 */


#include "bacula.h"
#include "stored.h"

/*
 * Convert a FileIndex into a printable
 *   ASCII string.  Not reentrant.
 * If the FileIndex is negative, it flags the
 *   record as a Label, otherwise it is simply
 *   the FileIndex of the current file.
 */
const char *FI_to_ascii(char *buf, int fi)
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


/*
 * Convert a Stream ID into a printable
 * ASCII string.  Not reentrant.

 * A negative stream number represents
 *   stream data that is continued from a
 *   record in the previous block.
 * If the FileIndex is negative, we are
 *   dealing with a Label, hence the
 *   stream is the JobId.
 */
const char *stream_to_ascii(char *buf, int stream, int fi)
{

   if (fi < 0) {
      sprintf(buf, "%d", stream);
      return buf;
   }
   if (stream < 0) {
      stream = -stream;
      stream &= STREAMMASK_TYPE;
      /* Stream was negative => all are continuation items */
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
         return "contUNIX-ATTR-EX";
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
         return "contHFSPLUS-ATTR";
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
      return "UNIX-ATTR-EX";
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
      return "HFSPLUS-ATTR";
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

/*
 * Return a new record entity
 */
DEV_RECORD *new_record(void)
{
   DEV_RECORD *rec;

   rec = (DEV_RECORD *)get_memory(sizeof(DEV_RECORD));
   memset(rec, 0, sizeof(DEV_RECORD));
   rec->data = get_pool_memory(PM_MESSAGE);
   rec->state = st_none;
   return rec;
}

void empty_record(DEV_RECORD *rec)
{
   rec->File = rec->Block = 0;
   rec->VolSessionId = rec->VolSessionTime = 0;
   rec->FileIndex = rec->Stream = 0;
   rec->data_len = rec->remainder = 0;
   rec->state_bits &= ~(REC_PARTIAL_RECORD|REC_BLOCK_EMPTY|REC_NO_MATCH|REC_CONTINUATION);
   rec->state = st_none;
}

/*
 * Free the record entity
 *
 */
void free_record(DEV_RECORD *rec)
{
   Dmsg0(950, "Enter free_record.\n");
   if (rec->data) {
      free_pool_memory(rec->data);
   }
   Dmsg0(950, "Data buf is freed.\n");
   free_pool_memory((POOLMEM *)rec);
   Dmsg0(950, "Leave free_record.\n");
}

static bool write_header_to_block(DEV_BLOCK *block, DEV_RECORD *rec)
{
   ser_declare;

   rec->remlen = block->buf_len - block->binbuf;
   /* Require enough room to write a full header */
   if (rec->remlen >= WRITE_RECHDR_LENGTH) {
      ser_begin(block->bufp, WRITE_RECHDR_LENGTH);
      if (BLOCK_VER == 1) {
         ser_uint32(rec->VolSessionId);
         ser_uint32(rec->VolSessionTime);
      } else {
         block->VolSessionId = rec->VolSessionId;
         block->VolSessionTime = rec->VolSessionTime;
      }
      ser_int32(rec->FileIndex);
      ser_int32(rec->Stream);
      ser_uint32(rec->data_len);

      block->bufp += WRITE_RECHDR_LENGTH;
      block->binbuf += WRITE_RECHDR_LENGTH;
      rec->remlen -= WRITE_RECHDR_LENGTH;
      rec->remainder = rec->data_len;
      if (rec->FileIndex > 0) {
         /* If data record, update what we have in this block */
         if (block->FirstIndex == 0) {
            block->FirstIndex = rec->FileIndex;
         }
         block->LastIndex = rec->FileIndex;
      }
   } else {
      rec->remainder = rec->data_len + WRITE_RECHDR_LENGTH;
      return false;
   }
   return true;
}

static void write_continue_header_to_block(DEV_BLOCK *block, DEV_RECORD *rec)
{
   ser_declare;

   rec->remlen = block->buf_len - block->binbuf;
   /*
    * We have unwritten bytes from a previous
    * time. Presumably we have a new buffer (possibly
    * containing a volume label), so the new header
    * should be able to fit in the block -- otherwise we have
    * an error.  Note, we have to continue splitting the
    * data record if it is longer than the block.
    *
    * First, write the header.
    *
    * Every time we write a header and it is a continuation
    * of a previous partially written record, we store the
    * Stream as -Stream in the record header.
    */
   ser_begin(block->bufp, WRITE_RECHDR_LENGTH);
   if (BLOCK_VER == 1) {
      ser_uint32(rec->VolSessionId);
      ser_uint32(rec->VolSessionTime);
   } else {
      block->VolSessionId = rec->VolSessionId;
      block->VolSessionTime = rec->VolSessionTime;
   }
   ser_int32(rec->FileIndex);
   if (rec->remainder > rec->data_len) {
      ser_int32(rec->Stream);      /* normal full header */
      ser_uint32(rec->data_len);
      rec->remainder = rec->data_len; /* must still do data record */
   } else {
      ser_int32(-rec->Stream);     /* mark this as a continuation record */
      ser_uint32(rec->remainder);  /* bytes to do */
   }

   /* Require enough room to write a full header */
   ASSERT(rec->remlen >= WRITE_RECHDR_LENGTH);

   block->bufp += WRITE_RECHDR_LENGTH;
   block->binbuf += WRITE_RECHDR_LENGTH;
   rec->remlen -= WRITE_RECHDR_LENGTH;
   if (rec->FileIndex > 0) {
      /* If data record, update what we have in this block */
      if (block->FirstIndex == 0) {
         block->FirstIndex = rec->FileIndex;
      }
      block->LastIndex = rec->FileIndex;
   }
}

static bool write_data_to_block(DEV_BLOCK *block, DEV_RECORD *rec)
{
   rec->remlen = block->buf_len - block->binbuf;
   /* Write as much of data as possible */
   if (rec->remlen >= rec->remainder) {
      memcpy(block->bufp, rec->data+rec->data_len-rec->remainder,
             rec->remainder);
      block->bufp += rec->remainder;
      block->binbuf += rec->remainder;
   } else {
      memcpy(block->bufp, rec->data+rec->data_len-rec->remainder,
             rec->remlen);
#ifdef xxxxxSMCHECK
      if (!sm_check_rtn(__FILE__, __LINE__, False)) {
         /* We damaged a buffer */
         Dmsg6(0, "Damaged block FI=%s SessId=%d Strm=%s len=%d\n"
            "rem=%d remainder=%d\n",
            FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
            stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len,
            rec->remlen, rec->remainder);
         Dmsg5(0, "Damaged block: bufp=%x binbuf=%d buf_len=%d rem=%d moved=%d\n",
            block->bufp, block->binbuf, block->buf_len, block->buf_len-block->binbuf,
            rec->remlen);
         Dmsg2(0, "Damaged block: buf=%x binbuffrombuf=%d \n",
            block->buf, block->bufp-block->buf);
         Emsg0(M_ABORT, 0, _("Damaged buffer\n"));
      }
#endif

      block->bufp += rec->remlen;
      block->binbuf += rec->remlen;
      rec->remainder -= rec->remlen;
      return false;                /* did partial transfer */
   }
   return true;
}

/*
 * Write a Record to the block
 *
 *  Returns: false means the block could not be written to tape/disk.
 *           true  on success (all bytes written to the block).
 */
bool DCR::write_record(DEV_RECORD *rec)
{
   while (!write_record_to_block(this, rec)) {
      Dmsg2(850, "!write_record_to_block data_len=%d rem=%d\n", rec->data_len,
                 rec->remainder);
      if (!write_block_to_device()) {
         Dmsg2(90, "Got write_block_to_dev error on device %s. %s\n",
            dev->print_name(), dev->bstrerror());
         return false;
      }
   }
   return true;
}

/*
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
bool write_record_to_block(DCR *dcr, DEV_RECORD *rec)
{
   char buf1[100], buf2[100];
   DEV_BLOCK *block = dcr->block;

   for ( ;; ) {
      ASSERT(block->binbuf == (uint32_t)(block->bufp - block->buf));
      ASSERT(block->buf_len >= block->binbuf);

      Dmsg6(890, "write_record_to_block() FI=%s SessId=%d Strm=%s len=%d\n"
         "rem=%d remainder=%d\n",
         FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
         stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len,
         rec->remlen, rec->remainder);

      switch (rec->state) {
      case st_none:
         /* Figure out what to do */
         rec->state = st_header;
         continue;              /* go to next state */

      case st_header:
         /*
          * Write header
          *
          * If rec->remainder is non-zero, we have been called a
          *  second (or subsequent) time to finish writing a record
          *  that did not previously fit into the block.
          */
          if (!write_header_to_block(block, rec)) {
             return false;        /* write block then come back here */
          }
          rec->state = st_data;  /* after header, now write data */
          continue;

      /* Write continuation header */
      case st_header_cont:
         write_continue_header_to_block(block, rec);
         rec->state = st_data;
         if (rec->remlen == 0) {
            return false;                   /* partial transfer */
         }
         continue;

      /* Write normal data */
      case st_data:
         /*
          * Write data
          *
          * Part of it may have already been transferred, and we
          * may not have enough room to transfer the whole this time.
          */
         if (rec->remainder > 0) {
            if (!write_data_to_block(block, rec)) {
               rec->state = st_header_cont;
               return false;
            }
         }
         rec->remainder = 0;                /* did whole transfer */
         rec->state = st_none;
         return true;

      default:
         Dmsg0(000, "Something went wrong. Default state.\n");
         rec->state = st_none;
         return true;
      }
   }
   return true;
}

/*
 * Test if we can write whole record to the block
 *
 *  Returns: false on failure
 *           true  on success (all bytes can be written)
 */
bool can_write_record_to_block(DEV_BLOCK *block, DEV_RECORD *rec)
{
   uint32_t remlen;

   remlen = block->buf_len - block->binbuf;
   if (rec->remainder == 0) {
      if (remlen >= WRITE_RECHDR_LENGTH) {
         remlen -= WRITE_RECHDR_LENGTH;
         rec->remainder = rec->data_len;
      } else {
         return false;
      }
   } else {
      return false;
   }
   if (rec->remainder > 0 && remlen < rec->remainder) {
      return false;
   }
   return true;
}

uint64_t get_record_address(DEV_RECORD *rec)
{
   return ((uint64_t)rec->File)<<32 | rec->Block;
}

/*
 * Read a Record from the block
 *  Returns: false if nothing read or if the continuation record does not match.
 *                 In both of these cases, a block read must be done.
 *           true  if at least the record header was read, this
 *                 routine may have to be called again with a new
 *                 block if the entire record was not read.
 */
bool read_record_from_block(DCR *dcr, DEV_RECORD *rec)
{
   ser_declare;
   uint32_t remlen;
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   int32_t  FileIndex;
   int32_t  Stream;
   uint32_t data_bytes;
   uint32_t rhl;
   char buf1[100], buf2[100];

   remlen = dcr->block->binbuf;

   /* Clear state flags */
   rec->state_bits = 0;
   if (dcr->block->dev->is_tape()) {
      rec->state_bits |= REC_ISTAPE;
   }
   rec->Block = ((DEVICE *)(dcr->block->dev))->EndBlock;
   rec->File = ((DEVICE *)(dcr->block->dev))->EndFile;

   /*
    * Get the header. There is always a full header,
    * otherwise we find it in the next block.
    */
   Dmsg3(450, "Block=%d Ver=%d size=%u\n", dcr->block->BlockNumber, dcr->block->BlockVer,
         dcr->block->block_len);
   if (dcr->block->BlockVer == 1) {
      rhl = RECHDR1_LENGTH;
   } else {
      rhl = RECHDR2_LENGTH;
   }
   if (remlen >= rhl) {
      Dmsg4(450, "Enter read_record_block: remlen=%d data_len=%d rem=%d blkver=%d\n",
            remlen, rec->data_len, rec->remainder, dcr->block->BlockVer);

      unser_begin(dcr->block->bufp, WRITE_RECHDR_LENGTH);
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

      /* If we are looking for more (remainder!=0), we reject anything
       *  where the VolSessionId and VolSessionTime don't agree
       */
      if (rec->remainder && (rec->VolSessionId != VolSessionId ||
                             rec->VolSessionTime != VolSessionTime)) {
         rec->state_bits |= REC_NO_MATCH;
         Dmsg0(450, "remainder and VolSession doesn't match\n");
         return false;             /* This is from some other Session */
      }

      /* if Stream is negative, it means that this is a continuation
       * of a previous partially written record.
       */
      if (Stream < 0) {               /* continuation record? */
         Dmsg1(500, "Got negative Stream => continuation. remainder=%d\n",
            rec->remainder);
         rec->state_bits |= REC_CONTINUATION;
         if (!rec->remainder) {       /* if we didn't read previously */
            rec->data_len = 0;        /* return data as if no continuation */
         } else if (rec->Stream != -Stream) {
            rec->state_bits |= REC_NO_MATCH;
            return false;             /* This is from some other Session */
         }
         rec->Stream = -Stream;       /* set correct Stream */
         rec->maskedStream = rec->Stream & STREAMMASK_TYPE;
      } else {                        /* Regular record */
         rec->Stream = Stream;
         rec->maskedStream = rec->Stream & STREAMMASK_TYPE;
         rec->data_len = 0;           /* transfer to beginning of data */
      }
      rec->VolSessionId = VolSessionId;
      rec->VolSessionTime = VolSessionTime;
      rec->FileIndex = FileIndex;
      if (FileIndex > 0) {
         if (dcr->block->FirstIndex == 0) {
            dcr->block->FirstIndex = FileIndex;
         }
         dcr->block->LastIndex = FileIndex;
      }

      Dmsg6(450, "rd_rec_blk() got FI=%s SessId=%d Strm=%s len=%u\n"
                 "remlen=%d data_len=%d\n",
         FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
         stream_to_ascii(buf2, rec->Stream, rec->FileIndex), data_bytes, remlen,
         rec->data_len);
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
      rec->state_bits |= (REC_NO_HEADER | REC_BLOCK_EMPTY);
      empty_block(dcr->block);                 /* mark block empty */
      return false;
   }

   /* Sanity check */
   if (data_bytes >= MAX_BLOCK_LENGTH) {
      /*
       * Something is wrong, force read of next block, abort 
       *   continuing with this block.
       */
      rec->state_bits |= (REC_NO_HEADER | REC_BLOCK_EMPTY);
      empty_block(dcr->block);
      Jmsg2(dcr->jcr, M_WARNING, 0, _("Sanity check failed. maxlen=%d datalen=%d. Block discarded.\n"),
         MAX_BLOCK_LENGTH, data_bytes);
      return false;
   }

   rec->data = check_pool_memory_size(rec->data, rec->data_len+data_bytes);

   /*
    * At this point, we have read the header, now we
    * must transfer as much of the data record as
    * possible taking into account: 1. A partial
    * data record may have previously been transferred,
    * 2. The current block may not contain the whole data
    * record.
    */
   if (remlen >= data_bytes) {
      /* Got whole record */
      memcpy(rec->data+rec->data_len, dcr->block->bufp, data_bytes);
      dcr->block->bufp += data_bytes;
      dcr->block->binbuf -= data_bytes;
      rec->data_len += data_bytes;
   } else {
      /* Partial record */
      memcpy(rec->data+rec->data_len, dcr->block->bufp, remlen);
      dcr->block->bufp += remlen;
      dcr->block->binbuf -= remlen;
      rec->data_len += remlen;
      rec->remainder = 1;             /* partial record transferred */
      Dmsg1(450, "read_record_block: partial xfered=%d\n", rec->data_len);
      rec->state_bits |= (REC_PARTIAL_RECORD | REC_BLOCK_EMPTY);
      return true;
   }
   rec->remainder = 0;
   Dmsg4(450, "Rtn full rd_rec_blk FI=%s SessId=%d Strm=%s len=%d\n",
      FI_to_ascii(buf1, rec->FileIndex), rec->VolSessionId,
      stream_to_ascii(buf2, rec->Stream, rec->FileIndex), rec->data_len);
   return true;                       /* transferred full record */
}
