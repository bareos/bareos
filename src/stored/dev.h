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
 * Definitions for using the Device functions in Bacula
 *  Tape and File storage access
 *
 * Kern Sibbald, MM
 *
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
 *
 */

#ifndef __DEV_H
#define __DEV_H 1

#undef DCR                            /* used by Bacula */

/* Return values from wait_for_sysop() */
enum {
   W_ERROR = 1,
   W_TIMEOUT,
   W_POLL,
   W_MOUNT,
   W_WAKE
};

/* Arguments to open_dev() */
enum {
   CREATE_READ_WRITE = 1,
   OPEN_READ_WRITE,
   OPEN_READ_ONLY,
   OPEN_WRITE_ONLY
};

/* Device types */
enum {
   B_FILE_DEV = 1,
   B_TAPE_DEV,
   B_DVD_DEV,
   B_FIFO_DEV,
   B_VTAPE_DEV,                       /* change to B_TAPE_DEV after init */
   B_FTP_DEV,
   B_VTL_DEV
};

/* Generic status bits returned from status_dev() */
#define BMT_TAPE           (1<<0)     /* is tape device */
#define BMT_EOF            (1<<1)     /* just read EOF */
#define BMT_BOT            (1<<2)     /* at beginning of tape */
#define BMT_EOT            (1<<3)     /* end of tape reached */
#define BMT_SM             (1<<4)     /* DDS setmark */
#define BMT_EOD            (1<<5)     /* DDS at end of data */
#define BMT_WR_PROT        (1<<6)     /* tape write protected */
#define BMT_ONLINE         (1<<7)     /* tape online */
#define BMT_DR_OPEN        (1<<8)     /* tape door open */
#define BMT_IM_REP_EN      (1<<9)     /* immediate report enabled */


/* Bits for device capabilities */
#define CAP_EOF            (1<<0)     /* has MTWEOF */
#define CAP_BSR            (1<<1)     /* has MTBSR */
#define CAP_BSF            (1<<2)     /* has MTBSF */
#define CAP_FSR            (1<<3)     /* has MTFSR */
#define CAP_FSF            (1<<4)     /* has MTFSF */
#define CAP_EOM            (1<<5)     /* has MTEOM */
#define CAP_REM            (1<<6)     /* is removable media */
#define CAP_RACCESS        (1<<7)     /* is random access device */
#define CAP_AUTOMOUNT      (1<<8)     /* Read device at start to see what is there */
#define CAP_LABEL          (1<<9)     /* Label blank tapes */
#define CAP_ANONVOLS       (1<<10)    /* Mount without knowing volume name */
#define CAP_ALWAYSOPEN     (1<<11)    /* always keep device open */
#define CAP_AUTOCHANGER    (1<<12)    /* AutoChanger */
#define CAP_OFFLINEUNMOUNT (1<<13)    /* Offline before unmount */
#define CAP_STREAM         (1<<14)    /* Stream device */
#define CAP_BSFATEOM       (1<<15)    /* Backspace file at EOM */
#define CAP_FASTFSF        (1<<16)    /* Fast forward space file */
#define CAP_TWOEOF         (1<<17)    /* Write two eofs for EOM */
#define CAP_CLOSEONPOLL    (1<<18)    /* Close device on polling */
#define CAP_POSITIONBLOCKS (1<<19)    /* Use block positioning */
#define CAP_MTIOCGET       (1<<20)    /* Basic support for fileno and blkno */
#define CAP_REQMOUNT       (1<<21)    /* Require mount/unmount */
#define CAP_CHECKLABELS    (1<<22)    /* Check for ANSI/IBM labels */
#define CAP_BLOCKCHECKSUM  (1<<23)    /* Create/test block checksum */

/* Test state */
#define dev_state(dev, st_state) ((dev)->state & (st_state))

/* Device state bits */
#define ST_XXXXXX          (1<<0)     /* was ST_OPENED */
#define ST_XXXXX           (1<<1)     /* was ST_TAPE */
#define ST_XXXX            (1<<2)     /* was ST_FILE */
#define ST_XXX             (1<<3)     /* was ST_FIFO */
#define ST_XX              (1<<4)     /* was ST_DVD */
#define ST_X               (1<<5)     /* was ST_PROG */

