/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * Kern Sibbald, March MMI
 * added BB02 format October MMII
 */
/**
 @file
 * tape block handling functions
 */

#include "bareos.h"
#include "stored.h"

#ifdef DEBUG_BLOCK_CHECKSUM
static const bool debug_block_checksum = true;
#else
static const bool debug_block_checksum = false;
#endif

#ifdef NO_TAPE_WRITE_TEST
static const bool no_tape_write_test = true;
#else
static const bool no_tape_write_test = false;
#endif

static bool terminate_writing_volume(DCR *dcr);
static bool do_new_file_bookkeeping(DCR *dcr);
static void reread_last_block(DCR *dcr);

bool forge_on = false;                /* proceed inspite of I/O errors */

/**
 * Dump the block header, then walk through
 * the block printing out the record headers.
 */
void dump_block(DEV_BLOCK *b, const char *msg)
{
   ser_declare;
   char *p;
   char Id[BLKHDR_ID_LENGTH+1];
   uint32_t CheckSum, BlockCheckSum;
   uint32_t block_len;
   uint32_t BlockNumber;
   uint32_t VolSessionId, VolSessionTime, data_len;
   int32_t  FileIndex;
   int32_t  Stream;
   int bhl, rhl;
   char buf1[100], buf2[100];

   unser_begin(b->buf, BLKHDR1_LENGTH);
   unser_uint32(CheckSum);
   unser_uint32(block_len);
   unser_uint32(BlockNumber);
   unser_bytes(Id, BLKHDR_ID_LENGTH);
   ASSERT(unser_length(b->buf) == BLKHDR1_LENGTH);
   Id[BLKHDR_ID_LENGTH] = 0;
   if (Id[3] == '2') {
      unser_uint32(VolSessionId);
      unser_uint32(VolSessionTime);
      bhl = BLKHDR2_LENGTH;
      rhl = RECHDR2_LENGTH;
   } else {
      VolSessionId = VolSessionTime = 0;
      bhl = BLKHDR1_LENGTH;
      rhl = RECHDR1_LENGTH;
   }

   if (block_len > 4000000) {
      Dmsg3(20, "Dump block %s 0x%x blocksize too big %u\n", msg, b, block_len);
      return;
   }

   BlockCheckSum = bcrc32((uint8_t *)b->buf+BLKHDR_CS_LENGTH,
                         block_len-BLKHDR_CS_LENGTH);
   Pmsg6(000, _("Dump block %s %x: size=%d BlkNum=%d\n"
                "               Hdrcksum=%x cksum=%x\n"),
      msg, b, block_len, BlockNumber, CheckSum, BlockCheckSum);
   p = b->buf + bhl;
   while (p < (b->buf + block_len+WRITE_RECHDR_LENGTH)) {
      unser_begin(p, WRITE_RECHDR_LENGTH);
      if (rhl == RECHDR1_LENGTH) {
         unser_uint32(VolSessionId);
         unser_uint32(VolSessionTime);
      }
      unser_int32(FileIndex);
      unser_int32(Stream);
      unser_uint32(data_len);
      Pmsg6(000, _("   Rec: VId=%u VT=%u FI=%s Strm=%s len=%d p=%x\n"),
           VolSessionId, VolSessionTime, FI_to_ascii(buf1, FileIndex),
           stream_to_ascii(buf2, Stream, FileIndex), data_len, p);
      p += data_len + rhl;
  }
}

/**
 * Create a new block structure.
 * We pass device so that the block can inherit the
 * min and max block sizes.
 */
DEV_BLOCK *new_block(DEVICE *dev)
{
   DEV_BLOCK *block = (DEV_BLOCK *)get_memory(sizeof(DEV_BLOCK));

   memset(block, 0, sizeof(DEV_BLOCK));

   if (dev->max_block_size == 0) {
      block->buf_len = dev->device->label_block_size;
      Dmsg1(100, "created new block of blocksize %d (dev->device->label_block_size) as dev->max_block_size is zero\n", block->buf_len);
   } else {
      block->buf_len = dev->max_block_size;
      Dmsg1(100, "created new block of blocksize %d (dev->max_block_size)\n", block->buf_len);
   }
   block->dev = dev;
   block->block_len = block->buf_len;  /* default block size */
   block->buf = get_memory(block->buf_len);
   empty_block(block);
   block->BlockVer = BLOCK_VER;       /* default write version */
   Dmsg1(650, "Returning new block=%x\n", block);
   return block;
}

/**
 * Duplicate an existing block (eblock)
 */
DEV_BLOCK *dup_block(DEV_BLOCK *eblock)
{
   DEV_BLOCK *block = (DEV_BLOCK *)get_memory(sizeof(DEV_BLOCK));
   int buf_len = sizeof_pool_memory(eblock->buf);

   memcpy(block, eblock, sizeof(DEV_BLOCK));
   block->buf = get_memory(buf_len);
   memcpy(block->buf, eblock->buf, buf_len);
   return block;
}

/**
 * Only the first block checksum error was reported.
 *   If there are more, report it now.
 */
void print_block_read_errors(JCR *jcr, DEV_BLOCK *block)
{
   if (block->read_errors > 1) {
      Jmsg(jcr, M_ERROR, 0, _("%d block read errors not printed.\n"),
         block->read_errors);
   }
}

/**
 * Free block
 */
