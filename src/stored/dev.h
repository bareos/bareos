/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Definitions for using the Device functions in Bareos Tape and File storage access
 *
 * Kern Sibbald, MM
 */

/*
 * Some details of how volume and device reservations work
 *
 * class VOLRES:
 *   set_in_use()     volume being used on current drive
 *   clear_in_use()   no longer being used.  Can be re-used or moved.
 *   set_swapping()   set volume being moved to another drive
 *   is_swapping()    volume is being moved to another drive
 *   clear_swapping() volume normal
 *
 * class DEVICE:
 *   set_load()       set to load volume
 *   needs_load()     volume must be loaded (i.e. set_load done)
 *   clear_load()     load done.
 *   set_unload()     set to unload volume
 *   needs_unload()    volume must be unloaded
 *   clear_unload()   volume unloaded
 *
 *    reservations are temporary until the drive is acquired
 *   inc_reserved()   increments num of reservations
 *   dec_reserved()   decrements num of reservations
 *   num_reserved()   number of reservations
 *
 * class DCR:
 *   set_reserved()   sets local reserve flag and calls dev->inc_reserved()
 *   clear_reserved() clears local reserve flag and calls dev->dec_reserved()
 *   is_reserved()    returns local reserved flag
 *   unreserve_device()  much more complete unreservation
 */

#ifndef __DEV_H
#define __DEV_H 1

#undef DCR                            /* used by Bareos */

/*
 * Return values from wait_for_sysop()
 */
enum {
   W_ERROR = 1,
   W_TIMEOUT,
   W_POLL,
   W_MOUNT,
   W_WAKE
};

/*
 * Arguments to open_dev()
 */
enum {
   CREATE_READ_WRITE = 1,
   OPEN_READ_WRITE,
   OPEN_READ_ONLY,
   OPEN_WRITE_ONLY
};

/*
 * Device types
 */
enum {
   B_FILE_DEV = 1,
   B_TAPE_DEV,
   B_FIFO_DEV,
   B_VTL_DEV,
   B_GFAPI_DEV,
   B_OBJECT_STORE_DEV,
   B_RADOS_DEV,
   B_CEPHFS_DEV
};

/*
 * IO directions
 */
enum {
   IO_DIRECTION_NONE = 0,
   IO_DIRECTION_IN,
   IO_DIRECTION_OUT,
   IO_DIRECTION_INOUT
};

/*
 * Generic status bits returned from status_dev()
 */
#define BMT_TAPE           (1 << 0)   /* is tape device */
#define BMT_EOF            (1 << 1)   /* just read EOF */
#define BMT_BOT            (1 << 2)   /* at beginning of tape */
#define BMT_EOT            (1 << 3)   /* end of tape reached */
#define BMT_SM             (1 << 4)   /* DDS setmark */
#define BMT_EOD            (1 << 5)   /* DDS at end of data */
#define BMT_WR_PROT        (1 << 6)   /* tape write protected */
#define BMT_ONLINE         (1 << 7)   /* tape online */
#define BMT_DR_OPEN        (1 << 8)   /* tape door open */
#define BMT_IM_REP_EN      (1 << 9)   /* immediate report enabled */

/*
 * Bits for device capabilities
 */
