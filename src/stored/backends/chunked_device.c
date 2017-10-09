/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2017 Planets Communications B.V.

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
 * Chunked volume device abstraction.
 *
 * Marco van Wieringen, February 2015
 */

#include "bareos.h"

#if defined(HAVE_OBJECTSTORE)
#include "stored.h"
#include "chunked_device.h"

#ifdef HAVE_MMAP
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#endif

/*
 * This implements a device abstraction that provides so called chunked
 * volumes. These chunks are kept in memory and flushed to the backing
 * store when requested. This class fully abstracts the chunked volumes
 * for the upper level device. The stacking for this device type is:
 *
 * <actual_device_type>::
 *          |
 *          v
 *   chunked_device::
 *          |
 *          v
 *       DEVICE::
 *
 * The public interfaces exported from this device are:
 *
 * setup_chunk() - Setup a chunked volume for reading or writing.
 * read_chunked() - Read a chunked volume.
 * write_chunked() - Write a chunked volume.
 * close_chunk() - Close a chunked volume.
 * truncate_chunked_volume() - Truncate a chunked volume.
 * chunked_volume_size() - Get the current size of a volume.
 * load_chunk() - Make sure we have the right chunk in memory.
 *
 * It also demands that the inheriting class implements the
 * following methods:
 *
 * flush_remote_chunk() - Flush a chunk to the remote backing store.
 * read_remote_chunk() - Read a chunk from the remote backing store.
 * chunked_remote_volume_size - Return the current size of a volume.
 * truncate_remote_chunked_volume() - Truncate a chunked volume on the
 *                                    remote backing store.
 */

/*
 * Actual thread runner that processes IO request from circular buffer.
 */
static void *io_thread(void *data)
{
   char ed1[50];
   chunked_device *dev = (chunked_device *)data;

   /*
    * Dequeue from the circular buffer until we are done.
    */
   while (1) {
      if (!dev->dequeue_chunk()) {
         break;
      }
   }

   Dmsg1(100, "Stopping IO-thread threadid=%s\n",
         edit_pthread(pthread_self(), ed1, sizeof(ed1)));

   return NULL;
}

/*
 * Allocate a new chunk buffer.
 */
char *chunked_device::allocate_chunkbuffer()
{
   char *buffer = NULL;

#ifdef HAVE_MMAP
   if (m_use_mmap) {
      buffer = (char *)::mmap(NULL, m_current_chunk->chunk_size,
                              (PROT_READ | PROT_WRITE),
                              (MAP_SHARED | MAP_ANONYMOUS),
                              -1, 0);
      Dmsg1(100, "Mapped %ld bytes for chunk buffer\n", m_current_chunk->chunk_size);
   } else {
#endif
      buffer = (char *)malloc(m_current_chunk->chunk_size);
#ifdef HAVE_MMAP
   }
#endif

   Dmsg2(100, "New allocated buffer of %d bytes at %p\n", m_current_chunk->chunk_size, buffer);

   return buffer;
}

/*
 * Free a chunk buffer.
 */
void chunked_device::free_chunkbuffer(char *buffer)
{
   Dmsg2(100, "Freeing buffer of %d bytes at %p\n", m_current_chunk->chunk_size, buffer);

#ifdef HAVE_MMAP
   if (m_use_mmap) {
      ::munmap(buffer, m_current_chunk->chunk_size);
      Dmsg1(100, "Unmapped %ld bytes used as chunk buffer\n", m_current_chunk->chunk_size);
   } else {
#endif
      free(buffer);

      /*
       * As we released a big memory chunk let the garbage collector run.
       */
      garbage_collect_memory();
#ifdef HAVE_MMAP
   }
#endif
}

/*
 * Free a chunk_io_request.
 */
void chunked_device::free_chunk_io_request(chunk_io_request *request)
{
   Dmsg2(100, "Freeing chunk io request of %d bytes at %p\n", sizeof(chunk_io_request), request);

   if (request->release) {
      free_chunkbuffer(request->buffer);
   }
   free((void *)request->volname);
   free(request);
}

