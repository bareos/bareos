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
 * Catalog header file
 *
 * by Kern E. Sibbald
 *
 * Anyone who accesses the database will need to include
 * this file.
 */

#ifndef __CATS_H_
#define __CATS_H_ 1

/* ==============================================================
 *
 *  What follows are definitions that are used "globally" for all
 *   the different SQL engines and both inside and external to the
 *   cats directory.
 */

#define faddr_t long

/*
 * Generic definitions of list types, list handlers and result handlers.
 */
enum e_list_type {
   NF_LIST,
   RAW_LIST,
   HORZ_LIST,
   VERT_LIST
};

/*
 * Structure used when calling db_get_query_ids()
 *  allows the subroutine to return a list of ids.
 */
class dbid_list : public SMARTALLOC {
public:
   DBId_t *DBId;                      /* array of DBIds */
   char *PurgedFiles;                 /* Array of PurgedFile flags */
   int num_ids;                       /* num of ids actually stored */
   int max_ids;                       /* size of id array */
   int num_seen;                      /* number of ids processed */
   int tot_ids;                       /* total to process */

   dbid_list();                       /* in sql.c */
   ~dbid_list();                      /* in sql.c */
};

/* Job information passed to create job record and update
 * job record at end of job. Note, although this record
 * contains all the fields found in the Job database record,
 * it also contains fields found in the JobMedia record.
 */

/*
 * Job record
 */
struct JOB_DBR {
   JobId_t JobId;
   char Job[MAX_NAME_LENGTH];         /* Job unique name */
   char Name[MAX_NAME_LENGTH];        /* Job base name */
   int JobType;                       /* actually char(1) */
   int JobLevel;                      /* actually char(1) */
   int JobStatus;                     /* actually char(1) */
   DBId_t ClientId;                   /* Id of client */
   DBId_t PoolId;                     /* Id of pool */
   DBId_t FileSetId;                  /* Id of FileSet */
   DBId_t PriorJobId;                 /* Id of migrated (prior) job */
   time_t SchedTime;                  /* Time job scheduled */
   time_t StartTime;                  /* Job start time */
   time_t EndTime;                    /* Job termination time of orig job */
   time_t RealEndTime;                /* Job termination time of this job */
   utime_t JobTDate;                  /* Backup time/date in seconds */
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   uint32_t JobFiles;
   uint32_t JobErrors;
   uint32_t JobMissingFiles;
   uint64_t JobBytes;
   uint64_t ReadBytes;
   uint64_t JobSumTotalBytes;         /* Total sum in bytes of all jobs but this one */
   int PurgedFiles;
   int HasBase;

   /* Note, FirstIndex, LastIndex, Start/End File and Block
    * are only used in the JobMedia record.
    */
   uint32_t FirstIndex;               /* First index this Volume */
   uint32_t LastIndex;                /* Last index this Volume */
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
   int limit;                         /* limit records to display */
   faddr_t rec_addr;
   uint32_t FileIndex;                /* added during Verify */
};

/* Job Media information used to create the media records
 * for each Volume used for the job.
 */
/*
 * JobMedia record
 */
struct JOBMEDIA_DBR {
   DBId_t JobMediaId;                 /* record id */
   JobId_t  JobId;                    /* JobId */
   DBId_t MediaId;                    /* MediaId */
   uint32_t FirstIndex;               /* First index this Volume */
   uint32_t LastIndex;                /* Last index this Volume */
   uint32_t StartFile;                /* File for start of data */
   uint32_t EndFile;                  /* End file on Volume */
   uint32_t StartBlock;               /* start block on tape */
   uint32_t EndBlock;                 /* last block */
// uint32_t Copy;                     /* identical copy */
};


