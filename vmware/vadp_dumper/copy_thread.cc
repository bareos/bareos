/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2019 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.vadp.

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
 * Marco van Wieringen, August 2014.
 */

/*
 * Copy thread used for producer/consumer problem with pthreads.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>

#include <pthread.h>
#include "copy_thread.h"

static CP_THREAD_CTX* cp_thread;

/*
 * Copy thread cancel handler.
 */
static void copy_cleanup_thread(void* data)
{
  CP_THREAD_CTX* context = (CP_THREAD_CTX*)data;

  pthread_mutex_unlock(&context->lock);
}

/*
 * Actual copy thread that copies data.
 */
static void* copy_thread(void* data)
{
  CP_THREAD_SAVE_DATA* save_data;
  CP_THREAD_CTX* context = (CP_THREAD_CTX*)data;

  if (pthread_mutex_lock(&context->lock) != 0) { goto bail_out; }

  /*
   * When we get canceled make sure we run the cleanup function.
   */
  pthread_cleanup_push(copy_cleanup_thread, data);

  while (1) {
    size_t cnt;

    /*
     * Wait for the moment we are supposed to start.
     * We are signalled by the restore thread.
     */
    pthread_cond_wait(&context->start, &context->lock);
    context->started = true;

    pthread_mutex_unlock(&context->lock);

    /*
     * Dequeue an item from the circular buffer.
     */
    save_data = (CP_THREAD_SAVE_DATA*)context->cb->dequeue();

    while (save_data) {
      cnt = cp_thread->output_function(save_data->sector_offset,
                                       save_data->data_len, save_data->data);
      if (cnt < save_data->data_len) { return NULL; }
      save_data = (CP_THREAD_SAVE_DATA*)context->cb->dequeue();
    }

    /*
     * Need to synchronize the main thread and this one so the main thread
     * cannot miss the conditional signal.
     */
    if (pthread_mutex_lock(&context->lock) != 0) { goto bail_out; }

    /*
     * Signal the main thread we flushed the data.
     */
    pthread_cond_signal(&context->flush);

    context->started = false;
    context->flushed = true;
  }

  pthread_cleanup_pop(1);

bail_out:
  return NULL;
}

/*
 * Create a copy thread.
 */
bool setup_copy_thread(IO_FUNCTION* input_function,
                       IO_FUNCTION* output_function)
{
  int nr_save_elements;
  CP_THREAD_CTX* new_context;

  new_context = (CP_THREAD_CTX*)malloc(sizeof(CP_THREAD_CTX));
  new_context->started = false;
  new_context->flushed = false;
  new_context->cb = new circbuf;

  nr_save_elements = new_context->cb->capacity();
  new_context->save_data = (CP_THREAD_SAVE_DATA*)malloc(
      nr_save_elements * sizeof(CP_THREAD_SAVE_DATA));
  memset(new_context->save_data, 0,
         nr_save_elements * sizeof(CP_THREAD_SAVE_DATA));
  new_context->nr_save_elements = nr_save_elements;

  new_context->input_function = input_function;
  new_context->output_function = output_function;

  if (pthread_mutex_init(&new_context->lock, NULL) != 0) { goto bail_out; }

  if (pthread_cond_init(&new_context->start, NULL) != 0) {
    pthread_mutex_destroy(&new_context->lock);
    goto bail_out;
  }

  if (pthread_create(&new_context->thread_id, NULL, copy_thread,
                     (void*)new_context) != 0) {
    pthread_cond_destroy(&new_context->start);
    pthread_mutex_destroy(&new_context->lock);
    goto bail_out;
  }

  cp_thread = new_context;
  return true;

bail_out:
  free(new_context->save_data);
  delete new_context->cb;
  free(new_context);

  return false;
}

/*
 * Read a new piece of data via the input_function and put it onto the circular
 * buffer.
 */
bool send_to_copy_thread(size_t sector_offset, size_t nbyte)
{
  int slotnr;
  circbuf* cb = cp_thread->cb;
  CP_THREAD_SAVE_DATA* save_data;

  /*
   * Find out which next slot will be used on the Circular Buffer.
   * The method will block when the circular buffer is full until a slot is
   * available.
   */
  slotnr = cb->next_slot();
  save_data = &cp_thread->save_data[slotnr];

  /*
   * If this is the first time we use this slot we need to allocate some memory.
   */
  if (!save_data->data) { save_data->data = malloc(nbyte + 1); }

  save_data->data_len =
      cp_thread->input_function(sector_offset, nbyte, save_data->data);
  if (save_data->data_len < 0) { return false; }
  save_data->sector_offset = sector_offset;

  cb->enqueue(save_data);

  /*
   * Signal the copy thread its time to start if it didn't start yet.
   */
  if (!cp_thread->started) { pthread_cond_signal(&cp_thread->start); }

  return true;
}

/*
 * Flush the copy thread.
 */
void flush_copy_thread()
{
  CP_THREAD_CTX* context = cp_thread;

  if (pthread_mutex_lock(&context->lock) != 0) { return; }

  /*
   * In essence the flush should work in one shot but be a bit more
   * conservative.
   */
  while (!context->flushed) {
    /*
     * Tell the copy thread to flush out all data.
     */
    context->cb->flush();

    /*
     * Wait for the copy thread to say it flushed the data out.
     */
    pthread_cond_wait(&context->flush, &context->lock);
  }

  context->flushed = false;

  pthread_mutex_unlock(&context->lock);
}

/*
 * Cleanup all data allocated for the copy thread.
 */
void cleanup_copy_thread()
{
  int slotnr;

  /*
   * Stop the copy thread.
   */
  if (!pthread_equal(cp_thread->thread_id, pthread_self())) {
    pthread_cancel(cp_thread->thread_id);
    pthread_join(cp_thread->thread_id, NULL);
  }

  /*
   * Free all data allocated along the way.
   */
  for (slotnr = 0; slotnr < cp_thread->nr_save_elements; slotnr++) {
    if (cp_thread->save_data[slotnr].data) {
      free(cp_thread->save_data[slotnr].data);
    }
  }
  free(cp_thread->save_data);
  delete cp_thread->cb;
  free(cp_thread);
  cp_thread = NULL;
}
