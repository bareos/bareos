/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2017 Bareos GmbH & Co. KG

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
 * Definitions for using the Device functions in Bareos Tape and File storage access
 */

/**
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

/**
 * Return values from wait_for_sysop()
 */
enum {
   W_ERROR = 1,
   W_TIMEOUT,
   W_POLL,
   W_MOUNT,
   W_WAKE
};

/**
 * Arguments to open_dev()
 */
enum {
   CREATE_READ_WRITE = 1,
   OPEN_READ_WRITE,
   OPEN_READ_ONLY,
   OPEN_WRITE_ONLY
};

/**
 * Device types
 */
enum {
   B_FILE_DEV = 1,
   B_TAPE_DEV,
   B_FIFO_DEV,
   B_VTL_DEV,
   B_GFAPI_DEV,
   B_DROPLET_DEV,
   B_RADOS_DEV,
   B_CEPHFS_DEV,
   B_ELASTO_DEV
};

/**
 * IO directions
 */
enum {
   IO_DIRECTION_NONE = 0,
   IO_DIRECTION_IN,
   IO_DIRECTION_OUT,
   IO_DIRECTION_INOUT
};

/**
 * Generic status bits returned from status_dev()
 */
enum {
   BMT_TAPE = 0,                      /**< Is tape device */
   BMT_EOF = 1,                       /**< Just read EOF */
   BMT_BOT = 2,                       /**< At beginning of tape */
   BMT_EOT = 3,                       /**< End of tape reached */
   BMT_SM = 4,                        /**< DDS setmark */
   BMT_EOD = 5,                       /**< DDS at end of data */
   BMT_WR_PROT = 6,                   /**< Tape write protected */
   BMT_ONLINE = 7,                    /**< Tape online */
   BMT_DR_OPEN = 8,                   /**< Tape door open */
   BMT_IM_REP_EN = 9                  /**< Immediate report enabled */
};

/**
 * Keep this set to the last entry in the enum.
 */
#define BMT_MAX BMT_IM_REP_EN

/**
 * Make sure you have enough bits to store all above bit fields.
 */
#define BMT_BYTES nbytes_for_bits(BMT_MAX + 1)

/**
 * Bits for device capabilities
 */
enum {
   CAP_EOF = 0,                       /**< Has MTWEOF */
   CAP_BSR = 1,                       /**< Has MTBSR */
   CAP_BSF = 2,                       /**< Has MTBSF */
   CAP_FSR = 3,                       /**< Has MTFSR */
   CAP_FSF = 4,                       /**< Has MTFSF */
   CAP_EOM = 5,                       /**< Has MTEOM */
   CAP_REM = 6,                       /**< Is removable media */
   CAP_RACCESS = 7,                   /**< Is random access device */
   CAP_AUTOMOUNT = 8,                 /**< Read device at start to see what is there */
   CAP_LABEL = 9,                     /**< Label blank tapes */
   CAP_ANONVOLS = 10,                 /**< Mount without knowing volume name */
   CAP_ALWAYSOPEN = 11,               /**< Always keep device open */
   CAP_AUTOCHANGER = 12,              /**< AutoChanger */
   CAP_OFFLINEUNMOUNT = 13,           /**< Offline before unmount */
   CAP_STREAM = 14,                   /**< Stream device */
   CAP_BSFATEOM = 15,                 /**< Backspace file at EOM */
   CAP_FASTFSF = 16,                  /**< Fast forward space file */
   CAP_TWOEOF = 17,                   /**< Write two eofs for EOM */
   CAP_CLOSEONPOLL = 18,              /**< Close device on polling */
   CAP_POSITIONBLOCKS = 19,           /**< Use block positioning */
   CAP_MTIOCGET = 20,                 /**< Basic support for fileno and blkno */
   CAP_REQMOUNT = 21,                 /**< Require mount/unmount */
   CAP_CHECKLABELS = 22,              /**< Check for ANSI/IBM labels */
   CAP_BLOCKCHECKSUM = 23,            /**< Create/test block checksum */
   CAP_IOERRATEOM = 24,               /**< IOError at EOM */
   CAP_IBMLINTAPE = 25,               /**< Using IBM lin_tape driver */
   CAP_ADJWRITESIZE = 26              /**< Adjust write size to min/max */
};