/*
 * Start the io-threads that are used for uploading.
 */
bool chunked_device::start_io_threads()
{
   char ed1[50];
   uint8_t thread_nr;
   pthread_t thread_id;
   thread_handle *handle;

   /*
    * Create a new ordered circular buffer for exchanging chunks between
    * the producer (the storage driver) and multiple consumers (io-threads).
    */
   if (m_io_slots) {
      m_cb = New(ordered_circbuf(m_io_threads * m_io_slots));
   } else {
      m_cb = New(ordered_circbuf(m_io_threads * OQSIZE));
   }

   /*
    * Start all IO threads and keep track of their thread ids in m_thread_ids.
    */
   if (!m_thread_ids) {
      m_thread_ids = New(alist(10, owned_by_alist));
   }

   for (thread_nr = 1; thread_nr <= m_io_threads; thread_nr++) {
      if (pthread_create(&thread_id, NULL, io_thread, (void *)this)) {
         return false;
      }

      handle = (thread_handle *)malloc(sizeof(thread_handle));
      memset(handle, 0, sizeof(thread_handle));
      handle->type = WAIT_JOIN_THREAD;
      memcpy(&handle->thread_id, &thread_id, sizeof(pthread_t));
      m_thread_ids->append(handle);

      Dmsg1(100, "Started new IO-thread threadid=%s\n",
            edit_pthread(thread_id, ed1, sizeof(ed1)));
   }

   m_io_threads_started = true;

   return true;
}

/*
 * Stop the io-threads that are used for uploading.
 */
void chunked_device::stop_threads()
{
   char ed1[50];
   thread_handle *handle;

   /*
    * Tell all IO threads that we flush the circular buffer.
    * As such they will get a NULL chunk_io_request back and exit.
    */
   m_cb->flush();

   /*
    * Wait for all threads to exit.
    */
   if (m_thread_ids) {
      foreach_alist(handle, m_thread_ids) {
         switch (handle->type) {
         case WAIT_CANCEL_THREAD:
            Dmsg1(100, "Canceling thread with threadid=%s\n",
                  edit_pthread(handle->thread_id, ed1, sizeof(ed1)));
            pthread_cancel(handle->thread_id);
            break;
         case WAIT_JOIN_THREAD:
            Dmsg1(100, "Waiting to join with threadid=%s\n",
                  edit_pthread(handle->thread_id, ed1, sizeof(ed1)));
            pthread_join(handle->thread_id, NULL);
            break;
         default:
            break;
         }
      }

      m_thread_ids->destroy();
      delete m_thread_ids;
      m_thread_ids = NULL;
   }
}

/*
 * Call back function for comparing two chunk_io_requests.
 */
static int compare_chunk_io_request(void *item1, void *item2)
{
   ocbuf_item *ocbuf1 = (ocbuf_item *)item1;
   ocbuf_item *ocbuf2 = (ocbuf_item *)item2;
   chunk_io_request *chunk1 = (chunk_io_request *)ocbuf1->data;
   chunk_io_request *chunk2 = (chunk_io_request *)ocbuf2->data;

   /*
    * Same volume name ?
    */
   if (bstrcmp(chunk1->volname, chunk2->volname)) {
      /*
       * Compare on chunk number.
       */
      if (chunk1->chunk == chunk2->chunk) {
         return 0;
      } else {
         return (chunk1->chunk < chunk2->chunk) ? -1 : 1;
      }
   } else {
      return strcmp(chunk1->volname, chunk2->volname);
   }
}

/*
 * Call back function for updating two chunk_io_requests.
 */