#define ST_LABEL           (1<<6)     /* label found */
#define ST_MALLOC          (1<<7)     /* dev packet malloc'ed in init_dev() */
#define ST_APPEND          (1<<8)     /* ready for Bacula append */
#define ST_READ            (1<<9)     /* ready for Bacula read */
#define ST_EOT             (1<<10)    /* at end of tape */
#define ST_WEOT            (1<<11)    /* Got EOT on write */
#define ST_EOF             (1<<12)    /* Read EOF i.e. zero bytes */
#define ST_NEXTVOL         (1<<13)    /* Start writing on next volume */
#define ST_SHORT           (1<<14)    /* Short block read */
#define ST_MOUNTED         (1<<15)    /* the device is mounted to the mount point */
#define ST_MEDIA           (1<<16)    /* Media found in mounted device */
#define ST_OFFLINE         (1<<17)    /* set offline by operator */
#define ST_PART_SPOOLED    (1<<18)    /* spooling part */
#define ST_FREESPACE_OK    (1<<19)    /* Have valid freespace for DVD */


/* Volume Catalog Information structure definition */
struct VOLUME_CAT_INFO {
   /* Media info for the current Volume */
   uint32_t VolCatJobs;               /* number of jobs on this Volume */
   uint32_t VolCatFiles;              /* Number of files */
   uint32_t VolCatBlocks;             /* Number of blocks */
   uint64_t VolCatBytes;              /* Number of bytes written */
   uint32_t VolCatParts;              /* Number of parts written */
   uint32_t VolCatMounts;             /* Number of mounts this volume */
   uint32_t VolCatErrors;             /* Number of errors this volume */
   uint32_t VolCatWrites;             /* Number of writes this volume */
   uint32_t VolCatReads;              /* Number of reads this volume */
   uint64_t VolCatRBytes;             /* Number of bytes read */
   uint32_t VolCatRecycles;           /* Number of recycles this volume */
   uint32_t EndFile;                  /* Last file number */
   uint32_t EndBlock;                 /* Last block number */
   int32_t  LabelType;                /* Bacula/ANSI/IBM */
   int32_t  Slot;                     /* >0=Slot loaded, 0=nothing, -1=unknown */
   uint32_t VolCatMaxJobs;            /* Maximum Jobs to write to volume */
   uint32_t VolCatMaxFiles;           /* Maximum files to write to volume */
   uint64_t VolCatMaxBytes;           /* Max bytes to write to volume */
   uint64_t VolCatCapacityBytes;      /* capacity estimate */
   btime_t  VolReadTime;              /* time spent reading */
   btime_t  VolWriteTime;             /* time spent writing this Volume */
   int64_t  VolMediaId;               /* MediaId */
   utime_t  VolFirstWritten;          /* Time of first write */
   utime_t  VolLastWritten;           /* Time of last write */
   bool     InChanger;                /* Set if vol in current magazine */
   bool     is_valid;                 /* set if this data is valid */
   char VolCatStatus[20];             /* Volume status */
   char VolCatName[MAX_NAME_LENGTH];  /* Desired volume to mount */
};

class DEVRES;                        /* Device resource defined in stored_conf.h */
class DCR; /* forward reference */
class VOLRES; /* forward reference */

/*
 * Device structure definition. There is one of these for
 *  each physical device. Everything here is "global" to
 *  that device and effects all jobs using the device.
 */