void free_block(DEV_BLOCK *block)
{
   if (block) {
      Dmsg1(999, "free_block buffer %x\n", block->buf);
      free_memory(block->buf);
      Dmsg1(999, "free_block block %x\n", block);
      free_memory((POOLMEM *)block);
   }
}

/**
 * Empty the block -- for writing
 */
void empty_block(DEV_BLOCK *block)
{
   block->binbuf = WRITE_BLKHDR_LENGTH;
   block->bufp = block->buf + block->binbuf;
   block->read_len = 0;
   block->write_failed = false;
   block->block_read = false;
   block->FirstIndex = block->LastIndex = 0;
}

/**
 * Create block header just before write. The space
 * in the buffer should have already been reserved by
 * init_block.
 */
static uint32_t ser_block_header(DEV_BLOCK *block, bool do_checksum)
{
   ser_declare;
   uint32_t CheckSum = 0;
   uint32_t block_len = block->binbuf;

   Dmsg1(1390, "ser_block_header: block_len=%d\n", block_len);
   ser_begin(block->buf, BLKHDR2_LENGTH);
   ser_uint32(CheckSum);
   ser_uint32(block_len);
   ser_uint32(block->BlockNumber);
   ser_bytes(WRITE_BLKHDR_ID, BLKHDR_ID_LENGTH);
   if (BLOCK_VER >= 2) {
      ser_uint32(block->VolSessionId);
      ser_uint32(block->VolSessionTime);
   }

   /*
    * Checksum whole block except for the checksum
    */
   if (do_checksum) {
      CheckSum = bcrc32((uint8_t *)block->buf+BLKHDR_CS_LENGTH,
                    block_len-BLKHDR_CS_LENGTH);
   }
   Dmsg1(1390, "ser_bloc_header: checksum=%x\n", CheckSum);
   ser_begin(block->buf, BLKHDR2_LENGTH);
   ser_uint32(CheckSum);              /* now add checksum to block header */
   return CheckSum;
}

/**
 * Unserialize the block header for reading block.
 * This includes setting all the buffer pointers correctly.
 *
 * Returns: false on failure (not a block)
 *          true  on success
 */
static inline bool unser_block_header(JCR *jcr, DEVICE *dev, DEV_BLOCK *block)
{
   ser_declare;
   char Id[BLKHDR_ID_LENGTH+1];
   uint32_t CheckSum, BlockCheckSum;
   uint32_t block_len;
   uint32_t block_end;
   uint32_t BlockNumber;
   int bhl;

   unser_begin(block->buf, BLKHDR_LENGTH);
   unser_uint32(CheckSum);
   unser_uint32(block_len);
   unser_uint32(BlockNumber);
   unser_bytes(Id, BLKHDR_ID_LENGTH);
   ASSERT(unser_length(block->buf) == BLKHDR1_LENGTH);

   Id[BLKHDR_ID_LENGTH] = 0;
   if (Id[3] == '1') {
      bhl = BLKHDR1_LENGTH;
      block->BlockVer = 1;
      block->bufp = block->buf + bhl;
      if (!bstrncmp(Id, BLKHDR1_ID, BLKHDR_ID_LENGTH)) {
         dev->dev_errno = EIO;
         Mmsg4(dev->errmsg, _("Volume data error at %u:%u! Wanted ID: \"%s\", got \"%s\". Buffer discarded.\n"),
            dev->file, dev->block_num, BLKHDR1_ID, Id);
         if (block->read_errors == 0 || verbose >= 2) {
            Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
         }
         block->read_errors++;
         return false;
      }
   } else if (Id[3] == '2') {
      unser_uint32(block->VolSessionId);
      unser_uint32(block->VolSessionTime);
      bhl = BLKHDR2_LENGTH;
      block->BlockVer = 2;
      block->bufp = block->buf + bhl;
      if (!bstrncmp(Id, BLKHDR2_ID, BLKHDR_ID_LENGTH)) {
         dev->dev_errno = EIO;
         Mmsg4(dev->errmsg, _("Volume data error at %u:%u! Wanted ID: \"%s\", got \"%s\". Buffer discarded.\n"),
            dev->file, dev->block_num, BLKHDR2_ID, Id);
         if (block->read_errors == 0 || verbose >= 2) {
            Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
         }
         block->read_errors++;
         return false;
      }
   } else {
      dev->dev_errno = EIO;
      Mmsg4(dev->errmsg, _("Volume data error at %u:%u! Wanted ID: \"%s\", got \"%s\". Buffer discarded.\n"),
          dev->file, dev->block_num, BLKHDR2_ID, Id);
      Dmsg1(50, "%s", dev->errmsg);
      if (block->read_errors == 0 || verbose >= 2) {
         Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
      }
      block->read_errors++;
      unser_uint32(block->VolSessionId);
      unser_uint32(block->VolSessionTime);
      return false;
   }

   /*
    * Sanity check
    */
   if (block_len > MAX_BLOCK_LENGTH) {
      dev->dev_errno = EIO;
      Mmsg3(dev->errmsg,  _("Volume data error at %u:%u! Block length %u is insane (too large), probably due to a bad archive.\n"),
         dev->file, dev->block_num, block_len);
      if (block->read_errors == 0 || verbose >= 2) {
         Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
      }
      block->read_errors++;
      return false;
   }

   Dmsg1(390, "unser_block_header block_len=%d\n", block_len);
   /*
    * Find end of block or end of buffer whichever is smaller
    */
   if (block_len > block->read_len) {
      block_end = block->read_len;
   } else {
      block_end = block_len;
   }
   block->binbuf = block_end - bhl;
   block->block_len = block_len;
   block->BlockNumber = BlockNumber;
   Dmsg3(390, "Read binbuf = %d %d block_len=%d\n", block->binbuf,
      bhl, block_len);
   if (block_len <= block->read_len && dev->do_checksum()) {
      BlockCheckSum = bcrc32((uint8_t *)block->buf+BLKHDR_CS_LENGTH,
                         block_len-BLKHDR_CS_LENGTH);
      if (BlockCheckSum != CheckSum) {
         dev->dev_errno = EIO;
         Mmsg6(dev->errmsg, _("Volume data error at %u:%u!\n"
                              "Block checksum mismatch in block=%u len=%d: calc=%x blk=%x\n"),
            dev->file, dev->block_num, (unsigned)BlockNumber,
            block_len, BlockCheckSum, CheckSum);
         if (block->read_errors == 0 || verbose >= 2) {
            Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
            dump_block(block, "with checksum error");
         }
         block->read_errors++;
         if (!forge_on) {
            return false;
         }
      }
   }
   return true;
}