/**
 * Keep this set to the last entry in the enum.
 */
#define CAP_MAX CAP_ADJWRITESIZE

/**
 * Make sure you have enough bits to store all above bit fields.
 */
#define CAP_BYTES nbytes_for_bits(CAP_MAX + 1)

/**
 * Device state bits
 */
enum {
   ST_LABEL = 0,                      /**< Label found */
   ST_MALLOC = 1,                     /**< Dev packet malloc'ed in init_dev() */
   ST_APPENDREADY = 2,                /**< Ready for Bareos append */
   ST_READREADY = 3,                  /**< Ready for Bareos read */
   ST_EOT = 4,                        /**< At end of tape */
   ST_WEOT = 5,                       /**< Got EOT on write */
   ST_EOF = 6,                        /**< Read EOF i.e. zero bytes */
   ST_NEXTVOL = 7,                    /**< Start writing on next volume */
   ST_SHORT = 8,                      /**< Short block read */
   ST_MOUNTED = 9,                    /**< The device is mounted to the mount point */
   ST_MEDIA = 10,                     /**< Media found in mounted device */
   ST_OFFLINE = 11,                   /**< Set offline by operator */
   ST_PART_SPOOLED = 12,              /**< Spooling part */
   ST_CRYPTOKEY = 13                  /**< The device has a crypto key loaded */
};

/**
 * Keep this set to the last entry in the enum.
 */
#define ST_MAX ST_CRYPTOKEY

/**
 * Make sure you have enough bits to store all above bit fields.
 */
#define ST_BYTES nbytes_for_bits(ST_MAX + 1)

/**
 * Volume Catalog Information structure definition
 */
struct VOLUME_CAT_INFO {
   /*
    * Media info for the current Volume
    */
   uint32_t VolCatJobs;               /**< number of jobs on this Volume */
   uint32_t VolCatFiles;              /**< Number of files */
   uint32_t VolCatBlocks;             /**< Number of blocks */
   uint64_t VolCatBytes;              /**< Number of bytes written */
   uint32_t VolCatMounts;             /**< Number of mounts this volume */
   uint32_t VolCatErrors;             /**< Number of errors this volume */
   uint32_t VolCatWrites;             /**< Number of writes this volume */
   uint32_t VolCatReads;              /**< Number of reads this volume */
   uint64_t VolCatRBytes;             /**< Number of bytes read */
   uint32_t VolCatRecycles;           /**< Number of recycles this volume */
   uint32_t EndFile;                  /**< Last file number */
   uint32_t EndBlock;                 /**< Last block number */
   int32_t LabelType;                 /**< Bareos/ANSI/IBM */
   int32_t Slot;                      /**< >0=Slot loaded, 0=nothing, -1=unknown */
   uint32_t VolCatMaxJobs;            /**< Maximum Jobs to write to volume */
   uint32_t VolCatMaxFiles;           /**< Maximum files to write to volume */
   uint64_t VolCatMaxBytes;           /**< Max bytes to write to volume */
   uint64_t VolCatCapacityBytes;      /**< capacity estimate */
   btime_t VolReadTime;               /**< time spent reading */
   btime_t VolWriteTime;              /**< time spent writing this Volume */
   int64_t VolMediaId;                /**< MediaId */
   utime_t VolFirstWritten;           /**< Time of first write */
   utime_t VolLastWritten;            /**< Time of last write */
   bool InChanger;                    /**< Set if vol in current magazine */
   bool is_valid;                     /**< set if this data is valid */
   char VolCatStatus[20];             /**< Volume status */
   char VolCatName[MAX_NAME_LENGTH];  /**< Desired volume to mount */
   char VolEncrKey[MAX_NAME_LENGTH];  /**< Encryption Key needed to read the media */
   uint32_t VolMinBlocksize;          /**< Volume Minimum Blocksize */
   uint32_t VolMaxBlocksize;          /**< Volume Maximum Blocksize */
};