class DEVICE: public SMARTALLOC {
protected:
   int m_fd;                          /* file descriptor */
private:
   int m_blocked;                     /* set if we must wait (i.e. change tape) */
   int m_count;                       /* Mutex use count -- DEBUG only */
   int m_num_reserved;                /* counter of device reservations */
   int32_t m_slot;                    /* slot loaded in drive or -1 if none */ 
   pthread_t m_pid;                   /* Thread that locked -- DEBUG only */
   bool m_unload;                     /* set when Volume must be unloaded */
   bool m_load;                       /* set when Volume must be loaded */

public:
   DEVICE() {};
   virtual ~DEVICE() {};
   DEVICE * volatile swap_dev;        /* Swap vol from this device */
   dlist *attached_dcrs;              /* attached DCR list */
   bthread_mutex_t m_mutex;           /* access control */
   bthread_mutex_t spool_mutex;       /* mutex for updating spool_size */
   bthread_mutex_t acquire_mutex;     /* mutex for acquire code */
   pthread_mutex_t read_acquire_mutex; /* mutex for acquire read code */
   pthread_cond_t wait;               /* thread wait variable */
   pthread_cond_t wait_next_vol;      /* wait for tape to be mounted */
   pthread_t no_wait_id;              /* this thread must not wait */
   int dev_prev_blocked;              /* previous blocked state */
   int num_waiting;                   /* number of threads waiting */
   int num_writers;                   /* number of writing threads */
   int capabilities;                  /* capabilities mask */
   int state;                         /* state mask */
   int dev_errno;                     /* Our own errno */
   int mode;                          /* read/write modes */
   int openmode;                      /* parameter passed to open_dev (useful to reopen the device) */
   int dev_type;                      /* device type */
   bool autoselect;                   /* Autoselect in autochanger */
   bool initiated;                    /* set when init_dev() called */
   int label_type;                    /* Bacula/ANSI/IBM label types */
   uint32_t drive_index;              /* Autochanger drive index (base 0) */
   POOLMEM *dev_name;                 /* Physical device name */
   POOLMEM *prt_name;                 /* Name used for display purposes */
   char *errmsg;                      /* nicely edited error message */
   uint32_t block_num;                /* current block number base 0 */
   uint32_t LastBlock;                /* last DEV_BLOCK number written to Volume */
   uint32_t file;                     /* current file number base 0 */
   uint64_t file_addr;                /* Current file read/write address */
   uint64_t file_size;                /* Current file size */
   uint32_t EndBlock;                 /* last block written */
   uint32_t EndFile;                  /* last file written */
   uint32_t min_block_size;           /* min block size */
   uint32_t max_block_size;           /* max block size */
   uint32_t max_concurrent_jobs;      /* maximum simultaneous jobs this drive */
   uint64_t max_volume_size;          /* max bytes to put on one volume */
   uint64_t max_file_size;            /* max file size to put in one file on volume */
   uint64_t volume_capacity;          /* advisory capacity */
   uint64_t max_spool_size;           /* maximum spool file size */
   uint64_t spool_size;               /* current spool size for this device */
   uint32_t max_rewind_wait;          /* max secs to allow for rewind */
   uint32_t max_open_wait;            /* max secs to allow for open */
   uint32_t max_open_vols;            /* max simultaneous open volumes */
   
   uint64_t max_part_size;            /* max part size */
   uint64_t part_size;                /* current part size */
   uint32_t part;                     /* current part number (starts at 0) */
   uint64_t part_start;               /* current part start address (relative to the whole volume) */
   uint32_t num_dvd_parts;            /* number of parts WRITTEN on the DVD */
   /* state ST_FREESPACE_OK is set if free_space is valid */
   uint64_t free_space;               /* current free space on medium (without the current part) */
   int free_space_errno;              /* indicates errno getting freespace */
   bool truncating;                   /* if set, we are currently truncating the DVD */
   bool blank_dvd;                    /* if set, we have a blank DVD in the drive */
   
   
   utime_t  vol_poll_interval;        /* interval between polling Vol mount */
   DEVRES *device;                    /* pointer to Device Resource */
   VOLRES *vol;                       /* Pointer to Volume reservation item */
   btimer_t *tid;                     /* timer id */

   VOLUME_CAT_INFO VolCatInfo;        /* Volume Catalog Information */
   VOLUME_LABEL VolHdr;               /* Actual volume label */
   char pool_name[MAX_NAME_LENGTH];   /* pool name */
   char pool_type[MAX_NAME_LENGTH];   /* pool type */

   char UnloadVolName[MAX_NAME_LENGTH];  /* Last wrong Volume mounted */
   bool poll;                         /* set to poll Volume */
   /* Device wait times ***FIXME*** look at durations */
   int min_wait;
   int max_wait;
   int max_num_wait;
   int wait_sec;
   int rem_wait_sec;
   int num_wait;

   btime_t last_timer;        /* used by read/write/seek to get stats (usec) */
   btime_t last_tick;         /* contains last read/write time (usec) */

   btime_t  DevReadTime;
   btime_t  DevWriteTime;
   uint64_t DevWriteBytes;
   uint64_t DevReadBytes;

   /* Methods */
   btime_t get_timer_count();         /* return the last timer interval (ms) */

