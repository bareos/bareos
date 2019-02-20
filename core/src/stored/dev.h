/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, MM
 */
/**
 * @file
 * Definitions for using the Device functions in Bareos Tape and File storage
 * access
 */

/**
 * Some details of how volume and device reservations work
 *
 * class VolumeReservationItem:
 *   SetInUse()     volume being used on current drive
 *   ClearInUse()   no longer being used.  Can be re-used or moved.
 *   SetSwapping()   set volume being moved to another drive
 *   IsSwapping()    volume is being moved to another drive
 *   ClearSwapping() volume normal
 *
 * class Device:
 *   SetLoad()       set to load volume
 *   needs_load()     volume must be loaded (i.e. SetLoad done)
 *   clear_load()     load done.
 *   SetUnload()     set to unload volume
 *   needs_unload()    volume must be unloaded
 *   ClearUnload()   volume unloaded
 *
 *    reservations are temporary until the drive is acquired
 *   IncReserved()   increments num of reservations
 *   DecReserved()   decrements num of reservations
 *   NumReserved()   number of reservations
 *
 * class DeviceControlRecord:
 *   SetReserved()   sets local reserve flag and calls dev->IncReserved()
 *   ClearReserved() clears local reserve flag and calls dev->DecReserved()
 *   IsReserved()    returns local reserved flag
 *   UnreserveDevice()  much more complete unreservation
 */

#ifndef BAREOS_STORED_DEV_H_
#define BAREOS_STORED_DEV_H_ 1


#include "stored/record.h"
#include "stored/lock.h"
#include "stored/block.h"
#include "lib/bsys.h"