#define CAP_EOF            (1 << 0)   /* has MTWEOF */
#define CAP_BSR            (1 << 1)   /* has MTBSR */
#define CAP_BSF            (1 << 2)   /* has MTBSF */
#define CAP_FSR            (1 << 3)   /* has MTFSR */
#define CAP_FSF            (1 << 4)   /* has MTFSF */
#define CAP_EOM            (1 << 5)   /* has MTEOM */
#define CAP_REM            (1 << 6)   /* is removable media */
#define CAP_RACCESS        (1 << 7)   /* is random access device */
#define CAP_AUTOMOUNT      (1 << 8)   /* Read device at start to see what is there */
#define CAP_LABEL          (1 << 9)   /* Label blank tapes */
#define CAP_ANONVOLS       (1 << 10)  /* Mount without knowing volume name */
#define CAP_ALWAYSOPEN     (1 << 11)  /* always keep device open */
#define CAP_AUTOCHANGER    (1 << 12)  /* AutoChanger */
#define CAP_OFFLINEUNMOUNT (1 << 13)  /* Offline before unmount */
#define CAP_STREAM         (1 << 14)  /* Stream device */
#define CAP_BSFATEOM       (1 << 15)  /* Backspace file at EOM */
#define CAP_FASTFSF        (1 << 16)  /* Fast forward space file */
#define CAP_TWOEOF         (1 << 17)  /* Write two eofs for EOM */
#define CAP_CLOSEONPOLL    (1 << 18)  /* Close device on polling */
#define CAP_POSITIONBLOCKS (1 << 19)  /* Use block positioning */
#define CAP_MTIOCGET       (1 << 20)  /* Basic support for fileno and blkno */
#define CAP_REQMOUNT       (1 << 21)  /* Require mount/unmount */
#define CAP_CHECKLABELS    (1 << 22)  /* Check for ANSI/IBM labels */
#define CAP_BLOCKCHECKSUM  (1 << 23)  /* Create/test block checksum */
#define CAP_IOERRATEOM     (1 << 24)  /* IOError at EOM */
#define CAP_IBMLINTAPE     (1 << 25)  /* Using IBM lin_tape driver */

/*
 * Device state bits
 */
#define ST_XXXXXX          (1 << 0)   /* was ST_OPENED */
#define ST_XXXXX           (1 << 1)   /* was ST_TAPE */
#define ST_XXXX            (1 << 2)   /* was ST_FILE */
#define ST_XXX             (1 << 3)   /* was ST_FIFO */
#define ST_XX              (1 << 4)   /* was ST_DVD */
#define ST_X               (1 << 5)   /* was ST_PROG */

#define ST_LABEL           (1 << 6)   /* label found */
#define ST_MALLOC          (1 << 7)   /* dev packet malloc'ed in init_dev() */
#define ST_APPENDREADY     (1 << 8)   /* Ready for Bareos append */
#define ST_READREADY       (1 << 9)   /* Ready for Bareos read */
#define ST_EOT             (1 << 10)  /* at end of tape */
#define ST_WEOT            (1 << 11)  /* Got EOT on write */
#define ST_EOF             (1 << 12)  /* Read EOF i.e. zero bytes */
#define ST_NEXTVOL         (1 << 13)  /* Start writing on next volume */
#define ST_SHORT           (1 << 14)  /* Short block read */
#define ST_MOUNTED         (1 << 15)  /* the device is mounted to the mount point */
#define ST_MEDIA           (1 << 16)  /* Media found in mounted device */
#define ST_OFFLINE         (1 << 17)  /* set offline by operator */
#define ST_PART_SPOOLED    (1 << 18)  /* spooling part */
#define ST_CRYPTOKEY       (1 << 19)  /* The device has a crypto key loaded */

/*
 * Volume Catalog Information structure definition
 */
struct VOLUME_CAT_INFO {
   /*
    * Media info for the current Volume
    */
   uint32_t VolCatJobs;               /* number of jobs on this Volume */
   uint32_t VolCatFiles;              /* Number of files */
   uint32_t VolCatBlocks;             /* Number of blocks */
   uint64_t VolCatBytes;              /* Number of bytes written */
   uint32_t VolCatMounts;             /* Number of mounts this volume */
   uint32_t VolCatErrors;             /* Number of errors this volume */
   uint32_t VolCatWrites;             /* Number of writes this volume */
   uint32_t VolCatReads;              /* Number of reads this volume */
   uint64_t VolCatRBytes;             /* Number of bytes read */
   uint32_t VolCatRecycles;           /* Number of recycles this volume */
   uint32_t EndFile;                  /* Last file number */
   uint32_t EndBlock;                 /* Last block number */
   int32_t LabelType;                 /* Bareos/ANSI/IBM */
   int32_t Slot;                      /* >0=Slot loaded, 0=nothing, -1=unknown */
   uint32_t VolCatMaxJobs;            /* Maximum Jobs to write to volume */
   uint32_t VolCatMaxFiles;           /* Maximum files to write to volume */
   uint64_t VolCatMaxBytes;           /* Max bytes to write to volume */
   uint64_t VolCatCapacityBytes;      /* capacity estimate */
   btime_t VolReadTime;               /* time spent reading */
   btime_t VolWriteTime;              /* time spent writing this Volume */
   int64_t VolMediaId;                /* MediaId */
   utime_t VolFirstWritten;           /* Time of first write */
   utime_t VolLastWritten;            /* Time of last write */
   bool InChanger;                    /* Set if vol in current magazine */
   bool is_valid;                     /* set if this data is valid */
   char VolCatStatus[20];             /* Volume status */
   char VolCatName[MAX_NAME_LENGTH];  /* Desired volume to mount */
   char VolEncrKey[MAX_NAME_LENGTH];  /* Encryption Key needed to read the media */
   uint32_t VolMinBlocksize;          /* Volume Minimum Blocksize */
   uint32_t VolMaxBlocksize;          /* Volume Maximum Blocksize */
};

