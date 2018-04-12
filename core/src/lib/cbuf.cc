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
#include "bareos.h"
#include "cbuf.h"

/*
 * Initialize a new circular buffer.
 */
int circbuf::init(int capacity)
{
   if (pthread_mutex_init(&m_lock, NULL) != 0) {
      return -1;
   }

   if (pthread_cond_init(&m_notfull, NULL) != 0) {
      pthread_mutex_destroy(&m_lock);
      return -1;
   }

   if (pthread_cond_init(&m_notempty, NULL) != 0) {
      pthread_cond_destroy(&m_notfull);
      pthread_mutex_destroy(&m_lock);
      return -1;
   }

   m_next_in = 0;
   m_next_out = 0;
   m_size = 0;
   m_capacity = capacity;
   if (m_data) {
      free(m_data);
   }
   m_data = (void **)malloc(m_capacity * sizeof(void *));

   return 0;
}

/*
 * Destroy a circular buffer.
 */
void circbuf::destroy()
{
   pthread_cond_destroy(&m_notempty);
   pthread_cond_destroy(&m_notfull);
   pthread_mutex_destroy(&m_lock);
   if (m_data) {
      free(m_data);
      m_data = NULL;
   }
}

/*
 * Enqueue a new item into the circular buffer.
 */
int circbuf::enqueue(void *data)
{
   if (pthread_mutex_lock(&m_lock) != 0) {
      return -1;
   }

   /*
    * Wait while the buffer is full.
    */
   while (full()) {
      pthread_cond_wait(&m_notfull, &m_lock);
   }
   m_data[m_next_in++] = data;
   m_size++;
   m_next_in %= m_capacity;

   /*
    * Let any waiting consumer know there is data.
    */
   pthread_cond_broadcast(&m_notempty);

   pthread_mutex_unlock(&m_lock);

   return 0;
}

/*
 * Dequeue an item from the circular buffer.
 */
void *circbuf::dequeue()
{
   void *data = NULL;

   if (pthread_mutex_lock(&m_lock) != 0) {
      return NULL;
   }

   /*
    * Wait while there is nothing in the buffer
    */
   while (empty() && !m_flush) {
      pthread_cond_wait(&m_notempty, &m_lock);
   }

   /*
    * When we are requested to flush and there is no data left return NULL.
    */
   if (empty() && m_flush) {
      goto bail_out;
   }

   data = m_data[m_next_out++];
   m_size--;
   m_next_out %= m_capacity;

   /*
    * Let all waiting producers know there is room.
    */
   pthread_cond_broadcast(&m_notfull);

bail_out:
   pthread_mutex_unlock(&m_lock);

   return data;
}

/*
 * Make sure there is a free next slot available on the circular buffer.
 * So the next enqueue will not block but we block now until one is available.
 */
int circbuf::next_slot()
{
   if (pthread_mutex_lock(&m_lock) != 0) {
      return -1;
   }

   /*
    * Wait while the buffer is full.
    */
   while (full()) {
      pthread_cond_wait(&m_notfull, &m_lock);
   }

   pthread_mutex_unlock(&m_lock);

   return m_next_in;
}

/*
 * Flush the circular buffer. Any waiting consumer will be wakened and will
 * see we are in flush state.
 */
int circbuf::flush()
{
   if (pthread_mutex_lock(&m_lock) != 0) {
      return -1;
   }

   /*
    * Set the flush flag.
    */
   m_flush = true;

   /*
    * Let all waiting consumers know there will be no more data.
    */
   pthread_cond_broadcast(&m_notempty);

   pthread_mutex_unlock(&m_lock);

   return 0;
}
