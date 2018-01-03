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
 * Ordered Circular buffer used for producer/consumer problem with pthread.
 */

#define OQSIZE 10             /* # of pointers in the queue */

enum oc_peek_types {
   PEEK_FIRST = 0,
   PEEK_LAST,
   PEEK_LIST,
   PEEK_CLONE
};

struct ocbuf_item {
   dlink link;
   uint32_t data_size;
   void *data;
};

class ordered_circbuf : public SMARTALLOC {
private:
   int m_size;
   int m_capacity;
   int m_reserved;
   bool m_flush;
   pthread_mutex_t m_lock;    /* Lock the structure */
   pthread_cond_t m_notfull;  /* Full -> not full condition */
   pthread_cond_t m_notempty; /* Empty -> not empty condition */
   dlist *m_data;             /* Circular buffer of pointers */

public:
   ordered_circbuf(int capacity = OQSIZE);
   ~ordered_circbuf();
   int init(int capacity);
   void destroy();
   void *enqueue(void *data,
                 uint32_t data_size,
                 int compare(void *item1, void *item2),
                 void update(void *item1, void *item2),
                 bool use_reserved_slot = false,
                 bool no_signal = false);
   void *dequeue(bool reserve_slot = false,
                 bool requeued = false,
                 struct timespec *ts = NULL,
                 int timeout = 300);
   void *peek(enum oc_peek_types type,
              void *data,
              int callback(void *item1, void *item2));
   int unreserve_slot();
   int flush();
   bool full() { return m_size == (m_capacity - m_reserved); };
   bool empty() { return m_size == 0; };
   bool is_flushing() { return m_flush; };
   int capacity() const { return m_capacity; };
};

/*
 * Constructor
 */
inline ordered_circbuf::ordered_circbuf(int capacity)
{
   init(capacity);
}

/*
 * Destructor
 */
inline ordered_circbuf::~ordered_circbuf()
{
   destroy();
}
