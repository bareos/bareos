/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
// Marco van Wieringen, August 2013.
/**
 * @file
 * Circular buffer used for producer/consumer problem with pthread.
 */

#ifndef BAREOS_LIB_CBUF_H_
#define BAREOS_LIB_CBUF_H_

#define QSIZE 10 /**< # of pointers in the queue */

class CircularBuffer {
  int size_ = 0;
  int next_in_ = 0;
  int next_out_ = 0;
  int capacity_ = 0;
  bool flush_ = 0;
  pthread_mutex_t lock_ = PTHREAD_MUTEX_INITIALIZER; /**< Lock the structure */
  pthread_cond_t notfull_
      = PTHREAD_COND_INITIALIZER; /**< Full -> not full condition */
  pthread_cond_t notempty_
      = PTHREAD_COND_INITIALIZER; /**< Empty -> not empty condition */
  void** data_ = nullptr;         /**< Circular buffer of pointers */

 public:
  CircularBuffer(int capacity = QSIZE);
  ~CircularBuffer();
  int init(int capacity);
  void destroy();
  int enqueue(void* data);
  void* dequeue();
  int NextSlot();
  int flush();
  bool full() { return size_ == capacity_; }
  bool empty() { return size_ == 0; }
  bool IsFlushing() { return flush_; }
  int capacity() const { return capacity_; }
};

// Constructor
inline CircularBuffer::CircularBuffer(int capacity) { init(capacity); }

// Destructor
inline CircularBuffer::~CircularBuffer() { destroy(); }

#endif  // BAREOS_LIB_CBUF_H_