struct BLOCKSIZES {
   uint32_t max_block_size;
   uint32_t min_block_size;
};

class DEVRES; /* Device resource defined in stored_conf.h */
class DCR; /* Forward reference */
class VOLRES; /* Forward reference */

/*
 * Device structure definition.
 *
 * There is one of these for each physical device. Everything here is "global" to
 * that device and effects all jobs using the device.
 */
class DEVICE: public SMARTALLOC {
protected:
   int m_fd;                          /* File descriptor */
private:
   int m_blocked;                     /* Set if we must wait (i.e. change tape) */
   int m_count;                       /* Mutex use count -- DEBUG only */
   int m_num_reserved;                /* Counter of device reservations */
   int32_t m_slot;                    /* Slot loaded in drive or -1 if none */
   pthread_t m_pid;                   /* Thread that locked -- DEBUG only */
   bool m_unload;                     /* Set when Volume must be unloaded */
   bool m_load;                       /* Set when Volume must be loaded */

public:
   DEVICE();
   virtual ~DEVICE() {};
   DEVICE * volatile swap_dev;        /* Swap vol from this device */
   dlist *attached_dcrs;              /* Attached DCR list */
   bthread_mutex_t m_mutex;           /* Access control */
   bthread_mutex_t spool_mutex;       /* Mutex for updating spool_size */
   bthread_mutex_t acquire_mutex;     /* Mutex for acquire code */
   pthread_mutex_t read_acquire_mutex; /* Mutex for acquire read code */
   pthread_cond_t wait;               /* Thread wait variable */
   pthread_cond_t wait_next_vol;      /* Wait for tape to be mounted */
   pthread_t no_wait_id;              /* This thread must not wait */
   int dev_prev_blocked;              /* Previous blocked state */
   int num_waiting;                   /* Number of threads waiting */
   int num_writers;                   /* Number of writing threads */
   int capabilities;                  /* Capabilities mask */
   int state;                         /* State mask */
   int dev_errno;                     /* Our own errno */
   int oflags;                        /* Read/write flags */
   int open_mode;                     /* Parameter passed to open_dev (useful to reopen the device) */
   int dev_type;                      /* Device type */
   bool autoselect;                   /* Autoselect in autochanger */
   bool norewindonclose;              /* Don't rewind tape drive on close */
   bool initiated;                    /* Set when init_dev() called */
   int label_type;                    /* Bareos/ANSI/IBM label types */
   uint32_t drive_index;              /* Autochanger drive index (base 0) */
   POOLMEM *dev_name;                 /* Physical device name */
   POOLMEM *prt_name;                 /* Name used for display purposes */
   char *errmsg;                      /* Nicely edited error message */
   uint32_t block_num;                /* Current block number base 0 */
   uint32_t LastBlock;                /* Last DEV_BLOCK number written to Volume */
   uint32_t file;                     /* Current file number base 0 */
   uint64_t file_addr;                /* Current file read/write address */
   uint64_t file_size;                /* Current file size */
   uint32_t EndBlock;                 /* Last block written */
   uint32_t EndFile;                  /* Last file written */
   uint32_t min_block_size;           /* Min block size currently set */
   uint32_t max_block_size;           /* Max block size currently set */
   uint32_t max_concurrent_jobs;      /* Maximum simultaneous jobs this drive */
   uint64_t max_volume_size;          /* Max bytes to put on one volume */
   uint64_t max_file_size;            /* Max file size to put in one file on volume */
   uint64_t volume_capacity;          /* Advisory capacity */
   uint64_t max_spool_size;           /* Maximum spool file size */
   uint64_t spool_size;               /* Current spool size for this device */
   uint32_t max_rewind_wait;          /* Max secs to allow for rewind */
   uint32_t max_open_wait;            /* Max secs to allow for open */
   uint32_t max_open_vols;            /* Max simultaneous open volumes */