/* Volume Parameter structure */
struct VOL_PARAMS {
   char VolumeName[MAX_NAME_LENGTH];  /* Volume name */
   char MediaType[MAX_NAME_LENGTH];   /* Media Type */
   char Storage[MAX_NAME_LENGTH];     /* Storage name */
   uint32_t VolIndex;                 /* Volume seqence no. */
   uint32_t FirstIndex;               /* First index this Volume */
   uint32_t LastIndex;                /* Last index this Volume */
   int32_t Slot;                      /* Slot */
   uint64_t StartAddr;                /* Start address */
   uint64_t EndAddr;                  /* End address */
   int32_t InChanger;                 /* InChanger flag */
// uint32_t Copy;                     /* identical copy */
// uint32_t Stripe;                   /* RAIT strip number */
};

/*
 * Attributes record -- NOT same as in database because
 * in general, this "record" creates multiple database
 * records (e.g. pathname, filename, fileattributes).
 */
struct ATTR_DBR {
   char *fname;                       /* full path & filename */
   char *link;                        /* link if any */
   char *attr;                        /* attributes statp */
   uint32_t FileIndex;
   uint32_t Stream;
   uint32_t FileType;
   uint32_t DeltaSeq;
   JobId_t  JobId;
   DBId_t ClientId;
   DBId_t PathId;
   DBId_t FilenameId;
   FileId_t FileId;
   char *Digest;
   int DigestType;
};

struct ROBJECT_DBR {
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

/*
 * File record -- same format as database
 */
struct FILE_DBR {
   FileId_t FileId;
   uint32_t FileIndex;
   JobId_t JobId;
   DBId_t FilenameId;
   DBId_t PathId;
   JobId_t MarkId;
   uint32_t DeltaSeq;
   char LStat[256];
   char Digest[BASE64_SIZE(CRYPTO_DIGEST_MAX_SIZE)];
   int DigestType;                    /* NO_SIG/MD5_SIG/SHA1_SIG */
};

/*
 * Pool record -- same format as database
 */
struct POOL_DBR {
   DBId_t PoolId;
   char Name[MAX_NAME_LENGTH];        /* Pool name */
   uint32_t NumVols;                  /* total number of volumes */
   uint32_t MaxVols;                  /* max allowed volumes */
   int32_t LabelType;                 /* BAREOS/ANSI/IBM */
   int32_t UseOnce;                   /* set to use once only */
   int32_t UseCatalog;                /* set to use catalog */
   int32_t AcceptAnyVolume;           /* set to accept any volume sequence */
   int32_t AutoPrune;                 /* set to prune automatically */
   int32_t Recycle;                   /* default Vol recycle flag */
   uint32_t ActionOnPurge;            /* action on purge, e.g. truncate the disk volume */
   utime_t VolRetention;              /* retention period in seconds */
   utime_t VolUseDuration;            /* time in secs volume can be used */
   uint32_t MaxVolJobs;               /* Max Jobs on Volume */
   uint32_t MaxVolFiles;              /* Max files on Volume */
   uint64_t MaxVolBytes;              /* Max bytes on Volume */
   DBId_t RecyclePoolId;              /* RecyclePool destination when media is purged */
   DBId_t ScratchPoolId;              /* ScratchPool source when media is needed */
   char PoolType[MAX_NAME_LENGTH];
   char LabelFormat[MAX_NAME_LENGTH];
   uint32_t MinBlocksize;             /* Minimum Block Size */
   uint32_t MaxBlocksize;             /* Maximum Block Size */

   /*
    * Extra stuff not in DB
    */
   faddr_t rec_addr;
};

struct DEVICE_DBR {
   DBId_t DeviceId;
   char Name[MAX_NAME_LENGTH];        /* Device name */
   DBId_t MediaTypeId;                /* MediaType */
   DBId_t StorageId;                  /* Storage id if autochanger */
   uint32_t DevMounts;                /* Number of times mounted */
   uint32_t DevErrors;                /* Number of read/write errors */
   uint64_t DevReadBytes;             /* Number of bytes read */
   uint64_t DevWriteBytes;            /* Number of bytes written */
   uint64_t DevReadTime;              /* time spent reading volume */
   uint64_t DevWriteTime;             /* time spent writing volume */
   uint64_t DevReadTimeSincCleaning;  /* read time since cleaning */
   uint64_t DevWriteTimeSincCleaning; /* write time since cleaning */
   time_t CleaningDate;               /* time last cleaned */
   utime_t CleaningPeriod;            /* time between cleanings */
};

struct STORAGE_DBR {
   DBId_t StorageId;
   char Name[MAX_NAME_LENGTH];        /* Device name */
   int AutoChanger;                   /* Set if autochanger */