static void update_chunk_io_request(void *item1, void *item2)
{
   ocbuf_item *ocbuf1 = (ocbuf_item *)item1;
   ocbuf_item *ocbuf2 = (ocbuf_item *)item2;
   chunk_io_request *chunk1 = (chunk_io_request *)ocbuf1->data;
   chunk_io_request *chunk2 = (chunk_io_request *)ocbuf2->data;

   /*
    * See if the new chunk_io_request has more bytes then
    * the chunk_io_request currently on the ordered circular
    * buffer. We can only have multiple chunk_io_requests for
    * the same chunk of a volume when a chunk was not fully
    * filled by one backup Job and a next one writes data to
    * the chunk before its being flushed to backing store. This
    * means all pointers are the same only the wbuflen and the
    * release flag of the chunk_io_request differ. So we only
    * copy those two fields and not the others.
    */
   if (chunk2->buffer == chunk1->buffer &&
       chunk2->wbuflen > chunk1->wbuflen) {
      chunk1->wbuflen = chunk2->wbuflen;
      chunk1->release = chunk2->release;
   }
   chunk2->release = false;
}

/*
 * Enqueue a chunk flush request onto the ordered circular buffer.
 */
bool chunked_device::enqueue_chunk(chunk_io_request *request)
{
   chunk_io_request *new_request,
                    *enqueued_request;

   Dmsg2(100, "Enqueueing chunk %d of volume %s\n", request->chunk, request->volname);

   if (!m_io_threads_started) {
      if (!start_io_threads()) {
         return false;
      }
   }

   new_request = (chunk_io_request *)malloc(sizeof(chunk_io_request));
   memset(new_request, 0, sizeof(chunk_io_request));
   new_request->volname = bstrdup(request->volname);
   new_request->chunk = request->chunk;
   new_request->buffer = request->buffer;
   new_request->wbuflen = request->wbuflen;
   new_request->release = request->release;

   Dmsg2(100, "Allocated chunk io request of %d bytes at %p\n", sizeof(chunk_io_request), new_request);

   /*
    * Enqueue the item onto the ordered circular buffer.
    * This returns either the same request as we passed
    * in or the previous flush request for the same chunk.
    */
   enqueued_request = (chunk_io_request *)m_cb->enqueue(new_request,
                                                        sizeof(chunk_io_request),
                                                        compare_chunk_io_request,
                                                        update_chunk_io_request,
                                                        false, /* use_reserved_slot */
                                                        false /* no_signal */);

   /*
    * Compare the return value from the enqueue.
    */
   if (enqueued_request && enqueued_request != new_request) {
      free_chunk_io_request(new_request);
   }

   return (enqueued_request) ? true : false;
}

/*
 * Dequeue a chunk flush request from the ordered circular buffer and process it.
 */