   utime_t vol_poll_interval;         /* Interval between polling Vol mount */
   DEVRES *device;                    /* Pointer to Device Resource */
   VOLRES *vol;                       /* Pointer to Volume reservation item */
   btimer_t *tid;                     /* Timer id */

   VOLUME_CAT_INFO VolCatInfo;        /* Volume Catalog Information */
   VOLUME_LABEL VolHdr;               /* Actual volume label */
   char pool_name[MAX_NAME_LENGTH];   /* Pool name */
   char pool_type[MAX_NAME_LENGTH];   /* Pool type */

   char UnloadVolName[MAX_NAME_LENGTH]; /* Last wrong Volume mounted */
   bool poll;                         /* Set to poll Volume */
   /* Device wait times ***FIXME*** look at durations */
   int min_wait;
   int max_wait;
   int max_num_wait;
   int wait_sec;
   int rem_wait_sec;
   int num_wait;

   btime_t last_timer;                /* Used by read/write/seek to get stats (usec) */
   btime_t last_tick;                 /* Contains last read/write time (usec) */

   btime_t  DevReadTime;
   btime_t  DevWriteTime;
   uint64_t DevWriteBytes;
   uint64_t DevReadBytes;

   /* Methods */
   btime_t get_timer_count();         /* Return the last timer interval (ms) */

