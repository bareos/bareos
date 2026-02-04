/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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
 * Catalog header file
 *
 * by Kern E. Sibbald
 */
/**
 * @file
 * Catalog header file
 * Anyone who accesses the database will need to include
 * this file.
 */

#ifndef BAREOS_CATS_CATS_H_
#define BAREOS_CATS_CATS_H_

#include "include/bareos.h"
#include "cats/column_data.h"
#include "lib/bstringlist.h"
#include "lib/output_formatter.h"
#include "lib/crypto.h"
#include "lib/base64.h"
#include "lib/source_location.h"

#include <bitset>
#include <string>
#include <stdexcept>
#include <system_error>
#include <vector>
template <typename T> class dlist;

/* import automatically generated SQL_QUERY */
#include "bdb_query_enum_class.h"

/* ==============================================================
 *
 *  What follows are definitions that are used "globally" for all
 *   the different SQL engines and both inside and external to the
 *   cats directory.
 */

#define faddr_t long

struct VolumeSessionInfo;

// Generic definitions of list types, list handlers and result handlers.
enum e_list_type
{
  NF_LIST,
  RAW_LIST,
  HORZ_LIST,
  VERT_LIST,
  E_LIST_INIT
};

/**
 * Structure used when calling db_get_query_ids()
 *  allows the subroutine to return a list of ids.
 */
class dbid_list {
 public:
  DBId_t* DBId = nullptr;      /**< array of DBIds */
  char* PurgedFiles = nullptr; /**< Array of PurgedFile flags */
  int num_ids = 0;             /**< num of ids actually stored */
  int max_ids = 0;             /**< size of id array */
  int num_seen = 0;            /**< number of ids processed */
  int tot_ids = 0;             /**< total to process */

  dbid_list();  /**< in sql.c */
  ~dbid_list(); /**< in sql.c */

  int size() const { return num_ids; }
  DBId_t get(int i) const;
};

/**
 * Job information passed to create job record and update
 * job record at end of job. Note, although this record
 * contains all the fields found in the Job database record,
 * it also contains fields found in the JobMedia record.
 */

struct JobDbRecord {
  JobId_t JobId = 0;
  char Job[MAX_NAME_LENGTH]{0};  /**< Job unique name */
  char Name[MAX_NAME_LENGTH]{0}; /**< Job base name */
  int JobType = 0;               /**< actually char(1) */
  int JobLevel = 0;              /**< actually char(1) */
  int JobStatus = 0;             /**< actually char(1) */
  DBId_t ClientId = 0;           /**< Id of client */
  DBId_t PoolId = 0;             /**< Id of pool */
  DBId_t FileSetId = 0;          /**< Id of FileSet */
  DBId_t PriorJobId = 0;         /**< Id of migrated (prior) job */
  time_t SchedTime = 0;          /**< Time job scheduled */
  time_t StartTime = 0;          /**< Job start time */
  time_t EndTime = 0;            /**< Job termination time of orig job */
  time_t RealEndTime = 0;        /**< Job termination time of this job */
  utime_t JobTDate = 0;          /**< Backup time/date in seconds */
  uint32_t VolSessionId = 0;
  uint32_t VolSessionTime = 0;
  uint32_t JobFiles = 0;
  uint32_t JobErrors = 0;
  uint32_t JobMissingFiles = 0;
  uint64_t JobBytes = 0;
  uint64_t ReadBytes = 0;
  uint64_t JobSumTotalBytes = 0; /**< All jobs but this one */
  int PurgedFiles = 0;
  int HasBase = 0;

  /* Note, FirstIndex, LastIndex, Start/End File and Block
   * are only used in the JobMedia record.
   */
  uint32_t FirstIndex = 0; /**< First index this Volume */
  uint32_t LastIndex = 0;  /**< Last index this Volume */
  uint32_t StartFile = 0;
  uint32_t EndFile = 0;
  uint32_t StartBlock = 0;
  uint32_t EndBlock = 0;

  char cSchedTime[MAX_TIME_LENGTH]{0};
  char cStartTime[MAX_TIME_LENGTH]{0};
  char cEndTime[MAX_TIME_LENGTH]{0};
  char cRealEndTime[MAX_TIME_LENGTH]{0};

  // Extra stuff not in DB
  uint64_t limit = 0;  /**< limit records to display */
  uint64_t offset = 0; /**< offset records to display */
  faddr_t rec_addr = 0;
  uint32_t FileIndex = 0; /**< added during Verify */
};

/* Job Media information used to create the media records
 * for each Volume used for the job.
 */
// JobMedia record
struct JobMediaDbRecord {
  DBId_t JobMediaId = 0;   /**< record id */
  JobId_t JobId = 0;       /**< JobId */
  DBId_t MediaId = 0;      /**< MediaId */
  uint32_t FirstIndex = 0; /**< First index this Volume */
  uint32_t LastIndex = 0;  /**< Last index this Volume */
  uint32_t StartFile = 0;  /**< File for start of data */
  uint32_t EndFile = 0;    /**< End file on Volume */
  uint32_t StartBlock = 0; /**< start block on tape */
  uint32_t EndBlock = 0;   /**< last block */
  uint64_t JobBytes = 0;   /**< job bytes */
};


struct VolumeParameters {
  char VolumeName[MAX_NAME_LENGTH]{0}; /**< Volume name */
  char MediaType[MAX_NAME_LENGTH]{0};  /**< Media Type */
  char Storage[MAX_NAME_LENGTH]{0};    /**< Storage name */
  uint32_t VolIndex = 0;               /**< Volume seqence no. */
  uint32_t FirstIndex = 0;             /**< First index this Volume */
  uint32_t LastIndex = 0;              /**< Last index this Volume */
  int32_t Slot = 0;                    /**< Slot */
  uint64_t StartAddr = 0;              /**< Start address */
  uint64_t EndAddr = 0;                /**< End address */
  int32_t InChanger = 0;               /**< InChanger flag */
  uint64_t JobBytes = 0;               /**< job bytes */
  // uint32_t Copy;                     /**< identical copy */
  // uint32_t Stripe;                   /**< RAIT strip number */
};

