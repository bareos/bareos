/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2017 Planets Communications B.V.
   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_BACKENDS_CHUNKED_DEVICE_H_
#define BAREOS_STORED_BACKENDS_CHUNKED_DEVICE_H_

namespace storagedaemon {

/*
 * Let io-threads check for work every 300 seconds.
 */
#define DEFAULT_RECHECK_INTERVAL 300

/*
 * Recheck interval when waiting that buffer gets written
 * (write buffer is empty).
 */
#define DEFAULT_RECHECK_INTERVAL_WRITE_BUFFER 10

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
 * Default 120 * 5 = 600 seconds, 10 minutes.
 */
#define INFLIGHT_RETRIES 120
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

#include "ordered_cbuf.h"

class chunked_device: public Device {
private:
   /*
    * Private Members
    */
   bool io_threads_started_;
   bool end_of_media_;
   bool readonly_;
   uint8_t inflight_chunks_;
   char *current_volname_;
   ordered_circbuf *cb_;
   alist *thread_ids_;
   chunk_descriptor *current_chunk_;

   /*
    * Private Methods
    */
   char *allocate_chunkbuffer();
   void FreeChunkbuffer(char *buffer);
   void FreeChunkIoRequest(chunk_io_request *request);
   bool StartIoThreads();
   void StopThreads();
   bool EnqueueChunk(chunk_io_request *request);
   bool FlushChunk(bool release_chunk, bool move_to_next_chunk);
   bool ReadChunk();

protected:
   /*
    * Protected Members
    */
   uint8_t io_threads_;
   uint8_t io_slots_;
   uint8_t retries_;
   uint64_t chunk_size_;
   boffset_t offset_;
   bool use_mmap_;

   /*
    * Protected Methods
    */
   bool SetInflightChunk(chunk_io_request *request);
   void ClearInflightChunk(chunk_io_request *request);
   bool IsInflightChunk(chunk_io_request *request);
   int NrInflightChunks();
   int SetupChunk(const char *pathname, int flags, int mode);
   ssize_t ReadChunked(int fd, void *buffer, size_t count);
   ssize_t WriteChunked(int fd, const void *buffer, size_t count);
   int close_chunk();
   bool TruncateChunkedVolume(DeviceControlRecord *dcr);
   ssize_t ChunkedVolumeSize();
   bool LoadChunk();

   /*
    * Methods implemented by inheriting class.
    */
   virtual bool FlushRemoteChunk(chunk_io_request *request) = 0;
   virtual bool ReadRemoteChunk(chunk_io_request *request) = 0;
   virtual ssize_t chunked_remote_volume_size() = 0;
   virtual bool TruncateRemoteChunkedVolume(DeviceControlRecord *dcr) = 0;

public:
   /*
    * Public Methods
    */
   chunked_device();
   virtual ~chunked_device();

   bool DequeueChunk();
   bool DeviceStatus(bsdDevStatTrig *dst);

   /*
    * Interface from Device
    */
   virtual int d_close(int fd) = 0;
   virtual int d_open(const char *pathname, int flags, int mode) = 0;
   virtual int d_ioctl(int fd, ioctl_req_t request, char *mt = NULL) = 0;
   virtual boffset_t d_lseek(DeviceControlRecord *dcr, boffset_t offset, int whence) = 0;
   virtual ssize_t d_read(int fd, void *buffer, size_t count) = 0;
   virtual ssize_t d_write(int fd, const void *buffer, size_t count) = 0;
   virtual bool d_truncate(DeviceControlRecord *dcr) = 0;
};

} /* namespace storagedaemon */

#endif /* BAREOS_STORED_BACKENDS_CHUNKED_DEVICE_H_ */
