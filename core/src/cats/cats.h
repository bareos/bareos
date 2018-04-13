/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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

#ifndef __CATS_H_
#define __CATS_H_ 1

/* import automatically generated SQL_QUERY_ENUM */
#include "bdb_query_enum_class.h"

/* ==============================================================
 *
 *  What follows are definitions that are used "globally" for all
 *   the different SQL engines and both inside and external to the
 *   cats directory.
 */

#define faddr_t long

/**
 * Generic definitions of list types, list handlers and result handlers.
 */
enum e_list_type {
   NF_LIST,
   RAW_LIST,
   HORZ_LIST,
   VERT_LIST
};

/**
 * Structure used when calling db_get_query_ids()
 *  allows the subroutine to return a list of ids.
 */
class dbid_list : public SmartAlloc {
public:
   DBId_t *DBId;                      /**< array of DBIds */
   char *PurgedFiles;                 /**< Array of PurgedFile flags */
   int num_ids;                       /**< num of ids actually stored */
   int max_ids;                       /**< size of id array */
   int num_seen;                      /**< number of ids processed */
   int tot_ids;                       /**< total to process */

   dbid_list();                       /**< in sql.c */
   ~dbid_list();                      /**< in sql.c */

   int size() const { return num_ids; }
   DBId_t get(int i) const;
};

/**
 * Job information passed to create job record and update
 * job record at end of job. Note, although this record
 * contains all the fields found in the Job database record,
 * it also contains fields found in the JobMedia record.
 */

/**
 * Job record
 */
struct JobDbRecord {
   JobId_t JobId;
   char Job[MAX_NAME_LENGTH];         /**< Job unique name */
   char Name[MAX_NAME_LENGTH];        /**< Job base name */
   int JobType;                       /**< actually char(1) */
   int JobLevel;                      /**< actually char(1) */
   int JobStatus;                     /**< actually char(1) */
   DBId_t ClientId;                   /**< Id of client */
   DBId_t PoolId;                     /**< Id of pool */
   DBId_t FileSetId;                  /**< Id of FileSet */
   DBId_t PriorJobId;                 /**< Id of migrated (prior) job */
   time_t SchedTime;                  /**< Time job scheduled */
   time_t StartTime;                  /**< Job start time */
   time_t EndTime;                    /**< Job termination time of orig job */
   time_t RealEndTime;                /**< Job termination time of this job */
   utime_t JobTDate;                  /**< Backup time/date in seconds */
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   uint32_t JobFiles;
   uint32_t JobErrors;
   uint32_t JobMissingFiles;
   uint64_t JobBytes;
   uint64_t ReadBytes;
   uint64_t JobSumTotalBytes;         /**< Total sum in bytes of all jobs but this one */
   int PurgedFiles;
   int HasBase;

   /* Note, FirstIndex, LastIndex, Start/End File and Block
    * are only used in the JobMedia record.
    */
   uint32_t FirstIndex;               /**< First index this Volume */
   uint32_t LastIndex;                /**< Last index this Volume */
   uint32_t StartFile;
   uint32_t EndFile;
   uint32_t StartBlock;
   uint32_t EndBlock;

   char cSchedTime[MAX_TIME_LENGTH];
   char cStartTime[MAX_TIME_LENGTH];
   char cEndTime[MAX_TIME_LENGTH];
   char cRealEndTime[MAX_TIME_LENGTH];

   /*
    * Extra stuff not in DB
    */
   int limit;                         /**< limit records to display */
   int offset;                        /**< offset records to display */
   faddr_t rec_addr;
   uint32_t FileIndex;                /**< added during Verify */
};

/* Job Media information used to create the media records
 * for each Volume used for the job.
 */
/**
 * JobMedia record
 */
struct JobMediaDbRecord {
   DBId_t JobMediaId;                 /**< record id */
   JobId_t  JobId;                    /**< JobId */
   DBId_t MediaId;                    /**< MediaId */
   uint32_t FirstIndex;               /**< First index this Volume */
   uint32_t LastIndex;                /**< Last index this Volume */
   uint32_t StartFile;                /**< File for start of data */
   uint32_t EndFile;                  /**< End file on Volume */
   uint32_t StartBlock;               /**< start block on tape */
   uint32_t EndBlock;                 /**< last block */
   uint64_t JobBytes;                 /**< job bytes */
};


/**
 * Volume Parameter structure
 */
struct VolumeParameters {
   char VolumeName[MAX_NAME_LENGTH];  /**< Volume name */
   char MediaType[MAX_NAME_LENGTH];   /**< Media Type */
   char Storage[MAX_NAME_LENGTH];     /**< Storage name */
   uint32_t VolIndex;                 /**< Volume seqence no. */
   uint32_t FirstIndex;               /**< First index this Volume */
   uint32_t LastIndex;                /**< Last index this Volume */
   int32_t Slot;                      /**< Slot */
   uint64_t StartAddr;                /**< Start address */
   uint64_t EndAddr;                  /**< End address */
   int32_t InChanger;                 /**< InChanger flag */
   uint64_t JobBytes;                 /**< job bytes */
// uint32_t Copy;                     /**< identical copy */
// uint32_t Stripe;                   /**< RAIT strip number */
};

/**
 * Attributes record -- NOT same as in database because
 * in general, this "record" creates multiple database
 * records (e.g. pathname, filename, fileattributes).
 */