/**
 * Attributes record -- NOT same as in database because
 * in general, this "record" creates multiple database
 * records (e.g. pathname, filename, fileattributes).
 */
struct AttributesDbRecord {
  char* fname = nullptr; /**< full path & filename */
  char* link = nullptr;  /**< link if any */
  char* attr = nullptr;  /**< attributes statp */
  uint32_t FileIndex = 0;
  uint32_t Stream = 0;
  uint32_t FileType = 0;
  uint32_t DeltaSeq = 0;
  JobId_t JobId = 0;
  DBId_t ClientId = 0;
  DBId_t PathId = 0;
  FileId_t FileId = 0;
  char* Digest = nullptr;
  int DigestType = 0;
  uint64_t Fhinfo = 0; /**< NDMP fh_info for DAR*/
  uint64_t Fhnode = 0; /**< NDMP fh_node for DAR*/
};

struct RestoreObjectDbRecord {
  char* object_name = nullptr;
  char* object = nullptr;
  char* plugin_name = nullptr;
  uint32_t object_len = 0;
  uint32_t object_full_len = 0;
  uint32_t object_index = 0;
  int32_t object_compression = 0;
  uint32_t FileIndex = 0;
  uint32_t Stream = 0;
  uint32_t FileType = 0;
  JobId_t JobId = 0;
  DBId_t RestoreObjectId = 0;
};

struct FileDbRecord {
  FileId_t FileId = 0;
  uint32_t FileIndex = 0;
  JobId_t JobId = 0;
  DBId_t PathId = 0;
  JobId_t MarkId = 0;
  uint32_t DeltaSeq = 0;
  char LStat[256]{0};
  char Digest[BASE64_SIZE(CRYPTO_DIGEST_MAX_SIZE)]{0};
  int DigestType = 0; /**< NO_SIG/MD5_SIG/SHA1_SIG */
};

struct PoolDbRecord {
  DBId_t PoolId = 0;
  char Name[MAX_NAME_LENGTH]{0}; /**< Pool name */
  uint32_t NumVols = 0;          /**< total number of volumes */
  uint32_t MaxVols = 0;          /**< max allowed volumes */
  int32_t LabelType = 0;         /**< BAREOS/ANSI/IBM */
  int32_t UseOnce = 0;           /**< set to use once only */
  int32_t UseCatalog = 0;        /**< set to use catalog */
  int32_t AcceptAnyVolume = 0;   /**< set to accept any volume sequence */
  int32_t AutoPrune = 0;         /**< set to prune automatically */
  int32_t Recycle = 0;           /**< default Vol recycle flag */
  uint32_t ActionOnPurge
      = 0; /**< action on purge, e.g. truncate the disk volume */
  utime_t VolRetention = 0;   /**< retention period in seconds */
  utime_t VolUseDuration = 0; /**< time in secs volume can be used */
  uint32_t MaxVolJobs = 0;    /**< Max Jobs on Volume */
  uint32_t MaxVolFiles = 0;   /**< Max files on Volume */
  uint64_t MaxVolBytes = 0;   /**< Max bytes on Volume */
  DBId_t RecyclePoolId = 0; /**< RecyclePool destination when media is purged */
  DBId_t ScratchPoolId = 0; /**< ScratchPool source when media is needed */
  char PoolType[MAX_NAME_LENGTH]{0};
  char LabelFormat[MAX_NAME_LENGTH]{0};
  uint32_t MinBlocksize = 0; /**< Minimum Block Size */
  uint32_t MaxBlocksize = 0; /**< Maximum Block Size */

  // Extra stuff not in DB
  faddr_t rec_addr = 0;
};

struct DeviceDbRecord {
  DBId_t DeviceId = 0;
  char Name[MAX_NAME_LENGTH]{0};         /**< Device name */
  DBId_t MediaTypeId = 0;                /**< MediaType */
  DBId_t StorageId = 0;                  /**< Storage id if autochanger */
  uint32_t DevMounts = 0;                /**< Number of times mounted */
  uint32_t DevErrors = 0;                /**< Number of read/write errors */
  uint64_t DevReadBytes = 0;             /**< Number of bytes read */
  uint64_t DevWriteBytes = 0;            /**< Number of bytes written */
  uint64_t DevReadTime = 0;              /**< time spent reading volume */
  uint64_t DevWriteTime = 0;             /**< time spent writing volume */
  uint64_t DevReadTimeSincCleaning = 0;  /**< read time since cleaning */
  uint64_t DevWriteTimeSincCleaning = 0; /**< write time since cleaning */
  time_t CleaningDate = 0;               /**< time last cleaned */
  utime_t CleaningPeriod = 0;            /**< time between cleanings */
};

struct StorageDbRecord {
  DBId_t StorageId = 0;
  char Name[MAX_NAME_LENGTH]{0}; /**< Device name */
  int AutoChanger = 0;           /**< Set if autochanger */

  // Extra stuff not in DB
  bool created = false; /**< set if created by db_create ... */
};

struct MediaTypeDbRecord {
  DBId_t MediaTypeId = 0;
  char MediaType[MAX_NAME_LENGTH]{0}; /**< MediaType string */
  int ReadOnly = 0;                   /**< Set if read-only */
};

