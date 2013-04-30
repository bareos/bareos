/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

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
 * Bacula JCR Structure definition for Daemons and the Library
 *  This definition consists of a "Global" definition common
 *  to all daemons and used by the library routines, and a
 *  daemon specific part that is enabled with #defines.
 *
 * Kern Sibbald, Nov MM
 *
 */


#ifndef __JCR_H_
#define __JCR_H_ 1

/* Backup/Verify level code. These are stored in the DB */
#define L_FULL                   'F'  /* Full backup */
#define L_INCREMENTAL            'I'  /* since last backup */
#define L_DIFFERENTIAL           'D'  /* since last full backup */
#define L_SINCE                  'S'
#define L_VERIFY_CATALOG         'C'  /* verify from catalog */
#define L_VERIFY_INIT            'V'  /* verify save (init DB) */
#define L_VERIFY_VOLUME_TO_CATALOG 'O'  /* verify Volume to catalog entries */
#define L_VERIFY_DISK_TO_CATALOG 'd'  /* verify Disk attributes to catalog */
#define L_VERIFY_DATA            'A'  /* verify data on volume */
#define L_BASE                   'B'  /* Base level job */
#define L_NONE                   ' '  /* None, for Restore and Admin */
#define L_VIRTUAL_FULL           'f'  /* Virtual full backup */


/* Job Types. These are stored in the DB */
#define JT_BACKUP                'B'  /* Backup Job */
#define JT_MIGRATED_JOB          'M'  /* A previous backup job that was migrated */
#define JT_VERIFY                'V'  /* Verify Job */
#define JT_RESTORE               'R'  /* Restore Job */
#define JT_CONSOLE               'U'  /* console program */
#define JT_SYSTEM                'I'  /* internal system "job" */
#define JT_ADMIN                 'D'  /* admin job */
#define JT_ARCHIVE               'A'  /* Archive Job */
#define JT_JOB_COPY              'C'  /* Copy of a Job */
#define JT_COPY                  'c'  /* Copy Job */
#define JT_MIGRATE               'g'  /* Migration Job */
#define JT_SCAN                  'S'  /* Scan Job */

/* Job Status. Some of these are stored in the DB */
#define JS_Canceled              'A'  /* canceled by user */
#define JS_Blocked               'B'  /* blocked */
#define JS_Created               'C'  /* created but not yet running */
#define JS_Differences           'D'  /* Verify differences */
#define JS_ErrorTerminated       'E'  /* Job terminated in error */
#define JS_WaitFD                'F'  /* waiting on File daemon */
#define JS_Incomplete            'I'  /* Incomplete Job */
#define JS_DataCommitting        'L'  /* Committing data (last despool) */
#define JS_WaitMount             'M'  /* waiting for Mount */
#define JS_Running               'R'  /* running */
#define JS_WaitSD                'S'  /* waiting on the Storage daemon */
#define JS_Terminated            'T'  /* terminated normally */
#define JS_Warnings              'W'  /* Terminated normally with warnings */

#define JS_AttrDespooling        'a'  /* SD despooling attributes */
#define JS_WaitClientRes         'c'  /* Waiting for Client resource */
#define JS_WaitMaxJobs           'd'  /* Waiting for maximum jobs */
#define JS_Error                 'e'  /* Non-fatal error */
#define JS_FatalError            'f'  /* Fatal error */
#define JS_AttrInserting         'i'  /* Doing batch insert file records */
#define JS_WaitJobRes            'j'  /* Waiting for job resource */
#define JS_DataDespooling        'l'  /* Doing data despooling */
#define JS_WaitMedia             'm'  /* waiting for new media */
#define JS_WaitPriority          'p'  /* Waiting for higher priority jobs to finish */
#define JS_WaitDevice            'q'  /* Queued waiting for device */
#define JS_WaitStoreRes          's'  /* Waiting for storage resource */
#define JS_WaitStartTime         't'  /* Waiting for start time */ 


/* Migration selection types */
enum {
   MT_SMALLEST_VOL = 1,
   MT_OLDEST_VOL,
   MT_POOL_OCCUPANCY,
   MT_POOL_TIME,
   MT_POOL_UNCOPIED_JOBS,
   MT_CLIENT,
   MT_VOLUME,
   MT_JOB,
   MT_SQLQUERY
};

