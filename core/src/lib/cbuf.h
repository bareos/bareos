/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, August 2013.
 */
/**
 * @file
 * Circular buffer used for producer/consumer problem with pthread.
 */


#define QSIZE 10              /**< # of pointers in the queue */

class DLL_IMP_EXP circbuf : public SmartAlloc {
   int size_;
   int next_in_;
   int next_out_;
   int capacity_;
   bool flush_;
   pthread_mutex_t lock_;    /**< Lock the structure */
   pthread_cond_t notfull_;  /**< Full -> not full condition */
   pthread_cond_t notempty_; /**< Empty -> not empty condition */
   void **data_;             /**< Circular buffer of pointers */

public:
   circbuf(int capacity = QSIZE);
   ~circbuf();
   int init(int capacity);
   void destroy();
   int enqueue(void *data);
   void *dequeue();
   int NextSlot();
   int flush();
   bool full() { return size_ == capacity_; }
   bool empty() { return size_ == 0; }
   bool IsFlushing() { return flush_; }
   int capacity() const { return capacity_; }
};

/**
 * Constructor
 */
inline circbuf::circbuf(int capacity)
{
   data_ = NULL;
   init(capacity);
}

/**
 * Destructor
 */
inline circbuf::~circbuf()
{
   destroy();
}