   /*
    * Extra stuff not in DB
    */
   bool created;                      /* set if created by db_create ... */
};

struct MEDIATYPE_DBR {
   DBId_t MediaTypeId;
   char MediaType[MAX_NAME_LENGTH];   /* MediaType string */
   int ReadOnly;                      /* Set if read-only */
};

/*
 * Media record -- same as the database
 */
struct MEDIA_DBR {
   DBId_t MediaId;                    /* Unique volume id */
   char VolumeName[MAX_NAME_LENGTH];  /* Volume name */
   char MediaType[MAX_NAME_LENGTH];   /* Media type */
   char EncrKey[MAX_NAME_LENGTH];     /* Encryption Key */
   DBId_t PoolId;                     /* Pool id */
   time_t FirstWritten;               /* Time Volume first written this usage */
   time_t LastWritten;                /* Time Volume last written */
   time_t LabelDate;                  /* Date/Time Volume labeled */
   time_t InitialWrite;               /* Date/Time Volume first written */
   int32_t  LabelType;                /* Label (BAREOS/ANSI/IBM) */
   uint32_t VolJobs;                  /* number of jobs on this medium */
   uint32_t VolFiles;                 /* Number of files */
   uint32_t VolBlocks;                /* Number of blocks */
   uint32_t VolMounts;                /* Number of times mounted */
   uint32_t VolErrors;                /* Number of read/write errors */
   uint32_t VolWrites;                /* Number of writes */
   uint32_t VolReads;                 /* Number of reads */
   uint64_t VolBytes;                 /* Number of bytes written */
   uint64_t MaxVolBytes;              /* Max bytes to write to Volume */
   uint64_t VolCapacityBytes;         /* capacity estimate */
   uint64_t VolReadTime;              /* time spent reading volume */
   uint64_t VolWriteTime;             /* time spent writing volume */
   utime_t VolRetention;              /* Volume retention in seconds */
   utime_t VolUseDuration;            /* time in secs volume can be used */
   uint32_t ActionOnPurge;            /* action on purge, e.g. truncate the disk volume */
   uint32_t MaxVolJobs;               /* Max Jobs on Volume */
   uint32_t MaxVolFiles;              /* Max files on Volume */
   int32_t Recycle;                   /* recycle yes/no */
   int32_t Slot;                      /* slot in changer */
   int32_t Enabled;                   /* 0=disabled, 1=enabled, 2=archived */
   int32_t InChanger;                 /* Volume currently in changer */
   DBId_t StorageId;                  /* Storage record Id */
   uint32_t EndFile;                  /* Last file on volume */
   uint32_t EndBlock;                 /* Last block on volume */
   uint32_t RecycleCount;             /* Number of times recycled */
   uint32_t MinBlocksize;             /* Minimum Block Size */
   uint32_t MaxBlocksize;             /* Maximum Block Size */
   char VolStatus[20];                /* Volume status */
   DBId_t DeviceId;                   /* Device where Vol last written */
   DBId_t LocationId;                 /* Where Volume is -- user defined */
   DBId_t ScratchPoolId;              /* Where to move if scratch */
   DBId_t RecyclePoolId;              /* Where to move when recycled */

   /*
    * Extra stuff not in DB
    */
   faddr_t rec_addr;                  /* found record address */