   int has_cap(int cap) const { return capabilities & cap; }
   void clear_cap(int cap) { capabilities &= ~cap; }
   void set_cap(int cap) { capabilities |= cap; }
   bool do_checksum() const { return (capabilities & CAP_BLOCKCHECKSUM) != 0; }
   int is_autochanger() const { return capabilities & CAP_AUTOCHANGER; }
   int requires_mount() const { return capabilities & CAP_REQMOUNT; }
   int is_removable() const { return capabilities & CAP_REM; }
   int is_tape() const { return (dev_type == B_TAPE_DEV || 
                                 dev_type == B_VTAPE_DEV); }
   int is_ftp() const { return dev_type == B_FTP_DEV; }
   int is_file() const { return (dev_type == B_FILE_DEV); }
   int is_fifo() const { return dev_type == B_FIFO_DEV; }
   int is_dvd() const  { return dev_type == B_DVD_DEV; }
   int is_vtl() const  { return dev_type == B_VTL_DEV; }
   int is_vtape() const  { return dev_type == B_VTAPE_DEV; }
   int is_open() const { return m_fd >= 0; }
   int is_offline() const { return state & ST_OFFLINE; }
   int is_labeled() const { return state & ST_LABEL; }
   int is_mounted() const { return state & ST_MOUNTED; }
   int is_unmountable() const { return (is_dvd() || (is_file() && is_removable())); }
   int num_reserved() const { return m_num_reserved; };
   int is_part_spooled() const { return state & ST_PART_SPOOLED; }
   int have_media() const { return state & ST_MEDIA; }
   int is_short_block() const { return state & ST_SHORT; }
   int is_busy() const { return (state & ST_READ) || num_writers || num_reserved(); }
   int at_eof() const { return state & ST_EOF; }
   int at_eot() const { return state & ST_EOT; }
   int at_weot() const { return state & ST_WEOT; }
   int can_append() const { return state & ST_APPEND; }
   int is_freespace_ok() const { return state & ST_FREESPACE_OK; }
   /*
    * can_write() is meant for checking at the end of a job to see
    * if we still have a tape (perhaps not if at end of tape
    * and the job is canceled).
    */
   int can_write() const { return is_open() && can_append() &&
                            is_labeled() && !at_weot(); }
   int can_read() const   { return state & ST_READ; }
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
   void set_ateof(); /* in dev.c */
   void set_ateot(); /* in dev.c */
   void set_eot() { state |= ST_EOT; };
   void set_eof() { state |= ST_EOF; };
   void set_append() { state |= ST_APPEND; };
   void set_labeled() { state |= ST_LABEL; };
   inline void set_read() { state |= ST_READ; };
   void set_offline() { state |= ST_OFFLINE; };
   void set_mounted() { state |= ST_MOUNTED; };
   void set_media() { state |= ST_MEDIA; };
   void set_short_block() { state |= ST_SHORT; };
   void set_freespace_ok() { state |= ST_FREESPACE_OK; }
   void set_part_spooled(int val) { if (val) state |= ST_PART_SPOOLED; \
          else state &= ~ST_PART_SPOOLED;
   };
   bool is_volume_to_unload() const { \
      return m_unload && strcmp(VolHdr.VolumeName, UnloadVolName) == 0; };
   void set_load() { m_load = true; };
   void inc_reserved() { m_num_reserved++; }
   void set_mounted(int val) { if (val) state |= ST_MOUNTED; \
          else state &= ~ST_MOUNTED; };
   void dec_reserved() { m_num_reserved--; ASSERT(m_num_reserved>=0); };
   void clear_append() { state &= ~ST_APPEND; };
   void clear_read() { state &= ~ST_READ; };
   void clear_labeled() { state &= ~ST_LABEL; };
   void clear_offline() { state &= ~ST_OFFLINE; };
   void clear_eot() { state &= ~ST_EOT; };
   void clear_eof() { state &= ~ST_EOF; };
   void clear_opened() { m_fd = -1; };
   void clear_mounted() { state &= ~ST_MOUNTED; };
   void clear_media() { state &= ~ST_MEDIA; };
   void clear_short_block() { state &= ~ST_SHORT; };
   void clear_freespace_ok() { state &= ~ST_FREESPACE_OK; };
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

   void set_unload();            /* in dev.c */
   void clear_volhdr();          /* in dev.c */
   void close();                 /* in dev.c */
   void close_part(DCR *dcr);    /* in dev.c */
   bool open(DCR *dcr, int mode); /* in dev.c */
   void term(void);              /* in dev.c */
   ssize_t read(void *buf, size_t len); /* in dev.c */
   ssize_t write(const void *buf, size_t len);  /* in dev.c */
   bool mount(int timeout);      /* in dev.c */
   bool unmount(int timeout);    /* in dev.c */
   void edit_mount_codes(POOL_MEM &omsg, const char *imsg); /* in dev.c */
   bool offline_or_rewind();     /* in dev.c */
   bool offline();               /* in dev.c */
   bool bsf(int count);          /* in dev.c */
   bool eod(DCR *dcr);           /* in dev.c */
   bool fsr(int num);            /* in dev.c */
   bool fsf(int num);            /* in dev.c */
   bool bsr(int num);            /* in dev.c */
   bool weof(int num);           /* in dev.c */
   void lock_door();             /* in dev.c */
   void unlock_door();           /* in dev.c */
   int32_t get_os_tape_file();   /* in dev.c */
   bool scan_dir_for_volume(DCR *dcr); /* in scan.c */
   bool reposition(DCR *dcr, uint32_t rfile, uint32_t rblock); /* in dev.c */
   void clrerror(int func);      /* in dev.c */
   void set_slot(int32_t slot);  /* in dev.c */
   void clear_slot();            /* in dev.c */