   int has_cap(int cap) const { return capabilities & cap; }
   void clear_cap(int cap) { capabilities &= ~cap; }
   void set_cap(int cap) { capabilities |= cap; }
   bool do_checksum() const { return (capabilities & CAP_BLOCKCHECKSUM) != 0; }
   int is_autochanger() const { return capabilities & CAP_AUTOCHANGER; }
   int requires_mount() const { return capabilities & CAP_REQMOUNT; }
   int is_removable() const { return capabilities & CAP_REM; }
   int is_tape() const { return (dev_type == B_TAPE_DEV); }
   int is_file() const { return (dev_type == B_FILE_DEV ||
                                 dev_type == B_GFAPI_DEV ||
                                 dev_type == B_OBJECT_STORE_DEV ||
                                 dev_type == B_RADOS_DEV ||
                                 dev_type == B_CEPHFS_DEV); }
   int is_fifo() const { return dev_type == B_FIFO_DEV; }
   int is_vtl() const  { return dev_type == B_VTL_DEV; }
   int is_open() const { return m_fd >= 0; }
   int is_offline() const { return state & ST_OFFLINE; }
   int is_labeled() const { return state & ST_LABEL; }
   int is_mounted() const { return state & ST_MOUNTED; }
   int is_unmountable() const { return ((is_file() && is_removable())); }
   int num_reserved() const { return m_num_reserved; };
   int is_part_spooled() const { return state & ST_PART_SPOOLED; }
   int have_media() const { return state & ST_MEDIA; }
   int is_short_block() const { return state & ST_SHORT; }
   int is_busy() const { return (state & ST_READREADY) || num_writers || num_reserved(); }
   int at_eof() const { return state & ST_EOF; }
   int at_eot() const { return state & ST_EOT; }
   int at_weot() const { return state & ST_WEOT; }
   int can_append() const { return state & ST_APPENDREADY; }
   int is_crypto_enabled() const { return state & ST_CRYPTOKEY; }
   /*
    * can_write() is meant for checking at the end of a job to see
    * if we still have a tape (perhaps not if at end of tape
    * and the job is canceled).
    */
   int can_write() const { return is_open() && can_append() &&
                            is_labeled() && !at_weot(); }
   int can_read() const   { return state & ST_READREADY; }
   bool can_steal_lock() const { return m_blocked &&
                    (m_blocked == BST_UNMOUNTED ||
                     m_blocked == BST_WAITING_FOR_SYSOP ||
                     m_blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP); };
   bool waiting_for_mount() const { return
                    (m_blocked == BST_UNMOUNTED ||
                     m_blocked == BST_WAITING_FOR_SYSOP ||
                     m_blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP); };
   bool must_unload() const { return m_unload; };
   bool must_load() const { return m_load; };
   const char *strerror() const;
   const char *archive_name() const;
   const char *name() const;
   const char *print_name() const;    /* Name for display purposes */
   void set_eot() { state |= ST_EOT; };
   void set_eof() { state |= ST_EOF; };
   void set_append() { state |= ST_APPENDREADY; };
   void set_labeled() { state |= ST_LABEL; };
   inline void set_read() { state |= ST_READREADY; };
   void set_offline() { state |= ST_OFFLINE; };
   void set_mounted() { state |= ST_MOUNTED; };
   void set_media() { state |= ST_MEDIA; };
   void set_short_block() { state |= ST_SHORT; };
   void set_crypto_enabled() { state |= ST_CRYPTOKEY; };
   void set_part_spooled(int val) { if (val) state |= ST_PART_SPOOLED; \
          else state &= ~ST_PART_SPOOLED;
   };
   bool is_volume_to_unload() const { \
      return m_unload && strcmp(VolHdr.VolumeName, UnloadVolName) == 0; };
   void set_load() { m_load = true; };
   void inc_reserved() { m_num_reserved++; }
   void dec_reserved() { m_num_reserved--; ASSERT(m_num_reserved>=0); };
   void clear_append() { state &= ~ST_APPENDREADY; };
   void clear_read() { state &= ~ST_READREADY; };
   void clear_labeled() { state &= ~ST_LABEL; };
   void clear_offline() { state &= ~ST_OFFLINE; };
   void clear_eot() { state &= ~ST_EOT; };
   void clear_eof() { state &= ~ST_EOF; };
   void clear_opened() { m_fd = -1; };
   void clear_mounted() { state &= ~ST_MOUNTED; };
   void clear_media() { state &= ~ST_MEDIA; };
   void clear_short_block() { state &= ~ST_SHORT; };
   void clear_crypto_enabled() { state &= ~ST_CRYPTOKEY; };
   void clear_unload() { m_unload = false; UnloadVolName[0] = 0; };
   void clear_load() { m_load = false; };
   char *bstrerror(void) { return errmsg; };
   char *print_errmsg() { return errmsg; };
   int32_t get_slot() const { return m_slot; };
   void setVolCatInfo(bool valid) { VolCatInfo.is_valid = valid; };
   bool haveVolCatInfo() const { return VolCatInfo.is_valid; };
   void setVolCatName(const char *name) {
     bstrncpy(VolCatInfo.VolCatName, name, sizeof(VolCatInfo.VolCatName));
     setVolCatInfo(false);
   };
   char *getVolCatName() { return VolCatInfo.VolCatName; };

   const char *mode_to_str(int mode);
   void set_unload();
   void clear_volhdr();
   void close(DCR *dcr);
   bool open(DCR *dcr, int mode);
   void term();
   ssize_t read(void *buf, size_t len);
   ssize_t write(const void *buf, size_t len);
   bool mount(DCR *dcr, int timeout);
   bool unmount(DCR *dcr, int timeout);
   void edit_mount_codes(POOL_MEM &omsg, const char *imsg);
   bool offline_or_rewind();
   bool scan_dir_for_volume(DCR *dcr);
   void set_slot(int32_t slot);
   void clear_slot();

   void set_blocksizes(DCR* dcr);
   void set_label_blocksize(DCR* dcr);

   uint32_t get_file() const { return file; };
   uint32_t get_block_num() const { return block_num; };
   int fd() const { return m_fd; };

