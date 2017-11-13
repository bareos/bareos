/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2017 Planets Communications B.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
 * Chunked device device abstraction.
 *
 * Marco van Wieringen, February 2015
 */

#ifndef CHUNKED_DEVICE_H
#define CHUNKED_DEVICE_H

/*
 * Let io-threads check for work every 300 seconds.
 */
#define DEFAULT_RECHECK_INTERVAL 300

/*
 * Chunk the volume into chunks of this size.
 * This is the lower limit used the exact chunksize is
 * configured as a device option.
 */
#define DEFAULT_CHUNK_SIZE 10 * 1024 * 1024

/*
 * Maximum number of chunks per volume.
 * When you change this make sure you update the %04d format
 * used in the code to format the chunk numbers e.g. 0000-9999
 */
#define MAX_CHUNKS 10000

/*
 * Busy wait retry for inflight chunks.
 * Default 12 * 5 = 60 seconds.
 */
#define INFLIGHT_RETRIES 12
#define INFLIGT_RETRY_TIME 5

enum thread_wait_type {
   WAIT_CANCEL_THREAD,     /* Perform a pthread_cancel() on exit. */
   WAIT_JOIN_THREAD        /* Perform a pthread_join() on exit. */
};

struct thread_handle {
   thread_wait_type type;  /* See WAIT_*_THREAD thread_wait_type enum */
   pthread_t thread_id;    /* Actual threadid */
};

struct chunk_io_request {
   const char *volname;    /* VolumeName */
   uint16_t chunk;         /* Chunk number */
   char *buffer;           /* Data */
   uint32_t wbuflen;       /* Size of the actual valid data in the chunk (Write) */
   uint32_t *rbuflen;      /* Size of the actual valid data in the chunk (Read) */
   uint8_t tries;          /* Number of times the flush was tried to the backing store */
   bool release;           /* Should we release the data to which the buffer points ? */
};

struct chunk_descriptor {
   ssize_t chunk_size;     /* Total size of the memory chunk */
   char *buffer;           /* Data */
   uint32_t buflen;        /* Size of the actual valid data in the chunk */
   boffset_t start_offset; /* Start offset of the current chunk */
   boffset_t end_offset;   /* End offset of the current chunk */
   bool need_flushing;     /* Data is dirty and needs flushing to backing store */
   bool chunk_setup;       /* Chunk is initialized and ready for use */
   bool writing;           /* We are currently writing */
   bool opened;            /* An open call was done */
};

#include "lib/ordered_cbuf.h"

class chunked_device: public DEVICE {
private:
   /*
    * Private Members
    */
   bool m_io_threads_started;
   bool m_end_of_media;
   bool m_readonly;
   uint8_t m_inflight_chunks;
   char *m_current_volname;
   ordered_circbuf *m_cb;
   alist *m_thread_ids;
   chunk_descriptor *m_current_chunk;

   /*
    * Private Methods
    */
   char *allocate_chunkbuffer();
   void free_chunkbuffer(char *buffer);
   void free_chunk_io_request(chunk_io_request *request);
   bool start_io_threads();
   void stop_threads();
   bool enqueue_chunk(chunk_io_request *request);
   bool flush_chunk(bool release_chunk, bool move_to_next_chunk);
   bool read_chunk();

protected:
   /*
    * Protected Members
    */
   uint8_t m_io_threads;
   uint8_t m_io_slots;
   uint8_t m_retries;
   uint64_t m_chunk_size;
   boffset_t m_offset;
   bool m_use_mmap;

   /*
    * Protected Methods
    */
   bool set_inflight_chunk(chunk_io_request *request);
   void clear_inflight_chunk(chunk_io_request *request);
   bool is_inflight_chunk(chunk_io_request *request);
   int nr_inflight_chunks();
   int setup_chunk(const char *pathname, int flags, int mode);
   ssize_t read_chunked(int fd, void *buffer, size_t count);
   ssize_t write_chunked(int fd, const void *buffer, size_t count);
   int close_chunk();
   bool truncate_chunked_volume(DCR *dcr);
   ssize_t chunked_volume_size();
   bool load_chunk();

   /*
    * Methods implemented by inheriting class.
    */
   virtual bool flush_remote_chunk(chunk_io_request *request) = 0;
   virtual bool read_remote_chunk(chunk_io_request *request) = 0;
   virtual ssize_t chunked_remote_volume_size() = 0;
   virtual bool truncate_remote_chunked_volume(DCR *dcr) = 0;

public:
   /*
    * Public Methods
    */
   chunked_device();
   virtual ~chunked_device();

   bool dequeue_chunk();
   bool device_status(bsdDevStatTrig *dst);

   /*
    * Interface from DEVICE
    */
   virtual int d_close(int fd) = 0;
   virtual int d_open(const char *pathname, int flags, int mode) = 0;
   virtual int d_ioctl(int fd, ioctl_req_t request, char *mt = NULL) = 0;
   virtual boffset_t d_lseek(DCR *dcr, boffset_t offset, int whence) = 0;
   virtual ssize_t d_read(int fd, void *buffer, size_t count) = 0;
   virtual ssize_t d_write(int fd, const void *buffer, size_t count) = 0;
   virtual bool d_truncate(DCR *dcr) = 0;
};
#endif /* CHUNKED_DEVICE_H */