bool chunked_device::dequeue_chunk()
{
   char ed1[50];
   struct timeval tv;
   struct timezone tz;
   struct timespec ts;
   bool requeued = false;
   chunk_io_request *new_request;

   /*
    * Loop while we are not done either due to the ordered circular buffer being flushed
    * some fatal error or successfully dequeueing a chunk flush request.
    */
   while (1) {
      /*
       * See if we are in the flushing state then we just return and exit the io-thread.
       */
      if (m_cb->is_flushing()) {
         return false;
      }

      /*
       * Calculate the next absolute timeout if we find out there is no work to be done.
       */
      gettimeofday(&tv, &tz);
      ts.tv_nsec = tv.tv_usec * 1000;
      ts.tv_sec = tv.tv_sec + DEFAULT_RECHECK_INTERVAL;

      /*
       * Dequeue the next item from the ordered circular buffer and reserve the slot as we
       * might need to put this item back onto the ordered circular buffer if we fail to
       * flush it to the remote backing store. Also let the dequeue wake up every
       * DEFAULT_RECHECK_INTERVAL seconds to retry failed previous uploads.
       */
      new_request = (chunk_io_request *)m_cb->dequeue(true, /* reserve_slot we may need to enqueue the request */
                                                      requeued, /* request is requeued due to failure ? */
                                                      &ts, DEFAULT_RECHECK_INTERVAL);
      if (!new_request) {
         return false;
      }

      Dmsg3(100, "Flushing chunk %d of volume %s by thread %s\n",
            new_request->chunk, new_request->volname,
            edit_pthread(pthread_self(), ed1, sizeof(ed1)));

      if (!flush_remote_chunk(new_request)) {
          chunk_io_request *enqueued_request;

         /*
          * We failed to flush the chunk to the backing store
          * so enqueue it again using the reserved slot by dequeue()
          * but don't signal the workers otherwise we would try uploading
          * the same chunk again and again by different io-threads.
          * As we set the requeued flag to the dequeue method on the ordered circular buffer
          * we will not try dequeueing any new item either until a new item is put
          * onto the ordered circular buffer or after the retry interval has expired.
          */
         Dmsg2(100, "Enqueueing chunk %d of volume %s for retry of upload later\n",
               new_request->chunk, new_request->volname);

         /*
          * Enqueue the item onto the ordered circular buffer.
          * This returns either the same request as we passed
          * in or the previous flush request for the same chunk.
          */
         enqueued_request = (chunk_io_request *)m_cb->enqueue(new_request,
                                                              sizeof(chunk_io_request),
                                                              compare_chunk_io_request,
                                                              update_chunk_io_request,
                                                              true, /* use_reserved_slot */
                                                              true /* no_signal */);
         /*
          * See if the enqueue succeeded.
          */
         if (!enqueued_request) {
            return false;
         }

         /*
          * Compare the return value from the enqueue against our new_request.
          * If it is different there was already a chunk io request for the
          * same chunk on the ordered circular buffer.
          */
         if (enqueued_request != new_request) {
            free_chunk_io_request(new_request);
         }

         requeued = true;
         continue;
      }

      /*
       * Unreserve the slot on the ordered circular buffer reserved by dequeue().
       */
      m_cb->unreserve_slot();

      /*
       * Processed the chunk so clean it up now.
       */
      free_chunk_io_request(new_request);

      return true;
   }
}

/*
 * Internal method for flushing a chunk to the backing store.
 * The retry logic is in the io-threads but if those are not
 * used we give this one try and otherwise drop the chunk and
 * return an IO error to the upper level callers. That way the
 * volume will go into error.
 */
bool chunked_device::flush_chunk(bool release_chunk, bool move_to_next_chunk)
{
   bool retval = false;
   chunk_io_request request;

   /*
    * Calculate in which chunk we are currently.
    */
   request.chunk = m_current_chunk->start_offset / m_current_chunk->chunk_size;
   request.volname = m_current_volname;
   request.buffer = m_current_chunk->buffer;
   request.wbuflen = m_current_chunk->buflen;
   request.release = release_chunk;

   if (m_io_threads) {
      retval = enqueue_chunk(&request);
   } else {
      retval = flush_remote_chunk(&request);
   }

   /*
    * Clear the need flushing flag.
    */
   m_current_chunk->need_flushing = false;

   /*
    * Change to the next chunk ?
    */
   if (move_to_next_chunk) {
      /*
       * If we enqueued the data we need to allocate a new buffer.
       */
      if (m_io_threads) {
         m_current_chunk->buffer = allocate_chunkbuffer();
      }
      m_current_chunk->start_offset += m_current_chunk->chunk_size;
      m_current_chunk->end_offset = m_current_chunk->start_offset + (m_current_chunk->chunk_size - 1);
      m_current_chunk->buflen = 0;
   } else {
      /*
       * If we enqueued the data we need to allocate a new buffer.
       */
      if (release_chunk && m_io_threads) {
         m_current_chunk->buffer = NULL;
      }
   }

   if (!retval) {
      Dmsg1(100, "%s", errmsg);
   }

   return retval;
}

/*
 * Internal method for reading a chunk from the backing store.
 */
