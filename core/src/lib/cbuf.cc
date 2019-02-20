/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
 * Marco van Wieringen, August 2013.
 */

/*
 * Circular buffer used for producer/consumer problem with pthreads.
 */
#include "include/bareos.h"
#include "cbuf.h"

/*
 * Initialize a new circular buffer.
 */
int CircularBuffer::init(int capacity)
{
  if (pthread_mutex_init(&lock_, NULL) != 0) { return -1; }

  if (pthread_cond_init(&notfull_, NULL) != 0) {
    pthread_mutex_destroy(&lock_);
    return -1;
  }

  if (pthread_cond_init(&notempty_, NULL) != 0) {
    pthread_cond_destroy(&notfull_);
    pthread_mutex_destroy(&lock_);
    return -1;
  }

  next_in_ = 0;
  next_out_ = 0;
  size_ = 0;
  capacity_ = capacity;
  if (data_) { free(data_); }
  data_ = (void**)malloc(capacity_ * sizeof(void*));

  return 0;
}

/*
 * Destroy a circular buffer.
 */
void CircularBuffer::destroy()
{
  pthread_cond_destroy(&notempty_);
  pthread_cond_destroy(&notfull_);
  pthread_mutex_destroy(&lock_);
  if (data_) {
    free(data_);
    data_ = NULL;
  }
}

/*
 * Enqueue a new item into the circular buffer.
 */
int CircularBuffer::enqueue(void* data)
{
  if (pthread_mutex_lock(&lock_) != 0) { return -1; }

  /*
   * Wait while the buffer is full.
   */
  while (full()) { pthread_cond_wait(&notfull_, &lock_); }
  data_[next_in_++] = data;
  size_++;
  next_in_ %= capacity_;

  /*
   * Let any waiting consumer know there is data.
   */
  pthread_cond_broadcast(&notempty_);

  pthread_mutex_unlock(&lock_);

  return 0;
}

/*
 * Dequeue an item from the circular buffer.
 */
void* CircularBuffer::dequeue()
{
  void* data = NULL;

  if (pthread_mutex_lock(&lock_) != 0) { return NULL; }

  /*
   * Wait while there is nothing in the buffer
   */
  while (empty() && !flush_) { pthread_cond_wait(&notempty_, &lock_); }

  /*
   * When we are requested to flush and there is no data left return NULL.
   */
  if (empty() && flush_) { goto bail_out; }

  data = data_[next_out_++];
  size_--;
  next_out_ %= capacity_;

  /*
   * Let all waiting producers know there is room.
   */
  pthread_cond_broadcast(&notfull_);

bail_out:
  pthread_mutex_unlock(&lock_);

  return data;
}

/*
 * Make sure there is a free next slot available on the circular buffer.
 * So the next enqueue will not block but we block now until one is available.
 */
int CircularBuffer::NextSlot()
{
  if (pthread_mutex_lock(&lock_) != 0) { return -1; }

  /*
   * Wait while the buffer is full.
   */
  while (full()) { pthread_cond_wait(&notfull_, &lock_); }

  pthread_mutex_unlock(&lock_);

  return next_in_;
}

/*
 * Flush the circular buffer. Any waiting consumer will be wakened and will
 * see we are in flush state.
 */
int CircularBuffer::flush()
{
  if (pthread_mutex_lock(&lock_) != 0) { return -1; }

  /*
   * Set the flush flag.
   */
  flush_ = true;

  /*
   * Let all waiting consumers know there will be no more data.
   */
  pthread_cond_broadcast(&notempty_);

  pthread_mutex_unlock(&lock_);

  return 0;
}