namespace storagedaemon {


/**
 * Return values from WaitForSysop()
 */
enum
{
  W_ERROR = 1,
  W_TIMEOUT,
  W_POLL,
  W_MOUNT,
  W_WAKE
};

/**
 * Arguments to open_dev()
 */
enum
{
  CREATE_READ_WRITE = 1,
  OPEN_READ_WRITE,
  OPEN_READ_ONLY,
  OPEN_WRITE_ONLY
};

/**
 * Device types
 */
enum
{
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
enum
{
  IO_DIRECTION_NONE = 0,
  IO_DIRECTION_IN,
  IO_DIRECTION_OUT,
  IO_DIRECTION_INOUT
};

/**
 * Generic status bits returned from StatusDev()
 */
enum
{
  BMT_TAPE = 0,     /**< Is tape device */
  BMT_EOF = 1,      /**< Just read EOF */
  BMT_BOT = 2,      /**< At beginning of tape */
  BMT_EOT = 3,      /**< End of tape reached */
  BMT_SM = 4,       /**< DDS setmark */
  BMT_EOD = 5,      /**< DDS at end of data */
  BMT_WR_PROT = 6,  /**< Tape write protected */
  BMT_ONLINE = 7,   /**< Tape online */
  BMT_DR_OPEN = 8,  /**< Tape door open */
  BMT_IM_REP_EN = 9 /**< Immediate report enabled */
};

/**
 * Keep this set to the last entry in the enum.
 */
#define BMT_MAX BMT_IM_REP_EN

/**
 * Make sure you have enough bits to store all above bit fields.
 */
#define BMT_BYTES NbytesForBits(BMT_MAX + 1)

/**
 * Bits for device capabilities
 */
enum
{
  CAP_EOF = 0,             /**< Has MTWEOF */
  CAP_BSR = 1,             /**< Has MTBSR */
  CAP_BSF = 2,             /**< Has MTBSF */
  CAP_FSR = 3,             /**< Has MTFSR */
  CAP_FSF = 4,             /**< Has MTFSF */
  CAP_EOM = 5,             /**< Has MTEOM */
  CAP_REM = 6,             /**< Is removable media */
  CAP_RACCESS = 7,         /**< Is random access device */
  CAP_AUTOMOUNT = 8,       /**< Read device at start to see what is there */
  CAP_LABEL = 9,           /**< Label blank tapes */
  CAP_ANONVOLS = 10,       /**< Mount without knowing volume name */
  CAP_ALWAYSOPEN = 11,     /**< Always keep device open */
  CAP_AUTOCHANGER = 12,    /**< AutoChanger */
  CAP_OFFLINEUNMOUNT = 13, /**< Offline before unmount */
  CAP_STREAM = 14,         /**< Stream device */
  CAP_BSFATEOM = 15,       /**< Backspace file at EOM */
  CAP_FASTFSF = 16,        /**< Fast forward space file */
  CAP_TWOEOF = 17,         /**< Write two eofs for EOM */
  CAP_CLOSEONPOLL = 18,    /**< Close device on polling */
  CAP_POSITIONBLOCKS = 19, /**< Use block positioning */
  CAP_MTIOCGET = 20,       /**< Basic support for fileno and blkno */
  CAP_REQMOUNT = 21,       /**< Require mount/unmount */
  CAP_CHECKLABELS = 22,    /**< Check for ANSI/IBM labels */
  CAP_BLOCKCHECKSUM = 23,  /**< Create/test block checksum */
  CAP_IOERRATEOM = 24,     /**< IOError at EOM */
  CAP_IBMLINTAPE = 25,     /**< Using IBM lin_tape driver */
  CAP_ADJWRITESIZE = 26    /**< Adjust write size to min/max */
};

/**
 * Keep this set to the last entry in the enum.
 */
constexpr int CAP_MAX = CAP_ADJWRITESIZE;

/**
 * Make sure you have enough bits to store all above bit fields.
 */
constexpr int CAP_BYTES = NbytesForBits(CAP_MAX + 1);

/**
 * Device state bits
 */
enum
{
  ST_LABEL = 0,         /**< Label found */
  ST_MALLOC = 1,        /**< Dev packet malloc'ed in InitDev() */
  ST_APPENDREADY = 2,   /**< Ready for Bareos append */
  ST_READREADY = 3,     /**< Ready for Bareos read */
  ST_EOT = 4,           /**< At end of tape */
  ST_WEOT = 5,          /**< Got EOT on write */
  ST_EOF = 6,           /**< Read EOF i.e. zero bytes */
  ST_NEXTVOL = 7,       /**< Start writing on next volume */
  ST_SHORT = 8,         /**< Short block read */
  ST_MOUNTED = 9,       /**< The device is mounted to the mount point */
  ST_MEDIA = 10,        /**< Media found in mounted device */
  ST_OFFLINE = 11,      /**< Set offline by operator */
  ST_PART_SPOOLED = 12, /**< Spooling part */
  ST_CRYPTOKEY = 13     /**< The device has a crypto key loaded */
};

/**
 * Keep this set to the last entry in the enum.
 */
#define ST_MAX ST_CRYPTOKEY

/**
 * Make sure you have enough bits to store all above bit fields.
 */
#define ST_BYTES NbytesForBits(ST_MAX + 1)

/**
 * Volume Catalog Information structure definition
 */
struct VolumeCatalogInfo {
  /*
   * Media info for the current Volume
   */
  uint32_t VolCatJobs;          /**< number of jobs on this Volume */
  uint32_t VolCatFiles;         /**< Number of files */
  uint32_t VolCatBlocks;        /**< Number of blocks */
  uint64_t VolCatBytes;         /**< Number of bytes written */
  uint32_t VolCatMounts;        /**< Number of mounts this volume */
  uint32_t VolCatErrors;        /**< Number of errors this volume */
  uint32_t VolCatWrites;        /**< Number of writes this volume */
  uint32_t VolCatReads;         /**< Number of reads this volume */
  uint64_t VolCatRBytes;        /**< Number of bytes read */
  uint32_t VolCatRecycles;      /**< Number of recycles this volume */
  uint32_t EndFile;             /**< Last file number */
  uint32_t EndBlock;            /**< Last block number */
  int32_t LabelType;            /**< Bareos/ANSI/IBM */
  int32_t Slot;                 /**< >0=Slot loaded, 0=nothing, -1=unknown */
  uint32_t VolCatMaxJobs;       /**< Maximum Jobs to write to volume */
  uint32_t VolCatMaxFiles;      /**< Maximum files to write to volume */
  uint64_t VolCatMaxBytes;      /**< Max bytes to write to volume */
  uint64_t VolCatCapacityBytes; /**< capacity estimate */
  btime_t VolReadTime;          /**< time spent reading */
  btime_t VolWriteTime;         /**< time spent writing this Volume */
  int64_t VolMediaId;           /**< MediaId */
  utime_t VolFirstWritten;      /**< Time of first write */
  utime_t VolLastWritten;       /**< Time of last write */
  bool InChanger;               /**< Set if vol in current magazine */
  bool is_valid;                /**< set if this data is valid */
  char VolCatStatus[20];        /**< Volume status */
  char VolCatName[MAX_NAME_LENGTH]; /**< Desired volume to mount */
  char VolEncrKey[MAX_NAME_LENGTH]; /**< Encryption Key needed to read the media
                                     */
  uint32_t VolMinBlocksize;         /**< Volume Minimum Blocksize */
  uint32_t VolMaxBlocksize;         /**< Volume Maximum Blocksize */
};

struct BlockSizes {
  uint32_t max_block_size;
  uint32_t min_block_size;
};

class DeviceResource;        /* Forward reference Device resource defined in
                                stored_conf.h */
class DeviceControlRecord;   /* Forward reference */
class VolumeReservationItem; /* Forward reference */

/**
 * Device specific status information either returned via Device::DeviceStatus()
 * method of via bsdEventDriveStatus and bsdEventVolumeStatus plugin events.
 */
typedef struct DeviceStatusTrigger {
  DeviceResource* device;
  POOLMEM* status;
  int status_length;
} bsdDevStatTrig;

/*
 * Device structure definition.
 *
 * There is one of these for each physical device. Everything here is "global"
 * to that device and effects all jobs using the device.
 */
class Device : public SmartAlloc {
 protected:
  int fd_; /**< File descriptor */
 private:
  int blocked_;        /**< Set if we must wait (i.e. change tape) */
  int count_;          /**< Mutex use count -- DEBUG only */
  int num_reserved_;   /**< Counter of device reservations */
  slot_number_t slot_; /**< Slot loaded in drive or -1 if none */
  pthread_t pid_;      /**< Thread that locked -- DEBUG only */
  bool unload_;        /**< Set when Volume must be unloaded */
  bool load_;          /**< Set when Volume must be loaded */