struct MediaDbRecord {
  DBId_t MediaId = 0;                  /**< Unique volume id */
  char VolumeName[MAX_NAME_LENGTH]{0}; /**< Volume name */
  char MediaType[MAX_NAME_LENGTH]{0};  /**< Media type */
  char EncrKey[MAX_NAME_LENGTH]{0};    /**< Encryption Key */
  DBId_t PoolId = 0;                   /**< Pool id */
  time_t FirstWritten = 0;       /**< Time Volume first written this usage */
  time_t LastWritten = 0;        /**< Time Volume last written */
  time_t LabelDate = 0;          /**< Date/Time Volume labeled */
  time_t InitialWrite = 0;       /**< Date/Time Volume first written */
  int32_t LabelType = 0;         /**< Label (BAREOS/ANSI/IBM) */
  uint32_t VolJobs = 0;          /**< number of jobs on this medium */
  uint32_t VolFiles = 0;         /**< Number of files */
  uint32_t VolBlocks = 0;        /**< Number of blocks */
  uint32_t VolMounts = 0;        /**< Number of times mounted */
  uint32_t VolErrors = 0;        /**< Number of read/write errors */
  uint32_t VolWrites = 0;        /**< Number of writes */
  uint32_t VolReads = 0;         /**< Number of reads */
  uint64_t VolBytes = 0;         /**< Number of bytes written */
  uint64_t MaxVolBytes = 0;      /**< Max bytes to write to Volume */
  uint64_t VolCapacityBytes = 0; /**< capacity estimate */
  uint64_t VolReadTime = 0;      /**< time spent reading volume */
  uint64_t VolWriteTime = 0;     /**< time spent writing volume */
  utime_t VolRetention = 0;      /**< Volume retention in seconds */
  utime_t VolUseDuration = 0;    /**< time in secs volume can be used */
  uint32_t ActionOnPurge
      = 0; /**< action on purge, e.g. truncate the disk volume */
  uint32_t MaxVolJobs = 0;   /**< Max Jobs on Volume */
  uint32_t MaxVolFiles = 0;  /**< Max files on Volume */
  int32_t Recycle = 0;       /**< recycle yes/no */
  int32_t Slot = 0;          /**< slot in changer */
  int32_t Enabled = 0;       /**< 0=disabled, 1=enabled, 2=archived */
  int32_t InChanger = 0;     /**< Volume currently in changer */
  DBId_t StorageId = 0;      /**< Storage record Id */
  uint32_t EndFile = 0;      /**< Last file on volume */
  uint32_t EndBlock = 0;     /**< Last block on volume */
  uint32_t RecycleCount = 0; /**< Number of times recycled */
  uint32_t MinBlocksize = 0; /**< Minimum Block Size */
  uint32_t MaxBlocksize = 0; /**< Maximum Block Size */
  char VolStatus[20]{0};     /**< Volume status */
  DBId_t DeviceId = 0;       /**< Device where Vol last written */
  DBId_t LocationId = 0;     /**< Where Volume is -- user defined */
  DBId_t ScratchPoolId = 0;  /**< Where to move if scratch */
  DBId_t RecyclePoolId = 0;  /**< Where to move when recycled */

  // Extra stuff not in DB
  faddr_t rec_addr = 0; /**< found record address */

  // Since the database returns times as strings, this is how we pass them back.
  char cFirstWritten[MAX_TIME_LENGTH]{0}; /**< FirstWritten returned from DB */
  char cLastWritten[MAX_TIME_LENGTH]{0};  /**< LastWritten returned from DB */
  char cLabelDate[MAX_TIME_LENGTH]{0};    /**< LabelData returned from DB */
  char cInitialWrite[MAX_TIME_LENGTH]{0}; /**< InitialWrite returned from DB */
  bool set_first_written = false;
  bool set_label_date = false;
};

struct ClientDbRecord {
  DBId_t ClientId = 0; /**< Unique Client id */
  int AutoPrune = 0;
  utime_t GraceTime = 0;   /**< Time remaining on gracetime */
  uint32_t QuotaLimit = 0; /**< The total softquota supplied if over grace */
  utime_t FileRetention = 0;
  utime_t JobRetention = 0;
  char Name[MAX_NAME_LENGTH]{0}; /**< Client name */
  char Uname[256]{0};            /**< Uname for client */
};

struct CounterDbRecord {
  char Counter[MAX_NAME_LENGTH]{0};
  int32_t MinValue{0};
  int32_t MaxValue{0};
  int32_t CurrentValue{0};
  char WrapCounter[MAX_NAME_LENGTH]{0};
};

struct FileSetDbRecord {
  DBId_t FileSetId = 0;             /**< Unique FileSet id */
  char FileSet[MAX_NAME_LENGTH]{0}; /**< FileSet name */
  char* FileSetText = nullptr;      /**< FileSet as Text */
  char MD5[50]{0};                  /**< MD5 signature of include/exclude */
  time_t CreateTime = 0;            /**< Date created */
  // This is where we return CreateTime
  char cCreateTime[MAX_TIME_LENGTH]{0}; /**< CreateTime as returned from DB */
  // Not in DB but returned by db_create_fileset()
  bool created = false; /**< set when record newly created */
};

struct DeviceStatisticsDbRecord {
  DBId_t DeviceId = 0;       /**< Device record id */
  time_t SampleTime = 0;     /**< Timestamp statistic was captured */
  uint64_t ReadTime = 0;     /**< Time spent reading volume */
  uint64_t WriteTime = 0;    /**< Time spent writing volume */
  uint64_t ReadBytes = 0;    /**< Number of bytes read */
  uint64_t WriteBytes = 0;   /**< Number of bytes written */
  uint64_t SpoolSize = 0;    /**< Number of bytes spooled */
  uint32_t NumWaiting = 0;   /**< Number of Jobs waiting for device */
  uint32_t NumWriters = 0;   /**< Number of writers to device */
  DBId_t MediaId = 0;        /**< MediaId used */
  uint64_t VolCatBytes = 0;  /**< Volume Bytes */
  uint64_t VolCatFiles = 0;  /**< Volume Files */
  uint64_t VolCatBlocks = 0; /**< Volume Blocks */
};

struct TapealertStatsDbRecord {
  DBId_t DeviceId = 0;     /**< Device record id */
  time_t SampleTime = 0;   /**< Timestamp statistic was captured */
  uint64_t AlertFlags = 0; /**< Tape Alerts raised */
};

struct JobStatisticsDbRecord {
  DBId_t DeviceId = 0;   /**< Device record id */
  time_t SampleTime = 0; /**< Timestamp statistic was captured */
  JobId_t JobId = 0;     /**< Job record id */
  uint32_t JobFiles = 0; /**< Number of Files in Job */
  uint64_t JobBytes = 0; /**< Number of Bytes in Job */
};

// Call back context for getting a 32/64 bit value from the database
class db_int64_ctx {
 public:
  int64_t value = 0; /**< value returned */
  int count = 0;     /**< number of values seen */