struct BLOCKSIZES {
   uint32_t max_block_size;
   uint32_t min_block_size;
};

class DEVRES; /* Forward reference Device resource defined in stored_conf.h */
class DCR; /* Forward reference */
class VOLRES; /* Forward reference */

/**
 * Device specific status information either returned via DEVICE::device_status()
 * method of via bsdEventDriveStatus and bsdEventVolumeStatus plugin events.
 */
typedef struct DevStatTrigger {
   DEVRES *device;
   POOLMEM *status;
   int status_length;
} bsdDevStatTrig;

/*
 * Device structure definition.
 *
 * There is one of these for each physical device. Everything here is "global" to
 * that device and effects all jobs using the device.
 */
class DEVICE: public SMARTALLOC {
protected:
   int m_fd;                          /**< File descriptor */
private:
   int m_blocked;                     /**< Set if we must wait (i.e. change tape) */
   int m_count;                       /**< Mutex use count -- DEBUG only */
   int m_num_reserved;                /**< Counter of device reservations */
   slot_number_t m_slot;              /**< Slot loaded in drive or -1 if none */
   pthread_t m_pid;                   /**< Thread that locked -- DEBUG only */
   bool m_unload;                     /**< Set when Volume must be unloaded */
   bool m_load;                       /**< Set when Volume must be loaded */

public:
   DEVICE();
   virtual ~DEVICE() {};
   DEVICE * volatile swap_dev;        /**< Swap vol from this device */
   dlist *attached_dcrs;              /**< Attached DCR list */
   bthread_mutex_t m_mutex;           /**< Access control */
   bthread_mutex_t spool_mutex;       /**< Mutex for updating spool_size */
   bthread_mutex_t acquire_mutex;     /**< Mutex for acquire code */
   pthread_mutex_t read_acquire_mutex; /**< Mutex for acquire read code */
   pthread_cond_t wait;               /**< Thread wait variable */
   pthread_cond_t wait_next_vol;      /**< Wait for tape to be mounted */
   pthread_t no_wait_id;              /**< This thread must not wait */
   int dev_prev_blocked;              /**< Previous blocked state */
   int num_waiting;                   /**< Number of threads waiting */
   int num_writers;                   /**< Number of writing threads */
   char capabilities[CAP_BYTES];      /**< Capabilities mask */
   char state[ST_BYTES];              /**< State mask */
   int dev_errno;                     /**< Our own errno */
   int oflags;                        /**< Read/write flags */
   int open_mode;                     /**< Parameter passed to open_dev (useful to reopen the device) */
   int dev_type;                      /**< Device type */
   bool autoselect;                   /**< Autoselect in autochanger */
   bool norewindonclose;              /**< Don't rewind tape drive on close */
   bool initiated;                    /**< Set when init_dev() called */
   int label_type;                    /**< Bareos/ANSI/IBM label types */
   drive_number_t drive;              /**< Autochanger logical drive number (base 0) */
   drive_number_t drive_index;        /**< Autochanger physical drive index (base 0) */
   POOLMEM *dev_name;                 /**< Physical device name */
   POOLMEM *dev_options;              /**< Device specific options */
   POOLMEM *prt_name;                 /**< Name used for display purposes */
   char *errmsg;                      /**< Nicely edited error message */
   uint32_t block_num;                /**< Current block number base 0 */
   uint32_t LastBlock;                /**< Last DEV_BLOCK number written to Volume */
   uint32_t file;                     /**< Current file number base 0 */
   uint64_t file_addr;                /**< Current file read/write address */
   uint64_t file_size;                /**< Current file size */
   uint32_t EndBlock;                 /**< Last block written */
   uint32_t EndFile;                  /**< Last file written */
   uint32_t min_block_size;           /**< Min block size currently set */
   uint32_t max_block_size;           /**< Max block size currently set */
   uint32_t max_concurrent_jobs;      /**< Maximum simultaneous jobs this drive */
   uint64_t max_volume_size;          /**< Max bytes to put on one volume */
   uint64_t max_file_size;            /**< Max file size to put in one file on volume */
   uint64_t volume_capacity;          /**< Advisory capacity */
   uint64_t max_spool_size;           /**< Maximum spool file size */
   uint64_t spool_size;               /**< Current spool size for this device */
   uint32_t max_rewind_wait;          /**< Max secs to allow for rewind */
   uint32_t max_open_wait;            /**< Max secs to allow for open */
   uint32_t max_open_vols;            /**< Max simultaneous open volumes */