struct AttributesDbRecord {
   char *fname;                       /**< full path & filename */
   char *link;                        /**< link if any */
   char *attr;                        /**< attributes statp */
   uint32_t FileIndex;
   uint32_t Stream;
   uint32_t FileType;
   uint32_t DeltaSeq;
   JobId_t  JobId;
   DBId_t ClientId;
   DBId_t PathId;
   FileId_t FileId;
   char *Digest;
   int DigestType;
   uint64_t Fhinfo;                  /**< NDMP fh_info for DAR*/
   uint64_t Fhnode;                  /**< NDMP fh_node for DAR*/
};

/**
 * Restore object database record
 */
struct RestoreObjectDbRecord {
   char *object_name;
   char *object;
   char *plugin_name;
   uint32_t object_len;
   uint32_t object_full_len;
   uint32_t object_index;
   int32_t object_compression;
   uint32_t FileIndex;
   uint32_t Stream;
   uint32_t FileType;
   JobId_t JobId;
   DBId_t RestoreObjectId;
};

/**
 * File record -- same format as database
 */
struct FileDbRecord {
   FileId_t FileId;
   uint32_t FileIndex;
   JobId_t JobId;
   DBId_t PathId;
   JobId_t MarkId;
   uint32_t DeltaSeq;
   char LStat[256];
   char Digest[BASE64_SIZE(CRYPTO_DIGEST_MAX_SIZE)];
   int DigestType;                    /**< NO_SIG/MD5_SIG/SHA1_SIG */
};

/**
 * Pool record -- same format as database
 */
struct PoolDbRecord {
   DBId_t PoolId;
   char Name[MAX_NAME_LENGTH];        /**< Pool name */
   uint32_t NumVols;                  /**< total number of volumes */
   uint32_t MaxVols;                  /**< max allowed volumes */
   int32_t LabelType;                 /**< BAREOS/ANSI/IBM */
   int32_t UseOnce;                   /**< set to use once only */
   int32_t UseCatalog;                /**< set to use catalog */
   int32_t AcceptAnyVolume;           /**< set to accept any volume sequence */
   int32_t AutoPrune;                 /**< set to prune automatically */
   int32_t Recycle;                   /**< default Vol recycle flag */
   uint32_t ActionOnPurge;            /**< action on purge, e.g. truncate the disk volume */
   utime_t VolRetention;              /**< retention period in seconds */
   utime_t VolUseDuration;            /**< time in secs volume can be used */
   uint32_t MaxVolJobs;               /**< Max Jobs on Volume */
   uint32_t MaxVolFiles;              /**< Max files on Volume */
   uint64_t MaxVolBytes;              /**< Max bytes on Volume */
   DBId_t RecyclePoolId;              /**< RecyclePool destination when media is purged */
   DBId_t ScratchPoolId;              /**< ScratchPool source when media is needed */
   char PoolType[MAX_NAME_LENGTH];
   char LabelFormat[MAX_NAME_LENGTH];
   uint32_t MinBlocksize;             /**< Minimum Block Size */
   uint32_t MaxBlocksize;             /**< Maximum Block Size */

   /*
    * Extra stuff not in DB
    */
   faddr_t rec_addr;
};

/**
 * Device record
 */
struct DeviceDbRecord {
   DBId_t DeviceId;
   char Name[MAX_NAME_LENGTH];        /**< Device name */
   DBId_t MediaTypeId;                /**< MediaType */
   DBId_t StorageId;                  /**< Storage id if autochanger */
   uint32_t DevMounts;                /**< Number of times mounted */
   uint32_t DevErrors;                /**< Number of read/write errors */
   uint64_t DevReadBytes;             /**< Number of bytes read */
   uint64_t DevWriteBytes;            /**< Number of bytes written */
   uint64_t DevReadTime;              /**< time spent reading volume */
   uint64_t DevWriteTime;             /**< time spent writing volume */
   uint64_t DevReadTimeSincCleaning;  /**< read time since cleaning */
   uint64_t DevWriteTimeSincCleaning; /**< write time since cleaning */
   time_t CleaningDate;               /**< time last cleaned */
   utime_t CleaningPeriod;            /**< time between cleanings */
};

/**
 * Storage database record
 */
struct StorageDbRecord {
   DBId_t StorageId;
   char Name[MAX_NAME_LENGTH];        /**< Device name */
   int AutoChanger;                   /**< Set if autochanger */

   /*
    * Extra stuff not in DB
    */
   bool created;                      /**< set if created by db_create ... */
};

/**
 * mediatype database record
 */
struct MediaTypeDbRecord {
   DBId_t MediaTypeId;
   char MediaType[MAX_NAME_LENGTH];   /**< MediaType string */
   int ReadOnly;                      /**< Set if read-only */
};

/**
 * Media record -- same as the database
 */
