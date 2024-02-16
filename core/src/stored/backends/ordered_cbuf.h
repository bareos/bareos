/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2017 Planets Communications B.V.
   Copyright (C) 2017-2022 Bareos GmbH & Co. KG

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
// Marco van Wieringen, December 2016.

// Ordered Circular buffer used for producer/consumer problem with pthread.

#ifndef BAREOS_STORED_BACKENDS_ORDERED_CBUF_H_
#define BAREOS_STORED_BACKENDS_ORDERED_CBUF_H_

#include <cstdint>
#include <pthread.h>
#include <lib/dlink.h>
template <typename T> class dlist;

#define OQSIZE 10 /* # of pointers in the queue */

namespace storagedaemon {

enum oc_peek_types
{
  PEEK_FIRST = 0,
  PEEK_LAST,
  PEEK_LIST,
  PEEK_CLONE
};

struct ocbuf_item {
  dlink<ocbuf_item> link;
  uint32_t data_size = 0;
  void* data = nullptr;
};

class ordered_circbuf {
 private:
  int size_ = 0;
  int capacity_ = 0;
  int reserved_ = 0;
  bool flush_ = false;
  pthread_mutex_t lock_ = PTHREAD_MUTEX_INITIALIZER; /* Lock the structure */
  pthread_cond_t notfull_
      = PTHREAD_COND_INITIALIZER; /* Full -> not full condition */
  pthread_cond_t notempty_
      = PTHREAD_COND_INITIALIZER;     /* Empty -> not empty condition */
  dlist<ocbuf_item>* data_ = nullptr; /* Circular buffer of pointers */

 public:
  ordered_circbuf(int capacity = OQSIZE);
  ~ordered_circbuf();
  int init(int capacity);
  void destroy();
  void* enqueue(void* data,
                uint32_t data_size,
                int compare(ocbuf_item*, ocbuf_item*),
                void update(void*, void*),
                bool use_reserved_slot = false,
                bool no_signal = false);
  void* dequeue(bool reserve_slot = false,
                bool requeued = false,
                struct timespec* ts = NULL,
                int timeout = 300);
  void* peek(enum oc_peek_types type,
             void* data,
             int callback(void* item1, void* item2));
  int unreserve_slot();
  int flush();
  bool full() { return size_ == (capacity_ - reserved_); }
  bool empty() { return size_ == 0; }
  bool IsFlushing() { return flush_; }
  int capacity() const { return capacity_; }
};

// Constructor
inline ordered_circbuf::ordered_circbuf(int capacity) { init(capacity); }

// Destructor
inline ordered_circbuf::~ordered_circbuf() { destroy(); }
}  // namespace storagedaemon

#endif  // BAREOS_STORED_BACKENDS_ORDERED_CBUF_H_