  db_int64_ctx() : value(0), count(0) {}
  ~db_int64_ctx() {}

 private:
  db_int64_ctx(const db_int64_ctx&); /**< prohibit pass by value */
  db_int64_ctx& operator=(
      const db_int64_ctx&); /**< prohibit class assignment */
};

/**
 * Call back context for getting a list of comma separated strings from the
 * database
 */
class db_list_ctx : public BStringList {
 public:
  void add(const db_list_ctx& list) { Append(list); }
  void add(const char* str) { Append(str); }
  void add(JobId_t id) { push_back(std::to_string(id)); }

  std::string GetAsString() const { return Join(','); }
  uint64_t GetFrontAsInteger() const
  {
    try {
      return std::stoull(at(0));
    } catch (...) {
      return 0;
    }
  }
};

typedef enum
{
  SQL_INTERFACE_TYPE_POSTGRESQL = 1,
  SQL_INTERFACE_TYPE_UNKNOWN = 99
} SQL_INTERFACETYPE;

typedef enum
{
  SQL_TYPE_POSTGRESQL = 1,
  SQL_TYPE_UNKNOWN = 99
} SQL_DBTYPE;

typedef void(DB_LIST_HANDLER)(void*, const char*);
typedef int(DB_RESULT_HANDLER)(void*, int, char**);

class pathid_cache;

// Initial size of query hash table and hint for number of pages.
#define QUERY_INITIAL_HASH_SIZE 1024
#define QUERY_HTABLE_PAGES 128

// Current database version number schema = 2000 + 10 * Major + Minor
#define BDB_VERSION 2250

typedef char** SQL_ROW;

typedef struct sql_field {
  const char* name = nullptr; /* name of column */
  int max_length = 0;         /* max length */
  uint32_t type = 0;          /* type */
  uint32_t flags = 0;         /* flags */
} SQL_FIELD;

class BareosSqlError : public std::runtime_error {
 public:
  BareosSqlError(const char* what) : std::runtime_error(what) {}
};

class BareosDb : public BareosDbQueryEnum {
 protected:
  brwlock_t lock_; /**< Transaction lock */
  SQL_INTERFACETYPE db_interface_type_
      = SQL_INTERFACE_TYPE_UNKNOWN;       /**< Type of backend used */
  SQL_DBTYPE db_type_ = SQL_TYPE_UNKNOWN; /**< Database type */
  uint32_t ref_count_ = 0;                /**< Reference count */
  bool connected_ = false;                /**< Connection made to db */
  bool have_batch_insert_ = false;        /**< Have batch insert support ? */
  bool try_reconnect_ = true;    /**< Try reconnecting DB connection ? */
  bool exit_on_fatal_ = false;   /**< Exit on FATAL DB errors ? */
  char* db_driver_ = nullptr;    /**< Database driver */
  char* db_driverdir_ = nullptr; /**< Database driver dir */
  char* db_name_ = nullptr;      /**< Database name */
  char* db_user_ = nullptr;      /**< Database user */
  char* db_address_ = nullptr;   /**< Host name address */
  char* db_socket_ = nullptr;    /**< Socket for local access */
  char* db_password_ = nullptr;  /**< Database password */
  char* last_query_text_
      = nullptr;           /**< Last query text obtained from query table */
  int db_port_ = 0;        /**< Port for host name address */
  int cached_path_len = 0; /**< Length of cached path */
  int changes = 0;         /**< Changes during transaction */
  int fnl = 0;             /**< File name length */
  int pnl = 0;             /**< Path name length */
  bool disabled_batch_insert_
      = false;                 /**< Explicitly disabled batch insert mode ? */
  bool is_private_ = false;    /**< Private connection ? */
  uint32_t cached_path_id = 0; /**< Cached path id */
  uint32_t last_hash_key_ = 0; /**< Last hash key lookup on query table */
  POOLMEM* fname = nullptr;    /**< Filename only */
  POOLMEM* path = nullptr;     /**< Path only */
  POOLMEM* cached_path = nullptr;   /**< Cached path name */
  POOLMEM* esc_name = nullptr;      /**< Escaped file name */
  POOLMEM* esc_path = nullptr;      /**< Escaped path name */
  POOLMEM* esc_obj = nullptr;       /**< Escaped restore object */
  POOLMEM* cmd = nullptr;           /**< SQL command string */
  POOLMEM* errmsg = nullptr;        /**< Nicely edited error message */
  const char** queries = nullptr;   /**< table of query texts */
  static const char* query_names[]; /**< table of query names */
  int num_rows_ = 0; /**< Number of rows returned by last query */
 private:
  int GetFilenameRecord(JobControlRecord* jcr);
  bool CreateBatchFileAttributesRecord(JobControlRecord* jcr,
                                       AttributesDbRecord* ar);
  bool CreateFilenameRecord(JobControlRecord* jcr, AttributesDbRecord* ar);
  void BuildPathHierarchy(JobControlRecord* jcr,
                          pathid_cache& ppathid_cache,
                          char* org_pathid,
                          char* path);
  bool UpdatePathHierarchyCache(JobControlRecord* jcr,
                                pathid_cache& ppathid_cache,
                                JobId_t JobId);
  void FillQueryVaList(POOLMEM*& query,
                       BareosDb::SQL_QUERY predefined_query,
                       va_list arg_ptr);
  void FillQueryVaList(PoolMem& query,
                       BareosDb::SQL_QUERY predefined_query,
                       va_list arg_ptr);

  int SqlNumRows(void)
  {
    // only valid for queries without handler
    return num_rows_;
  }

  /* each public function should either
   * 1. Lock the db itself, or
   * 2. assert that it currently holds the lock,
   * if it modifies the database state in anyway.
   * A function should never lock the db itself, if it returns some internal
   * db state, i.e. error messages, SqlResults, etc. */

 public:
  BareosDb() {}
  virtual ~BareosDb() {}

  const char* get_db_name(void) { return db_name_; }
  const char* get_db_user(void) { return db_user_; }
  bool IsConnected(void) { return connected_; }
  bool BatchInsertAvailable(void) { return have_batch_insert_; }
  bool IsPrivate(void) { return is_private_; }
  void IncrementRefcount(void) { ref_count_++; }

