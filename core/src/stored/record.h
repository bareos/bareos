/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, MM
 */
/**
 * @file
 * Record, and label definitions for Bareos media data format.
 */

#ifndef BAREOS_STORED_RECORD_H_
#define BAREOS_STORED_RECORD_H_ 1

namespace storagedaemon {

/**
 * Return codes from read_device_volume_label()
 */
enum {
   VOL_NOT_READ = 1,                      /**< Volume label not read */
   VOL_OK,                                /**< volume name OK */
   VOL_NO_LABEL,                          /**< volume not labeled */
   VOL_IO_ERROR,                          /**< volume I/O error */
   VOL_NAME_ERROR,                        /**< Volume name mismatch */
   VOL_CREATE_ERROR,                      /**< Error creating label */
   VOL_VERSION_ERROR,                     /**< Bareos version error */
   VOL_LABEL_ERROR,                       /**< Bad label type */
   VOL_NO_MEDIA                           /**< Hard error -- no media present */
};

enum rec_state {
   st_none,                               /**< No state */
   st_header,                             /**< Write header */
   st_header_cont,
   st_data
};


/*  See block.h for RECHDR_LENGTH */

/*
 * This is the Media structure for a record header.
 *  NB: when it is written it is serialized.

   uint32_t VolSessionId;
   uint32_t VolSessionTime;

 * The above 8 bytes are only written in a BB01 block, BB02
 *  and later blocks contain these values in the block header
 *  rather than the record header.

   int32_t  FileIndex;
   int32_t  Stream;
   uint32_t data_len;

 */

/**
 * Record state bit definitions
 */
enum {
   REC_NO_HEADER = 0,                 /**< No header read */
   REC_PARTIAL_RECORD = 1,            /**< Returning partial record */
   REC_BLOCK_EMPTY = 2,               /**< Not enough data in block */
   REC_NO_MATCH = 3,                  /**< No match on continuation data */
   REC_CONTINUATION = 4,              /**< Continuation record found */
   REC_ISTAPE = 5                     /**< Set if device is tape */
};

/*
 * Keep this set to the last entry in the enum.
 */
#define REC_STATE_MAX REC_ISTAPE

/*
 * Make sure you have enough bits to store all above bit fields.
 */
#define REC_STATE_BYTES NbytesForBits(REC_STATE_MAX + 1)

#define IsPartialRecord(r) (BitIsSet(REC_PARTIAL_RECORD, (r)->state_bits))
#define IsBlockEmpty(r) (BitIsSet(REC_BLOCK_EMPTY, (r)->state_bits))

/*
 * DeviceRecord for reading and writing records.
 * It consists of a Record Header, and the Record Data
 *
 * This is the memory structure for the record header.
 */
struct BootStrapRecord;                           /* satisfy forward reference */
struct DeviceRecord {
   dlink link;                        /**< link for chaining in read_record.c */
   /**<
    * File and Block are always returned during reading and writing records.
    */
   uint32_t File;                     /**< File number */
   uint32_t Block;                    /**< Block number */
   uint32_t VolSessionId;             /**< Sequential id within this session */
   uint32_t VolSessionTime;           /**< Session start time */
   int32_t FileIndex;                 /**< Sequential file number */
   int32_t Stream;                    /**< Full Stream number with high bits */
   int32_t maskedStream;              /**< Masked Stream without high bits */
   uint32_t data_len;                 /**< Current record length */
   uint32_t remainder;                /**< Remaining bytes to read/write */
   char state_bits[REC_STATE_BYTES];  /**< State bits */
   rec_state state;                   /**< State of WriteRecordToBlock */
   BootStrapRecord *bsr;                          /**< Pointer to bsr that matched */
   POOLMEM *data;                     /**< Record data. This MUST be a memory pool item */
   int32_t match_stat;                /**< BootStrapRecord match status */
   uint32_t last_VolSessionId;        /**< Used in sequencing FI for Vbackup */
   uint32_t last_VolSessionTime;
   int32_t last_FileIndex;
   int32_t last_Stream;               /**< Used in SD-SD replication */
   bool own_mempool;                  /**< Do we own the POOLMEM pointed to in data ? */
};

/*
 * Values for LabelType that are put into the FileIndex field
 * Note, these values are negative to distinguish them
 * from user records where the FileIndex is forced positive.
 */
#define PRE_LABEL   -1                /**< Vol label on unwritten tape */
#define VOL_LABEL   -2                /**< Volume label first file */
#define EOM_LABEL   -3                /**< Writen at end of tape */
#define SOS_LABEL   -4                /**< Start of Session */
#define EOS_LABEL   -5                /**< End of Session */
#define EOT_LABEL   -6                /**< End of physical tape (2 eofs) */
#define SOB_LABEL   -7                /**< Start of object -- file/directory */
#define EOB_LABEL   -8                /**< End of object (after all streams) */

/*
 * Volume Label Record.  This is the in-memory definition. The
 * tape definition is defined in the serialization code itself
 * ser_volume_label() and UnserVolumeLabel() and is slightly different.
 */
struct Volume_Label {
  /*
   * The first items in this structure are saved
   * in the Device buffer, but are not actually written
   * to the tape.
   */
  int32_t LabelType;                  /**< This is written in header only */
  uint32_t LabelSize;                 /**< length of serialized label */
  /*
   * The items below this line are stored on
   * the tape
   */
  char Id[32];                        /**< Bareos Immortal ... */