struct MediaDbRecord {
   DBId_t MediaId;                    /**< Unique volume id */
   char VolumeName[MAX_NAME_LENGTH];  /**< Volume name */
   char MediaType[MAX_NAME_LENGTH];   /**< Media type */
   char EncrKey[MAX_NAME_LENGTH];     /**< Encryption Key */
   DBId_t PoolId;                     /**< Pool id */
   time_t FirstWritten;               /**< Time Volume first written this usage */
   time_t LastWritten;                /**< Time Volume last written */
   time_t LabelDate;                  /**< Date/Time Volume labeled */
   time_t InitialWrite;               /**< Date/Time Volume first written */
   int32_t  LabelType;                /**< Label (BAREOS/ANSI/IBM) */
   uint32_t VolJobs;                  /**< number of jobs on this medium */
   uint32_t VolFiles;                 /**< Number of files */
   uint32_t VolBlocks;                /**< Number of blocks */
   uint32_t VolMounts;                /**< Number of times mounted */
   uint32_t VolErrors;                /**< Number of read/write errors */
   uint32_t VolWrites;                /**< Number of writes */
   uint32_t VolReads;                 /**< Number of reads */
   uint64_t VolBytes;                 /**< Number of bytes written */
   uint64_t MaxVolBytes;              /**< Max bytes to write to Volume */
   uint64_t VolCapacityBytes;         /**< capacity estimate */
   uint64_t VolReadTime;              /**< time spent reading volume */
   uint64_t VolWriteTime;             /**< time spent writing volume */
   utime_t VolRetention;              /**< Volume retention in seconds */
   utime_t VolUseDuration;            /**< time in secs volume can be used */
   uint32_t ActionOnPurge;            /**< action on purge, e.g. truncate the disk volume */
   uint32_t MaxVolJobs;               /**< Max Jobs on Volume */
   uint32_t MaxVolFiles;              /**< Max files on Volume */
   int32_t Recycle;                   /**< recycle yes/no */
   int32_t Slot;                      /**< slot in changer */
   int32_t Enabled;                   /**< 0=disabled, 1=enabled, 2=archived */
   int32_t InChanger;                 /**< Volume currently in changer */
   DBId_t StorageId;                  /**< Storage record Id */
   uint32_t EndFile;                  /**< Last file on volume */
   uint32_t EndBlock;                 /**< Last block on volume */
   uint32_t RecycleCount;             /**< Number of times recycled */
   uint32_t MinBlocksize;             /**< Minimum Block Size */
   uint32_t MaxBlocksize;             /**< Maximum Block Size */
   char VolStatus[20];                /**< Volume status */
   DBId_t DeviceId;                   /**< Device where Vol last written */
   DBId_t LocationId;                 /**< Where Volume is -- user defined */
   DBId_t ScratchPoolId;              /**< Where to move if scratch */
   DBId_t RecyclePoolId;              /**< Where to move when recycled */

   /*
    * Extra stuff not in DB
    */
   faddr_t rec_addr;                  /**< found record address */

   /*
    * Since the database returns times as strings, this is how we pass them back.
    */
   char cFirstWritten[MAX_TIME_LENGTH]; /**< FirstWritten returned from DB */
   char cLastWritten[MAX_TIME_LENGTH];  /**< LastWritten returned from DB */
   char cLabelDate[MAX_TIME_LENGTH];    /**< LabelData returned from DB */
   char cInitialWrite[MAX_TIME_LENGTH]; /**< InitialWrite returned from DB */
   bool set_first_written;
   bool set_label_date;
};

/**
 * Client record -- same as the database
 */
struct ClientDbRecord {
   DBId_t ClientId;                   /**< Unique Client id */
   int AutoPrune;
   utime_t GraceTime;                 /**< Time remaining on gracetime */
   uint32_t QuotaLimit;               /**< The total softquota supplied if over grace */
   utime_t FileRetention;
   utime_t JobRetention;
   char Name[MAX_NAME_LENGTH];        /**< Client name */
   char Uname[256];                   /**< Uname for client */
};

/**
 * Counter record -- same as in database
 */
struct CounterDbRecord {
   char Counter[MAX_NAME_LENGTH];
   int32_t MinValue;
   int32_t MaxValue;
   int32_t CurrentValue;
   char WrapCounter[MAX_NAME_LENGTH];
};

/**
 * FileSet record -- same as the database
 */
struct FileSetDbRecord {
   DBId_t FileSetId;                  /**< Unique FileSet id */
   char FileSet[MAX_NAME_LENGTH];     /**< FileSet name */
   char *FileSetText;                 /**< FileSet as Text */
   char MD5[50];                      /**< MD5 signature of include/exclude */
   time_t CreateTime;                 /**< Date created */
   /*
    * This is where we return CreateTime
    */
   char cCreateTime[MAX_TIME_LENGTH]; /**< CreateTime as returned from DB */
   /*
    * Not in DB but returned by db_create_fileset()
    */
   bool created;                      /**< set when record newly created */
};

/**
 * Device Statistics record -- same as in database
 */
struct DeviceStatisticsDbRecord {
   DBId_t DeviceId;                   /**< Device record id */
   time_t SampleTime;                 /**< Timestamp statistic was captured */
   uint64_t ReadTime;                 /**< Time spent reading volume */
   uint64_t WriteTime;                /**< Time spent writing volume */
   uint64_t ReadBytes;                /**< Number of bytes read */
   uint64_t WriteBytes;               /**< Number of bytes written */
   uint64_t SpoolSize;                /**< Number of bytes spooled */
   uint32_t NumWaiting;               /**< Number of Jobs waiting for device */
   uint32_t NumWriters;               /**< Number of writers to device */
   DBId_t MediaId;                    /**< MediaId used */
   uint64_t VolCatBytes;              /**< Volume Bytes */
   uint64_t VolCatFiles;              /**< Volume Files */
   uint64_t VolCatBlocks;             /**< Volume Blocks */
};

/**
 * TapeAlert record -- same as in database
 */
struct TapealertStatsDbRecord {
   DBId_t DeviceId;                   /**< Device record id */
   time_t SampleTime;                 /**< Timestamp statistic was captured */
   uint64_t AlertFlags;               /**< Tape Alerts raised */
};