  /* bvfs.c */
  bool BvfsUpdatePathHierarchyCache(JobControlRecord* jcr, const char* jobids);
  void BvfsUpdateCache(JobControlRecord* jcr);
  int BvfsLsDirs(PoolMem& query, void* ctx);
  int BvfsBuildLsFileQuery(PoolMem& query,
                           DB_RESULT_HANDLER* ResultHandler,
                           void* ctx);

  /* sql.c */
 private:
  void ListDashes(OutputFormatter* send);

 public:
  char* strerror(libbareos::source_location loc
                 = libbareos::source_location::current());
  bool CheckMaxConnections(JobControlRecord* jcr, uint32_t max_concurrent_jobs);
  bool CheckTablesVersion(JobControlRecord* jcr);
  bool QueryDb(JobControlRecord* jcr,
               const char* select_cmd,
               libbareos::source_location loc
               = libbareos::source_location::current());
  int InsertDb(JobControlRecord* jcr,
               const char* select_cmd,
               libbareos::source_location loc
               = libbareos::source_location::current());
  int DeleteDb(JobControlRecord* jcr,
               const char* DeleteCmd,
               libbareos::source_location loc
               = libbareos::source_location::current());
  int UpdateDb(JobControlRecord* jcr,
               const char* UpdateCmd,
               libbareos::source_location loc
               = libbareos::source_location::current());
  int GetSqlRecordMax(JobControlRecord* jcr);
  void SplitPathAndFile(JobControlRecord* jcr, const char* fname);
  int ListResult(void* vctx, int nb_col, char** row);
  int ListResult(JobControlRecord* jcr,
                 OutputFormatter* send,
                 e_list_type type);
  bool OpenBatchConnection(JobControlRecord* jcr);
  void DbDebugPrint(FILE* fp);

  /* sql_create.cc */
 private:
  bool CreateFileRecord(JobControlRecord* jcr, AttributesDbRecord* ar);
  bool CreatePathRecord(JobControlRecord* jcr, AttributesDbRecord* ar);
  void CleanupBaseFile(JobControlRecord* jcr);

 public:
  bool CreateFileAttributesRecord(JobControlRecord* jcr,
                                  AttributesDbRecord* ar);
  bool CreateJobRecord(JobControlRecord* jcr, JobDbRecord* jr);
  bool CreateMediaRecord(JobControlRecord* jcr, MediaDbRecord* media_dbr);
  bool CreateClientRecord(JobControlRecord* jcr, ClientDbRecord* cr);
  bool CreateFilesetRecord(JobControlRecord* jcr, FileSetDbRecord* fsr);
  bool CreatePoolRecord(JobControlRecord* jcr, PoolDbRecord* pool_dbr);
  int DeleteNullJobmediaRecords(JobControlRecord* jcr, std::uint32_t jobid);
  bool CreateJobmediaRecord(JobControlRecord* jcr, JobMediaDbRecord* jr);
  bool CreateCounterRecord(JobControlRecord* jcr, CounterDbRecord* cr);
  bool CreateDeviceRecord(JobControlRecord* jcr, DeviceDbRecord* dr);
  bool CreateStorageRecord(JobControlRecord* jcr, StorageDbRecord* sr);
  bool CreateMediatypeRecord(JobControlRecord* jcr, MediaTypeDbRecord* mr);
  bool WriteBatchFileRecords(JobControlRecord* jcr);
  bool CreateAttributesRecord(JobControlRecord* jcr, AttributesDbRecord* ar);
  bool CreateRestoreObjectRecord(JobControlRecord* jcr,
                                 RestoreObjectDbRecord* ar);
  bool CreateBaseFileAttributesRecord(JobControlRecord* jcr,
                                      AttributesDbRecord* ar);
  bool CommitBaseFileAttributesRecord(JobControlRecord* jcr);
  bool CreateBaseFileList(JobControlRecord* jcr, const char* jobids);
  bool CreateQuotaRecord(JobControlRecord* jcr, ClientDbRecord* cr);
  bool CreateNdmpLevelMapping(JobControlRecord* jcr,
                              JobDbRecord* jr,
                              char* filesystem);
  bool CreateNdmpEnvironmentString(JobControlRecord* jcr,
                                   JobDbRecord* jr,
                                   char* name,
                                   char* value);
  bool CreateJobStatistics(JobControlRecord* jcr, JobStatisticsDbRecord* jsr);
  bool CreateDeviceStatistics(JobControlRecord* jcr,
                              DeviceStatisticsDbRecord* dsr);
  bool CreateTapealertStatistics(JobControlRecord* jcr,
                                 TapealertStatsDbRecord* tsr);

  /* sql_delete.cc */
  bool DeletePoolRecord(JobControlRecord* jcr, PoolDbRecord* pool_dbr);
  bool DeleteMediaRecord(JobControlRecord* jcr, MediaDbRecord* mr);
  void PurgeFiles(const char* jobids);
  void PurgeJobs(const char* jobids);

  /* sql_find.cc */

  enum class SqlFindResult
  {
    kError,
    kSuccess,
    kEmptyResultSet
  };

  virtual SqlFindResult FindLastJobStartTimeForJobAndClient(
      JobControlRecord* jcr,
      std::string job_basename,
      std::string client_name,
      std::vector<char>& stime_out);

  bool FindLastJobStartTime(JobControlRecord* jcr,
                            JobDbRecord* jr,
                            POOLMEM*& stime,
                            char* job,
                            int JobLevel);
  bool FindJobStartTime(JobControlRecord* jcr,
                        JobDbRecord* jr,
                        POOLMEM*& stime,
                        char* job);
  bool FindLastJobid(JobControlRecord* jcr, const char* Name, JobDbRecord* jr);
  bool FindJobById(JobControlRecord* jcr, const std::string id);
  int FindNextVolume(JobControlRecord* jcr,
                     int index,
                     bool InChanger,
                     MediaDbRecord* mr,
                     const char* unwanted_volumes);
  bool FindFailedJobSince(JobControlRecord* jcr,
                          JobDbRecord* jr,
                          POOLMEM* stime,
                          int& JobLevel);