   utime_t vol_poll_interval;         /**< Interval between polling Vol mount */
   DEVRES *device;                    /**< Pointer to Device Resource */
   VOLRES *vol;                       /**< Pointer to Volume reservation item */
   btimer_t *tid;                     /**< Timer id */

   VOLUME_CAT_INFO VolCatInfo;        /**< Volume Catalog Information */
   VOLUME_LABEL VolHdr;               /**< Actual volume label */
   char pool_name[MAX_NAME_LENGTH];   /**< Pool name */
   char pool_type[MAX_NAME_LENGTH];   /**< Pool type */

   char UnloadVolName[MAX_NAME_LENGTH]; /**< Last wrong Volume mounted */
   bool poll;                         /**< Set to poll Volume */
   /* Device wait times ***FIXME*** look at durations */
   int min_wait;
   int max_wait;
   int max_num_wait;
   int wait_sec;
   int rem_wait_sec;
   int num_wait;

   btime_t last_timer;                /**< Used by read/write/seek to get stats (usec) */
   btime_t last_tick;                 /**< Contains last read/write time (usec) */

   btime_t  DevReadTime;
   btime_t  DevWriteTime;
   uint64_t DevWriteBytes;
   uint64_t DevReadBytes;

   /* Methods */
   btime_t get_timer_count();         /**< Return the last timer interval (ms) */

   bool has_cap(int cap) const { return bit_is_set(cap, capabilities) ; }
   void clear_cap(int cap) { clear_bit(cap, capabilities); }
   void set_cap(int cap) { set_bit(cap, capabilities);  }
   bool do_checksum() const { return bit_is_set(CAP_BLOCKCHECKSUM, capabilities); }
   bool is_autochanger() const { return bit_is_set(CAP_AUTOCHANGER, capabilities); }
   bool requires_mount() const { return bit_is_set(CAP_REQMOUNT, capabilities); }
   bool is_removable() const { return bit_is_set(CAP_REM, capabilities); }
   bool is_tape() const { return (dev_type == B_TAPE_DEV); }
   bool is_file() const { return (dev_type == B_FILE_DEV ||
                                  dev_type == B_GFAPI_DEV ||
                                  dev_type == B_DROPLET_DEV ||
                                  dev_type == B_RADOS_DEV ||
                                  dev_type == B_CEPHFS_DEV ||
                                  dev_type == B_ELASTO_DEV); }
   bool is_fifo() const { return dev_type == B_FIFO_DEV; }
   bool is_vtl() const  { return dev_type == B_VTL_DEV; }
   bool is_open() const { return m_fd >= 0; }
   bool is_offline() const { return bit_is_set(ST_OFFLINE, state); }
   bool is_labeled() const { return bit_is_set(ST_LABEL, state); }
   bool is_mounted() const { return bit_is_set(ST_MOUNTED, state); }
   bool is_unmountable() const { return ((is_file() && is_removable())); }
   int num_reserved() const { return m_num_reserved; };
   bool is_part_spooled() const { return bit_is_set(ST_PART_SPOOLED, state); }
   bool have_media() const { return bit_is_set(ST_MEDIA, state); }
   bool is_short_block() const { return bit_is_set(ST_SHORT, state); }
   bool is_busy() const { return bit_is_set(ST_READREADY, state) || num_writers || num_reserved(); }
   bool at_eof() const { return bit_is_set(ST_EOF, state); }
   bool at_eot() const { return bit_is_set(ST_EOT, state); }
   bool at_weot() const { return bit_is_set(ST_WEOT, state); }
   bool can_append() const { return bit_is_set(ST_APPENDREADY, state); }
   bool is_crypto_enabled() const { return bit_is_set(ST_CRYPTOKEY, state); }

