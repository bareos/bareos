/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

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

/**
 * Device Control Record.
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

#ifndef BAREOS_STORED_DEVICE_CONTROL_RECORD_H_
#define BAREOS_STORED_DEVICE_CONTROL_RECORD_H_

#include "stored/autoxflate.h"
#include "stored/volume_catalog_info.h"

namespace storagedaemon {

#define CHECK_BLOCK_NUMBERS true
#define NO_BLOCK_NUMBER_CHECK false

enum get_vol_info_rw
{
  GET_VOL_INFO_FOR_WRITE,
  GET_VOL_INFO_FOR_READ
};

class Device;
class DeviceResource;
struct DeviceBlock;
struct DeviceRecord;

/* clang-format off */

class DeviceControlRecord {
 private:
  bool dev_locked_{};              /**< Set if dev already locked */
  int dev_lock_{};                 /**< Non-zero if rLock already called */
  bool reserved_{};                /**< Set if reserved device */
  bool found_in_use_{};            /**< Set if a volume found in use */
  bool will_write_{};              /**< Set if DeviceControlRecord will be used for writing */

 public:
  JobControlRecord* jcr{};         /**< Pointer to JobControlRecord */
  pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;  /**< Access control */
  pthread_mutex_t r_mutex = PTHREAD_MUTEX_INITIALIZER; /**< rLock pre-mutex */
  Device* volatile dev{};          /**< Pointer to device */
  DeviceResource* device_resource{};    /**< Pointer to device resource */
  DeviceBlock* block{};            /**< Pointer to current block */
  DeviceRecord* rec{};             /**< Pointer to record being processed */
  DeviceRecord* before_rec{};      /**< Pointer to record before translation */
  DeviceRecord* after_rec{};       /**< Pointer to record after translation */
  pthread_t tid{};                 /**< Thread running this dcr */
  bool spool_data{};         /**< Set to spool data */
  int spool_fd{};            /**< Fd if spooling */
  bool spooling{};           /**< Set when actually spooling */
  bool despooling{};         /**< Set when despooling */
  bool despool_wait{};       /**< Waiting for despooling */
  bool NewVol{};             /**< Set if new Volume mounted */
  bool WroteVol{};           /**< Set if Volume written */
  bool NewFile{};            /**< Set when EOF written */
  bool reserved_volume{};    /**< Set if we reserved a volume */
  bool any_volume{};         /**< Any OK for dir_find_next... */
  bool attached_to_dev{};    /**< Set when attached to dev */
  bool keep_dcr{};           /**< Do not free dcr in release_dcr */
  AutoXflateMode autodeflate{AutoXflateMode::IO_DIRECTION_NONE};
  AutoXflateMode autoinflate{AutoXflateMode::IO_DIRECTION_NONE};
  uint32_t VolFirstIndex{};        /**< First file index this Volume */
  uint32_t VolLastIndex{};         /**< Last file index this Volume */
  uint32_t FileIndex{};            /**< Current File Index */
  uint32_t EndFile{};              /**< End file written */
  uint32_t StartFile{};            /**< Start write file */
  uint32_t StartBlock{};           /**< Start write block */
  uint32_t EndBlock{};             /**< Ending block written */
  int64_t VolMediaId{};            /**< MediaId */
  int64_t job_spool_size{};        /**< Current job spool size */
  int64_t max_job_spool_size{};    /**< Max job spool size */
  uint32_t VolMinBlocksize{};      /**< Minimum Blocksize */
  uint32_t VolMaxBlocksize{};      /**< Maximum Blocksize */
  char VolumeName[MAX_NAME_LENGTH]{}; /**< Volume name */
  char pool_name[MAX_NAME_LENGTH]{};  /**< Pool name */
  char pool_type[MAX_NAME_LENGTH]{};  /**< Pool type */
  char media_type[MAX_NAME_LENGTH]{}; /**< Media type */
  char dev_name[MAX_NAME_LENGTH]{};   /**< Dev name */
  int Copy{};                         /**< Identical copy number */
  int Stripe{};                       /**< RAIT stripe */
  VolumeCatalogInfo VolCatInfo;       /**< Catalog info for desired volume */

  DeviceControlRecord();
  virtual ~DeviceControlRecord() = default;

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

  // Methods in askdir.c
  virtual DeviceControlRecord* get_new_spooling_dcr();
  virtual bool DirFindNextAppendableVolume() { return true; }
  virtual bool DirUpdateVolumeInfo([[maybe_unused]] bool label,[[maybe_unused]]  bool update_LastWritten)
  {
    return true;
  }
  virtual bool DirCreateJobmediaRecord([[maybe_unused]] bool zero) { return true; }
  virtual bool DirUpdateFileAttributes([[maybe_unused]] DeviceRecord* record) { return true; }
  virtual bool DirAskSysopToMountVolume(int mode);
  virtual bool DirAskSysopToCreateAppendableVolume() { return true; }
  virtual bool DirGetVolumeInfo(enum get_vol_info_rw writing);
  virtual bool DirAskToUpdateFileList() { return true;}
  virtual bool DirAskToUpdateJobRecord() { return true;}

  // Methods in lock.c
  void dblock(int why);
  void mLock(bool locked);
  void mUnlock();

  // Methods in record.c
  bool WriteRecord();

  // Methods in reserve.c
  void ClearReserved();
  void SetReserved();
  void UnreserveDevice();

  // Methods in vol_mgr.c
  bool Can_i_use_volume();
  bool Can_i_write_volume();

  // Methods in mount.c
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

  // Methods in block.c
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

  // Methods in label.c
  bool RewriteVolumeLabel(bool recycle);
};
/* clang-format on */

}  // namespace storagedaemon
#endif  // BAREOS_STORED_DEVICE_CONTROL_RECORD_H_