  /* sql_get.cc */
 private:
  int GetPathRecord(JobControlRecord* jcr);
  bool GetFileRecord(JobControlRecord* jcr,
                     JobDbRecord* jr,
                     FileDbRecord* fdbr);

 public:
  bool GetVolumeJobids(MediaDbRecord* mr, db_list_ctx* lst);
  bool GetMediaIdsInPool(PoolDbRecord* pool_record, std::vector<DBId_t>* lst);
  bool GetBaseFileList(JobControlRecord* jcr,
                       bool use_md5,
                       DB_RESULT_HANDLER* ResultHandler,
                       void* ctx);
  int GetPathRecord(JobControlRecord* jcr, const char* new_path);
  bool GetPoolRecord(JobControlRecord* jcr, PoolDbRecord* pdbr);
  bool GetStorageRecord(JobControlRecord* jcr, StorageDbRecord* sdbr);
  bool GetJobRecord(JobControlRecord* jcr, JobDbRecord* jr);
  int GetJobVolumeNames(JobControlRecord* jcr,
                        JobId_t JobId,
                        POOLMEM*& VolumeNames);
  bool GetFileAttributesRecord(JobControlRecord* jcr,
                               char* filename,
                               JobDbRecord* jr,
                               FileDbRecord* fdbr);
  int GetFilesetRecord(JobControlRecord* jcr, FileSetDbRecord* fsr);
  bool GetMediaRecord(JobControlRecord* jcr, MediaDbRecord* mr);
  bool GetAllVolumeNames(db_list_ctx* volumenames);
  int GetNumMediaRecords(JobControlRecord* jcr);
  int GetNumPoolRecords(JobControlRecord* jcr);
  int GetPoolIds(JobControlRecord* jcr, int* num_ids, DBId_t** ids);
  bool GetClientIds(JobControlRecord* jcr, int* num_ids, DBId_t** ids);
  int GetStorageIds(JobControlRecord* jcr, int* num_ids, DBId_t** ids);
  bool PrepareMediaSqlQuery(JobControlRecord* jcr,
                            MediaDbRecord* mr,
                            PoolMem& volumes);
  bool GetMediaIds(JobControlRecord* jcr,
                   MediaDbRecord* mr,
                   PoolMem& volumes,
                   int* num_ids,
                   DBId_t** ids);
  int GetJobVolumeParameters(JobControlRecord* jcr,
                             JobId_t JobId,
                             VolumeParameters** VolParams);
  bool GetClientRecord(JobControlRecord* jcr, ClientDbRecord* cdbr);
  bool GetCounterRecord(JobControlRecord* jcr, CounterDbRecord* cr);
  bool GetQueryDbids(JobControlRecord* jcr, PoolMem& query, dbid_list& ids);
  bool GetFileList(JobControlRecord* jcr,
                   const char* jobids,
                   bool use_md5,
                   bool use_delta,
                   DB_RESULT_HANDLER* ResultHandler,
                   void* ctx);
  bool GetBaseJobid(JobControlRecord* jcr, JobDbRecord* jr, JobId_t* jobid);
  bool AccurateGetJobids(JobControlRecord* jcr,
                         JobDbRecord* jr,
                         db_list_ctx* jobids);
  db_list_ctx FilterZeroFileJobs(db_list_ctx& jobids);
  bool GetQuotaRecord(JobControlRecord* jcr, ClientDbRecord* cr);
  bool get_quota_jobbytes(JobControlRecord* jcr,
                          JobDbRecord* jr,
                          utime_t JobRetention);
  bool get_quota_jobbytes_nofailed(JobControlRecord* jcr,
                                   JobDbRecord* jr,
                                   utime_t JobRetention);
  int GetNdmpLevelMapping(JobControlRecord* jcr,
                          JobDbRecord* jr,
                          char* filesystem);
  bool GetNdmpEnvironmentString(const std::string& query,
                                DB_RESULT_HANDLER* ResultHandler,
                                void* ctx);
  bool GetNdmpEnvironmentString(const VolumeSessionInfo& vsi,
                                int32_t FileIndex,
                                DB_RESULT_HANDLER* ResultHandler,
                                void* ctx);
  bool GetNdmpEnvironmentString(JobId_t JobId,
                                DB_RESULT_HANDLER* ResultHandler,
                                void* ctx);
  bool GetNdmpEnvironmentString(JobId_t JobId,
                                int32_t FileIndex,
                                DB_RESULT_HANDLER* ResultHandler,
                                void* ctx);
  bool PrepareMediaSqlQuery(JobControlRecord* jcr,
                            MediaDbRecord* mr,
                            PoolMem& querystring,
                            PoolMem& volumes);
  bool VerifyMediaIdsFromSingleStorage(JobControlRecord* jcr,
                                       dbid_list& mediaIds);

  /* sql_list.cc */
  void ListPoolRecords(JobControlRecord* jcr,
                       PoolDbRecord* pr,
                       OutputFormatter* sendit,
                       e_list_type type);
  void ListJobRecords(JobControlRecord* jcr,
                      JobDbRecord* jr,
                      const char* range,
                      const char* clientname,
                      std::vector<char> jobstatusarray,
                      std::vector<char> joblevels,
                      std::vector<char> jobtypes,
                      const char* volumename,
                      const char* poolname,
                      utime_t since_time,
                      bool last,
                      bool count,
                      OutputFormatter* sendit,
                      e_list_type type);
  void ListJobTotals(JobControlRecord* jcr,
                     JobDbRecord* jr,
                     OutputFormatter* sendit);
  void ListFilesForJob(JobControlRecord* jcr,
                       uint32_t jobid,
                       OutputFormatter* sendit);
  void ListFilesets(JobControlRecord* jcr,
                    JobDbRecord* jr,
                    const char* range,
                    OutputFormatter* sendit,
                    e_list_type type);
  void ListMediaRecords(JobControlRecord* jcr,
                        MediaDbRecord* mdbr,
                        const char* range,
                        bool count,
                        OutputFormatter* sendit,
                        e_list_type type);
  void ListJobmediaRecords(JobControlRecord* jcr,
                           JobId_t JobId,
                           OutputFormatter* sendit,
                           e_list_type type);
  void ListVolumesOfJobid(JobControlRecord* jcr,
                          JobId_t JobId,
                          OutputFormatter* sendit,
                          e_list_type type);
  void ListJoblogRecords(JobControlRecord* jcr,
                         JobId_t JobId,
                         const char* range,
                         bool count,
                         OutputFormatter* sendit,
                         e_list_type type);
  void ListLogRecords(JobControlRecord* jcr,
                      const char* clientname,
                      const char* range,
                      bool reverse,
                      OutputFormatter* sendit,
                      e_list_type type);
  void ListJobstatisticsRecords(JobControlRecord* jcr,
                                uint32_t JobId,
                                OutputFormatter* sendit,
                                e_list_type type);

