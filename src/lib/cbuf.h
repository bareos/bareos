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

class circbuf : public SMARTALLOC {
   int m_size;
   int m_next_in;
   int m_next_out;
   int m_capacity;
   bool m_flush;
   pthread_mutex_t m_lock;    /**< Lock the structure */
   pthread_cond_t m_notfull;  /**< Full -> not full condition */
   pthread_cond_t m_notempty; /**< Empty -> not empty condition */
   void *m_data[QSIZE];       /**< Circular buffer of pointers */

public:
   circbuf();
   ~circbuf();
   int init();
   void destroy();
   int enqueue(void *data);
   void *dequeue();
   int next_slot();
   int flush();
   bool full() { return m_size == m_capacity; };
   bool empty() { return m_size == 0; };
   int capacity() const { return m_capacity; };
};

/**
 * Constructor
 */
inline circbuf::circbuf()
{
   init();
}

/**
 * Destructor
 */
inline circbuf::~circbuf()
{
   destroy();
}