   /*
    * Tape specific operations.
    */
   virtual bool offline() { return true; };
   virtual bool weof(int num) { return true; };
   virtual bool fsf(int num) { return true; };
   virtual bool bsf(int num) { return false; };
   virtual bool fsr(int num) { return false; };
   virtual bool bsr(int num) { return false; };
   virtual bool load_dev() { return true; };
   virtual void lock_door() {};
   virtual void unlock_door() {};
   virtual void clrerror(int func) {};
   virtual void set_os_device_parameters(DCR *dcr) {};
   virtual int32_t get_os_tape_file() { return -1; };

   /*
    * Generic operations.
    */
   virtual void open_device(DCR *dcr, int omode);
   virtual uint32_t status_dev();
   virtual bool eod(DCR *dcr);
   virtual void set_ateof();
   virtual void set_ateot();
   virtual bool rewind(DCR *dcr);
   virtual bool update_pos(DCR *dcr);
   virtual bool reposition(DCR *dcr, uint32_t rfile, uint32_t rblock);
   virtual bool mount_backend(DCR *dcr, int timeout) { return true; };
   virtual bool unmount_backend(DCR *dcr, int timeout) { return true; };
   boffset_t lseek(DCR *dcr, boffset_t offset, int whence) { return d_lseek(dcr, offset, whence); };
   bool truncate(DCR *dcr) { return d_truncate(dcr); };

   /*
    * Low level operations
    */
   virtual int d_ioctl(int fd, ioctl_req_t request, char *mt_com = NULL) = 0;
   virtual int d_open(const char *pathname, int flags, int mode) = 0;
   virtual int d_close(int fd) = 0;
   virtual ssize_t d_read(int fd, void *buffer, size_t count) = 0;
   virtual ssize_t d_write(int fd, const void *buffer, size_t count) = 0;
   virtual boffset_t d_lseek(DCR *dcr, boffset_t offset, int whence) = 0;
   virtual bool d_truncate(DCR *dcr) = 0;

   /*
    * Locking and blocking calls
    */
#ifdef  SD_DEBUG_LOCK
   void dbg_rLock(const char *, int, bool locked = false);
   void dbg_rUnlock(const char *, int);
   void dbg_Lock(const char *, int);
   void dbg_Unlock(const char *, int);
   void dbg_Lock_acquire(const char *, int);
   void dbg_Unlock_acquire(const char *, int);
   void dbg_Lock_read_acquire(const char *, int);
   void dbg_Unlock_read_acquire(const char *, int);
#else
   void rLock(bool locked = false);
   void rUnlock();
   void Lock();
   void Unlock();
   void Lock_acquire();
   void Unlock_acquire();
   void Lock_read_acquire();
   void Unlock_read_acquire();
   void Lock_VolCatInfo();
   void Unlock_VolCatInfo();
#endif
   int init_mutex();
   int init_acquire_mutex();
   int init_read_acquire_mutex();
   int init_volcat_mutex();
   void set_mutex_priorities();
   int next_vol_timedwait(const struct timespec *timeout);
   void dblock(int why);
   void dunblock(bool locked = false);
   bool is_device_unmounted();
   void set_blocked(int block) { m_blocked = block; };
   int blocked() const { return m_blocked; };
   bool is_blocked() const { return m_blocked != BST_NOT_BLOCKED; };
   const char *print_blocked() const;

protected:
   void set_mode(int mode);
};

inline const char *DEVICE::strerror() const { return errmsg; }
inline const char *DEVICE::archive_name() const { return dev_name; }
inline const char *DEVICE::print_name() const { return prt_name; }

#define CHECK_BLOCK_NUMBERS true
#define NO_BLOCK_NUMBER_CHECK false

enum get_vol_info_rw {
   GET_VOL_INFO_FOR_WRITE,
   GET_VOL_INFO_FOR_READ
};

/*
 * Device Context (or Control) Record.
 *
 * There is one of these records for each Job that is using
 * the device. Items in this record are "local" to the Job and
 * do not affect other Jobs. Note, a job can have multiple
 * DCRs open, each pointing to a different device.
 *
 * Normally, there is only one JCR thread per DCR. However, the
 * big and important exception to this is when a Job is being
 * canceled. At that time, there may be two threads using the
 * same DCR. Consequently, when creating/attaching/detaching
 * and freeing the DCR we must lock it (m_mutex).
 */