  enum class CollapseMode
  {
    NoCollapse,
    Collapse
  };

  bool ListSqlQuery(JobControlRecord* jcr,
                    const char* query,
                    OutputFormatter* sendit,
                    e_list_type type,
                    const bool verbose);
  bool ListSqlQuery(JobControlRecord* jcr,
                    SQL_QUERY query,
                    OutputFormatter* sendit,
                    e_list_type type,
                    const bool verbose);
  bool ListSqlQuery(JobControlRecord* jcr,
                    const char* query,
                    OutputFormatter* sendit,
                    e_list_type type,
                    const char* description,
                    const bool verbose = false,
                    const CollapseMode collapse = CollapseMode::NoCollapse);
  bool ListSqlQuery(JobControlRecord* jcr,
                    SQL_QUERY query,
                    OutputFormatter* sendit,
                    e_list_type type,
                    const char* description,
                    const bool verbose,
                    const CollapseMode collapse = CollapseMode::NoCollapse);

  void ListClientRecords(JobControlRecord* jcr,
                         char* clientname,
                         OutputFormatter* sendit,
                         e_list_type type);
  void ListCopiesRecords(JobControlRecord* jcr,
                         const char* range,
                         const char* jobids,
                         OutputFormatter* sendit,
                         e_list_type type);
  void ListBaseFilesForJob(JobControlRecord* jcr,
                           JobId_t jobid,
                           OutputFormatter* sendit);

  /* sql_query.cc */
  const char* get_predefined_query_name(SQL_QUERY query);
  const char* get_predefined_query(SQL_QUERY query);

  void FillQuery(SQL_QUERY predefined_query, ...);
  void FillQuery(POOLMEM*& query, SQL_QUERY predefined_query, ...);
  void FillQuery(PoolMem& query, SQL_QUERY predefined_query, ...);

  bool SqlQuery(SQL_QUERY query, ...);
  bool SqlQuery(const char* query);
  bool SqlExec(const char* query);  // like SqlQuery, but does not store result
  bool SqlQuery(const char* query, DB_RESULT_HANDLER* ResultHandler, void* ctx);

  /* sql_update.cc */
  bool UpdateJobStartRecord(JobControlRecord* jcr, JobDbRecord* jr);
  bool UpdateRunningJobRecord(JobControlRecord* jcr);
  bool UpdateJobEndRecord(JobControlRecord* jcr, JobDbRecord* jr);
  bool UpdateClientRecord(JobControlRecord* jcr, ClientDbRecord* cr);
  bool UpdatePoolRecord(JobControlRecord* jcr, PoolDbRecord* pr);
  bool UpdateStorageRecord(JobControlRecord* jcr, StorageDbRecord* sr);
  bool UpdateMediaRecord(JobControlRecord* jcr, MediaDbRecord* mr);
  bool UpdateMediaDefaults(JobControlRecord* jcr, MediaDbRecord* mr);
  bool UpdateCounterRecord(JobControlRecord* jcr, CounterDbRecord* cr);
  bool UpdateQuotaGracetime(JobControlRecord* jcr, JobDbRecord* jr);
  bool UpdateQuotaSoftlimit(JobControlRecord* jcr, JobDbRecord* jr);
  bool ResetQuotaRecord(JobControlRecord* jcr, ClientDbRecord* jr);
  bool UpdateNdmpLevelMapping(JobControlRecord* jcr,
                              JobDbRecord* jr,
                              char* filesystem,
                              int level);
  bool AddDigestToFileRecord(JobControlRecord* jcr,
                             FileId_t FileId,
                             char* digest,
                             int type);
  bool MarkFileRecord(JobControlRecord* jcr, FileId_t FileId, JobId_t JobId);
  void MakeInchangerUnique(JobControlRecord* jcr, MediaDbRecord* mr);
  int UpdateStats(JobControlRecord* jcr, utime_t age);
  void UpgradeCopies(const char* jobids);

  /* Low level methods */
  bool MatchDatabase(const char* db_driver,
                     const char* db_name,
                     const char* db_address,
                     int db_port);
  BareosDb* CloneDatabaseConnection(JobControlRecord* jcr,
                                    bool mult_db_connections,
                                    bool get_pooled_connection = true,
                                    bool need_private = false);
  int GetTypeIndex(void) { return db_type_; }
  const char* GetType(void);
  void LockDb(const char* file, int line);
  void UnlockDb(const char* file, int line);
  void PrintLockInfo(FILE* fp);

  /* Virtual low level methods */
  virtual void ThreadCleanup(void) {}
  virtual void EscapeString(JobControlRecord* jcr,
                            char* snew,
                            const char* old,
                            int len);
  virtual void UnescapeObject(JobControlRecord* jcr,
                              char* from,
                              int32_t expected_len,
                              POOLMEM*& dest,
                              int32_t* len);

  /* Pure virtual low level methods */
  // returns an error string on error
  virtual const char* OpenDatabase(JobControlRecord* jcr) = 0;
  virtual void CloseDatabase(JobControlRecord* jcr) = 0;
  virtual void StartTransaction(JobControlRecord* jcr) = 0;
  virtual void EndTransaction(JobControlRecord* jcr) = 0;