   bool update_freespace();      /* in dvd.c */

   uint32_t get_file() const { return file; };
   uint32_t get_block_num() const { return block_num; };
   int fd() const { return m_fd; };

   /* low level operations */
   virtual int d_ioctl(int fd, ioctl_req_t request, char *mt_com=NULL);
   virtual int d_open(const char *pathname, int flags);
   virtual int d_close(int fd);
   virtual ssize_t d_read(int fd, void *buffer, size_t count);
   virtual ssize_t d_write(int fd, const void *buffer, size_t count);
   virtual boffset_t lseek(DCR *dcr, boffset_t offset, int whence);
   virtual bool update_pos(DCR *dcr);
   virtual bool rewind(DCR *dcr);
   virtual bool truncate(DCR *dcr);
   virtual void open_device(DCR *dcr, int omode);
   /* 
    * Locking and blocking calls
    */
#ifdef  SD_DEBUG_LOCK
   void dbg_rLock(const char *, int, bool locked=false);    /* in lock.c */
   void dbg_rUnlock(const char *, int);                     /* in lock.c */
   void dbg_Lock(const char *, int);                        /* in lock.c */
   void dbg_Unlock(const char *, int);                      /* in lock.c */
   void dbg_Lock_acquire(const char *, int);                /* in lock.c */
   void dbg_Unlock_acquire(const char *, int);              /* in lock.c */
   void dbg_Lock_read_acquire(const char *, int);           /* in lock.c */
   void dbg_Unlock_read_acquire(const char *, int);         /* in lock.c */
#else
   void rLock(bool locked=false);         /* in lock.c */
   void rUnlock();                        /* in lock.c */
   void Lock();                           /* in lock.c */
   void Unlock();                         /* in lock.c */
   void Lock_acquire();                   /* in lock.c */
   void Unlock_acquire();                 /* in lock.c */
   void Lock_read_acquire();              /* in lock.c */
   void Unlock_read_acquire();            /* in lock.c */
   void Lock_VolCatInfo();                /* in lock.c */
   void Unlock_VolCatInfo();              /* in lock.c */
#endif
   int init_mutex();                      /* in lock.c */
   int init_acquire_mutex();              /* in lock.c */
   int init_read_acquire_mutex();         /* in lock.c */
   int init_volcat_mutex();               /* in lock.c */
   void set_mutex_priorities();           /* in lock.c */
   int next_vol_timedwait(const struct timespec *timeout);  /* in lock.c */
   void dblock(int why);                  /* in lock.c */
   void dunblock(bool locked=false);      /* in lock.c */
   bool is_device_unmounted();            /* in lock.c */
   void set_blocked(int block) { m_blocked = block; };
   int blocked() const { return m_blocked; };
   bool is_blocked() const { return m_blocked != BST_NOT_BLOCKED; };
   const char *print_blocked() const;     /* in dev.c */

private:
   bool do_tape_mount(int mount, int dotimeout);  /* in dev.c */
   bool do_file_mount(int mount, int dotimeout);  /* in dev.c */
   void set_mode(int omode);                      /* in dev.c */
   void open_tape_device(DCR *dcr, int omode);    /* in dev.c */
   void open_file_device(DCR *dcr, int omode);    /* in dev.c */
};

inline const char *DEVICE::strerror() const { return errmsg; }
inline const char *DEVICE::archive_name() const { return dev_name; }
inline const char *DEVICE::print_name() const { return prt_name; }


#define CHECK_BLOCK_NUMBERS    true
#define NO_BLOCK_NUMBER_CHECK  false

/*
 * Device Context (or Control) Record.
 *  There is one of these records for each Job that is using
 *  the device. Items in this record are "local" to the Job and
 *  do not affect other Jobs. Note, a job can have multiple
 *  DCRs open, each pointing to a different device. 
 * Normally, there is only one JCR thread per DCR. However, the
 *  big and important exception to this is when a Job is being
 *  canceled. At that time, there may be two threads using the 
 *  same DCR. Consequently, when creating/attaching/detaching
 *  and freeing the DCR we must lock it (m_mutex).
 */