 public:
  Device();
  virtual ~Device() {}
  Device* volatile swap_dev;          /**< Swap vol from this device */
  dlist* attached_dcrs;               /**< Attached DeviceControlRecord list */
  pthread_mutex_t mutex_;             /**< Access control */
  pthread_mutex_t spool_mutex;        /**< Mutex for updating spool_size */
  pthread_mutex_t acquire_mutex;      /**< Mutex for acquire code */
  pthread_mutex_t read_acquire_mutex; /**< Mutex for acquire read code */
  pthread_cond_t wait;                /**< Thread wait variable */
  pthread_cond_t wait_next_vol;       /**< Wait for tape to be mounted */
  pthread_t no_wait_id;               /**< This thread must not wait */
  int dev_prev_blocked;               /**< Previous blocked state */
  int num_waiting;                    /**< Number of threads waiting */
  int num_writers;                    /**< Number of writing threads */
  char capabilities[CAP_BYTES];       /**< Capabilities mask */
  char state[ST_BYTES];               /**< State mask */
  int dev_errno;                      /**< Our own errno */
  int oflags;                         /**< Read/write flags */
  int open_mode; /**< Parameter passed to open_dev (useful to reopen the device)
                  */
  int dev_type;  /**< Device type */
  bool autoselect;            /**< Autoselect in autochanger */
  bool norewindonclose;       /**< Don't rewind tape drive on close */
  bool initiated;             /**< Set when InitDev() called */
  int label_type;             /**< Bareos/ANSI/IBM label types */
  drive_number_t drive;       /**< Autochanger logical drive number (base 0) */
  drive_number_t drive_index; /**< Autochanger physical drive index (base 0) */
  POOLMEM* dev_name;          /**< Physical device name */
  POOLMEM* dev_options;       /**< Device specific options */
  POOLMEM* prt_name;          /**< Name used for display purposes */
  char* errmsg;               /**< Nicely edited error message */
  uint32_t block_num;         /**< Current block number base 0 */
  uint32_t LastBlock;         /**< Last DeviceBlock number written to Volume */
  uint32_t file;              /**< Current file number base 0 */
  uint64_t file_addr;         /**< Current file read/write address */
  uint64_t file_size;         /**< Current file size */
  uint32_t EndBlock;          /**< Last block written */
  uint32_t EndFile;           /**< Last file written */
  uint32_t min_block_size;    /**< Min block size currently set */
  uint32_t max_block_size;    /**< Max block size currently set */
  uint32_t max_concurrent_jobs; /**< Maximum simultaneous jobs this drive */
  uint64_t max_volume_size;     /**< Max bytes to put on one volume */
  uint64_t max_file_size;   /**< Max file size to put in one file on volume */
  uint64_t volume_capacity; /**< Advisory capacity */
  uint64_t max_spool_size;  /**< Maximum spool file size */
  uint64_t spool_size;      /**< Current spool size for this device */
  uint32_t max_rewind_wait; /**< Max secs to allow for rewind */
  uint32_t max_open_wait;   /**< Max secs to allow for open */
  uint32_t max_open_vols;   /**< Max simultaneous open volumes */