class SD_IMP_EXP DCR : public SMARTALLOC {
private:
   bool m_dev_locked;                 /* Set if dev already locked */
   int m_dev_lock;                    /* Non-zero if rLock already called */
   bool m_reserved;                   /* Set if reserved device */
   bool m_found_in_use;               /* Set if a volume found in use */
   bool m_will_write;                 /* Set if DCR will be used for writing */

public:
   dlink dev_link;                    /* Link to attach to dev */
   JCR *jcr;                          /* Pointer to JCR */
   bthread_mutex_t m_mutex;           /* Access control */
   pthread_mutex_t r_mutex;           /* rLock pre-mutex */
   DEVICE * volatile dev;             /* Pointer to device */
   DEVRES *device;                    /* Pointer to device resource */
   DEV_BLOCK *block;                  /* Pointer to current block */
   DEV_RECORD *rec;                   /* Pointer to record being processed */
   DEV_RECORD *before_rec;            /* Pointer to record before translation */
   DEV_RECORD *after_rec;             /* Pointer to record after translation */
   pthread_t tid;                     /* Thread running this dcr */
   int spool_fd;                      /* Fd if spooling */
   bool spool_data;                   /* Set to spool data */
   bool spooling;                     /* Set when actually spooling */
   bool despooling;                   /* Set when despooling */
   bool despool_wait;                 /* Waiting for despooling */
   bool NewVol;                       /* Set if new Volume mounted */
   bool WroteVol;                     /* Set if Volume written */
   bool NewFile;                      /* Set when EOF written */
   bool reserved_volume;              /* Set if we reserved a volume */
   bool any_volume;                   /* Any OK for dir_find_next... */
   bool attached_to_dev;              /* Set when attached to dev */
   bool keep_dcr;                     /* Do not free dcr in release_dcr */
   uint32_t autodeflate;              /* Try to autodeflate streams */
   uint32_t autoinflate;              /* Try to autoinflate streams */
   uint32_t VolFirstIndex;            /* First file index this Volume */
   uint32_t VolLastIndex;             /* Last file index this Volume */
   uint32_t FileIndex;                /* Current File Index */
   uint32_t EndFile;                  /* End file written */
   uint32_t StartFile;                /* Start write file */
   uint32_t StartBlock;               /* Start write block */
   uint32_t EndBlock;                 /* Ending block written */
   int64_t  VolMediaId;               /* MediaId */
   int64_t job_spool_size;            /* Current job spool size */
   int64_t max_job_spool_size;        /* Max job spool size */
   uint32_t VolMinBlocksize;          /* Minimum Blocksize */
   uint32_t VolMaxBlocksize;          /* Maximum Blocksize */
   char VolumeName[MAX_NAME_LENGTH];  /* Volume name */
   char pool_name[MAX_NAME_LENGTH];   /* Pool name */
   char pool_type[MAX_NAME_LENGTH];   /* Pool type */
   char media_type[MAX_NAME_LENGTH];  /* Media type */
   char dev_name[MAX_NAME_LENGTH];    /* Dev name */
   int Copy;                          /* Identical copy number */
   int Stripe;                        /* RAIT stripe */
   VOLUME_CAT_INFO VolCatInfo;        /* Catalog info for desired volume */

   /*
    * Constructor/Destructor.
    */
   DCR();
   virtual ~DCR() {};

   /*
    * Methods
    */
   void set_dev(DEVICE *ndev) { dev = ndev; };
   void set_found_in_use() { m_found_in_use = true; };
   void set_will_write() { m_will_write = true; };
   void setVolCatInfo(bool valid) { VolCatInfo.is_valid = valid; };

   void clear_found_in_use() { m_found_in_use = false; };
   void clear_will_write() { m_will_write = false; };

   bool is_reserved() const { return m_reserved; };
   bool is_dev_locked() { return m_dev_locked; }
   bool is_writing() const { return m_will_write; };