bool chunked_device::read_chunk()
{
   chunk_io_request request;

   /*
    * Calculate in which chunk we are currently.
    */
   request.chunk = m_current_chunk->start_offset / m_current_chunk->chunk_size;
   request.volname = m_current_volname;
   request.buffer = m_current_chunk->buffer;
   request.wbuflen = m_current_chunk->chunk_size;
   request.rbuflen = &m_current_chunk->buflen;
   request.release = false;

   m_current_chunk->end_offset = m_current_chunk->start_offset + (m_current_chunk->chunk_size - 1);

   if (!read_remote_chunk(&request)) {
      /*
       * If the chunk doesn't exist on the backing store it has a size of 0 bytes.
       */
      m_current_chunk->buflen = 0;
      return false;
   }

   return true;
}

/*
 * Setup a chunked volume for reading or writing.
 */
void chunked_device::setup_chunk(int flags)
{
   if (!m_current_chunk) {
      m_current_chunk = (chunk_descriptor *)malloc(sizeof(chunk_descriptor));
      memset(m_current_chunk, 0, sizeof(chunk_descriptor));
      if (m_chunk_size > DEFAULT_CHUNK_SIZE) {
         m_current_chunk->chunk_size = m_chunk_size;
      } else {
         m_current_chunk->chunk_size = DEFAULT_CHUNK_SIZE;
      }
      m_current_chunk->start_offset = -1;
      m_current_chunk->end_offset = -1;
   }

   /*
    * Reopen of a device.
    */
   if (m_current_chunk->opened) {
      /*
       * Invalidate chunk.
       */
      m_current_chunk->buflen = 0;
      m_current_chunk->start_offset = -1;
      m_current_chunk->end_offset = -1;
   }

   if (flags & O_RDWR) {
      m_current_chunk->writing = true;
   }

   m_current_chunk->opened = true;
   m_current_chunk->chunk_setup = false;

   /*
    * We need to limit the maximum size of a chunked volume to MAX_CHUNKS * chunk_size).
    */
   if (max_volume_size == 0 || max_volume_size > (uint64_t)(MAX_CHUNKS * m_current_chunk->chunk_size)) {
      max_volume_size = MAX_CHUNKS * m_current_chunk->chunk_size;
   }

   /*
    * On open set begin offset to 0.
    */
   m_offset = 0;

   /*
    * On open we are no longer at the End of the Media.
    */
   m_end_of_media = false;

   /*
    * Keep track of the volume currently mounted.
    */
   if (m_current_volname) {
      free(m_current_volname);
   }

   m_current_volname = bstrdup(getVolCatName());
}

/*
 * Read a chunked volume.
 */