/**
 * Write a block to the device, with locking and unlocking
 *
 * Returns: true  on success
 *        : false on failure
 *
 */
bool DCR::write_block_to_device()
{
   bool status = true;
   DCR *dcr = this;

   if (dcr->spooling) {
      status = write_block_to_spool_file(dcr);
      return status;
   }

   if (!dcr->is_dev_locked()) {        /* device already locked? */
      /*
       * Note, do not change this to dcr->r_dlock
       */
      dev->rLock();                  /* no, lock it */
   }

   /*
    * If a new volume has been mounted since our last write
    * Create a JobMedia record for the previous volume written,
    * and set new parameters to write this volume
    *
    * The same applies for if we are in a new file.
    */
   if (dcr->NewVol || dcr->NewFile) {
      if (job_canceled(jcr)) {
         status = false;
         Dmsg0(100, "Canceled\n");
         goto bail_out;
      }
      /* Create a jobmedia record for this job */
      if (!dcr->dir_create_jobmedia_record(false)) {
         dev->dev_errno = EIO;
         Jmsg2(jcr, M_FATAL, 0, _("Could not create JobMedia record for Volume=\"%s\" Job=%s\n"),
            dcr->getVolCatName(), jcr->Job);
         set_new_volume_parameters(dcr);
         status = false;
         Dmsg0(100, "cannot create media record\n");
         goto bail_out;
      }
      if (dcr->NewVol) {
         /*
          * Note, setting a new volume also handles any pending new file
          */
         set_new_volume_parameters(dcr);
      } else {
         set_new_file_parameters(dcr);
      }
   }

   if (!dcr->write_block_to_dev()) {
       if (job_canceled(jcr) || jcr->is_JobType(JT_SYSTEM)) {
          status = false;
       } else {
          status = fixup_device_block_write_error(dcr);
       }
   }

bail_out:
   if (!dcr->is_dev_locked()) {        /* did we lock dev above? */
      /*
       * Note, do not change this to dcr->dunlock
       */
      dev->Unlock();                   /* unlock it now */
   }
   return status;
}

/**
 * Write a block to the device
 *
 * Returns: true  on success or EOT
 *          false on hard error
 */