  utime_t vol_poll_interval;  /**< Interval between polling Vol mount */
  DeviceResource* device;     /**< Pointer to Device Resource */
  VolumeReservationItem* vol; /**< Pointer to Volume reservation item */
  btimer_t* tid;              /**< Timer id */

  VolumeCatalogInfo VolCatInfo;    /**< Volume Catalog Information */
  VOLUME_LABEL VolHdr;             /**< Actual volume label */
  char pool_name[MAX_NAME_LENGTH]; /**< Pool name */
  char pool_type[MAX_NAME_LENGTH]; /**< Pool type */

  char UnloadVolName[MAX_NAME_LENGTH]; /**< Last wrong Volume mounted */
  bool poll;                           /**< Set to poll Volume */
  /* Device wait times ***FIXME*** look at durations */
  int min_wait;
  int max_wait;
  int max_num_wait;
  int wait_sec;
  int rem_wait_sec;
  int num_wait;

  btime_t last_timer; /**< Used by read/write/seek to get stats (usec) */
  btime_t last_tick;  /**< Contains last read/write time (usec) */

  btime_t DevReadTime;
  btime_t DevWriteTime;
  uint64_t DevWriteBytes;
  uint64_t DevReadBytes;

  /* Methods */
  btime_t GetTimerCount(); /**< Return the last timer interval (ms) */

  bool HasCap(int cap) const { return BitIsSet(cap, capabilities); }
  void ClearCap(int cap) { ClearBit(cap, capabilities); }
  void SetCap(int cap) { SetBit(cap, capabilities); }
  bool DoChecksum() const { return BitIsSet(CAP_BLOCKCHECKSUM, capabilities); }
  bool IsAutochanger() const { return BitIsSet(CAP_AUTOCHANGER, capabilities); }
  bool RequiresMount() const { return BitIsSet(CAP_REQMOUNT, capabilities); }
  bool IsRemovable() const { return BitIsSet(CAP_REM, capabilities); }
  bool IsTape() const { return (dev_type == B_TAPE_DEV); }
  bool IsFile() const
  {
    return (dev_type == B_FILE_DEV || dev_type == B_GFAPI_DEV ||
            dev_type == B_DROPLET_DEV || dev_type == B_RADOS_DEV ||
            dev_type == B_CEPHFS_DEV || dev_type == B_ELASTO_DEV);
  }
  bool IsFifo() const { return dev_type == B_FIFO_DEV; }
  bool IsVtl() const { return dev_type == B_VTL_DEV; }
  bool IsOpen() const { return fd_ >= 0; }
  bool IsOffline() const { return BitIsSet(ST_OFFLINE, state); }
  bool IsLabeled() const { return BitIsSet(ST_LABEL, state); }
  bool IsMounted() const { return BitIsSet(ST_MOUNTED, state); }
  bool IsUnmountable() const { return ((IsFile() && IsRemovable())); }
  int NumReserved() const { return num_reserved_; }
  bool is_part_spooled() const { return BitIsSet(ST_PART_SPOOLED, state); }
  bool have_media() const { return BitIsSet(ST_MEDIA, state); }
  bool IsShortBlock() const { return BitIsSet(ST_SHORT, state); }
  bool IsBusy() const
  {
    return BitIsSet(ST_READREADY, state) || num_writers || NumReserved();
  }
  bool AtEof() const { return BitIsSet(ST_EOF, state); }
  bool AtEot() const { return BitIsSet(ST_EOT, state); }
  bool AtWeot() const { return BitIsSet(ST_WEOT, state); }
  bool CanAppend() const { return BitIsSet(ST_APPENDREADY, state); }
  bool IsCryptoEnabled() const { return BitIsSet(ST_CRYPTOKEY, state); }

