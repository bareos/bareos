/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, January MMI
 */
/**
 * @file
 * BAREOS work queue routines. Permits passing work to
 * multiple threads.
 *
 * Kern Sibbald, January MMI
 *
 */

#ifndef BAREOS_LIB_WORKQ_H_
#define BAREOS_LIB_WORKQ_H_ 1

class ConfigurationParser;

/**
 * Structure to keep track of work queue request
 */
typedef struct workq_ele_tag {
   struct workq_ele_tag *next;
   ConfigurationParser  *config;
   void                 *data;
} workq_ele_t;

/**
 * Structure describing a work queue
 */
typedef struct workq_tag {
   pthread_mutex_t   mutex;           /* queue access control */
   pthread_cond_t    work;            /* wait for work */
   pthread_attr_t    attr;            /* create detached threads */
   workq_ele_t       *first, *last;   /* work queue */
   int               valid;           /* queue initialized */
   int               quit;            /* workq should quit */
   int               max_workers;     /* max threads */
   int               num_workers;     /* current threads */
   void             *(*HandleConnectionRequest)(ConfigurationParser *config, void *arg);
} workq_t;

#define WORKQ_VALID  0xdec1992

extern int WorkqInit(
              workq_t *wq,
              int     threads,                                        /* maximum threads */
              void   *(*engine)(ConfigurationParser* config, void *)  /* engine routine */
                    );
extern int WorkqDestroy(workq_t *wq);
extern int WorkqAdd(workq_t *wq, ConfigurationParser *config, void *element, workq_ele_t **work_item);

#endif /* BAREOS_LIB_WORKQ_H_ */