bool DCR::write_block_to_dev()
{
   ssize_t status = 0;
   uint32_t wlen;                     /* length to write */
   int hit_max1, hit_max2;
   bool ok = true;
   DCR *dcr = this;
   uint32_t checksum;

   if (no_tape_write_test) {
      empty_block(block);
      return true;
   }
   if (job_canceled(jcr)) {
      Dmsg0(100, "return write_block_to_dev, job is canceled\n");
      return false;
   }

   ASSERT(block->binbuf == ((uint32_t) (block->bufp - block->buf)));

   wlen = block->binbuf;
   if (wlen <= WRITE_BLKHDR_LENGTH) {  /* Does block have data in it? */
      Dmsg0(100, "return write_block_to_dev no data to write\n");
      return true;
   }

   /* dump_block(block, "before write"); */
   if (dev->at_weot()) {
      Dmsg0(100, "return write_block_to_dev with ST_WEOT\n");
      dev->dev_errno = ENOSPC;
      Jmsg1(jcr, M_FATAL, 0,  _("Cannot write block. Device at EOM. dev=%s\n"), dev->print_name());
      Dmsg1(100, "Attempt to write on read-only Volume. dev=%s\n", dev->print_name());
      return false;
   }
   if (!dev->can_append()) {
      dev->dev_errno = EIO;
      Jmsg1(jcr, M_FATAL, 0, _("Attempt to write on read-only Volume. dev=%s\n"), dev->print_name());
      Dmsg1(100, "Attempt to write on read-only Volume. dev=%s\n", dev->print_name());
      return false;
   }

   if (!dev->is_open()) {
      Jmsg1(jcr, M_FATAL, 0, _("Attempt to write on closed device=%s\n"), dev->print_name());
      Dmsg1(100, "Attempt to write on closed device=%s\n", dev->print_name());
      return false;
   }

   /*
    * Clear to the end of the buffer if it is not full,
    * and on devices with CAP_ADJWRITESIZE set, apply min and fixed blocking.
    */
   if (wlen != block->buf_len) {
      uint32_t blen = wlen;                  /* current buffer length */

      Dmsg2(250, "binbuf=%d buf_len=%d\n", block->binbuf, block->buf_len);

      if (!dev->has_cap(CAP_ADJWRITESIZE)) {
         Dmsg1(400, "%s: block write size is not adjustable", dev->print_name());
      } else {
         /* (dev->has_cap(CAP_ADJWRITESIZE)) */
         if (dev->min_block_size == dev->max_block_size) {
            /*
             * Fixed block size
             */
            wlen = block->buf_len;    /* fixed block size already rounded */
         } else if (wlen < dev->min_block_size) {
            /*
             * Min block size
             */
            wlen =  ((dev->min_block_size + TAPE_BSIZE - 1) / TAPE_BSIZE) * TAPE_BSIZE;
         } else {
            /*
             * Ensure size is rounded
             */
            wlen = ((wlen + TAPE_BSIZE - 1) / TAPE_BSIZE) * TAPE_BSIZE;
         }

         if (wlen - blen > 0) {
            memset(block->bufp, 0, wlen - blen); /* clear garbage */
         }
      }

   }

   Dmsg5(400, "dev=%s: writing %d bytes as block of %d bytes. Block sizes: min=%d, max=%d\n",
         dev->print_name(), block->binbuf, wlen, dev->min_block_size, dev->max_block_size);

   checksum = ser_block_header(block, dev->do_checksum());

   /*
    * Limit maximum Volume size to value specified by user
    */
   hit_max1 = (dev->max_volume_size > 0) &&
              ((dev->VolCatInfo.VolCatBytes + block->binbuf)) >= dev->max_volume_size;
   hit_max2 = (dev->VolCatInfo.VolCatMaxBytes > 0) &&
              ((dev->VolCatInfo.VolCatBytes + block->binbuf)) >= dev->VolCatInfo.VolCatMaxBytes;

   if (hit_max1 || hit_max2) {
      char ed1[50];
      uint64_t max_cap;

      Dmsg0(100, "==== Output bytes Triggered medium max capacity.\n");
      if (hit_max1) {
         max_cap = dev->max_volume_size;
      } else {
         max_cap = dev->VolCatInfo.VolCatMaxBytes;
      }
      Jmsg(jcr, M_INFO, 0, _("User defined maximum volume capacity %s exceeded on device %s.\n"),
            edit_uint64_with_commas(max_cap, ed1),  dev->print_name());
      terminate_writing_volume(dcr);
      reread_last_block(dcr);   /* DEBUG */
      dev->dev_errno = ENOSPC;

      return false;
   }

   /*
    * Limit maximum File size on volume to user specified value
    */
   if ((dev->max_file_size > 0) &&
       (dev->file_size+block->binbuf) >= dev->max_file_size) {
      dev->file_size = 0;             /* reset file size */

      if (!dev->weof(1)) {            /* write eof */
         Dmsg0(50, "WEOF error in max file size.\n");
         Jmsg(jcr, M_FATAL, 0, _("Unable to write EOF. ERR=%s\n"),
            dev->bstrerror());
         terminate_writing_volume(dcr);
         dev->dev_errno = ENOSPC;
         return false;
      }
      if (!write_ansi_ibm_labels(dcr, ANSI_EOF_LABEL, dev->VolHdr.VolumeName)) {
         return false;
      }

      if (!do_new_file_bookkeeping(dcr)) {
         /*
          * Error message already sent
          */
         return false;
      }
   }

   dev->VolCatInfo.VolCatWrites++;
   Dmsg1(1300, "Write block of %u bytes\n", wlen);
#ifdef DEBUG_BLOCK_ZEROING
   uint32_t *bp = (uint32_t *)block->buf;
   if (bp[0] == 0 && bp[1] == 0 && bp[2] == 0 && block->buf[12] == 0) {
      Jmsg0(jcr, M_ABORT, 0, _("Write block header zeroed.\n"));
   }
#endif

   /*
    * Do write here,
    * make a somewhat feeble attempt to recover
    * from the OS telling us it is busy.
    */
   int retry = 0;
   errno = 0;
   status = 0;
   do {
      if (retry > 0 && status == -1 && errno == EBUSY) {
         berrno be;
         Dmsg4(100, "===== write retry=%d status=%d errno=%d: ERR=%s\n",
               retry, status, errno, be.bstrerror());
         bmicrosleep(5, 0);    /* pause a bit if busy or lots of errors */
         dev->clrerror(-1);
      }
      status = dev->write(block->buf, (size_t)wlen);
   } while (status == -1 && (errno == EBUSY) && retry++ < 3);

   if (debug_block_checksum) {
      uint32_t achecksum = ser_block_header(block, dev->do_checksum());
      if (checksum != achecksum) {
         Jmsg2(jcr, M_ERROR, 0, _("Block checksum changed during write: before=%ud after=%ud\n"),
            checksum, achecksum);
         dump_block(block, "with checksum error");
      }
   }

#ifdef DEBUG_BLOCK_ZEROING
   if (bp[0] == 0 && bp[1] == 0 && bp[2] == 0 && block->buf[12] == 0) {
      Jmsg0(jcr, M_ABORT, 0, _("Write block header zeroed.\n"));
   }
#endif

   if (status != (ssize_t)wlen) {
      /*
       * Some devices simply report EIO when the volume is full.
       * With a little more thought we may be able to check
       * capacity and distinguish real errors and EOT
       * conditions.  In any case, we probably want to
       * simulate an End of Medium.
       */
      if (status == -1) {
         berrno be;
         dev->clrerror(-1);
         if (dev->dev_errno == 0) {
            dev->dev_errno = ENOSPC;        /* out of space */
         }
         if (dev->dev_errno != ENOSPC) {
            dev->VolCatInfo.VolCatErrors++;
            Jmsg4(jcr, M_ERROR, 0, _("Write error at %u:%u on device %s. ERR=%s.\n"),
                  dev->file, dev->block_num, dev->print_name(), be.bstrerror());
         }
      } else {
        dev->dev_errno = ENOSPC;            /* out of space */
      }

      if (dev->dev_errno == ENOSPC) {
         Jmsg(jcr, M_INFO, 0, _("End of Volume \"%s\" at %u:%u on device %s. Write of %u bytes got %d.\n"),
              dev->getVolCatName(), dev->file, dev->block_num, dev->print_name(), wlen, status);
      } else {
         berrno be;

         be.set_errno(dev->dev_errno);
         Mmsg5(dev->errmsg, _("Write error on fd=%d at file:blk %u:%u on device %s. ERR=%s.\n"),
               dev->fd(), dev->file, dev->block_num, dev->print_name(), be.bstrerror());
      }

      generate_plugin_event(jcr, bsdEventWriteError, dcr);

      if (dev->dev_errno != ENOSPC) {
         Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
      }

      if (debug_level >= 100) {
         berrno be;

         be.set_errno(dev->dev_errno);
         Dmsg7(100, "=== Write error. fd=%d size=%u rtn=%d dev_blk=%d blk_blk=%d errno=%d: ERR=%s\n",
               dev->fd(), wlen, status, dev->block_num, block->BlockNumber, dev->dev_errno,
               be.bstrerror(dev->dev_errno));
      }

      ok = terminate_writing_volume(dcr);
      if (!ok && !forge_on) {
         return false;
      }
      if (ok) {
         reread_last_block(dcr);
      }
      return false;
   }

   /*
    * We successfully wrote the block, now do housekeeping
    */
   Dmsg2(1300, "VolCatBytes=%d newVolCatBytes=%d\n",
         (int)dev->VolCatInfo.VolCatBytes, (int)(dev->VolCatInfo.VolCatBytes+wlen));
   dev->VolCatInfo.VolCatBytes += wlen;
   dev->VolCatInfo.VolCatBlocks++;
   dev->EndBlock = dev->block_num;
   dev->EndFile  = dev->file;
   dev->LastBlock = block->BlockNumber;
   block->BlockNumber++;

   /*
    * Update dcr values
    */
   if (dev->is_tape()) {
      dcr->EndBlock = dev->EndBlock;
      dcr->EndFile  = dev->EndFile;
      dev->block_num++;
   } else {
      /*
       * Save address of block just written
       */
      uint64_t addr = dev->file_addr + wlen - 1;
      dcr->EndBlock = (uint32_t)addr;
      dcr->EndFile = (uint32_t)(addr >> 32);
      dev->block_num = dcr->EndBlock;
      dev->file = dcr->EndFile;
   }
   dcr->VolMediaId = dev->VolCatInfo.VolMediaId;
   if (dcr->VolFirstIndex == 0 && block->FirstIndex > 0) {
      dcr->VolFirstIndex = block->FirstIndex;
   }
   if (block->LastIndex > 0) {
      dcr->VolLastIndex = block->LastIndex;
   }
   dcr->WroteVol = true;
   dev->file_addr += wlen;            /* update file address */
   dev->file_size += wlen;

   Dmsg2(1300, "write_block: wrote block %d bytes=%d\n", dev->block_num, wlen);
   empty_block(block);
   return true;
}