  /**
   * CanWrite() is meant for checking at the end of a job to see
   * if we still have a tape (perhaps not if at end of tape
   * and the job is canceled).
   */
  bool CanWrite() const
  {
    return IsOpen() && CanAppend() && IsLabeled() && !AtWeot();
  }
  bool CanRead() const { return BitIsSet(ST_READREADY, state); }
  bool CanStealLock() const;
  bool waiting_for_mount() const;
  bool MustUnload() const { return unload_; }
  bool must_load() const { return load_; }
  const char* strerror() const;
  const char* archive_name() const;
  const char* name() const;
  const char* print_name() const; /**< Name for display purposes */
  void SetEot() { SetBit(ST_EOT, state); }
  void SetEof() { SetBit(ST_EOF, state); }
  void SetAppend() { SetBit(ST_APPENDREADY, state); }
  void SetLabeled() { SetBit(ST_LABEL, state); }
  inline void SetRead() { SetBit(ST_READREADY, state); }
  void set_offline() { SetBit(ST_OFFLINE, state); }
  void SetMounted() { SetBit(ST_MOUNTED, state); }
  void set_media() { SetBit(ST_MEDIA, state); }
  void SetShortBlock() { SetBit(ST_SHORT, state); }
  void SetCryptoEnabled() { SetBit(ST_CRYPTOKEY, state); }
  void SetPartSpooled(int val)
  {
    if (val)
      SetBit(ST_PART_SPOOLED, state);
    else
      ClearBit(ST_PART_SPOOLED, state);
  }
  bool IsVolumeToUnload() const
  {
    return unload_ && strcmp(VolHdr.VolumeName, UnloadVolName) == 0;
  }
  void SetLoad() { load_ = true; }
  void IncReserved() { num_reserved_++; }
  void DecReserved()
  {
    num_reserved_--;
    ASSERT(num_reserved_ >= 0);
  }
  void ClearAppend() { ClearBit(ST_APPENDREADY, state); }
  void ClearRead() { ClearBit(ST_READREADY, state); }
  void ClearLabeled() { ClearBit(ST_LABEL, state); }
  void clear_offline() { ClearBit(ST_OFFLINE, state); }
  void ClearEot() { ClearBit(ST_EOT, state); }
  void ClearEof() { ClearBit(ST_EOF, state); }
  void ClearOpened() { fd_ = -1; }
  void ClearMounted() { ClearBit(ST_MOUNTED, state); }
  void clear_media() { ClearBit(ST_MEDIA, state); }
  void ClearShortBlock() { ClearBit(ST_SHORT, state); }
  void ClearCryptoEnabled() { ClearBit(ST_CRYPTOKEY, state); }
  void ClearUnload()
  {
    unload_ = false;
    UnloadVolName[0] = 0;
  }
  void clear_load() { load_ = false; }
  char* bstrerror(void) { return errmsg; }
  char* print_errmsg() { return errmsg; }
  slot_number_t GetSlot() const { return slot_; }
  void setVolCatInfo(bool valid) { VolCatInfo.is_valid = valid; }
  bool haveVolCatInfo() const { return VolCatInfo.is_valid; }
  void setVolCatName(const char* name)
  {
    bstrncpy(VolCatInfo.VolCatName, name, sizeof(VolCatInfo.VolCatName));
    setVolCatInfo(false);
  }
  char* getVolCatName() { return VolCatInfo.VolCatName; }

  const char* mode_to_str(int mode);
  void SetUnload();
  void ClearVolhdr();
  bool close(DeviceControlRecord* dcr);
  bool open(DeviceControlRecord* dcr, int mode);
  void term();
  ssize_t read(void* buf, size_t len);
  ssize_t write(const void* buf, size_t len);
  bool mount(DeviceControlRecord* dcr, int timeout);
  bool unmount(DeviceControlRecord* dcr, int timeout);
  void EditMountCodes(PoolMem& omsg, const char* imsg);
  bool OfflineOrRewind();
  bool ScanDirForVolume(DeviceControlRecord* dcr);
  void SetSlot(slot_number_t slot);
  void ClearSlot();

  void SetBlocksizes(DeviceControlRecord* dcr);
  void SetLabelBlocksize(DeviceControlRecord* dcr);

  uint32_t GetFile() const { return file; }
  uint32_t GetBlockNum() const { return block_num; }
  int fd() const { return fd_; }