#define job_canceled(jcr) \
  (jcr->JobStatus == JS_Canceled || \
   jcr->JobStatus == JS_ErrorTerminated || \
   jcr->JobStatus == JS_FatalError \
  )

#define job_waiting(jcr) \
 (jcr->job_started &&    \
  (jcr->JobStatus == JS_WaitFD       || \
   jcr->JobStatus == JS_WaitSD       || \
   jcr->JobStatus == JS_WaitMedia    || \
   jcr->JobStatus == JS_WaitMount    || \
   jcr->JobStatus == JS_WaitStoreRes || \
   jcr->JobStatus == JS_WaitJobRes   || \
   jcr->JobStatus == JS_WaitClientRes|| \
   jcr->JobStatus == JS_WaitMaxJobs  || \
   jcr->JobStatus == JS_WaitPriority || \
   jcr->SDJobStatus == JS_WaitMedia  || \
   jcr->SDJobStatus == JS_WaitMount  || \
   jcr->SDJobStatus == JS_WaitDevice || \
   jcr->SDJobStatus == JS_WaitMaxJobs))



#define foreach_jcr(jcr) \
   for (jcr=jcr_walk_start(); jcr; (jcr=jcr_walk_next(jcr)) )

#define endeach_jcr(jcr) jcr_walk_end(jcr)

#define SD_APPEND 1
#define SD_READ   0

/* Forward referenced structures */
class JCR;
class BSOCK;
struct FF_PKT;
class  B_DB;
struct ATTR_DBR;
class Plugin;
struct save_pkt;
struct bpContext;
struct xattr_private_data_t;

#ifdef FILE_DAEMON
class htable;
struct acl_data_t;
struct xattr_data_t;

struct CRYPTO_CTX {
   bool pki_sign;                     /* Enable PKI Signatures? */
   bool pki_encrypt;                  /* Enable PKI Encryption? */
   DIGEST *digest;                    /* Last file's digest context */
   X509_KEYPAIR *pki_keypair;         /* Encryption key pair */
   alist *pki_signers;                /* Trusted Signers */
   alist *pki_recipients;             /* Trusted Recipients */
   CRYPTO_SESSION *pki_session;       /* PKE Public Keys + Symmetric Session Keys */
   POOLMEM *pki_session_encoded;      /* Cached DER-encoded copy of pki_session */
   int32_t pki_session_encoded_size;  /* Size of DER-encoded pki_session */
   POOLMEM *crypto_buf;               /* Encryption/Decryption buffer */
};
#endif

typedef void (JCR_free_HANDLER)(JCR *jcr);

/* Job Control Record (JCR) */
class JCR {
private:
   pthread_mutex_t mutex;             /* jcr mutex */
   volatile int32_t _use_count;       /* use count */
   int32_t m_JobType;                 /* backup, restore, verify ... */
   int32_t m_JobLevel;                /* Job level */
   bool my_thread_killable;           /* can we kill the thread? */
public:
   void lock() {P(mutex); };
   void unlock() {V(mutex); };
   void inc_use_count(void) {lock(); _use_count++; unlock(); };
   void dec_use_count(void) {lock(); _use_count--; unlock(); };
   int32_t use_count() const { return _use_count; };
   void init_mutex(void) {pthread_mutex_init(&mutex, NULL); };
   void destroy_mutex(void) {pthread_mutex_destroy(&mutex); };
   bool is_job_canceled() {return job_canceled(this); };
   bool is_canceled() {return job_canceled(this); };
   bool is_incomplete() { return JobStatus == JS_Incomplete; };
   bool is_JobLevel(int32_t JobLevel) { return JobLevel == m_JobLevel; };
   bool is_JobType(int32_t JobType) { return JobType == m_JobType; };
   bool is_JobStatus(int32_t aJobStatus) { return aJobStatus == JobStatus; };
   void setJobLevel(int32_t JobLevel) { m_JobLevel = JobLevel; };
   void setJobType(int32_t JobType) { m_JobType = JobType; };
   void forceJobStatus(int32_t aJobStatus) { JobStatus = aJobStatus; };
   void setJobStarted();
   int32_t getJobType() const { return m_JobType; };
   int32_t getJobLevel() const { return m_JobLevel; };
   int32_t getJobStatus() const { return JobStatus; };
   bool no_client_used() const {
      return (m_JobType == JT_MIGRATE || m_JobType == JT_COPY ||
              m_JobLevel == L_VIRTUAL_FULL);
   };
   const char *get_OperationName();       /* in lib/jcr.c */
   const char *get_ActionName(bool past); /* in lib/jcr.c */
   void setJobStatus(int newJobStatus);   /* in lib/jcr.c */
   bool sendJobStatus();                  /* in lib/jcr.c */
   bool sendJobStatus(int newJobStatus);  /* in lib/jcr.c */
   bool JobReads();                       /* in lib/jcr.c */
   void my_thread_send_signal(int sig);   /* in lib/jcr.c */
   void set_killable(bool killable);      /* in lib/jcr.c */
   bool is_killable() const { return my_thread_killable; };