class DCR {
private:
   bool m_dev_locked;                 /* set if dev already locked */
   int m_dev_lock;                    /* non-zero if rLock already called */
   bool m_reserved;                   /* set if reserved device */
   bool m_found_in_use;               /* set if a volume found in use */

public:
   dlink dev_link;                    /* link to attach to dev */
   JCR *jcr;                          /* pointer to JCR */
   bthread_mutex_t m_mutex;           /* access control */
   pthread_mutex_t r_mutex;           /* rLock pre-mutex */
   DEVICE * volatile dev;             /* pointer to device */
   DEVRES *device;                    /* pointer to device resource */
   DEV_BLOCK *block;                  /* pointer to block */
   DEV_RECORD *rec;                   /* pointer to record */
   pthread_t tid;                     /* Thread running this dcr */
   int spool_fd;                      /* fd if spooling */
   bool spool_data;                   /* set to spool data */
   bool spooling;                     /* set when actually spooling */
   bool despooling;                   /* set when despooling */
   bool despool_wait;                 /* waiting for despooling */
   bool NewVol;                       /* set if new Volume mounted */
   bool WroteVol;                     /* set if Volume written */
   bool NewFile;                      /* set when EOF written */
   bool reserved_volume;              /* set if we reserved a volume */
   bool any_volume;                   /* Any OK for dir_find_next... */
   bool attached_to_dev;              /* set when attached to dev */
   bool keep_dcr;                     /* do not free dcr in release_dcr */
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
   char VolumeName[MAX_NAME_LENGTH];  /* Volume name */
   char pool_name[MAX_NAME_LENGTH];   /* pool name */
   char pool_type[MAX_NAME_LENGTH];   /* pool type */
   char media_type[MAX_NAME_LENGTH];  /* media type */
   char dev_name[MAX_NAME_LENGTH];    /* dev name */
   int Copy;                          /* identical copy number */
   int Stripe;                        /* RAIT stripe */
   VOLUME_CAT_INFO VolCatInfo;        /* Catalog info for desired volume */

   /* Methods */
   void set_dev(DEVICE *ndev) { dev = ndev; }; 
   void inc_dev_lock() { m_dev_lock++; };
   void dec_dev_lock() { m_dev_lock--; };
   bool found_in_use() const { return m_found_in_use; };
   void set_found_in_use() { m_found_in_use = true; };
   void clear_found_in_use() { m_found_in_use = false; };
   bool is_reserved() const { return m_reserved; };
   bool is_dev_locked() { return m_dev_locked; }
   void setVolCatInfo(bool valid) { VolCatInfo.is_valid = valid; };
   bool haveVolCatInfo() const { return VolCatInfo.is_valid; };
   void setVolCatName(const char *name) {
     bstrncpy(VolCatInfo.VolCatName, name, sizeof(VolCatInfo.VolCatName));
     setVolCatInfo(false);
   };
   char *getVolCatName() { return VolCatInfo.VolCatName; };

   /* Methods in lock.c */
   void dblock(int why) { dev->dblock(why); }
#ifdef  SD_DEBUG_LOCK
   void dbg_mLock(const char *, int, bool locked);    /* in lock.c */
   void dbg_mUnlock(const char *, int);               /* in lock.c */
#else
   void mLock(bool locked);
   void mUnlock();
#endif

   /* Methods in record.c */
   bool write_record(DEV_RECORD *rec);

   /* Methods in reserve.c */
   void clear_reserved();
   void set_reserved();
   void unreserve_device();

   /* Methods in vol_mgr.c */
   bool can_i_use_volume();
   bool can_i_write_volume();

   /* Methods in mount.c */
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

   /* Methods in block.c */
   bool write_block_to_device();
   bool write_block_to_dev();
   bool read_block_from_device(bool check_block_numbers);
   bool read_block_from_dev(bool check_block_numbers);

   /* Methods in label.c */
   bool rewrite_volume_label(bool recycle);
   
};

/* Get some definition of function to position
 *  to the end of the medium in MTEOM. System
 *  dependent. Arrgggg!
 */
#ifndef MTEOM
#ifdef  MTSEOD
#define MTEOM MTSEOD
#endif
#ifdef MTEOD
#undef MTEOM
#define MTEOM MTEOD
#endif
#endif

#endif