   /*
    * Since the database returns times as strings, this is how we pass them back.
    */
   char cFirstWritten[MAX_TIME_LENGTH]; /* FirstWritten returned from DB */
   char cLastWritten[MAX_TIME_LENGTH];  /* LastWritten returned from DB */
   char cLabelDate[MAX_TIME_LENGTH];    /* LabelData returned from DB */
   char cInitialWrite[MAX_TIME_LENGTH]; /* InitialWrite returned from DB */
   bool set_first_written;
   bool set_label_date;
};

/*
 * Client record -- same as the database
 */
struct CLIENT_DBR {
   DBId_t ClientId;                   /* Unique Client id */
   int AutoPrune;
   utime_t GraceTime;                 /* Time remaining on gracetime */
   uint32_t QuotaLimit;               /* The total softquota supplied if over grace */
   utime_t FileRetention;
   utime_t JobRetention;
   char Name[MAX_NAME_LENGTH];        /* Client name */
   char Uname[256];                   /* Uname for client */
};

/*
 * Counter record -- same as in database
 */
struct COUNTER_DBR {
   char Counter[MAX_NAME_LENGTH];
   int32_t MinValue;
   int32_t MaxValue;
   int32_t CurrentValue;
   char WrapCounter[MAX_NAME_LENGTH];
};

/*
 * FileSet record -- same as the database
 */
struct FILESET_DBR {
   DBId_t FileSetId;                  /* Unique FileSet id */
   char FileSet[MAX_NAME_LENGTH];     /* FileSet name */
   char *FileSetText;                 /* FileSet as Text */
   char MD5[50];                      /* MD5 signature of include/exclude */
   time_t CreateTime;                 /* Date created */
   /*
    * This is where we return CreateTime
    */
   char cCreateTime[MAX_TIME_LENGTH]; /* CreateTime as returned from DB */
   /*
    * Not in DB but returned by db_create_fileset()
    */
   bool created;                      /* set when record newly created */
};

/*
 * Device Statistics record -- same as in database
 */
struct DEVICE_STATS_DBR {
   DBId_t DeviceId;                   /* Device record id */
   time_t SampleTime;                 /* Timestamp statistic was captured */
   uint64_t ReadTime;                 /* Time spent reading volume */
   uint64_t WriteTime;                /* Time spent writing volume */
   uint64_t ReadBytes;                /* Number of bytes read */
   uint64_t WriteBytes;               /* Number of bytes written */
   uint64_t SpoolSize;                /* Number of bytes spooled */
   uint32_t NumWaiting;               /* Number of Jobs waiting for device */
   uint32_t NumWriters;               /* Number of writers to device */
   DBId_t MediaId;                    /* MediaId used */
   uint64_t VolCatBytes;              /* Volume Bytes */
   uint64_t VolCatFiles;              /* Volume Files */
   uint64_t VolCatBlocks;             /* Volume Blocks */
};

/*
 * TapeAlert record -- same as in database
 */
struct TAPEALERT_STATS_DBR {
   DBId_t DeviceId;                   /* Device record id */
   time_t SampleTime;                 /* Timestamp statistic was captured */
   uint64_t AlertFlags;               /* Tape Alerts raised */
};

/*
 * Job Statistics record -- same as in database
 */
struct JOB_STATS_DBR {
   DBId_t DeviceId;                   /* Device record id */
   time_t SampleTime;                 /* Timestamp statistic was captured */
   JobId_t JobId;                     /* Job record id */
   uint32_t JobFiles;                 /* Number of Files in Job */
   uint64_t JobBytes;                 /* Number of Bytes in Job */
};

/*
 * Call back context for getting a 32/64 bit value from the database
 */
class db_int64_ctx {
public:
   int64_t value;                     /* value returned */
   int count;                         /* number of values seen */