/**
 * Job Statistics record -- same as in database
 */
struct JobStatisticsDbRecord {
   DBId_t DeviceId;                   /**< Device record id */
   time_t SampleTime;                 /**< Timestamp statistic was captured */
   JobId_t JobId;                     /**< Job record id */
   uint32_t JobFiles;                 /**< Number of Files in Job */
   uint64_t JobBytes;                 /**< Number of Bytes in Job */
};

/**
 * Call back context for getting a 32/64 bit value from the database
 */
class db_int64_ctx {
public:
   int64_t value;                     /**< value returned */
   int count;                         /**< number of values seen */

   db_int64_ctx() : value(0), count(0) {};
   ~db_int64_ctx() {};
private:
   db_int64_ctx(const db_int64_ctx&);            /**< prohibit pass by value */
   db_int64_ctx &operator=(const db_int64_ctx&); /**< prohibit class assignment */
};

/**
 * Call back context for getting a list of comma separated strings from the database
 */
class db_list_ctx {
public:
   POOLMEM *list;                     /* list */
   int count;                         /* number of values seen */

   db_list_ctx() { list = get_pool_memory(PM_FNAME); reset(); }
   ~db_list_ctx() { free_pool_memory(list); list = NULL; }
   void reset() { *list = 0; count = 0;}
   void add(const db_list_ctx &str) {
      if (str.count > 0) {
         if (*list) {
            pm_strcat(list, ",");
         }
         pm_strcat(list, str.list);
         count += str.count;
      }
   }
   void add(const char *str) {
      if (count > 0) {
         pm_strcat(list, ",");
      }
      pm_strcat(list, str);
      count++;
   }
private:
   db_list_ctx(const db_list_ctx&);            /**< prohibit pass by value */
   db_list_ctx &operator=(const db_list_ctx&); /**< prohibit class assignment */
};

typedef enum {
   SQL_INTERFACE_TYPE_MYSQL      = 0,
   SQL_INTERFACE_TYPE_POSTGRESQL = 1,
   SQL_INTERFACE_TYPE_SQLITE3    = 2,
   SQL_INTERFACE_TYPE_INGRES     = 3,
   SQL_INTERFACE_TYPE_DBI        = 4
} SQL_INTERFACETYPE;

typedef enum {
   SQL_TYPE_MYSQL      = 0,
   SQL_TYPE_POSTGRESQL = 1,
   SQL_TYPE_SQLITE3    = 2,
   SQL_TYPE_INGRES     = 3,
   SQL_TYPE_UNKNOWN    = 99
} SQL_DBTYPE;

typedef void (DB_LIST_HANDLER)(void *, const char *);
typedef int (DB_RESULT_HANDLER)(void *, int, char **);

#define db_lock(mdb)   mdb->_lock_db(__FILE__, __LINE__)
#define db_unlock(mdb) mdb->_unlock_db(__FILE__, __LINE__)

class pathid_cache;

/*
 * Initial size of query hash table and hint for number of pages.
 */
#define QUERY_INITIAL_HASH_SIZE 1024
#define QUERY_HTABLE_PAGES 128

/**
 * Current database version number for all drivers
 */
#define BDB_VERSION 2171

#ifdef _BDB_PRIV_INTERFACE_
/*
 * Generic definition of a sql_row.
 */
typedef char ** SQL_ROW;

/*
 * Generic definition of a a sql_field.
 */
typedef struct sql_field {
   char *name;                        /* name of column */
   int max_length;                    /* max length */
   uint32_t type;                     /* type */
   uint32_t flags;                    /* flags */
} SQL_FIELD;
#endif