   /**
    * can_write() is meant for checking at the end of a job to see
    * if we still have a tape (perhaps not if at end of tape
    * and the job is canceled).
    */
   bool can_write() const { return is_open() && can_append() &&
                                   is_labeled() && !at_weot(); }
   bool can_read() const { return bit_is_set(ST_READREADY, state); }
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
   const char *print_name() const;    /**< Name for display purposes */
   void set_eot() { set_bit(ST_EOT, state); };
   void set_eof() { set_bit(ST_EOF, state); };
   void set_append() { set_bit(ST_APPENDREADY, state); };
   void set_labeled() { set_bit(ST_LABEL, state); };
   inline void set_read() { set_bit(ST_READREADY, state); };
   void set_offline() { set_bit(ST_OFFLINE, state); };
   void set_mounted() { set_bit(ST_MOUNTED, state); };
   void set_media() { set_bit(ST_MEDIA, state); };
   void set_short_block() { set_bit(ST_SHORT, state); };
   void set_crypto_enabled() { set_bit(ST_CRYPTOKEY, state); };
   void set_part_spooled(int val) {
      if (val)
         set_bit(ST_PART_SPOOLED, state);
      else
         clear_bit(ST_PART_SPOOLED, state);
   };
   bool is_volume_to_unload() const { \
      return m_unload && strcmp(VolHdr.VolumeName, UnloadVolName) == 0; };
   void set_load() { m_load = true; };
   void inc_reserved() { m_num_reserved++; }
   void dec_reserved() { m_num_reserved--; ASSERT(m_num_reserved>=0); };
   void clear_append() { clear_bit(ST_APPENDREADY, state); };
   void clear_read() { clear_bit(ST_READREADY, state); };
   void clear_labeled() { clear_bit(ST_LABEL, state); };
   void clear_offline() { clear_bit(ST_OFFLINE, state); };
   void clear_eot() { clear_bit(ST_EOT, state); };
   void clear_eof() { clear_bit(ST_EOF, state); };
   void clear_opened() { m_fd = -1; };
   void clear_mounted() { clear_bit(ST_MOUNTED, state); };
   void clear_media() { clear_bit(ST_MEDIA, state); };
   void clear_short_block() { clear_bit(ST_SHORT, state); };
   void clear_crypto_enabled() { clear_bit(ST_CRYPTOKEY, state); };
   void clear_unload() { m_unload = false; UnloadVolName[0] = 0; };
   void clear_load() { m_load = false; };
   char *bstrerror(void) { return errmsg; };
   char *print_errmsg() { return errmsg; };
   slot_number_t get_slot() const { return m_slot; };
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
   bool close(DCR *dcr);
   bool open(DCR *dcr, int mode);
   void term();
   ssize_t read(void *buf, size_t len);
   ssize_t write(const void *buf, size_t len);
   bool mount(DCR *dcr, int timeout);
   bool unmount(DCR *dcr, int timeout);
   void edit_mount_codes(POOL_MEM &omsg, const char *imsg);
   bool offline_or_rewind();
   bool scan_dir_for_volume(DCR *dcr);
   void set_slot(slot_number_t slot);
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
   virtual char *status_dev();
   virtual bool eod(DCR *dcr);
   virtual void set_ateof();
   virtual void set_ateot();
   virtual bool rewind(DCR *dcr);
   virtual bool update_pos(DCR *dcr);
   virtual bool reposition(DCR *dcr, uint32_t rfile, uint32_t rblock);
   virtual bool mount_backend(DCR *dcr, int timeout) { return true; };
   virtual bool unmount_backend(DCR *dcr, int timeout) { return true; };
   virtual bool device_status(bsdDevStatTrig *dst) { return false; };
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

/**
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
   bool m_dev_locked;                 /**< Set if dev already locked */
   int m_dev_lock;                    /**< Non-zero if rLock already called */
   bool m_reserved;                   /**< Set if reserved device */
   bool m_found_in_use;               /**< Set if a volume found in use */
   bool m_will_write;                 /**< Set if DCR will be used for writing */

public:
   dlink dev_link;                    /**< Link to attach to dev */
   JCR *jcr;                          /**< Pointer to JCR */
   bthread_mutex_t m_mutex;           /**< Access control */
   pthread_mutex_t r_mutex;           /**< rLock pre-mutex */
   DEVICE * volatile dev;             /**< Pointer to device */
   DEVRES *device;                    /**< Pointer to device resource */
   DEV_BLOCK *block;                  /**< Pointer to current block */
   DEV_RECORD *rec;                   /**< Pointer to record being processed */
   DEV_RECORD *before_rec;            /**< Pointer to record before translation */
   DEV_RECORD *after_rec;             /**< Pointer to record after translation */
   pthread_t tid;                     /**< Thread running this dcr */
   int spool_fd;                      /**< Fd if spooling */
   bool spool_data;                   /**< Set to spool data */
   bool spooling;                     /**< Set when actually spooling */
   bool despooling;                   /**< Set when despooling */
   bool despool_wait;                 /**< Waiting for despooling */
   bool NewVol;                       /**< Set if new Volume mounted */
   bool WroteVol;                     /**< Set if Volume written */
   bool NewFile;                      /**< Set when EOF written */
   bool reserved_volume;              /**< Set if we reserved a volume */
   bool any_volume;                   /**< Any OK for dir_find_next... */
   bool attached_to_dev;              /**< Set when attached to dev */
   bool keep_dcr;                     /**< Do not free dcr in release_dcr */
   uint32_t autodeflate;              /**< Try to autodeflate streams */
   uint32_t autoinflate;              /**< Try to autoinflate streams */
   uint32_t VolFirstIndex;            /**< First file index this Volume */
   uint32_t VolLastIndex;             /**< Last file index this Volume */
   uint32_t FileIndex;                /**< Current File Index */
   uint32_t EndFile;                  /**< End file written */
   uint32_t StartFile;                /**< Start write file */
   uint32_t StartBlock;               /**< Start write block */
   uint32_t EndBlock;                 /**< Ending block written */
   int64_t  VolMediaId;               /**< MediaId */
   int64_t job_spool_size;            /**< Current job spool size */
   int64_t max_job_spool_size;        /**< Max job spool size */
   uint32_t VolMinBlocksize;          /**< Minimum Blocksize */
   uint32_t VolMaxBlocksize;          /**< Maximum Blocksize */
   char VolumeName[MAX_NAME_LENGTH];  /**< Volume name */
   char pool_name[MAX_NAME_LENGTH];   /**< Pool name */
   char pool_type[MAX_NAME_LENGTH];   /**< Pool type */
   char media_type[MAX_NAME_LENGTH];  /**< Media type */
   char dev_name[MAX_NAME_LENGTH];    /**< Dev name */
   int Copy;                          /**< Identical copy number */
   int Stripe;                        /**< RAIT stripe */
   VOLUME_CAT_INFO VolCatInfo;        /**< Catalog info for desired volume */

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