static void reread_last_block(DCR *dcr)
{
#define CHECK_LAST_BLOCK
#ifdef  CHECK_LAST_BLOCK
   bool ok = true;
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;
   DEV_BLOCK *block = dcr->block;
   /*
    * If the device is a tape and it supports backspace record,
    *   we backspace over one or two eof marks depending on
    *   how many we just wrote, then over the last record,
    *   then re-read it and verify that the block number is
    *   correct.
    */
   if (dev->is_tape() && dev->has_cap(CAP_BSR)) {
      /*
       * Now back up over what we wrote and read the last block
       */
      if (!dev->bsf(1)) {
         berrno be;
         ok = false;
         Jmsg(jcr, M_ERROR, 0, _("Backspace file at EOT failed. ERR=%s\n"),
              be.bstrerror(dev->dev_errno));
      }
      if (ok && dev->has_cap(CAP_TWOEOF) && !dev->bsf(1)) {
         berrno be;
         ok = false;
         Jmsg(jcr, M_ERROR, 0, _("Backspace file at EOT failed. ERR=%s\n"),
              be.bstrerror(dev->dev_errno));
      }
      /*
       * Backspace over record
       */
      if (ok && !dev->bsr(1)) {
         berrno be;
         ok = false;
         Jmsg(jcr, M_ERROR, 0, _("Backspace record at EOT failed. ERR=%s\n"),
              be.bstrerror(dev->dev_errno));
         /*
          *  On FreeBSD systems, if the user got here, it is likely that his/her
          *    tape drive is "frozen".  The correct thing to do is a
          *    rewind(), but if we do that, higher levels in cleaning up, will
          *    most likely write the EOS record over the beginning of the
          *    tape.  The rewind *is* done later in mount.c when another
          *    tape is requested. Note, the clrerror() call in bsr()
          *    calls ioctl(MTCERRSTAT), which *should* fix the problem.
          */
      }
      if (ok) {
         DEV_BLOCK *lblock = new_block(dev);
         /*
          * Note, this can destroy dev->errmsg
          */
         dcr->block = lblock;
         if (!dcr->read_block_from_dev(NO_BLOCK_NUMBER_CHECK)) {
            Jmsg(jcr, M_ERROR, 0, _("Re-read last block at EOT failed. ERR=%s"), dev->errmsg);
         } else {
            /*
             * If we wrote block and the block numbers don't agree
             *  we have a possible problem.
             */
            if (lblock->BlockNumber != dev->LastBlock) {
                if (dev->LastBlock > (lblock->BlockNumber + 1)) {
                   Jmsg(jcr, M_FATAL, 0, _(
                        "Re-read of last block: block numbers differ by more than one.\n"
                        "Probable tape misconfiguration and data loss. Read block=%u Want block=%u.\n"),
                        lblock->BlockNumber, dev->LastBlock);
                 } else {
                   Jmsg(jcr, M_ERROR, 0, _(
                        "Re-read of last block OK, but block numbers differ. Read block=%u Want block=%u.\n"),
                        lblock->BlockNumber, dev->LastBlock);
                 }
            } else {
               Jmsg(jcr, M_INFO, 0, _("Re-read of last block succeeded.\n"));
            }
         }
         free_block(lblock);
         dcr->block = block;
      }
   }
#endif
}