ssize_t chunked_device::read_chunked(int fd, void *buffer, size_t count)
{
   ssize_t retval = 0;

   if (m_current_chunk->opened) {
      ssize_t wanted_offset;
      ssize_t bytes_left;

      /*
       * Shortcut logic see if m_end_of_media is set then we are at the End of the Media
       */
      if (m_end_of_media) {
         goto bail_out;
      }

      /*
       * If we are starting reading without the chunk being setup it means we
       * are start reading at the beginning of the file otherwise the d_lseek method
       * would have read in the correct chunk.
       */
      if (!m_current_chunk->chunk_setup) {
         m_current_chunk->start_offset = 0;

         /*
          * See if we have to allocate a new buffer.
          */
         if (!m_current_chunk->buffer) {
            m_current_chunk->buffer = allocate_chunkbuffer();
         }

         if (!read_chunk()) {
            retval = -1;
            goto bail_out;
         }
         m_current_chunk->chunk_setup = true;
      }

      /*
       * See if we can fulfill the wanted read from the current chunk.
       */
      if (m_current_chunk->start_offset <= m_offset &&
          m_current_chunk->end_offset >= (boffset_t)((m_offset + count) - 1)) {
         wanted_offset = (m_offset % m_current_chunk->chunk_size);

         bytes_left = MIN((ssize_t)count, (m_current_chunk->buflen - wanted_offset));
         Dmsg2(200, "Reading %d bytes at offset %d from chunk buffer\n", bytes_left, wanted_offset);

         if (bytes_left < 0) {
            retval = -1;
            goto bail_out;
         }

         if (bytes_left > 0) {
            memcpy(buffer, m_current_chunk->buffer + wanted_offset, bytes_left);
         }
         m_offset += bytes_left;
         retval = bytes_left;
         goto bail_out;
      } else {
         ssize_t offset = 0;

         /*
          * We cannot fulfill the read from the current chunk, see how much
          * is available and return that and see if by reading the next chunk
          * we can fulfill the whole read.
          */
         while (retval < (ssize_t)count) {
            /*
             * See how much is left in this chunk.
             */
            wanted_offset = (m_offset % m_current_chunk->chunk_size);
            bytes_left = MIN((ssize_t)count, (m_current_chunk->buflen - wanted_offset));

            if (bytes_left > 0) {
               Dmsg2(200, "Reading %d bytes at offset %d from chunk buffer\n", bytes_left, wanted_offset);

               memcpy(buffer, m_current_chunk->buffer + wanted_offset, bytes_left);
               m_offset += bytes_left;
               offset += bytes_left;
               retval += bytes_left;
            }

            /*
             * Read in the next chunk.
             */
            m_current_chunk->start_offset += m_current_chunk->chunk_size;
            if (!read_chunk()) {
               switch (dev_errno) {
               case EIO:
                  /*
                   * If the are no more chunks to read we return only the bytes available.
                   * We also set m_end_of_media as we are at the end of media.
                   */
                  m_end_of_media = true;
                  goto bail_out;
               default:
                  retval = -1;
                  goto bail_out;
               }
            } else {
               bytes_left = MIN((boffset_t)(count - retval), m_current_chunk->buflen);

               if (bytes_left > 0) {
                  Dmsg2(200, "Reading %d bytes at offset %d from chunk buffer\n", bytes_left, 0);

                  memcpy((char *)buffer + offset, m_current_chunk->buffer, bytes_left);
                  m_offset += bytes_left;
                  retval += bytes_left;
               }
            }
         }
      }
   } else {
      errno = EBADF;
      retval = -1;
   }

bail_out:
   return retval;
}

/*
 * Write a chunked volume.
 */
ssize_t chunked_device::write_chunked(int fd, const void *buffer, size_t count)
{
   ssize_t retval = 0;

   if (m_current_chunk->opened) {
      ssize_t wanted_offset;

      /*
       * If we are starting writing without the chunk being setup it means we
       * are start writing to an empty file because otherwise the d_lseek method
       * would have read in the correct chunk.
       */
      if (!m_current_chunk->chunk_setup) {
         m_current_chunk->start_offset = 0;
         m_current_chunk->end_offset = (m_current_chunk->chunk_size - 1);
         m_current_chunk->buflen = 0;
         m_current_chunk->chunk_setup = true;

         /*
          * See if we have to allocate a new buffer.
          */
         if (!m_current_chunk->buffer) {
            m_current_chunk->buffer = allocate_chunkbuffer();
         }
      }

      /*
       * See if we can write the whole data inside the current chunk.
       */
      if (m_current_chunk->start_offset <= m_offset &&
          m_current_chunk->end_offset >= (boffset_t)((m_offset + count) - 1)) {

         wanted_offset = (m_offset % m_current_chunk->chunk_size);

         Dmsg2(200, "Writing %d bytes at offset %d in chunk buffer\n", count, wanted_offset);

         memcpy(m_current_chunk->buffer + wanted_offset, buffer, count);

         m_offset += count;
         if ((wanted_offset + count) > m_current_chunk->buflen) {
            m_current_chunk->buflen = wanted_offset + count;
         }
         m_current_chunk->need_flushing = true;
         retval = count;
      } else {
         ssize_t bytes_left;
         ssize_t offset = 0;

         /*
          * Things don't fit so first write as many bytes as can be written into
          * the current chunk and then flush it and write the next bytes into the
          * next chunk.
          */
         while (retval < (ssize_t)count) {
            /*
             * See how much is left in this chunk.
             */
            wanted_offset = (m_offset % m_current_chunk->chunk_size);
            bytes_left = ((m_current_chunk->end_offset - (m_current_chunk->start_offset + wanted_offset)) + 1);

            if (bytes_left > 0) {
               Dmsg2(200, "Writing %d bytes at offset %d in chunk buffer\n", bytes_left, wanted_offset);

               memcpy(m_current_chunk->buffer + wanted_offset, buffer, bytes_left);
               m_offset += bytes_left;
               if ((wanted_offset + bytes_left) > m_current_chunk->buflen) {
                  m_current_chunk->buflen = wanted_offset + bytes_left;
               }
               m_current_chunk->need_flushing = true;
               retval += bytes_left;

               /*
                * Keep track of the number of bytes we already consumed.
                */
               offset += bytes_left;
            }

            /*
             * Flush out the current chunk.
             */
            if (!flush_chunk(true /* release */, true /* move_to_next_chunk */)) {
               retval = -1;
               goto bail_out;
            }

            bytes_left = MIN((boffset_t)(count - retval), ((m_current_chunk->end_offset - m_current_chunk->start_offset) + 1));
            if (bytes_left > 0) {
               Dmsg2(200, "Writing %d bytes at offset %d in chunk buffer\n", bytes_left, 0);

               memcpy(m_current_chunk->buffer, (char *)buffer + offset, bytes_left);
               m_current_chunk->buflen = bytes_left;
               m_current_chunk->need_flushing = true;
               m_offset += bytes_left;
               retval += bytes_left;
            }
         }
      }
   } else {
      errno = EBADF;
      retval = -1;
   }

bail_out:
   return retval;
}

