/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, MM
 */
/**
 * @file
 * Block definitions for Bareos media data format.
 */
#ifndef __BLOCK_H
#define __BLOCK_H 1

#define MAX_BLOCK_LENGTH  20000000      /**< this is a sort of sanity check */
#define DEFAULT_BLOCK_SIZE (512 * 126)  /**< 64,512 N.B. do not use 65,536 here
                                           the POSIX standard defaults the size of a
                                           tape record to 126 blocks (63k). */

/* Block Header definitions. */
#define BLKHDR1_ID                       "BB01"
#define BLKHDR2_ID                       "BB02"
#define BLKHDR_ID_LENGTH                  4
#define BLKHDR_CS_LENGTH                  4     /**< checksum length */
#define BLKHDR1_LENGTH                   16     /**< Total length */
#define BLKHDR2_LENGTH                   24     /**< Total length */

#define WRITE_BLKHDR_ID     BLKHDR2_ID
#define WRITE_BLKHDR_LENGTH BLKHDR2_LENGTH
#define BLOCK_VER                        2

/* Record header definitions */
#define RECHDR1_LENGTH                   20
#define RECHDR2_LENGTH                   12
#define WRITE_RECHDR_LENGTH RECHDR2_LENGTH

/* Tape label and version definitions */
#define BareosId                         "Bareos 2.0 immortal\n"
#define OldBaculaId                      "Bacula 1.0 immortal\n"
#define OlderBaculaId                    "Bacula 0.9 mortal\n"
#define BareosTapeVersion                20
#define OldCompatibleBareosTapeVersion1  11
#define OldCompatibleBareosTapeVersion2  10
#define OldCompatibleBareosTapeVersion3  9

/**
 * This is the Media structure for a block header
 *  Note, when written, it is serialized.

   uint32_t CheckSum;
   uint32_t block_len;
   uint32_t BlockNumber;
   char     Id[BLKHDR_ID_LENGTH];

 * for BB02 block, we also have

   uint32_t VolSessionId;
   uint32_t VolSessionTime;
 */

class Device;                         /* for forward reference */

/**
 * DeviceBlock for reading and writing blocks.
 * This is the basic unit that is written to the device, and
 * it contains a Block Header followd by Records.  Note,
 * at times (when reading a file), this block may contain
 * multiple blocks.
 *
 *  This is the memory structure for a device block.
 */
struct DeviceBlock {
   DeviceBlock *next;                   /* pointer to next one */
   Device *dev;                       /* pointer to device */
   /* binbuf is the number of bytes remaining in the buffer.
    *   For writes, it is bytes not yet written.
    *   For reads, it is remaining bytes not yet read.
    */
   uint32_t binbuf;                   /* bytes in buffer */
   uint32_t block_len;                /* length of current block read */
   uint32_t buf_len;                  /* max/default block length */
   uint32_t BlockNumber;              /* sequential Bareos block number */
   uint32_t read_len;                 /* bytes read into buffer, if zero, block empty */
   uint32_t VolSessionId;             /* */
   uint32_t VolSessionTime;           /* */
   uint32_t read_errors;              /* block errors (checksum, header, ...) */
   int      BlockVer;                 /* block version 1 or 2 */
   bool     write_failed;             /* set if write failed */
   bool     block_read;               /* set when block read */
   int32_t  FirstIndex;               /* first index this block */
   int32_t  LastIndex;                /* last index this block */
   char    *bufp;                     /* pointer into buffer */
   POOLMEM *buf;                      /* actual data buffer */
};

#define block_write_navail(block) ((block)->buf_len - (block)->binbuf)
#define block_is_empty(block) ((block)->read_len == 0)

#endif