class CATS_IMP_EXP BareosDb: public SmartAlloc, public B_DB_QUERY_ENUM_CLASS {
protected:
   /*
    * Members
    */
   brwlock_t lock_;                      /**< Transaction lock */
   dlink link_;                          /**< Queue control */
   SQL_INTERFACETYPE db_interface_type_; /**< Type of backend used */
   SQL_DBTYPE db_type_;                  /**< Database type */
   uint32_t ref_count_;                  /**< Reference count */
   bool connected_;                      /**< Connection made to db */
   bool have_batch_insert_;              /**< Have batch insert support ? */
   bool try_reconnect_;                  /**< Try reconnecting DB connection ? */
   bool exit_on_fatal_;                  /**< Exit on FATAL DB errors ? */
   char *db_driver_;                     /**< Database driver */
   char *db_driverdir_;                  /**< Database driver dir */
   char *db_name_;                       /**< Database name */
   char *db_user_;                       /**< Database user */
   char *db_address_;                    /**< Host name address */
   char *db_socket_;                     /**< Socket for local access */
   char *db_password_;                   /**< Database password */
   char *last_query_text_;               /**< Last query text obtained from query table */
   int db_port_;                         /**< Port for host name address */
   int cached_path_len;                   /**< Length of cached path */
   int changes;                           /**< Changes during transaction */
   int fnl;                               /**< File name length */
   int pnl;                               /**< Path name length */
   bool disabled_batch_insert_;          /**< Explicitly disabled batch insert mode ? */
   bool is_private_;                     /**< Private connection ? */
   uint32_t cached_path_id;               /**< Cached path id */
   uint32_t last_hash_key_;              /**< Last hash key lookup on query table */
   POOLMEM *fname;                        /**< Filename only */
   POOLMEM *path;                         /**< Path only */
   POOLMEM *cached_path;                  /**< Cached path name */
   POOLMEM *esc_name;                     /**< Escaped file name */
   POOLMEM *esc_path;                     /**< Escaped path name */
   POOLMEM *esc_obj;                      /**< Escaped restore object */
   POOLMEM *cmd;                          /**< SQL command string */
   POOLMEM *errmsg;                       /**< Nicely edited error message */
   const char **queries;                  /**< table of query texts */
   static const char *query_names[];      /**< table of query names */

private:
   /*
    * Methods
    */
   int get_filename_record(JobControlRecord *jcr);
   bool get_file_record(JobControlRecord *jcr, JobDbRecord *jr, FileDbRecord *fdbr);
   bool create_batch_file_attributes_record(JobControlRecord *jcr, AttributesDbRecord *ar);
   bool create_filename_record(JobControlRecord *jcr, AttributesDbRecord *ar);
   bool create_file_record(JobControlRecord *jcr, AttributesDbRecord *ar);
   void cleanup_base_file(JobControlRecord *jcr);
   void build_path_hierarchy(JobControlRecord *jcr, pathid_cache &ppathid_cache, char *org_pathid, char *path);
   bool update_path_hierarchy_cache(JobControlRecord *jcr, pathid_cache &ppathid_cache, JobId_t JobId);
   void fill_query_va_list(POOLMEM *&query, BareosDb::SQL_QUERY_ENUM predefined_query, va_list arg_ptr);
   void fill_query_va_list(PoolMem &query, BareosDb::SQL_QUERY_ENUM predefined_query, va_list arg_ptr);

public:
   /*
    * Methods
    */
   BareosDb() {};
   virtual ~BareosDb() {};
   const char *get_db_name(void) { return db_name_; };
   const char *get_db_user(void) { return db_user_; };
   bool is_connected(void) { return connected_; };
   bool batch_insert_available(void) { return have_batch_insert_; };
   bool is_private(void) { return is_private_; };
   void set_private(bool is_private) { is_private_ = is_private; };
   void increment_refcount(void) { ref_count_++; };

   /* bvfs.c */
   bool bvfs_update_path_hierarchy_cache(JobControlRecord *jcr, char *jobids);
   void bvfs_update_cache(JobControlRecord *jcr);
   int bvfs_ls_dirs(PoolMem &query, void *ctx);
   int bvfs_build_ls_file_query(PoolMem &query, DB_RESULT_HANDLER *result_handler, void *ctx);

   /* sql.c */
   char *strerror();
   bool check_max_connections(JobControlRecord *jcr, uint32_t max_concurrent_jobs);
   bool check_tables_version(JobControlRecord *jcr);
   bool QueryDB(const char *file, int line, JobControlRecord *jcr, const char *select_cmd);
   bool InsertDB(const char *file, int line, JobControlRecord *jcr, const char *select_cmd);
   int DeleteDB(const char *file, int line, JobControlRecord *jcr, const char *delete_cmd);
   bool UpdateDB(const char *file, int line, JobControlRecord *jcr, const char *update_cmd, int nr_afr);
   int get_sql_record_max(JobControlRecord *jcr);
   void split_path_and_file(JobControlRecord *jcr, const char *fname);
   void list_dashes(OUTPUT_FORMATTER *send);
   int list_result(void *vctx, int nb_col, char **row);
   int list_result(JobControlRecord *jcr, OUTPUT_FORMATTER *send, e_list_type type);
   bool open_batch_connection(JobControlRecord *jcr);
   void db_debug_print(FILE *fp);

   /* sql_create.c */
   bool create_path_record(JobControlRecord *jcr, AttributesDbRecord *ar);
   bool create_file_attributes_record(JobControlRecord *jcr, AttributesDbRecord *ar);
   bool create_job_record(JobControlRecord *jcr, JobDbRecord *jr);
   bool create_media_record(JobControlRecord *jcr, MediaDbRecord *media_dbr);
   bool create_client_record(JobControlRecord *jcr, ClientDbRecord *cr);
   bool create_fileset_record(JobControlRecord *jcr, FileSetDbRecord *fsr);
   bool create_pool_record(JobControlRecord *jcr, PoolDbRecord *pool_dbr);
   bool create_jobmedia_record(JobControlRecord *jcr, JobMediaDbRecord *jr);
   bool create_counter_record(JobControlRecord *jcr, CounterDbRecord *cr);
   bool create_device_record(JobControlRecord *jcr, DeviceDbRecord *dr);
   bool create_storage_record(JobControlRecord *jcr, StorageDbRecord *sr);
   bool create_mediatype_record(JobControlRecord *jcr, MediaTypeDbRecord *mr);
   bool write_batch_file_records(JobControlRecord *jcr);
   bool create_attributes_record(JobControlRecord *jcr, AttributesDbRecord *ar);
   bool create_restore_object_record(JobControlRecord *jcr, RestoreObjectDbRecord *ar);
   bool create_base_file_attributes_record(JobControlRecord *jcr, AttributesDbRecord *ar);
   bool commit_base_file_attributes_record(JobControlRecord *jcr);
   bool create_base_file_list(JobControlRecord *jcr, char *jobids);
   bool create_quota_record(JobControlRecord *jcr, ClientDbRecord *cr);
   bool create_ndmp_level_mapping(JobControlRecord *jcr, JobDbRecord *jr, char *filesystem);
   bool create_ndmp_environment_string(JobControlRecord *jcr, JobDbRecord *jr, char *name, char *value);
   bool create_job_statistics(JobControlRecord *jcr, JobStatisticsDbRecord *jsr);
   bool create_device_statistics(JobControlRecord *jcr, DeviceStatisticsDbRecord *dsr);
   bool create_tapealert_statistics(JobControlRecord *jcr, TapealertStatsDbRecord *tsr);