/**
 * If this routine is called, we do our bookkeeping and
 * then assure that the volume will not be written any more.
 */
static bool terminate_writing_volume(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   bool ok = true;

   /* Create a JobMedia record to indicated end of tape */
   dev->VolCatInfo.VolCatFiles = dev->file;
   if (!dcr->dir_create_jobmedia_record(false)) {
      Dmsg0(50, "Error from create JobMedia\n");
      dev->dev_errno = EIO;
        Mmsg2(dev->errmsg, _("Could not create JobMedia record for Volume=\"%s\" Job=%s\n"),
            dcr->getVolCatName(), dcr->jcr->Job);
       Jmsg(dcr->jcr, M_FATAL, 0, "%s", dev->errmsg);
       ok = false;
   }
   dcr->block->write_failed = true;
   if (!dev->weof(1)) {         /* end the tape */
      dev->VolCatInfo.VolCatErrors++;
      Jmsg(dcr->jcr, M_ERROR, 0, _("Error writing final EOF to tape. This Volume may not be readable.\n"
           "%s"), dev->errmsg);
      ok = false;
      Dmsg0(50, "Error writing final EOF to volume.\n");
   }
   if (ok) {
      ok = write_ansi_ibm_labels(dcr, ANSI_EOV_LABEL, dev->VolHdr.VolumeName);
   }
   bstrncpy(dev->VolCatInfo.VolCatStatus, "Full", sizeof(dev->VolCatInfo.VolCatStatus));
   dev->VolCatInfo.VolCatFiles = dev->file;   /* set number of files */

   if (!dcr->dir_update_volume_info(false, true)) {
      Mmsg(dev->errmsg, _("Error sending Volume info to Director.\n"));
      ok = false;
      Dmsg0(50, "Error updating volume info.\n");
   }
   Dmsg1(50, "dir_update_volume_info terminate writing -- %s\n", ok?"OK":"ERROR");

   /*
    * Walk through all attached dcrs setting flag to call
    * set_new_file_parameters() when that dcr is next used.
    */
   DCR *mdcr;
   foreach_dlist(mdcr, dev->attached_dcrs) {
      if (mdcr->jcr->JobId == 0) {
         continue;
      }
      mdcr->NewFile = true;        /* set reminder to do set_new_file_params */
   }
   /*
    * Set new file/block parameters for current dcr
    */
   set_new_file_parameters(dcr);

   if (ok && dev->has_cap(CAP_TWOEOF) && !dev->weof(1)) {  /* end the tape */
      dev->VolCatInfo.VolCatErrors++;
      /*
       * This may not be fatal since we already wrote an EOF
       */
      Jmsg(dcr->jcr, M_ERROR, 0, "%s", dev->errmsg);
      Dmsg0(50, "Writing second EOF failed.\n");
   }

   dev->set_ateot();                  /* no more writing this tape */
   Dmsg1(50, "*** Leave terminate_writing_volume -- %s\n", ok?"OK":"ERROR");
   return ok;
}

/**
 * Do bookkeeping when a new file is created on a Volume. This is
 *  also done for disk files to generate the jobmedia records for
 *  quick seeking.
 */
