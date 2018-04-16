/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2013 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 *   set_in_use()     volume being used on current drive
 *   clear_in_use()   no longer being used.  Can be re-used or moved.
 *   set_swapping()   set volume being moved to another drive
 *   is_swapping()    volume is being moved to another drive
 *   clear_swapping() volume normal
 *
 */

#ifndef __VOL_MGR_H
#define __VOL_MGR_H 1

class VolumeReservationItem;
VolumeReservationItem *vol_walk_start();
VolumeReservationItem *vol_walk_next(VolumeReservationItem *prev_vol);
void vol_walk_end(VolumeReservationItem *vol);
VolumeReservationItem *read_vol_walk_start();
VolumeReservationItem *read_vol_walk_next(VolumeReservationItem *prev_vol);
void read_vol_walk_end(VolumeReservationItem *vol);

/**
 * Volume reservation class -- see vol_mgr.c and reserve.c
 */
class VolumeReservationItem {
   bool swapping_;                   /**< set when swapping to another drive */
   bool in_use_;                     /**< set when volume reserved or in use */
   bool reading_;                    /**< set when reading */
   slot_number_t slot_;              /**< slot of swapping volume */
   uint32_t JobId_;                  /**< JobId for read volumes */
   volatile int32_t use_count_;      /**< Use count */
   pthread_mutex_t mutex_;           /**< Vol muntex */
public:
   dlink link;
   char *vol_name;                    /**< Volume name */
   Device *dev;                       /**< Pointer to device to which we are attached */

   void init_mutex() { pthread_mutex_init(&mutex_, NULL); }
   void destroy_mutex() { pthread_mutex_destroy(&mutex_); }
   void Lock() { P(mutex_); }
   void Unlock() { V(mutex_); }
   void inc_use_count(void) {P(mutex_); use_count_++; V(mutex_); }
   void dec_use_count(void) {P(mutex_); use_count_--; V(mutex_); }
   int32_t use_count() const { return use_count_; }
   bool is_swapping() const { return swapping_; }
   bool is_reading() const { return reading_; }
   bool is_writing() const { return !reading_; }
   void set_reading() { reading_ = true; }
   void clear_reading() { reading_ = false; }
   void set_swapping() { swapping_ = true; }
   void clear_swapping() { swapping_ = false; }
   bool is_in_use() const { return in_use_; }
   void set_in_use() { in_use_ = true; }
   void clear_in_use() { in_use_ = false; }
   void set_slot(slot_number_t slot) { slot_ = slot; }
   void clear_slot() { slot_ = -1; }
   slot_number_t get_slot() const { return slot_; }
   uint32_t get_jobid() const { return JobId_; }
   void set_jobid(uint32_t JobId) { JobId_ = JobId; }
};

#define foreach_vol(vol) \
   for (vol=vol_walk_start(); vol; (vol = vol_walk_next(vol)) )

#define endeach_vol(vol) vol_walk_end(vol)

#define foreach_read_vol(vol) \
   for (vol=read_vol_walk_start(); vol; (vol = read_vol_walk_next(vol)) )

#define endeach_read_vol(vol) read_vol_walk_end(vol)

#endif