   /* sql_delete.c */
   bool delete_pool_record(JobControlRecord *jcr, PoolDbRecord *pool_dbr);
   bool delete_media_record(JobControlRecord *jcr, MediaDbRecord *mr);
   bool purge_media_record(JobControlRecord *jcr, MediaDbRecord *mr);

   /* sql_find.c */
   bool find_last_job_start_time(JobControlRecord *jcr, JobDbRecord *jr, POOLMEM *&stime, char *job, int JobLevel);
   bool find_job_start_time(JobControlRecord *jcr, JobDbRecord *jr, POOLMEM *&stime, char *job);
   bool find_last_jobid(JobControlRecord *jcr, const char *Name, JobDbRecord *jr);
   int find_next_volume(JobControlRecord *jcr, int index, bool InChanger, MediaDbRecord *mr, const char *unwanted_volumes);
   bool find_failed_job_since(JobControlRecord *jcr, JobDbRecord *jr, POOLMEM *stime, int &JobLevel);

   /* sql_get.c */
   bool get_volume_jobids(JobControlRecord *jcr, MediaDbRecord *mr, db_list_ctx *lst);
   bool get_base_file_list(JobControlRecord *jcr, bool use_md5, DB_RESULT_HANDLER *result_handler,void *ctx);
   int get_path_record(JobControlRecord *jcr);
   int get_path_record(JobControlRecord *jcr, const char *new_path);
   bool get_pool_record(JobControlRecord *jcr, PoolDbRecord *pdbr);
   bool get_storage_record(JobControlRecord *jcr, StorageDbRecord *sdbr);
   bool get_job_record(JobControlRecord *jcr, JobDbRecord *jr);
   int get_job_volume_names(JobControlRecord *jcr, JobId_t JobId, POOLMEM *&VolumeNames);
   bool get_file_attributes_record(JobControlRecord *jcr, char *filename, JobDbRecord *jr, FileDbRecord *fdbr);
   int get_fileset_record(JobControlRecord *jcr, FileSetDbRecord *fsr);
   bool get_media_record(JobControlRecord *jcr, MediaDbRecord *mr);
   int get_num_media_records(JobControlRecord *jcr);
   int get_num_pool_records(JobControlRecord *jcr);
   int get_pool_ids(JobControlRecord *jcr, int *num_ids, DBId_t **ids);
   bool get_client_ids(JobControlRecord *jcr, int *num_ids, DBId_t **ids);
   int get_storage_ids(JobControlRecord *jcr, int *num_ids, DBId_t **ids);
   bool prepare_media_sql_query(JobControlRecord *jcr, MediaDbRecord *mr, PoolMem &volumes);
   bool get_media_ids(JobControlRecord *jcr, MediaDbRecord *mr, PoolMem &volumes, int *num_ids, DBId_t **ids);
   int get_job_volume_parameters(JobControlRecord *jcr, JobId_t JobId, VolumeParameters **VolParams);
   bool get_client_record(JobControlRecord *jcr, ClientDbRecord *cdbr);
   bool get_counter_record(JobControlRecord *jcr, CounterDbRecord *cr);
   bool get_query_dbids(JobControlRecord *jcr, PoolMem &query, dbid_list &ids);
   bool get_file_list(JobControlRecord *jcr, char *jobids, bool use_md5, bool use_delta, DB_RESULT_HANDLER *result_handler, void *ctx);
   bool get_base_jobid(JobControlRecord *jcr, JobDbRecord *jr, JobId_t *jobid);
   bool accurate_get_jobids(JobControlRecord *jcr, JobDbRecord *jr, db_list_ctx *jobids);
   bool get_used_base_jobids(JobControlRecord *jcr, POOLMEM *jobids, db_list_ctx *result);
   bool get_quota_record(JobControlRecord *jcr, ClientDbRecord *cr);
   bool get_quota_jobbytes(JobControlRecord *jcr, JobDbRecord *jr, utime_t JobRetention);
   bool get_quota_jobbytes_nofailed(JobControlRecord *jcr, JobDbRecord *jr, utime_t JobRetention);
   int get_ndmp_level_mapping(JobControlRecord *jcr, JobDbRecord *jr, char *filesystem);
   bool get_ndmp_environment_string(JobControlRecord *jcr, JobDbRecord *jr, DB_RESULT_HANDLER *result_handler, void *ctx);
   bool get_ndmp_environment_string(JobControlRecord *jcr, JobId_t JobId, DB_RESULT_HANDLER *result_handler, void *ctx);
   bool prepare_media_sql_query(JobControlRecord *jcr, MediaDbRecord *mr, PoolMem *querystring, PoolMem &volumes);
   bool verify_media_ids_from_single_storage(JobControlRecord *jcr, dbid_list &mediaIds);