   db_int64_ctx() : value(0), count(0) {};
   ~db_int64_ctx() {};
private:
   db_int64_ctx(const db_int64_ctx&);            /* prohibit pass by value */
   db_int64_ctx &operator=(const db_int64_ctx&); /* prohibit class assignment */
};

/*
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
   db_list_ctx(const db_list_ctx&);            /* prohibit pass by value */
   db_list_ctx &operator=(const db_list_ctx&); /* prohibit class assignment */
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

#define db_lock(mdb)   mdb->_db_lock(__FILE__, __LINE__)
#define db_unlock(mdb) mdb->_db_unlock(__FILE__, __LINE__)

/*
 * Current database version number for all drivers
 */
#define BDB_VERSION 2004

class CATS_IMP_EXP B_DB: public SMARTALLOC {
protected:
   brwlock_t m_lock;                      /* transaction lock */
   dlink m_link;                          /* queue control */
   SQL_INTERFACETYPE m_db_interface_type; /* type of backend used */
   SQL_DBTYPE m_db_type;                  /* database type */
   int m_ref_count;                       /* reference count */
   bool m_connected;                      /* connection made to db */
   bool m_have_batch_insert;              /* have batch insert support ? */
   bool m_try_reconnect;                  /* Try reconnecting DB connection ? */
   bool m_exit_on_fatal;                  /* Exit on FATAL DB errors ? */
   char *m_db_driver;                     /* database driver */
   char *m_db_driverdir;                  /* database driver dir */
   char *m_db_name;                       /* database name */
   char *m_db_user;                       /* database user */
   char *m_db_address;                    /* host name address */
   char *m_db_socket;                     /* socket for local access */
   char *m_db_password;                   /* database password */
   int m_db_port;                         /* port for host name address */
   bool m_disabled_batch_insert;          /* explicitly disabled batch insert mode ? */
   bool m_is_private;                     /* private connection ? */

public:
   POOLMEM *errmsg;                       /* nicely edited error message */
   POOLMEM *cmd;                          /* SQL command string */
   POOLMEM *cached_path;                  /* cached path name */
   int cached_path_len;                   /* length of cached path */
   uint32_t cached_path_id;               /* cached path id */
   int changes;                           /* changes during transaction */
   POOLMEM *fname;                        /* Filename only */
   POOLMEM *path;                         /* Path only */
   POOLMEM *esc_name;                     /* Escaped file name */
   POOLMEM *esc_path;                     /* Escaped path name */
   POOLMEM *esc_obj;                      /* Escaped restore object */
   int fnl;                               /* file name length */
   int pnl;                               /* path name length */

   /* methods */
   B_DB() {};
   virtual ~B_DB() {};
   const char *get_db_name(void) { return m_db_name; };
   const char *get_db_user(void) { return m_db_user; };
   bool is_connected(void) { return m_connected; };
   bool batch_insert_available(void) { return m_have_batch_insert; };
   bool is_private(void) { return m_is_private; };
   void set_private(bool is_private) { m_is_private = is_private; };
   void increment_refcount(void) { m_ref_count++; };

   /* low level methods */
   bool db_match_database(const char *db_driver, const char *db_name,
                          const char *db_address, int db_port);
   B_DB *db_clone_database_connection(JCR *jcr,
                                      bool mult_db_connections,
                                      bool get_pooled_connection = true,
                                      bool need_private = false);
   int db_get_type_index(void) { return m_db_type; };
   const char *db_get_type(void);
   void _db_lock(const char *file, int line);
   void _db_unlock(const char *file, int line);
   bool db_sql_query(const char *query, int flags=0);
   void print_lock_info(FILE *fp);

   /* Virtual low level methods */
   virtual void db_thread_cleanup(void) {};
   virtual void db_escape_string(JCR *jcr, char *snew, char *old, int len);
   virtual char *db_escape_object(JCR *jcr, char *old, int len);
   virtual void db_unescape_object(JCR *jcr, char *from, int32_t expected_len,
                                   POOLMEM **dest, int32_t *len);

   /* Pure virtual low level methods */
   virtual bool db_open_database(JCR *jcr) = 0;
   virtual void db_close_database(JCR *jcr) = 0;
   virtual bool db_validate_connection(void) = 0;
   virtual void db_start_transaction(JCR *jcr) = 0;
   virtual void db_end_transaction(JCR *jcr) = 0;
   virtual bool db_sql_query(const char *query, DB_RESULT_HANDLER *result_handler, void *ctx) = 0;