  /*
   * Tape specific operations.
   */
  virtual bool offline() { return true; }
  virtual bool weof(int num) { return true; }
  virtual bool fsf(int num) { return true; }
  virtual bool bsf(int num) { return false; }
  virtual bool fsr(int num) { return false; }
  virtual bool bsr(int num) { return false; }
  virtual bool LoadDev() { return true; }
  virtual void LockDoor(){};
  virtual void UnlockDoor(){};
  virtual void clrerror(int func){};
  virtual void SetOsDeviceParameters(DeviceControlRecord* dcr){};
  virtual int32_t GetOsTapeFile() { return -1; }

  /*
   * Generic operations.
   */
  virtual void OpenDevice(DeviceControlRecord* dcr, int omode);
  virtual char* StatusDev();
  virtual bool eod(DeviceControlRecord* dcr);
  virtual void SetAteof();
  virtual void SetAteot();
  virtual bool rewind(DeviceControlRecord* dcr);
  virtual bool UpdatePos(DeviceControlRecord* dcr);
  virtual bool Reposition(DeviceControlRecord* dcr,
                          uint32_t rfile,
                          uint32_t rblock);
  virtual bool MountBackend(DeviceControlRecord* dcr, int timeout)
  {
    return true;
  }
  virtual bool UnmountBackend(DeviceControlRecord* dcr, int timeout)
  {
    return true;
  }
  virtual bool DeviceStatus(bsdDevStatTrig* dst) { return false; }
  boffset_t lseek(DeviceControlRecord* dcr, boffset_t offset, int whence)
  {
    return d_lseek(dcr, offset, whence);
  }
  bool truncate(DeviceControlRecord* dcr) { return d_truncate(dcr); }
  bool flush(DeviceControlRecord* dcr) { return d_flush(dcr); };

  /*
   * Low level operations
   */
  virtual int d_ioctl(int fd, ioctl_req_t request, char* mt_com = NULL) = 0;
  virtual int d_open(const char* pathname, int flags, int mode) = 0;
  virtual int d_close(int fd) = 0;
  virtual ssize_t d_read(int fd, void* buffer, size_t count) = 0;
  virtual ssize_t d_write(int fd, const void* buffer, size_t count) = 0;
  virtual boffset_t d_lseek(DeviceControlRecord* dcr,
                            boffset_t offset,
                            int whence) = 0;
  virtual bool d_truncate(DeviceControlRecord* dcr) = 0;
  virtual bool d_flush(DeviceControlRecord* dcr) { return true; };

    /*
     * Locking and blocking calls
     */
#ifdef SD_DEBUG_LOCK
  void dbg_rLock(const char*, int, bool locked = false);
  void dbg_rUnlock(const char*, int);
  void dbg_Lock(const char*, int);
  void dbg_Unlock(const char*, int);
  void dbg_Lock_acquire(const char*, int);
  void dbg_Unlock_acquire(const char*, int);
  void dbg_Lock_read_acquire(const char*, int);
  void dbg_Unlock_read_acquire(const char*, int);
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
  int InitMutex();
  int InitAcquireMutex();
  int InitReadAcquireMutex();
  int init_volcat_mutex();
  int NextVolTimedwait(const struct timespec* timeout);
  void dblock(int why);
  void dunblock(bool locked = false);
  bool IsDeviceUnmounted();
  void SetBlocked(int block) { blocked_ = block; }
  bool IsBlocked() const;
  int blocked() const { return blocked_; }
  const char* print_blocked() const;

 protected:
  void set_mode(int mode);
};

inline const char* Device::strerror() const { return errmsg; }
inline const char* Device::archive_name() const { return dev_name; }
inline const char* Device::print_name() const { return prt_name; }

#define CHECK_BLOCK_NUMBERS true
#define NO_BLOCK_NUMBER_CHECK false

enum get_vol_info_rw
{
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
 * Normally, there is only one JobControlRecord thread per DeviceControlRecord.
 * However, the big and important exception to this is when a Job is being
 * canceled. At that time, there may be two threads using the
 * same DeviceControlRecord. Consequently, when creating/attaching/detaching
 * and freeing the DeviceControlRecord we must lock it (mutex_).
 */
class DeviceControlRecord : public SmartAlloc {
 private:
  bool dev_locked_;   /**< Set if dev already locked */
  int dev_lock_;      /**< Non-zero if rLock already called */
  bool reserved_;     /**< Set if reserved device */
  bool found_in_use_; /**< Set if a volume found in use */
  bool will_write_; /**< Set if DeviceControlRecord will be used for writing */