   /* Global part of JCR common to all daemons */
   dlink link;                        /* JCR chain link */
   pthread_t my_thread_id;            /* id of thread controlling jcr */
   BSOCK *dir_bsock;                  /* Director bsock or NULL if we are him */
   BSOCK *store_bsock;                /* Storage connection socket */
   BSOCK *file_bsock;                 /* File daemon connection socket */
   JCR_free_HANDLER *daemon_free_jcr; /* Local free routine */
   dlist *msg_queue;                  /* Queued messages */
   pthread_mutex_t msg_queue_mutex;   /* message queue mutex */
   bool dequeuing_msgs;               /* Set when dequeuing messages */
   alist job_end_push;                /* Job end pushed calls */
   POOLMEM *VolumeName;               /* Volume name desired -- pool_memory */
   POOLMEM *errmsg;                   /* edited error message */
   char Job[MAX_NAME_LENGTH];         /* Unique name of this Job */
   char event[MAX_NAME_LENGTH];       /* Current event (python) */
   uint32_t eventType;                /* Current event type (plugin) */

   uint32_t JobId;                    /* Director's JobId */
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   uint32_t JobFiles;                 /* Number of files written, this job */
   uint32_t JobErrors;                /* Number of non-fatal errors this job */
   uint32_t JobWarnings;              /* Number of warning messages */
   uint32_t LastRate;                 /* Last sample bytes/sec */
   uint64_t JobBytes;                 /* Number of bytes processed this job */
   uint64_t LastJobBytes;             /* Last sample number bytes */
   uint64_t ReadBytes;                /* Bytes read -- before compression */
   FileId_t FileId;                   /* Last FileId used */
   volatile int32_t JobStatus;        /* ready, running, blocked, terminated */
   int32_t JobPriority;               /* Job priority */
   time_t sched_time;                 /* job schedule time, i.e. when it should start */
   time_t start_time;                 /* when job actually started */
   time_t run_time;                   /* used for computing speed */
   time_t last_time;                  /* Last sample time */
   time_t end_time;                   /* job end time */
   time_t wait_time_sum;              /* cumulative wait time since job start */
   time_t wait_time;                  /* timestamp when job have started to wait */
   time_t job_started_time;           /* Time when the MaxRunTime start to count */
   POOLMEM *client_name;              /* client name */
   POOLMEM *JobIds;                   /* User entered string of JobIds */
   POOLMEM *RestoreBootstrap;         /* Bootstrap file to restore */
   POOLMEM *stime;                    /* start time for incremental/differential */
   char *sd_auth_key;                 /* SD auth key */
   MSGS *jcr_msgs;                    /* Copy of message resource -- actually used */
   uint32_t ClientId;                 /* Client associated with Job */
   char *where;                       /* prefix to restore files to */
   char *RegexWhere;                  /* file relocation in restore */
   alist *where_bregexp;              /* BREGEXP alist for path manipulation */
   int32_t cached_pnl;                /* cached path length */
   POOLMEM *cached_path;              /* cached path */
   bool prefix_links;                 /* Prefix links with Where path */
   bool gui;                          /* set if gui using console */
   bool authenticated;                /* set when client authenticated */
   bool cached_attribute;             /* set if attribute is cached */
   bool batch_started;                /* is batch mode already started ? */
   bool cmd_plugin;                   /* Set when processing a command Plugin = */
   bool opt_plugin;                   /* Set when processing an option Plugin = */
   bool keep_path_list;               /* Keep newly created path in a hash */
   bool accurate;                     /* true if job is accurate */
   bool HasBase;                      /* True if job use base jobs */
   bool rerunning;                    /* rerunning an incomplete job */
   bool job_started;                  /* Set when the job is actually started */

