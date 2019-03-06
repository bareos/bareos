/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2017 Planets Communications B.V.

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
 * Marco van Wieringen, December 2016.
 */

/*
 * Ordered Circular buffer used for producer/consumer problem with pthreads.
 */
#include "include/bareos.h"
#include "lib/dlist.h"

#include "ordered_cbuf.h"
namespace storagedaemon {


/*
 * Initialize a new ordered circular buffer.
 */
int ordered_circbuf::init(int capacity)
{
  struct ocbuf_item* item = NULL;

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

  size_ = 0;
  capacity_ = capacity;
  reserved_ = 0;
  if (data_) {
    data_->destroy();
    delete data_;
  }
  data_ = New(dlist(item, &item->link));

  return 0;
}

/*
 * Destroy a ordered circular buffer.
 */
void ordered_circbuf::destroy()
{
  pthread_cond_destroy(&notempty_);
  pthread_cond_destroy(&notfull_);
  pthread_mutex_destroy(&lock_);
  if (data_) {
    data_->destroy();
    delete data_;
  }
}

/*
 * Enqueue a new item into the ordered circular buffer.
 */
void* ordered_circbuf::enqueue(void* data,
                               uint32_t data_size,
                               int compare(void* item1, void* item2),
                               void update(void* item1, void* item2),
                               bool use_reserved_slot,
                               bool no_signal)
{
  struct ocbuf_item *new_item, *item;

  if (pthread_mutex_lock(&lock_) != 0) { return NULL; }

  /*
   * See if we should use a reserved slot and there are actually slots reserved.
   */
  if (!use_reserved_slot || !reserved_) {
    /*
     * Wait while the buffer is full.
     */
    while (full()) { pthread_cond_wait(&notfull_, &lock_); }
  }

  /*
   * Decrease the number of reserved slots if we should use a reserved slot.
   * We do this even when we don't really add a new item to the ordered
   * circular list to keep the reserved slot counting consistent.
   */
  if (use_reserved_slot) { reserved_--; }

  /*
   * Binary insert the data into the ordered circular buffer. If the
   * item returned is not our new_item it means there is already an
   * entry with the same keys on the ordered circular list. We then
   * just call the update function callback which should perform the
   * right actions to update the already existing item with the new
   * data in the new item. The compare function callback is used to binary
   * insert the item at the right location in the ordered circular list.
   */
  new_item = (struct ocbuf_item*)malloc(sizeof(struct ocbuf_item));
  new_item->data = data;
  new_item->data_size = data_size;

  item = (struct ocbuf_item*)data_->binary_insert(new_item, compare);
  if (item == new_item) {
    size_++;
  } else {
    /*
     * Update the data on the ordered circular list with the new data.
     * e.g. replace the old with the new data but don't allocate a new
     * item on the ordered circular list.
     */
    update(item->data, new_item->data);

    /*
     * Release the unused ocbuf_item.
     */
    free(new_item);

    /*
     * Update data to point to the data that was attached to the original
     * ocbuf_item.
     */
    data = item->data;
  }

  /*
   * See if we need to signal any workers that work is available or not.
   */
  if (!no_signal) {
    /*
     * Let any waiting consumer know there is data.
     */
    pthread_cond_broadcast(&notempty_);
  }

  pthread_mutex_unlock(&lock_);

  /*
   * Return the data that is current e.g. either the new data passed in or
   * the already existing data on the ordered circular list.
   */
  return data;
}

/*
 * Dequeue an item from the ordered circular buffer.
 */
void* ordered_circbuf::dequeue(bool reserve_slot,
                               bool requeued,
                               struct timespec* ts,
                               int timeout)
{
  void* data = NULL;
  struct ocbuf_item* item;

  if (pthread_mutex_lock(&lock_) != 0) { return NULL; }

  /*
   * Wait while there is nothing in the buffer
   */
  while ((requeued || empty()) && !flush_) {
    /*
     * The requeued state is only valid one time so clear it.
     */
    requeued = false;

    /*
     * See if we should block indefinitely or wake up
     * after the given timer has expired and calculate
     * the next time we need to wakeup. This way we check
     * after the timer expired if there is work to be done
     * this is something we need if the worker threads can
     * put work back onto the circular queue and uses
     * enqueue with the no_signal flag set.
     */
    if (ts) {
      pthread_cond_timedwait(&notempty_, &lock_, ts);

      /*
       * See if there is really work to be done.
       * We could be woken by the broadcast but some other iothread
       * could take the work as we have to wait to reacquire the lock_.
       * Only one thread will be in the critical section and be able to
       * hold the lock.
       */
      if (empty() && !flush_) {
        struct timeval tv;
        struct timezone tz;

        /*
         * Calculate the next absolute timeout if we find
         * out there is no work to be done.
         */
        gettimeofday(&tv, &tz);
        ts->tv_nsec = tv.tv_usec * 1000;
        ts->tv_sec = tv.tv_sec + timeout;

        continue;
      }
    } else {
      pthread_cond_wait(&notempty_, &lock_);

      /*
       * See if there is really work to be done.
       * We could be woken by the broadcast but some other iothread
       * could take the work as we have to wait to reacquire the lock_.
       * Only one thread will be in the critical section and be able to
       * hold the lock.
       */
      if (empty() && !flush_) { continue; }
    }
  }

  /*
   * When we are requested to flush and there is no data left return NULL.
   */
  if (empty() && flush_) { goto bail_out; }

  /*
   * Get the first item from the dlist and remove it.
   */
  item = (struct ocbuf_item*)data_->first();
  if (!item) { goto bail_out; }

  data_->remove(item);
  size_--;

  /*
   * Let all waiting producers know there is room.
   */
  pthread_cond_broadcast(&notfull_);

  /*
   * Extract the payload and drop the placeholder.
   */
  data = item->data;
  free(item);

  /*
   * Increase the reserved slot count when we are asked to reserve the slot.
   */
  if (reserve_slot) { reserved_++; }

bail_out:
  pthread_mutex_unlock(&lock_);

  return data;
}

/*
 * Peek on the buffer for a certain item.
 * We return a copy of the data on the ordered circular buffer.
 * Any pointers in that data may become invallid after its returned
 * to the calling function. As such you should not rely on the data.
 */
void* ordered_circbuf::peek(enum oc_peek_types type,
                            void* data,
                            int callback(void* item1, void* item2))
{
  void* retval = NULL;
  struct ocbuf_item* item;

  if (pthread_mutex_lock(&lock_) != 0) { return NULL; }

  /*
   * There is nothing to be seen on an empty ordered circular buffer.
   */
  if (empty()) { goto bail_out; }

  /*
   * Depending on the peek type start somewhere on the ordered list and
   * walk forward or back.
   */
  switch (type) {
    case PEEK_FIRST:
      item = (struct ocbuf_item*)data_->first();
      while (item) {
        if (callback(item->data, data) == 0) {
          retval = malloc(item->data_size);
          memcpy(retval, item->data, item->data_size);
          goto bail_out;
        }

        item = (struct ocbuf_item*)data_->next(item);
      }
      break;
    case PEEK_LAST:
      item = (struct ocbuf_item*)data_->last();
      while (item) {
        if (callback(item->data, data) == 0) {
          retval = malloc(item->data_size);
          memcpy(retval, item->data, item->data_size);
          goto bail_out;
        }

        item = (struct ocbuf_item*)data_->prev(item);
      }
      break;
    case PEEK_LIST:
      item = (struct ocbuf_item*)data_->first();
      while (item) {
        callback(item->data, data);
        item = (struct ocbuf_item*)data_->next(item);
      }
      break;
    case PEEK_CLONE:
      item = (struct ocbuf_item*)data_->first();
      while (item) {
        if (callback(item->data, data) == 0) {
          retval = data;
          break;
        }
        item = (struct ocbuf_item*)data_->next(item);
      }
      break;
    default:
      goto bail_out;
  }

bail_out:
  pthread_mutex_unlock(&lock_);

  return retval;
}

/*
 * Unreserve a slot which was reserved by dequeue().
 */
int ordered_circbuf::unreserve_slot()
{
  int retval = -1;

  if (pthread_mutex_lock(&lock_) != 0) { goto bail_out; }

  /*
   * Make sure any slots are still reserved. Otherwise people
   * are playing games and should pay the price for doing so.
   */
  if (reserved_) {
    reserved_--;

    /*
     * Let all waiting producers know there is room.
     */
    pthread_cond_broadcast(&notfull_);

    retval = 0;
  }
  pthread_mutex_unlock(&lock_);

bail_out:
  return retval;
}

/*
 * Flush the ordered circular buffer.
 * Any waiting consumer will be wakened and will see we are in flush state.
 */
int ordered_circbuf::flush()
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
} /* namespace storagedaemon */