 public:
  dlink dev_link;                   /**< Link to attach to dev */
  JobControlRecord* jcr;            /**< Pointer to JobControlRecord */
  pthread_mutex_t mutex_;           /**< Access control */
  pthread_mutex_t r_mutex;          /**< rLock pre-mutex */
  Device* volatile dev;             /**< Pointer to device */
  DeviceResource* device;           /**< Pointer to device resource */
  DeviceBlock* block;               /**< Pointer to current block */
  DeviceRecord* rec;                /**< Pointer to record being processed */
  DeviceRecord* before_rec;         /**< Pointer to record before translation */
  DeviceRecord* after_rec;          /**< Pointer to record after translation */
  pthread_t tid;                    /**< Thread running this dcr */
  int spool_fd;                     /**< Fd if spooling */
  bool spool_data;                  /**< Set to spool data */
  bool spooling;                    /**< Set when actually spooling */
  bool despooling;                  /**< Set when despooling */
  bool despool_wait;                /**< Waiting for despooling */
  bool NewVol;                      /**< Set if new Volume mounted */
  bool WroteVol;                    /**< Set if Volume written */
  bool NewFile;                     /**< Set when EOF written */
  bool reserved_volume;             /**< Set if we reserved a volume */
  bool any_volume;                  /**< Any OK for dir_find_next... */
  bool attached_to_dev;             /**< Set when attached to dev */
  bool keep_dcr;                    /**< Do not free dcr in release_dcr */
  uint32_t autodeflate;             /**< Try to autodeflate streams */
  uint32_t autoinflate;             /**< Try to autoinflate streams */
  uint32_t VolFirstIndex;           /**< First file index this Volume */
  uint32_t VolLastIndex;            /**< Last file index this Volume */
  uint32_t FileIndex;               /**< Current File Index */
  uint32_t EndFile;                 /**< End file written */
  uint32_t StartFile;               /**< Start write file */
  uint32_t StartBlock;              /**< Start write block */
  uint32_t EndBlock;                /**< Ending block written */
  int64_t VolMediaId;               /**< MediaId */
  int64_t job_spool_size;           /**< Current job spool size */
  int64_t max_job_spool_size;       /**< Max job spool size */
  uint32_t VolMinBlocksize;         /**< Minimum Blocksize */
  uint32_t VolMaxBlocksize;         /**< Maximum Blocksize */
  char VolumeName[MAX_NAME_LENGTH]; /**< Volume name */
  char pool_name[MAX_NAME_LENGTH];  /**< Pool name */
  char pool_type[MAX_NAME_LENGTH];  /**< Pool type */
  char media_type[MAX_NAME_LENGTH]; /**< Media type */
  char dev_name[MAX_NAME_LENGTH];   /**< Dev name */
  int Copy;                         /**< Identical copy number */
  int Stripe;                       /**< RAIT stripe */
  VolumeCatalogInfo VolCatInfo;     /**< Catalog info for desired volume */

  /*
   * Constructor/Destructor.
   */
  DeviceControlRecord();
  virtual ~DeviceControlRecord(){};

  /*
   * Methods
   */
  void SetDev(Device* ndev) { dev = ndev; }
  void SetFoundInUse() { found_in_use_ = true; }
  void SetWillWrite() { will_write_ = true; }
  void setVolCatInfo(bool valid) { VolCatInfo.is_valid = valid; }

  void ClearFoundInUse() { found_in_use_ = false; }
  void clear_will_write() { will_write_ = false; }

  bool IsReserved() const { return reserved_; }
  bool IsDevLocked() { return dev_locked_; }
  bool IsWriting() const { return will_write_; }

  void IncDevLock() { dev_lock_++; }
  void DecDevLock() { dev_lock_--; }
  bool FoundInUse() const { return found_in_use_; }

  bool haveVolCatInfo() const { return VolCatInfo.is_valid; }
  void setVolCatName(const char* name)
  {
    bstrncpy(VolCatInfo.VolCatName, name, sizeof(VolCatInfo.VolCatName));
    setVolCatInfo(false);
  }
  char* getVolCatName() { return VolCatInfo.VolCatName; }