   void *Python_job;                  /* Python Job Object */
   void *Python_events;               /* Python Events Object */
   POOLMEM *attr;                     /* Attribute string from SD */
   B_DB *db;                          /* database pointer */
   B_DB *db_batch;                    /* database pointer for batch and accurate */
   uint64_t nb_base_files;            /* Number of base files */
   uint64_t nb_base_files_used;       /* Number of useful files in base */

   ATTR_DBR *ar;                      /* DB attribute record */
   guid_list *id_list;                /* User/group id to name list */

   bpContext *plugin_ctx_list;        /* list of contexts for plugins */
   bpContext *plugin_ctx;             /* current plugin context */
   Plugin *plugin;                    /* plugin instance */
   save_pkt *plugin_sp;               /* plugin save packet */
   char *plugin_options;              /* user set options for plugin */
   POOLMEM *comment;                  /* Comment for this Job */
   int64_t max_bandwidth;             /* Bandwidth limit for this Job */
   htable *path_list;                 /* Directory list (used by findlib) */

   /* Daemon specific part of JCR */
   /* This should be empty in the library */

#ifdef DIRECTOR_DAEMON
   /* Director Daemon specific data part of JCR */
   pthread_t SD_msg_chan;             /* Message channel thread id */
   pthread_cond_t term_wait;          /* Wait for job termination */
   workq_ele_t *work_item;            /* Work queue item if scheduled */
   BSOCK *ua;                         /* User agent */
   JOB *job;                          /* Job resource */
   JOB *verify_job;                   /* Job resource of verify previous job */
   alist *rstorage;                   /* Read storage possibilities */
   STORE *rstore;                     /* Selected read storage */
   alist *wstorage;                   /* Write storage possibilities */
   STORE *wstore;                     /* Selected write storage */
   CLIENT *client;                    /* Client resource */
   POOL *pool;                        /* Pool resource = write for migration */
   POOL *rpool;                       /* Read pool. Used only in migration */
   POOL *full_pool;                   /* Full backup pool resource */
   POOL *inc_pool;                    /* Incremental backup pool resource */
   POOL *diff_pool;                   /* Differential backup pool resource */
   FILESET *fileset;                  /* FileSet resource */
   CAT *catalog;                      /* Catalog resource */
   MSGS *messages;                    /* Default message handler */
   uint32_t SDJobFiles;               /* Number of files written, this job */
   uint64_t SDJobBytes;               /* Number of bytes processed this job */
   uint32_t SDErrors;                 /* Number of non-fatal errors */
   volatile int32_t SDJobStatus;      /* Storage Job Status */
   volatile int32_t FDJobStatus;      /* File daemon Job Status */
   uint32_t ExpectedFiles;            /* Expected restore files */
   uint32_t MediaId;                  /* DB record IDs associated with this job */
   uint32_t FileIndex;                /* Last FileIndex processed */
   utime_t MaxRunSchedTime;           /* max run time in seconds from Scheduled time*/
   POOLMEM *fname;                    /* name to put into catalog */
   POOLMEM *component_fname;          /* Component info file name */
   FILE *component_fd;                /* Component info file desc */
   JOB_DBR jr;                        /* Job DB record for current job */
   JOB_DBR previous_jr;               /* previous job database record */
   JOB *previous_job;                 /* Job resource of migration previous job */
   JCR *mig_jcr;                      /* JCR for migration/copy job */
   char FSCreateTime[MAX_TIME_LENGTH]; /* FileSet CreateTime as returned from DB */
   char since[MAX_TIME_LENGTH];       /* since time */
   char PrevJob[MAX_NAME_LENGTH];     /* Previous job name assiciated with since time */
   union {
      JobId_t RestoreJobId;           /* Id specified by UA */
      JobId_t MigrateJobId;
   };
   POOLMEM *client_uname;             /* client uname */
   POOLMEM *pool_source;              /* Where pool came from */
   POOLMEM *rpool_source;             /* Where migrate read pool came from */
   POOLMEM *rstore_source;            /* Where read storage came from */
   POOLMEM *wstore_source;            /* Where write storage came from */
   POOLMEM *catalog_source;           /* Where catalog came from */
   uint32_t replace;                  /* Replace option */
   int32_t NumVols;                   /* Number of Volume used in pool */
   int32_t reschedule_count;          /* Number of times rescheduled */
   int32_t FDVersion;                 /* File daemon version number */
   int64_t spool_size;                /* Spool size for this job */
   volatile bool sd_msg_thread_done;  /* Set when Storage message thread done */
   bool wasVirtualFull;               /* set if job was VirtualFull */
   bool IgnoreDuplicateJobChecking;   /* set in migration jobs */
   bool spool_data;                   /* Spool data in SD */
   bool acquired_resource_locks;      /* set if resource locks acquired */
   bool term_wait_inited;             /* Set when cond var inited */
   bool fn_printed;                   /* printed filename */
   bool write_part_after_job;         /* Write part after job in SD */
   bool needs_sd;                     /* set if SD needed by Job */
   bool cloned;                       /* set if cloned */
   bool unlink_bsr;                   /* Unlink bsr file created */
   bool VSS;                          /* VSS used by FD */
   bool Encrypt;                      /* Encryption used by FD */
   bool stats_enabled;                /* Keep all job records in a table for long term statistics */
   bool no_maxtime;                   /* Don't check Max*Time for this JCR */
   bool keep_sd_auth_key;             /* Clear or not the SD auth key after connection*/
   bool use_accurate_chksum;          /* Use or not checksum option in accurate code */
   bool run_pool_override;
   bool run_full_pool_override;
   bool run_inc_pool_override;
   bool run_diff_pool_override;
   bool sd_canceled;                  /* set if SD canceled */
   bool RescheduleIncompleteJobs;     /* set if incomplete can be rescheduled */
#endif /* DIRECTOR_DAEMON */


#ifdef FILE_DAEMON
   /* File Daemon specific part of JCR */
   uint32_t num_files_examined;       /* files examined this job */
   POOLMEM *last_fname;               /* last file saved/verified */
   POOLMEM *job_metadata;             /* VSS job metadata */
   acl_data_t *acl_data;              /* ACLs for backup/restore */
   xattr_data_t *xattr_data;          /* Extended Attributes for backup/restore */
   int32_t last_type;                 /* type of last file saved/verified */
   int incremental;                   /* set if incremental for SINCE */
   utime_t mtime;                     /* begin time for SINCE */
   int listing;                       /* job listing in estimate */
   long Ticket;                       /* Ticket */
   char *big_buf;                     /* I/O buffer */
   POOLMEM *compress_buf;             /* Compression buffer */
   int32_t compress_buf_size;         /* Length of compression buffer */
   void *pZLIB_compress_workset;      /* zlib compression session data */
   void *LZO_compress_workset;        /* lzo compression session data */
   int32_t replace;                   /* Replace options */
   int32_t buf_size;                  /* length of buffer */
   FF_PKT *ff;                        /* Find Files packet */
   char stored_addr[MAX_NAME_LENGTH]; /* storage daemon address */
   char PrevJob[MAX_NAME_LENGTH];     /* Previous job name assiciated with since time */
   uint32_t ExpectedFiles;            /* Expected restore files */
   uint32_t StartFile;
   uint32_t EndFile;
   uint32_t StartBlock;
   uint32_t EndBlock;
   pthread_t heartbeat_id;            /* id of heartbeat thread */
   volatile bool hb_started;          /* heartbeat running */
   BSOCK *hb_bsock;                   /* duped SD socket */
   BSOCK *hb_dir_bsock;               /* duped DIR socket */
   alist *RunScripts;                 /* Commands to run before and after job */
   CRYPTO_CTX crypto;                 /* Crypto ctx */
   DIRRES* director;                  /* Director resource */
   bool VSS;                          /* VSS used by FD */
   bool got_metadata;                 /* set when found job_metatdata */
   bool multi_restore;                /* Dir can do multiple storage restore */
   htable *file_list;                 /* Previous file list (accurate mode) */
   uint64_t base_size;                /* compute space saved with base job */
#endif /* FILE_DAEMON */


#ifdef STORAGE_DAEMON
   /* Storage Daemon specific part of JCR */
   JCR *next_dev;                     /* next JCR attached to device */
   JCR *prev_dev;                     /* previous JCR attached to device */
   char *dir_auth_key;                /* Dir auth key */
   pthread_cond_t job_start_wait;     /* Wait for FD to start Job */
   int32_t type;
   DCR *read_dcr;                     /* device context for reading */
   DCR *dcr;                          /* device context record */
   alist *dcrs;                       /* list of dcrs open */
   POOLMEM *job_name;                 /* base Job name (not unique) */
   POOLMEM *fileset_name;             /* FileSet */
   POOLMEM *fileset_md5;              /* MD5 for FileSet */
   VOL_LIST *VolList;                 /* list to read */
   int32_t NumWriteVolumes;           /* number of volumes written */
   int32_t NumReadVolumes;            /* total number of volumes to read */
   int32_t CurReadVolume;             /* current read volume number */
   int32_t label_errors;              /* count of label errors */
   bool session_opened;
   long Ticket;                       /* ticket for this job */
   bool ignore_label_errors;          /* ignore Volume label errors */
   bool spool_attributes;             /* set if spooling attributes */
   bool no_attributes;                /* set if no attributes wanted */
   int64_t spool_size;                /* Spool size for this job */
   bool spool_data;                   /* set to spool data */
   int32_t CurVol;                    /* Current Volume count */
   DIRRES* director;                  /* Director resource */
   alist *write_store;                /* list of write storage devices sent by DIR */ 
   alist *read_store;                 /* list of read devices sent by DIR */
   alist *reserve_msgs;               /* reserve fail messages */
   bool write_part_after_job;         /* Set to write part after job */
   bool PreferMountedVols;            /* Prefer mounted vols rather than new */
   bool Resched;                      /* Job may be rescheduled */
   bool bscan_insert_jobmedia_records; /*Bscan: needs to insert job media records */