/*
 * Close a chunked volume.
 */
int chunked_device::close_chunk()
{
   int retval = -1;

   if (m_current_chunk->opened) {
      if (m_current_chunk->need_flushing) {
         if (flush_chunk(true /* release */, false /* move_to_next_chunk */)) {
            retval = 0;
         } else {
            dev_errno = EIO;
         }
      }

      /*
       * Invalidate chunk.
       */
      m_current_chunk->writing = false;
      m_current_chunk->opened = false;
      m_current_chunk->chunk_setup = false;
      m_current_chunk->buflen = 0;
      m_current_chunk->start_offset = -1;
      m_current_chunk->end_offset = -1;
   } else {
      errno = EBADF;
   }

   return retval;
}

/*
 * Truncate a chunked volume.
 */
bool chunked_device::truncate_chunked_volume(DCR *dcr)
{
   if (m_current_chunk->opened) {
      if (!truncate_remote_chunked_volume(dcr)) {
         return false;
      }

      /*
       * Reinitialize the initial chunk.
       */
      m_current_chunk->start_offset = 0;
      m_current_chunk->end_offset = (m_current_chunk->chunk_size - 1);
      m_current_chunk->buflen = 0;
      m_current_chunk->chunk_setup = true;
      m_current_chunk->need_flushing = false;

      /*
       * Reinitialize the volume name on a relabel we could get a new name.
       */
      if (m_current_volname) {
         free(m_current_volname);
      }

      m_current_volname = bstrdup(getVolCatName());
   }

   return true;
}

static int compare_volume_name(void *item1, void *item2)
{
   const char *volname = (const char *)item2;
   chunk_io_request *request = (chunk_io_request *)item1;

   return strcmp(request->volname, volname);
}

/*
 * Get the current size of a volume.
 */
ssize_t chunked_device::chunked_volume_size()
{
   /*
    * See if we are using io-threads or not and the ordered circbuf is created and not empty.
    */
   if (m_io_threads > 0 && m_cb && !m_cb->empty()) {
      char *volname;
      chunk_io_request *request;

      volname = getVolCatName();

      /*
       * Peek on the ordered circular queue if there are any pending IO-requests
       * for this volume. If there are use that as the indication of the size of
       * the volume and don't contact the remote storage as there is still data
       * inflight and as such we need to look at the last chunk that is still not
       * uploaded of the volume.
       */
      request = (chunk_io_request *)m_cb->peek(PEEK_LAST, volname, compare_volume_name);
      if (request) {
         ssize_t retval;

         /*
          * Calculate the size of the volume based on the last chunk inflight .
          */
         retval = (request->chunk * m_current_chunk->chunk_size) + request->wbuflen;

         /*
          * The peek method gives us a cloned chunk_io_request with pointers to
          * the original chunk_io_request. We just need to free the structure not
          * the content so we call free() here and not free_chunk_io_request() !
          */
         free(request);

         return retval;
      }
   }

   /*
    * Get the actual length by contacting the remote backing store.
    */
   return chunked_remote_volume_size();
}