  /*
   * Methods in askdir.c
   */
  virtual DeviceControlRecord* get_new_spooling_dcr();
  virtual bool DirFindNextAppendableVolume() { return true; }
  virtual bool DirUpdateVolumeInfo(bool label, bool update_LastWritten)
  {
    return true;
  }
  virtual bool DirCreateJobmediaRecord(bool zero) { return true; }
  virtual bool DirUpdateFileAttributes(DeviceRecord* record) { return true; }
  virtual bool DirAskSysopToMountVolume(int mode);
  virtual bool DirAskSysopToCreateAppendableVolume() { return true; }
  virtual bool DirGetVolumeInfo(enum get_vol_info_rw writing);

  /*
   * Methods in lock.c
   */
  void dblock(int why) { dev->dblock(why); }
#ifdef SD_DEBUG_LOCK
  void dbg_mLock(const char*, int, bool locked);
  void dbg_mUnlock(const char*, int);
#else
  void mLock(bool locked);
  void mUnlock();
#endif

  /*
   * Methods in record.c
   */
  bool WriteRecord();

  /*
   * Methods in reserve.c
   */
  void ClearReserved();
  void SetReserved();
  void UnreserveDevice();

  /*
   * Methods in vol_mgr.c
   */
  bool Can_i_use_volume();
  bool Can_i_write_volume();

  /*
   * Methods in mount.c
   */
  bool MountNextWriteVolume();
  bool MountNextReadVolume();
  void MarkVolumeInError();
  void mark_volume_not_inchanger();
  int TryAutolabel(bool opened);
  bool find_a_volume();
  bool IsSuitableVolumeMounted();
  bool is_eod_valid();
  int CheckVolumeLabel(bool& ask, bool& autochanger);
  void ReleaseVolume();
  void DoSwapping(bool IsWriting);
  bool DoUnload();
  bool DoLoad(bool IsWriting);
  bool IsTapePositionOk();

  /*
   * Methods in block.c
   */
  bool WriteBlockToDevice();
  bool WriteBlockToDev();

  enum ReadStatus
  {
    Error = 0,
    Ok,
    EndOfFile,
    EndOfTape
  };

  ReadStatus ReadBlockFromDevice(bool check_block_numbers);
  ReadStatus ReadBlockFromDev(bool check_block_numbers);

  /*
   * Methods in label.c
   */
  bool RewriteVolumeLabel(bool recycle);
};

class StorageDaemonDeviceControlRecord : public DeviceControlRecord {
 public:
  /*
   * Virtual Destructor.
   */
  ~StorageDaemonDeviceControlRecord(){};

  /*
   * Methods overriding default implementations.
   */
  bool DirFindNextAppendableVolume() override;
  bool DirUpdateVolumeInfo(bool label, bool update_LastWritten) override;
  bool DirCreateJobmediaRecord(bool zero) override;
  bool DirUpdateFileAttributes(DeviceRecord* record) override;
  bool DirAskSysopToMountVolume(int mode) override;
  bool DirAskSysopToCreateAppendableVolume() override;
  bool DirGetVolumeInfo(enum get_vol_info_rw writing) override;
  DeviceControlRecord* get_new_spooling_dcr() override;
};

class BTAPE_DCR : public DeviceControlRecord {
 public:
  /*
   * Virtual Destructor.
   */
  ~BTAPE_DCR(){};

  /*
   * Methods overriding default implementations.
   */
  bool DirFindNextAppendableVolume() override;
  bool DirCreateJobmediaRecord(bool zero) override;
  bool DirAskSysopToMountVolume(int mode) override;
  bool DirAskSysopToCreateAppendableVolume() override;
  DeviceControlRecord* get_new_spooling_dcr() override;
};

Device* InitDev(JobControlRecord* jcr, DeviceResource* device);
bool CanOpenMountedDev(Device* dev);
bool LoadDev(Device* dev);
int WriteBlock(Device* dev);
void AttachJcrToDevice(Device* dev, JobControlRecord* jcr);
void DetachJcrFromDevice(Device* dev, JobControlRecord* jcr);
JobControlRecord* next_attached_jcr(Device* dev, JobControlRecord* jcr);
void InitDeviceWaitTimers(DeviceControlRecord* dcr);
void InitJcrDeviceWaitTimers(JobControlRecord* jcr);
bool DoubleDevWaitTime(Device* dev);

/*
 * Get some definition of function to position to the end of the medium in
 * MTEOM. System dependent.
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

} /* namespace storagedaemon */

#endif