   /* Parmaters for Open Read Session */
   BSR *bsr;                          /* Bootstrap record -- has everything */
   bool mount_next_volume;            /* set to cause next volume mount */
   uint32_t read_VolSessionId;
   uint32_t read_VolSessionTime;
   uint32_t read_StartFile;
   uint32_t read_EndFile;
   uint32_t read_StartBlock;
   uint32_t read_EndBlock;
   /* Device wait times */
   int32_t min_wait;
   int32_t max_wait;
   int32_t max_num_wait;
   int32_t wait_sec;
   int32_t rem_wait_sec;
   int32_t num_wait;
#endif /* STORAGE_DAEMON */

};

/*
 * Setting a NULL in tsd doesn't clear the tsd but instead tells
 *   pthreads not to call the tsd destructor. Consequently, we 
 *   define this *invalid* jcr address and stuff it in the tsd
 *   when the jcr is not valid.
 */
#define INVALID_JCR ((JCR *)(-1))


/*
 * Structure for all daemons that keeps some summary
 *  info on the last job run.
 */
struct s_last_job {
   dlink link;
   int32_t Errors;                    /* FD/SD errors */
   int32_t JobType;
   int32_t JobStatus;
   int32_t JobLevel;
   uint32_t JobId;
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   uint32_t JobFiles;
   uint64_t JobBytes;
   utime_t start_time;
   utime_t end_time;
   char Job[MAX_NAME_LENGTH];
};