   /* By default, we use db_sql_query */
   virtual bool db_big_sql_query(const char *query,
                                 DB_RESULT_HANDLER *result_handler, void *ctx) {
      return db_sql_query(query, result_handler, ctx);
   };
};

/* sql_query Query Flags */
#define QF_STORE_RESULT 0x01

/* flush the batch insert connection every x changes */
#define BATCH_FLUSH 800000

/* Use for better error location printing */
#define UPDATE_DB(jcr, db, cmd) UpdateDB(__FILE__, __LINE__, jcr, db, cmd, 1)
#define UPDATE_DB_NO_AFR(jcr, db, cmd) UpdateDB(__FILE__, __LINE__, jcr, db, cmd, 0)
#define INSERT_DB(jcr, db, cmd) InsertDB(__FILE__, __LINE__, jcr, db, cmd)
#define QUERY_DB(jcr, db, cmd) QueryDB(__FILE__, __LINE__, jcr, db, cmd)
#define DELETE_DB(jcr, db, cmd) DeleteDB(__FILE__, __LINE__, jcr, db, cmd)

/*
 * Pooled backend connection.
 */
struct SQL_POOL_ENTRY {
   int id;                                /* Unique ID, connection numbering can have holes and the pool is not sorted on it */
   int reference_count;                   /* Reference count for this entry */
   time_t last_update;                    /* When was this connection last updated either used or put back on the pool */
   B_DB *db_handle;                       /* Connection handle to the database */
   dlink link;                            /* list management */
};

/*
 * Pooled backend list descriptor (one defined per backend defined in config)
 */
struct SQL_POOL_DESCRIPTOR {
   dlist *pool_entries;                   /* Linked list of all pool entries */
   bool active;                           /* Is this an active pool, after a config reload an pool is made inactive */
   time_t last_update;                    /* When was this pool last updated */
   int min_connections;                   /* Minimum number of connections in the connection pool */
   int max_connections;                   /* Maximum number of connections in the connection pool */
   int increment_connections;             /* Increase/Decrease the number of connection in the pool with this value */
   int idle_timeout;                      /* Number of seconds to wait before tearing down a connection */
   int validate_timeout;                  /* Number of seconds after which an idle connection should be validated */
   int nr_connections;                    /* Number of active connections in the pool */
   dlink link;                            /* list management */
};

#include "protos.h"
#include "jcr.h"
#include "sql_cmds.h"

/*
 * Object used in db_list_xxx function
 */
class LIST_CTX {
public:
   char line[256];              /* Used to print last dash line */
   int32_t num_rows;

   e_list_type type;            /* Vertical/Horizontal */
   OUTPUT_FORMATTER *send;      /* send data back */
   bool once;                   /* Used to print header one time */
   B_DB *mdb;
   JCR *jcr;

   void empty() {
      once = false;
      line[0] = '\0';
   }

   void send_dashes() {
      if (*line) {
         send->decoration(line);
      }
   }

   LIST_CTX(JCR *j, B_DB *m, OUTPUT_FORMATTER *h, e_list_type t) {
      line[0] = '\0';
      once = false;
      num_rows = 0;
      type = t;
      send = h;
      jcr = j;
      mdb = m;
   }
};

/*
 * Some functions exported by sql.c for use within the cats directory.
 */
int list_result(void *vctx, int cols, char **row);
int list_result(JCR *jcr, B_DB *mdb, OUTPUT_FORMATTER *send, e_list_type type);
int get_sql_record_max(JCR *jcr, B_DB *mdb);
bool check_tables_version(JCR *jcr, B_DB *mdb);
bool db_check_max_connections(JCR *jcr, B_DB *mdb, uint32_t nb);

void print_dashes(B_DB *mdb);
void print_result(B_DB *mdb);
bool QueryDB(const char *file, int line, JCR *jcr, B_DB *db, char *select_cmd);
bool InsertDB(const char *file, int line, JCR *jcr, B_DB *db, char *select_cmd);
int DeleteDB(const char *file, int line, JCR *jcr, B_DB *db, char *delete_cmd);
bool UpdateDB(const char *file, int line, JCR *jcr, B_DB *db, char *update_cmd, int nr_afr);
void split_path_and_file(JCR *jcr, B_DB *mdb, const char *fname);
#endif /* __CATS_H_ */