static bool do_new_file_bookkeeping(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;

   /*
    * Create a JobMedia record so restore can seek
    */
   if (!dcr->dir_create_jobmedia_record(false)) {
      Dmsg0(50, "Error from create_job_media.\n");
      dev->dev_errno = EIO;
      Jmsg2(jcr, M_FATAL, 0, _("Could not create JobMedia record for Volume=\"%s\" Job=%s\n"),
           dcr->getVolCatName(), jcr->Job);
      terminate_writing_volume(dcr);
      dev->dev_errno = EIO;
      return false;
   }
   dev->VolCatInfo.VolCatFiles = dev->file;
   if (!dcr->dir_update_volume_info(false, false)) {
      Dmsg0(50, "Error from update_vol_info.\n");
      terminate_writing_volume(dcr);
      dev->dev_errno = EIO;
      return false;
   }
   Dmsg0(100, "dir_update_volume_info max file size -- OK\n");

   /*
    * Walk through all attached dcrs setting flag to call
    * set_new_file_parameters() when that dcr is next used.
    */
   DCR *mdcr;
   foreach_dlist(mdcr, dev->attached_dcrs) {
      if (mdcr->jcr->JobId == 0) {
         continue;
      }
      mdcr->NewFile = true;        /* set reminder to do set_new_file_params */
   }
   /*
    * Set new file/block parameters for current dcr
    */
   set_new_file_parameters(dcr);
   return true;
}

/**
 * Read block with locking
 */
bool DCR::read_block_from_device(bool check_block_numbers)
{
   bool ok;

   Dmsg0(250, "Enter read_block_from_device\n");
   dev->rLock();
   ok = read_block_from_dev(check_block_numbers);
   dev->Unlock();
   Dmsg0(250, "Leave read_block_from_device\n");
   return ok;
}

/**
 * Read the next block into the block structure and unserialize
 *  the block header.  For a file, the block may be partially
 *  or completely in the current buffer.
 */