   void inc_dev_lock() { m_dev_lock++; };
   void dec_dev_lock() { m_dev_lock--; };
   bool found_in_use() const { return m_found_in_use; };

   bool haveVolCatInfo() const { return VolCatInfo.is_valid; };
   void setVolCatName(const char *name) {
     bstrncpy(VolCatInfo.VolCatName, name, sizeof(VolCatInfo.VolCatName));
     setVolCatInfo(false);
   };
   char *getVolCatName() { return VolCatInfo.VolCatName; };

   /*
    * Methods in askdir.c
    */
   virtual DCR *get_new_spooling_dcr();
   virtual bool dir_find_next_appendable_volume() { return true; };
   virtual bool dir_update_volume_info(bool label, bool update_LastWritten) { return true; };
   virtual bool dir_create_jobmedia_record(bool zero) { return true; };
   virtual bool dir_update_file_attributes(DEV_RECORD *record) { return true; };
   virtual bool dir_ask_sysop_to_mount_volume(int mode);
   virtual bool dir_ask_sysop_to_create_appendable_volume() { return true; };
   virtual bool dir_get_volume_info(enum get_vol_info_rw writing);

   /*
    * Methods in lock.c
    */
   void dblock(int why) { dev->dblock(why); }
#ifdef  SD_DEBUG_LOCK
   void dbg_mLock(const char *, int, bool locked);
   void dbg_mUnlock(const char *, int);
#else
   void mLock(bool locked);
   void mUnlock();
#endif

   /*
    * Methods in record.c
    */
   bool write_record();

   /*
    * Methods in reserve.c
    */
   void clear_reserved();
   void set_reserved();
   void unreserve_device();

   /*
    * Methods in vol_mgr.c
    */
   bool can_i_use_volume();
   bool can_i_write_volume();

   /*
    * Methods in mount.c
    */
   bool mount_next_write_volume();
   bool mount_next_read_volume();
   void mark_volume_in_error();
   void mark_volume_not_inchanger();
   int try_autolabel(bool opened);
   bool find_a_volume();
   bool is_suitable_volume_mounted();
   bool is_eod_valid();
   int check_volume_label(bool &ask, bool &autochanger);
   void release_volume();
   void do_swapping(bool is_writing);
   bool do_unload();
   bool do_load(bool is_writing);
   bool is_tape_position_ok();

   /*
    * Methods in block.c
    */
   bool write_block_to_device();
   bool write_block_to_dev();
   bool read_block_from_device(bool check_block_numbers);
   bool read_block_from_dev(bool check_block_numbers);

   /*
    * Methods in label.c
    */
   bool rewrite_volume_label(bool recycle);
};

class SD_IMP_EXP SD_DCR : public DCR {
public:
   /*
    * Virtual Destructor.
    */
   ~SD_DCR() {};

   /*
    * Methods overriding default implementations.
    */
   bool dir_find_next_appendable_volume();
   bool dir_update_volume_info(bool label, bool update_LastWritten);
   bool dir_create_jobmedia_record(bool zero);
   bool dir_update_file_attributes(DEV_RECORD *record);
   bool dir_ask_sysop_to_mount_volume(int mode);
   bool dir_ask_sysop_to_create_appendable_volume();
   bool dir_get_volume_info(enum get_vol_info_rw writing);
   DCR *get_new_spooling_dcr();
};

class BTAPE_DCR : public DCR {
public:
   /*
    * Virtual Destructor.
    */
   ~BTAPE_DCR() {};

   /*
    * Methods overriding default implementations.
    */
   bool dir_find_next_appendable_volume();
   bool dir_create_jobmedia_record(bool zero);
   bool dir_ask_sysop_to_mount_volume(int mode);
   bool dir_ask_sysop_to_create_appendable_volume();
   DCR *get_new_spooling_dcr();
};

/*
 * Get some definition of function to position to the end of the medium in MTEOM. System dependent.
 */
#ifndef MTEOM
#ifdef MTSEOD
#define MTEOM MTSEOD
#endif
#ifdef MTEOD
#undef MTEOM
#define MTEOM MTEOD
#endif
#endif
#endif