extern struct s_last_job last_job;
extern DLL_IMP_EXP dlist *last_jobs;


/* The following routines are found in lib/jcr.c */
extern int get_next_jobid_from_list(char **p, uint32_t *JobId);
extern bool init_jcr_subsystem(void);
extern JCR *new_jcr(int size, JCR_free_HANDLER *daemon_free_jcr);
extern JCR *get_jcr_by_id(uint32_t JobId);
extern JCR *get_jcr_by_session(uint32_t SessionId, uint32_t SessionTime);
extern JCR *get_jcr_by_partial_name(char *Job);
extern JCR *get_jcr_by_full_name(char *Job);
extern JCR *get_next_jcr(JCR *jcr);
extern void set_jcr_job_status(JCR *jcr, int JobStatus);
extern int DLL_IMP_EXP num_jobs_run;

#ifdef DEBUG
extern void b_free_jcr(const char *file, int line, JCR *jcr);
#define free_jcr(jcr) b_free_jcr(__FILE__, __LINE__, (jcr))
#else
extern void free_jcr(JCR *jcr);
#endif

/* Used to display specific job information after a fatal signal */
typedef void (dbg_jcr_hook_t)(JCR *jcr, FILE *fp);
extern void dbg_jcr_add_hook(dbg_jcr_hook_t *fct);

#endif /* __JCR_H_ */
