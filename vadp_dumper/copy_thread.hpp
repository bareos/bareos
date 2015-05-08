/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Bareos GmbH & Co. KG

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
#include "cbuf.hpp"

typedef size_t (IO_FUNCTION)(size_t sector_offset, size_t nbyte, void *buf);

struct CP_THREAD_SAVE_DATA {
   size_t sector_offset;                  /* Sector offset where to write data */
   size_t data_len;                       /* Length of Data */
   void *data;                            /* Data */
};

struct CP_THREAD_CTX {
   int nr_save_elements;                  /* Number of save items in save_data */
   CP_THREAD_SAVE_DATA *save_data;        /* To save data (cached structure build during restore) */
   circbuf *cb;                           /* Circular buffer for passing work to copy thread */
   bool started;                          /* Copy thread consuming data */
   bool flushed;                          /* Copy thread flushed data */
   pthread_t thread_id;                   /* Id of copy thread */
   pthread_mutex_t lock;                  /* Lock the structure */
   pthread_cond_t start;                  /* Start consuming data from the Circular buffer */
   pthread_cond_t flush;                  /* Flush data from the Circular buffer */
   IO_FUNCTION *input_function;           /* IO function that performs the output I/O */
   IO_FUNCTION *output_function;          /* IO function that performs the output I/O */
};

bool setup_copy_thread(IO_FUNCTION *input_function, IO_FUNCTION *output_function);
bool send_to_copy_thread(size_t sector_offset, size_t nbyte);
void flush_copy_thread();
void cleanup_copy_thread();