  uint32_t VerNum;                    /**< Label version number */

  /* VerNum <= 10 */
  float64_t label_date;               /**< Date tape labeled */
  float64_t label_time;               /**< Time tape labeled */

  /* VerNum >= 11 */
  btime_t   label_btime;              /**< tdate tape labeled */
  btime_t   write_btime;              /**< tdate tape written */

  /* Unused with VerNum >= 11 */
  float64_t write_date;               /**< Date this label written */
  float64_t write_time;               /**< Time this label written */

  char VolumeName[MAX_NAME_LENGTH];   /**< Volume name */
  char PrevVolumeName[MAX_NAME_LENGTH]; /**< Previous Volume Name */
  char PoolName[MAX_NAME_LENGTH];     /**< Pool name */
  char PoolType[MAX_NAME_LENGTH];     /**< Pool type */
  char MediaType[MAX_NAME_LENGTH];    /**< Type of this media */

  char HostName[MAX_NAME_LENGTH];     /**< Host name of writing computer */
  char LabelProg[50];                 /**< Label program name */
  char ProgVersion[50];               /**< Program version */
  char ProgDate[50];                  /**< Program build date/time */

};

#define SER_LENGTH_Volume_Label 1024   /**< max serialised length of volume label */
#define SER_LENGTH_Session_Label 1024  /**< max serialised length of session label */

typedef struct Volume_Label VOLUME_LABEL;

/**
 * Session Start/End Label
 *  This record is at the beginning and end of each session
 */
struct Session_Label {
  char Id[32];                        /**< Bareos Immortal ... */

  uint32_t VerNum;                    /**< Label version number */

  uint32_t JobId;                     /**< Job id */
  uint32_t VolumeIndex;               /**< Sequence no of volume for this job */

  /* VerNum >= 11 */
  btime_t   write_btime;              /**< Tdate this label written */

  /* VerNum < 11 */
  float64_t write_date;               /**< Date this label written */

  /* Unused VerNum >= 11 */
  float64_t write_time;               /**< Time this label written */

  char PoolName[MAX_NAME_LENGTH];     /**< Pool name */
  char PoolType[MAX_NAME_LENGTH];     /**< Pool type */
  char JobName[MAX_NAME_LENGTH];      /**< base Job name */
  char ClientName[MAX_NAME_LENGTH];
  char Job[MAX_NAME_LENGTH];          /**< Unique name of this Job */
  char FileSetName[MAX_NAME_LENGTH];
  char FileSetMD5[MAX_NAME_LENGTH];
  uint32_t JobType;
  uint32_t JobLevel;
  /* The remainder are part of EOS label only */
  uint32_t JobFiles;
  uint64_t JobBytes;
  uint32_t StartBlock;
  uint32_t EndBlock;
  uint32_t StartFile;
  uint32_t EndFile;
  uint32_t JobErrors;
  uint32_t JobStatus;                 /**< Job status */

};
typedef struct Session_Label SESSION_LABEL;

#define SERIAL_BUFSIZE 1024           /**< Volume serialisation buffer size */

/**
 * Read context used to keep track of what is processed or not.
 */
struct Read_Context {
   DeviceRecord *rec;                   /**< Record currently being processed */
   dlist *recs;                       /**< Linked list of record packets open */
   SESSION_LABEL sessrec;             /**< Start Of Session record info */
   uint32_t records_processed;        /**< Number of records processed from this block */
   int32_t lastFileIndex;             /**< Last File Index processed */
};

struct DelayedDataStream {
   int32_t stream;                     /**< stream less new bits */
   char *content;                      /**< stream data */
   uint32_t content_length;            /**< stream length */
};

#define READ_NO_FILEINDEX -999999

#include "lib/mem_pool.h"


class DeviceControlRecord; /* Forward Reference */
class DeviceBlock;         /* Forward Reference */

DLL_IMP_EXP const char *FI_to_ascii(char *buf, int fi);
DLL_IMP_EXP const char *stream_to_ascii(char *buf, int stream, int fi);
DLL_IMP_EXP const char *record_to_str(PoolMem &resultbuffer, JobControlRecord *jcr, const DeviceRecord *rec);
DLL_IMP_EXP void DumpRecord(const char *tag, const DeviceRecord *rec);
DLL_IMP_EXP bool WriteRecordToBlock(DeviceControlRecord *dcr, DeviceRecord *rec);
DLL_IMP_EXP bool CanWriteRecordToBlock(DeviceBlock *block, const DeviceRecord *rec);
DLL_IMP_EXP bool ReadRecordFromBlock(DeviceControlRecord *dcr, DeviceRecord *rec);
DLL_IMP_EXP DeviceRecord *new_record(bool with_data = true);
DLL_IMP_EXP void EmptyRecord(DeviceRecord *rec);
DLL_IMP_EXP void CopyRecordState(DeviceRecord *dst, DeviceRecord *src);
DLL_IMP_EXP void FreeRecord(DeviceRecord *rec);
DLL_IMP_EXP uint64_t GetRecordAddress(const DeviceRecord *rec);

} /* namespace storagedaemon */

#endif