bool DCR::read_block_from_dev(bool check_block_numbers)
{
   ssize_t status;
   int looping;
   int retry;
   DCR *dcr = this;

   if (job_canceled(jcr)) {
      Mmsg(dev->errmsg, _("Job failed or canceled.\n"));
      block->read_len = 0;
      return false;
   }

   if (dev->at_eot()) {
      Mmsg(dev->errmsg, _("Attempt to read past end of tape or file.\n"));
      block->read_len = 0;
      return false;
   }
   looping = 0;
   Dmsg1(250, "Full read in read_block_from_device() len=%d\n",
         block->buf_len);

   if (!dev->is_open()) {
      Mmsg4(dev->errmsg, _("Attempt to read closed device: fd=%d at file:blk %u:%u on device %s\n"),
         dev->fd(), dev->file, dev->block_num, dev->print_name());
      Jmsg(dcr->jcr, M_WARNING, 0, "%s", dev->errmsg);
      block->read_len = 0;
      return false;
    }

reread:
   if (looping > 1) {
      dev->dev_errno = EIO;
      Mmsg1(dev->errmsg, _("Block buffer size looping problem on device %s\n"),
         dev->print_name());
      Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
      block->read_len = 0;
      return false;
   }

   retry = 0;
   errno = 0;
   status = 0;

   do {
      if ((retry > 0 && status == -1 && errno == EBUSY)) {
         berrno be;
         Dmsg4(100, "===== read retry=%d status=%d errno=%d: ERR=%s\n",
               retry, status, errno, be.bstrerror());
         bmicrosleep(10, 0);    /* pause a bit if busy or lots of errors */
         dev->clrerror(-1);
      }
      status = dev->read(block->buf, (size_t)block->buf_len);

   } while (status == -1 && (errno == EBUSY || errno == EINTR || errno == EIO) && retry++ < 3);

   if (status < 0) {
      berrno be;

      dev->clrerror(-1);
      Dmsg1(250, "Read device got: ERR=%s\n", be.bstrerror());
      block->read_len = 0;
      Mmsg5(dev->errmsg, _("Read error on fd=%d at file:blk %u:%u on device %s. ERR=%s.\n"),
            dev->fd(), dev->file, dev->block_num, dev->print_name(), be.bstrerror());

      generate_plugin_event(jcr, bsdEventReadError, dcr);

      Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
      if (dev->at_eof()) {        /* EOF just seen? */
         dev->set_eot();          /* yes, error => EOT */
      }
      return false;
   }

   Dmsg3(250, "Read device got %d bytes at %u:%u\n", status,
      dev->file, dev->block_num);

   if (status == 0) {             /* Got EOF ! */
      dev->block_num = 0;
      block->read_len = 0;
      Mmsg3(dev->errmsg, _("Read zero bytes at %u:%u on device %s.\n"),
         dev->file, dev->block_num, dev->print_name());
      if (dev->at_eof()) {       /* EOF already read? */
         dev->set_eot();         /* yes, 2 EOFs => EOT */
         return 0;
      }
      dev->set_ateof();
      return false;             /* return eof */
   }

   /*
    * Continue here for successful read
    */
   block->read_len = status;      /* save length read */
   if (block->read_len == 80 &&
      (dcr->VolCatInfo.LabelType != B_BAREOS_LABEL ||
       dcr->device->label_type != B_BAREOS_LABEL)) {
      /* ***FIXME*** should check label */
      Dmsg2(100, "Ignore 80 byte ANSI label at %u:%u\n", dev->file, dev->block_num);
      dev->clear_eof();
      goto reread;             /* skip ANSI/IBM label */
   }

   if (block->read_len < BLKHDR2_LENGTH) {
      dev->dev_errno = EIO;
      Mmsg4(dev->errmsg, _("Volume data error at %u:%u! Very short block of %d bytes on device %s discarded.\n"),
         dev->file, dev->block_num, block->read_len, dev->print_name());
      Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
      dev->set_short_block();
      block->read_len = block->binbuf = 0;
      Dmsg2(200, "set block=%p binbuf=%d\n", block, block->binbuf);
      return false;             /* return error */
   }

// BlockNumber = block->BlockNumber + 1;
   if (!unser_block_header(jcr, dev, block)) {
      if (forge_on) {
         dev->file_addr += block->read_len;
         dev->file_size += block->read_len;
         goto reread;
      }
      return false;
   }

   /*
    * If the block is bigger than the buffer, we reposition for
    *  re-reading the block, allocate a buffer of the correct size,
    *  and go re-read.
    */
   if (block->block_len > block->buf_len) {
      dev->dev_errno = EIO;
      Mmsg2(dev->errmsg,  _("Block length %u is greater than buffer %u. Attempting recovery.\n"),
         block->block_len, block->buf_len);
      Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
      Pmsg1(000, "%s", dev->errmsg);
      /*
       * Attempt to reposition to re-read the block
       */
      if (dev->is_tape()) {
         Dmsg0(250, "BSR for reread; block too big for buffer.\n");
         if (!dev->bsr(1)) {
            Mmsg(dev->errmsg, "%s", dev->bstrerror());
            Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
            block->read_len = 0;
            return false;
         }
      } else {
         Dmsg0(250, "Seek to beginning of block for reread.\n");
         boffset_t pos = dev->lseek(dcr, (boffset_t)0, SEEK_CUR); /* get curr pos */
         pos -= block->read_len;
         dev->lseek(dcr, pos, SEEK_SET);
         dev->file_addr = pos;
      }
      Mmsg1(dev->errmsg, _("Setting block buffer size to %u bytes.\n"), block->block_len);
      Jmsg(jcr, M_INFO, 0, "%s", dev->errmsg);
      Pmsg1(000, "%s", dev->errmsg);
      /*
       * Set new block length
       */
      dev->max_block_size = block->block_len;
      block->buf_len = block->block_len;
      free_memory(block->buf);
      block->buf = get_memory(block->buf_len);
      empty_block(block);
      looping++;
      goto reread;                    /* re-read block with correct block size */
   }

   if (block->block_len > block->read_len) {
      dev->dev_errno = EIO;
      Mmsg4(dev->errmsg, _("Volume data error at %u:%u! Short block of %d bytes on device %s discarded.\n"),
         dev->file, dev->block_num, block->read_len, dev->print_name());
      Jmsg(jcr, M_ERROR, 0, "%s", dev->errmsg);
      dev->set_short_block();
      block->read_len = block->binbuf = 0;
      return false;             /* return error */
   }

   dev->clear_short_block();
   dev->clear_eof();
   dev->VolCatInfo.VolCatReads++;
   dev->VolCatInfo.VolCatRBytes += block->read_len;

   dev->EndBlock = dev->block_num;
   dev->EndFile  = dev->file;
   dev->block_num++;

   /*
    * Update dcr values
    */
   if (dev->is_tape()) {
      dcr->EndBlock = dev->EndBlock;
      dcr->EndFile  = dev->EndFile;
   } else {
      /*
       * We need to take care about a short block in EndBlock/File computation
       */
      uint32_t len = MIN(block->read_len, block->block_len);
      uint64_t addr = dev->file_addr + len - 1;
      dcr->EndBlock = (uint32_t)addr;
      dcr->EndFile = (uint32_t)(addr >> 32);
      dev->block_num = dev->EndBlock = dcr->EndBlock;
      dev->file = dev->EndFile = dcr->EndFile;
   }
   dcr->VolMediaId = dev->VolCatInfo.VolMediaId;
   dev->file_addr += block->read_len;
   dev->file_size += block->read_len;

   /*
    * If we read a short block on disk,
    * seek to beginning of next block. This saves us
    * from shuffling blocks around in the buffer. Take a
    * look at this from an efficiency stand point later, but
    * it should only happen once at the end of each job.
    *
    * I've been lseek()ing negative relative to SEEK_CUR for 30
    *   years now. However, it seems that with the new off_t definition,
    *   it is not possible to seek negative amounts, so we use two
    *   lseek(). One to get the position, then the second to do an
    *   absolute positioning -- so much for efficiency.  KES Sep 02.
    */
   Dmsg0(250, "At end of read block\n");
   if (block->read_len > block->block_len && !dev->is_tape()) {
      char ed1[50];
      boffset_t pos = dev->lseek(dcr, (boffset_t)0, SEEK_CUR); /* get curr pos */
      Dmsg1(250, "Current lseek pos=%s\n", edit_int64(pos, ed1));
      pos -= (block->read_len - block->block_len);
      dev->lseek(dcr, pos, SEEK_SET);
      Dmsg3(250, "Did lseek pos=%s blk_size=%d rdlen=%d\n",
         edit_int64(pos, ed1), block->block_len,
            block->read_len);
      dev->file_addr = pos;
      dev->file_size = pos;
   }
   Dmsg2(250, "Exit read_block read_len=%d block_len=%d\n",
      block->read_len, block->block_len);
   block->block_read = true;
   return true;
}