/*
 * Make sure we have the right chunk in memory.
 */
bool chunked_device::load_chunk()
{
   boffset_t start_offset;

   start_offset = (m_offset / m_current_chunk->chunk_size) * m_current_chunk->chunk_size;

   /*
    * See if we have to allocate a new buffer.
    */
   if (!m_current_chunk->buffer) {
      m_current_chunk->buffer = allocate_chunkbuffer();
   }

   /*
    * If the wrong chunk is loaded populate the chunk buffer with the right data.
    */
   if (start_offset != m_current_chunk->start_offset) {
      m_current_chunk->buflen = 0;
      m_current_chunk->start_offset = start_offset;
      if (!read_chunk()) {
         switch (dev_errno) {
         case EIO:
            if (m_current_chunk->writing) {
               m_current_chunk->end_offset = start_offset + (m_current_chunk->chunk_size - 1);
            }
            break;
         default:
            return false;
         }
      }
   }
   m_current_chunk->chunk_setup = true;

   return true;
}

static int list_io_request(void *request, void *data)
{
   chunk_io_request *io_request = (chunk_io_request *)request;
   bsdDevStatTrig *dst = (bsdDevStatTrig *)data;
   POOL_MEM status(PM_MESSAGE);

   status.bsprintf("   /%s/%04d - %ld\n", io_request->volname, io_request->chunk, io_request->wbuflen);
   dst->status_length = pm_strcat(dst->status, status.c_str());

   return 0;
}

/*
 * Return specific device status information.
 */
bool chunked_device::device_status(bsdDevStatTrig *dst)
{
   /*
    * See if we are using io-threads or not and the ordered circbuf is created and not empty.
    */
   dst->status_length = 0;
   if (m_io_threads > 0 && m_cb) {
      if (!m_cb->empty()) {
         dst->status_length = pm_strcpy(dst->status, _("Pending IO flush requests:\n"));

         /*
          * Peek on the ordered circular queue and list all pending requests.
          */
         m_cb->peek(PEEK_LIST, dst, list_io_request);
      } else {
         dst->status_length = pm_strcpy(dst->status, _("No Pending IO flush requests\n"));
      }
   }

   return (dst->status_length > 0);
}

chunked_device::~chunked_device()
{
   if (m_thread_ids) {
      stop_threads();
   }

   if (m_cb) {
      /*
       * If there is any work on the ordered circular buffer remove it.
       */
      if (!m_cb->empty()) {
         chunk_io_request *request;
         do {
            request = (chunk_io_request *)m_cb->dequeue();
            if (request) {
               request->release = true;
               free_chunk_io_request(request);
            }
         } while (!m_cb->empty());
      }

      delete m_cb;
      m_cb = NULL;
   }

   if (m_current_chunk) {
      if (m_current_chunk->buffer) {
         free_chunkbuffer(m_current_chunk->buffer);
      }
      free(m_current_chunk);
      m_current_chunk = NULL;
   }

   if (m_current_volname) {
      free(m_current_volname);
   }
}

chunked_device::chunked_device()
{
   m_current_volname = NULL;
   m_current_chunk = NULL;
   m_io_threads = 0;
   m_io_slots = 0;
   m_chunk_size = 0;
   m_io_threads_started = false;
   m_end_of_media = false;
   m_cb = NULL;
   m_io_threads = 0;
   m_chunk_size = 0;
   m_offset = 0;
   m_use_mmap = false;
}
#endif /* HAVE_OBJECTSTORE */