   /* sql_list.c */
   void list_pool_records(JobControlRecord *jcr, PoolDbRecord *pr, OUTPUT_FORMATTER *sendit, e_list_type type);
   void list_job_records(JobControlRecord *jcr, JobDbRecord *jr, const char *range, const char *clientname,
                            int jobstatus, int joblevel, const char* volumename, utime_t since_time, bool last,
                            bool count, OUTPUT_FORMATTER *sendit, e_list_type type);
   void list_job_totals(JobControlRecord *jcr, JobDbRecord *jr, OUTPUT_FORMATTER *sendit);
   void list_files_for_job(JobControlRecord *jcr, uint32_t jobid, OUTPUT_FORMATTER *sendit);
   void list_filesets(JobControlRecord *jcr, JobDbRecord *jr, const char *range, OUTPUT_FORMATTER *sendit, e_list_type type);
   void list_storage_records(JobControlRecord *jcr, OUTPUT_FORMATTER *sendit, e_list_type type);
   void list_media_records(JobControlRecord *jcr, MediaDbRecord *mdbr, const char *range, bool count, OUTPUT_FORMATTER *sendit, e_list_type type);
   void list_jobmedia_records(JobControlRecord *jcr, JobId_t JobId, OUTPUT_FORMATTER *sendit, e_list_type type);
   void list_volumes_of_jobid(JobControlRecord *jcr, JobId_t JobId, OUTPUT_FORMATTER *sendit, e_list_type type);
   void list_joblog_records(JobControlRecord *jcr, JobId_t JobId, const char *range, bool count, OUTPUT_FORMATTER *sendit, e_list_type type);
   void list_log_records(JobControlRecord *jcr, const char *clientname, const char *range,
                         bool reverse, OUTPUT_FORMATTER *sendit, e_list_type type);
   void list_jobstatistics_records(JobControlRecord *jcr, uint32_t JobId, OUTPUT_FORMATTER *sendit, e_list_type type);
   bool list_sql_query(JobControlRecord *jcr, const char *query, OUTPUT_FORMATTER *sendit, e_list_type type, bool verbose);
   bool list_sql_query(JobControlRecord *jcr, SQL_QUERY_ENUM query, OUTPUT_FORMATTER *sendit, e_list_type type, bool verbose);
   bool list_sql_query(JobControlRecord *jcr, const char *query, OUTPUT_FORMATTER *sendit, e_list_type type,
                       const char *description, bool verbose = false);
   bool list_sql_query(JobControlRecord *jcr, SQL_QUERY_ENUM query, OUTPUT_FORMATTER *sendit,
                       e_list_type type, const char *description, bool verbose);
   void list_client_records(JobControlRecord *jcr, char *clientname, OUTPUT_FORMATTER *sendit, e_list_type type);
   void list_copies_records(JobControlRecord *jcr, const char *range, const char *jobids, OUTPUT_FORMATTER *sendit, e_list_type type);
   void list_base_files_for_job(JobControlRecord *jcr, JobId_t jobid, OUTPUT_FORMATTER *sendit);

   /* sql_query.c */
   const char *get_predefined_query_name(SQL_QUERY_ENUM query);
   const char *get_predefined_query(SQL_QUERY_ENUM query);

   void fill_query(SQL_QUERY_ENUM predefined_query, ...);
   void fill_query(POOLMEM *&query, SQL_QUERY_ENUM predefined_query, ...);
   void fill_query(PoolMem &query, SQL_QUERY_ENUM predefined_query, ...);

   bool sql_query(SQL_QUERY_ENUM query, ...);
   bool sql_query(const char *query, int flags = 0);
   bool sql_query(const char *query, DB_RESULT_HANDLER *result_handler, void *ctx);

   /* sql_update.c */
   bool update_job_start_record(JobControlRecord *jcr, JobDbRecord *jr);
   bool update_job_end_record(JobControlRecord *jcr, JobDbRecord *jr);
   bool update_client_record(JobControlRecord *jcr, ClientDbRecord *cr);
   bool update_pool_record(JobControlRecord *jcr, PoolDbRecord *pr);
   bool update_storage_record(JobControlRecord *jcr, StorageDbRecord *sr);
   bool update_media_record(JobControlRecord *jcr, MediaDbRecord *mr);
   bool update_media_defaults(JobControlRecord *jcr, MediaDbRecord *mr);
   bool update_counter_record(JobControlRecord *jcr, CounterDbRecord *cr);
   bool update_quota_gracetime(JobControlRecord *jcr, JobDbRecord *jr);
   bool update_quota_softlimit(JobControlRecord *jcr, JobDbRecord *jr);
   bool reset_quota_record(JobControlRecord *jcr, ClientDbRecord *jr);
   bool update_ndmp_level_mapping(JobControlRecord *jcr, JobDbRecord *jr, char *filesystem, int level);
   bool add_digest_to_file_record(JobControlRecord *jcr, FileId_t FileId, char *digest, int type);
   bool mark_file_record(JobControlRecord *jcr, FileId_t FileId, JobId_t JobId);
   void make_inchanger_unique(JobControlRecord *jcr, MediaDbRecord *mr);
   int update_stats(JobControlRecord *jcr, utime_t age);

   /* Low level methods */
   bool match_database(const char *db_driver, const char *db_name,
                       const char *db_address, int db_port);
   BareosDb *clone_database_connection(JobControlRecord *jcr,
                                   bool mult_db_connections,
                                   bool get_pooled_connection = true,
                                   bool need_private = false);
   int get_type_index(void) { return db_type_; };
   const char *get_type(void);
   void _lock_db(const char *file, int line);
   void _unlock_db(const char *file, int line);
   void print_lock_info(FILE *fp);

