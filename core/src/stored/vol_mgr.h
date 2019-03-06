/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2013 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
*/
/*
 * Pulled out of dev.h
 *
 * Kern Sibbald, MMXIII
 *
 */
/**
 * @file
 * volume management definitions
 *
 */

/*
 * Some details of how volume reservations work
 *
 * class VolumeReservationItem:
 *   SetInUse()     volume being used on current drive
 *   ClearInUse()   no longer being used.  Can be re-used or moved.
 *   SetSwapping()   set volume being moved to another drive
 *   IsSwapping()    volume is being moved to another drive
 *   ClearSwapping() volume normal
 *
 */

#ifndef BAREOS_STORED_VOL_MGR_H_
#define BAREOS_STORED_VOL_MGR_H_ 1

class dlist;

namespace storagedaemon {

class VolumeReservationItem;
VolumeReservationItem* vol_walk_start();
VolumeReservationItem* VolWalkNext(VolumeReservationItem* prev_vol);
void VolWalkEnd(VolumeReservationItem* vol);
VolumeReservationItem* read_vol_walk_start();
VolumeReservationItem* ReadVolWalkNext(VolumeReservationItem* prev_vol);
void ReadVolWalkEnd(VolumeReservationItem* vol);

/**
 * Volume reservation class -- see vol_mgr.c and reserve.c
 */
class VolumeReservationItem {
  bool swapping_;              /**< set when swapping to another drive */
  bool in_use_;                /**< set when volume reserved or in use */
  bool reading_;               /**< set when reading */
  slot_number_t slot_;         /**< slot of swapping volume */
  uint32_t JobId_;             /**< JobId for read volumes */
  volatile int32_t use_count_; /**< Use count */
  pthread_mutex_t mutex_;      /**< Vol muntex */
 public:
  dlink link;
  char* vol_name; /**< Volume name */
  Device* dev;    /**< Pointer to device to which we are attached */

  void InitMutex() { pthread_mutex_init(&mutex_, NULL); }
  void DestroyMutex() { pthread_mutex_destroy(&mutex_); }
  void Lock() { P(mutex_); }
  void Unlock() { V(mutex_); }
  void IncUseCount(void)
  {
    P(mutex_);
    use_count_++;
    V(mutex_);
  }
  void DecUseCount(void)
  {
    P(mutex_);
    use_count_--;
    V(mutex_);
  }
  int32_t UseCount() const { return use_count_; }
  bool IsSwapping() const { return swapping_; }
  bool is_reading() const { return reading_; }
  bool IsWriting() const { return !reading_; }
  void SetReading() { reading_ = true; }
  void clear_reading() { reading_ = false; }
  void SetSwapping() { swapping_ = true; }
  void ClearSwapping() { swapping_ = false; }
  bool IsInUse() const { return in_use_; }
  void SetInUse() { in_use_ = true; }
  void ClearInUse() { in_use_ = false; }
  void SetSlot(slot_number_t slot) { slot_ = slot; }
  void ClearSlot() { slot_ = -1; }
  slot_number_t GetSlot() const { return slot_; }
  uint32_t GetJobid() const { return JobId_; }
  void SetJobid(uint32_t JobId) { JobId_ = JobId; }
};

#define foreach_vol(vol) \
  for (vol = vol_walk_start(); vol; (vol = VolWalkNext(vol)))

#define endeach_vol(vol) VolWalkEnd(vol)

#define foreach_read_vol(vol) \
  for (vol = read_vol_walk_start(); vol; (vol = ReadVolWalkNext(vol)))

#define endeach_read_vol(vol) ReadVolWalkEnd(vol)

void InitVolListLock();
void TermVolListLock();
VolumeReservationItem* reserve_volume(DeviceControlRecord* dcr,
                                      const char* VolumeName);
bool FreeVolume(Device* dev);
bool IsVolListEmpty();
dlist* dup_vol_list(JobControlRecord* jcr);
void FreeTempVolList(dlist* temp_vol_list);
bool VolumeUnused(DeviceControlRecord* dcr);
void CreateVolumeLists();
void FreeVolumeLists();
bool IsVolumeInUse(DeviceControlRecord* dcr);
void AddReadVolume(JobControlRecord* jcr, const char* VolumeName);
void RemoveReadVolume(JobControlRecord* jcr, const char* VolumeName);

} /* namespace storagedaemon */

#endif