  /* By default, we use db_sql_query */
  virtual bool BigSqlQuery(const char* query,
                           DB_RESULT_HANDLER* ResultHandler,
                           void* ctx)
  {
    return SqlQuery(query, ResultHandler, ctx);
  }

 private:
  virtual char* EscapeObject(JobControlRecord* jcr, char* old, int len);
  virtual void SqlFieldSeek(int field) = 0;
  virtual int SqlNumFields(void) = 0;
  virtual void SqlFreeResult(void) = 0;
  virtual SQL_ROW SqlFetchRow(void) = 0;

 protected:
  enum class query_flag : size_t
  {
    DiscardResult,
    Count,
  };

  struct query_flags {
    std::bitset<static_cast<size_t>(query_flag::Count)> set_flags;

    query_flags() = default;
    constexpr query_flags(std::initializer_list<query_flag> initial_flags)
    {
      for (auto flag : initial_flags) {
        auto pos = static_cast<size_t>(flag);
        set_flags.set(pos);
      }
    }

    template <query_flag Flag> void set()
    {
      constexpr auto pos = static_cast<size_t>(Flag);
      static_assert(pos < static_cast<size_t>(query_flag::Count));
      set_flags.set(pos);
    }

    bool test(query_flag Flag)
    {
      return set_flags.test(static_cast<size_t>(Flag));
    }
  };


 private:
  virtual bool SqlQueryWithoutHandler(const char* query, query_flags flags = {})
      = 0;
  virtual bool SqlQueryWithHandler(const char* query,
                                   DB_RESULT_HANDLER* ResultHandler,
                                   void* ctx)
      = 0;
  virtual const char* sql_strerror(void) = 0;
  virtual void SqlDataSeek(int row) = 0;
  virtual int SqlAffectedRows(void) = 0;
  virtual uint64_t SqlInsertAutokeyRecord(const char* query,
                                          const char* table_name)
      = 0;
  virtual SQL_FIELD* SqlFetchField(void) = 0;
  virtual bool SqlFieldIsNotNull(int field_type) = 0;
  virtual bool SqlFieldIsNumeric(int field_type) = 0;
  virtual bool SqlBatchStartFileTable(JobControlRecord* jcr) = 0;
  virtual bool SqlBatchEndFileTable(JobControlRecord* jcr, const char* error)
      = 0;
  virtual bool SqlBatchInsertFileTable(JobControlRecord* jcr,
                                       AttributesDbRecord* ar)
      = 0;

 protected:
  void CheckOwnership(libbareos::source_location l
                      = libbareos::source_location::current())
  {
    if (!is_private_) { RwlCheckWriterIsMe(&lock_, l); }
  }
};

BareosDb* db_init_database(JobControlRecord* jcr,
                           const char* db_driver,
                           const char* db_name,
                           const char* db_user,
                           const char* db_password,
                           const char* db_address,
                           int db_port,
                           const char* db_socket,
                           bool mult_db_connections,
                           bool disable_batch_insert,
                           bool try_reconnect,
                           bool exit_on_fatal,
                           bool need_private = false);

/* flush the batch insert connection every x changes */
#define BATCH_FLUSH 800000

class DbLocker {
  BareosDb* db_handle_;
  const char* file;
  int line;

 public:
  DbLocker(BareosDb* db_handle,
           libbareos::source_location l = libbareos::source_location::current())
      : db_handle_(db_handle)
      , file{l.file_name()}
      , line{static_cast<int>(l.line())}
  {
    db_handle_->LockDb(file, line);
  }
  ~DbLocker() { db_handle_->UnlockDb(file, line); }

  DbLocker(const DbLocker& other) = delete;
  DbLocker& operator=(const DbLocker&) = delete;
  DbLocker(DbLocker&& other) = delete;
};

// Pooled backend connection.
struct SqlPoolEntry {
  int id = 0; /**< Unique ID, connection numbering can have holes and the pool
             is not sorted on it */
  int reference_count = 0; /**< Reference count for this entry */
  time_t last_update = 0;  /**< When was this connection last updated either
                          used  or put back on the pool */
  BareosDb* db_handle = nullptr; /**< Connection handle to the database */
  dlink<SqlPoolEntry> link;      /**< list management */
};

// Pooled backend list descriptor (one defined per backend defined in config)
struct SqlPoolDescriptor {
  bool active = false;    /**< Is this an active pool, after a config reload an
                      pool is    made inactive */
  time_t last_update = 0; /**< When was this pool last updated */
  int min_connections
      = 0; /**< Minimum number of connections in the connection pool
            */
  int max_connections
      = 0; /**< Maximum number of connections in the connection pool
            */
  int increment_connections = 0; /**< Increase/Decrease the number of
                                connection in the pool with this value */
  int idle_timeout = 0;     /**< Number of seconds to wait before tearing down a
                           connection */
  int validate_timeout = 0; /**< Number of seconds after which an idle
                           connection should be validated */
  int nr_connections = 0;   /**< Number of active connections in the pool */
  dlink<SqlPoolDescriptor> link; /**< list management */
};

#include "include/jcr.h"

// Object used in db_list_xxx function
class ListContext {
 public:
  char line[256]{0}; /**< Used to print last dash line */
  int32_t num_rows = 0;

  e_list_type type = E_LIST_INIT;  /**< Vertical/Horizontal */
  OutputFormatter* send = nullptr; /**< send data back */
  bool once = false;               /**< Used to print header one time */
  BareosDb* mdb = nullptr;
  JobControlRecord* jcr = nullptr;

  ListContext(JobControlRecord* j,
              BareosDb* m,
              OutputFormatter* h,
              e_list_type t)
  {
    line[0] = '\0';
    once = false;
    num_rows = 0;
    type = t;
    send = h;
    jcr = j;
    mdb = m;
  }
};

// Some functions exported by sql.c for use within the cats directory.
int ListResult(void* vctx, int cols, char** row);
int ListResult(JobControlRecord* jcr,
               BareosDb* mdb,
               OutputFormatter* send,
               e_list_type type);
#endif  // BAREOS_CATS_CATS_H_