   /* Virtual low level methods */
   virtual void thread_cleanup(void) {};
   virtual void escape_string(JobControlRecord *jcr, char *snew, char *old, int len);
   virtual char *escape_object(JobControlRecord *jcr, char *old, int len);
   virtual void unescape_object(JobControlRecord *jcr, char *from, int32_t expected_len,
                                POOLMEM *&dest, int32_t *len);

   /* Pure virtual low level methods */
   virtual bool open_database(JobControlRecord *jcr) = 0;
   virtual void close_database(JobControlRecord *jcr) = 0;
   virtual bool validate_connection(void) = 0;
   virtual void start_transaction(JobControlRecord *jcr) = 0;
   virtual void end_transaction(JobControlRecord *jcr) = 0;

   /* By default, we use db_sql_query */
   virtual bool big_sql_query(const char *query,
                              DB_RESULT_HANDLER *result_handler, void *ctx) {
      return sql_query(query, result_handler, ctx);
   };

#ifdef _BDB_PRIV_INTERFACE_
   /*
    * Backend methods
    */
private:
   virtual int sql_num_rows(void) = 0;
   virtual void sql_field_seek(int field) = 0;
   virtual int sql_num_fields(void) = 0;
   virtual void sql_free_result(void) = 0;
   virtual SQL_ROW sql_fetch_row(void) = 0;
   virtual bool sql_query_without_handler(const char *query, int flags = 0) = 0;
   virtual bool sql_query_with_handler(const char *query, DB_RESULT_HANDLER *result_handler, void *ctx) = 0;
   virtual const char *sql_strerror(void) = 0;
   virtual void sql_data_seek(int row) = 0;
   virtual int sql_affected_rows(void) = 0;
   virtual uint64_t sql_insert_autokey_record(const char *query, const char *table_name) = 0;
   virtual SQL_FIELD *sql_fetch_field(void) = 0;
   virtual bool sql_field_is_not_null(int field_type) = 0;
   virtual bool sql_field_is_numeric(int field_type) = 0;
   virtual bool sql_batch_start(JobControlRecord *jcr) = 0;
   virtual bool sql_batch_end(JobControlRecord *jcr, const char *error) = 0;
   virtual bool sql_batch_insert(JobControlRecord *jcr, AttributesDbRecord *ar) = 0;
#endif
};

#ifdef _BDB_PRIV_INTERFACE_
#include "bdb_priv.h"
#endif

/* sql_query Query Flags */
#define QF_STORE_RESULT 0x01

/* flush the batch insert connection every x changes */
#define BATCH_FLUSH 800000

/* Use for better error location printing */
#define UPDATE_DB(jcr, cmd) UpdateDB(__FILE__, __LINE__, jcr, cmd, 1)
#define UPDATE_DB_NO_AFR(jcr, cmd) UpdateDB(__FILE__, __LINE__, jcr, cmd, 0)
#define INSERT_DB(jcr, cmd) InsertDB(__FILE__, __LINE__, jcr,  cmd)
#define QUERY_DB(jcr, cmd) QueryDB(__FILE__, __LINE__, jcr, cmd)
#define DELETE_DB(jcr, cmd) DeleteDB(__FILE__, __LINE__, jcr, cmd)

/**
 * Pooled backend connection.
 */
struct SqlPoolEntry {
   int id;                                /**< Unique ID, connection numbering can have holes and the pool is not sorted on it */
   int reference_count;                   /**< Reference count for this entry */
   time_t last_update;                    /**< When was this connection last updated either used or put back on the pool */
   BareosDb *db_handle;                       /**< Connection handle to the database */
   dlink link;                            /**< list management */
};

/**
 * Pooled backend list descriptor (one defined per backend defined in config)
 */
struct SqlPoolDescriptor {
   dlist *pool_entries;                   /**< Linked list of all pool entries */
   bool active;                           /**< Is this an active pool, after a config reload an pool is made inactive */
   time_t last_update;                    /**< When was this pool last updated */
   int min_connections;                   /**< Minimum number of connections in the connection pool */
   int max_connections;                   /**< Maximum number of connections in the connection pool */
   int increment_connections;             /**< Increase/Decrease the number of connection in the pool with this value */
   int idle_timeout;                      /**< Number of seconds to wait before tearing down a connection */
   int validate_timeout;                  /**< Number of seconds after which an idle connection should be validated */
   int nr_connections;                    /**< Number of active connections in the pool */
   dlink link;                            /**< list management */
};

#include "protos.h"
#include "jcr.h"

/**
 * Object used in db_list_xxx function
 */
class ListContext {
public:
   char line[256];              /**< Used to print last dash line */
   int32_t num_rows;

   e_list_type type;            /**< Vertical/Horizontal */
   OUTPUT_FORMATTER *send;      /**< send data back */
   bool once;                   /**< Used to print header one time */
   BareosDb *mdb;
   JobControlRecord *jcr;

   void empty() {
      once = false;
      line[0] = '\0';
   }

   void send_dashes() {
      if (*line) {
         send->decoration(line);
      }
   }

   ListContext(JobControlRecord *j, BareosDb *m, OUTPUT_FORMATTER *h, e_list_type t) {
      line[0] = '\0';
      once = false;
      num_rows = 0;
      type = t;
      send = h;
      jcr = j;
      mdb = m;
   }
};

/**
 * Some functions exported by sql.c for use within the cats directory.
 */
int list_result(void *vctx, int cols, char **row);
int list_result(JobControlRecord *jcr, BareosDb *mdb, OUTPUT_FORMATTER *send, e_list_type type);
#endif /* __CATS_H_ */
