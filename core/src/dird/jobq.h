/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, July MMIII
 *
 * This code adapted from Bareos work queue code, which was
 * adapted from "Programming with POSIX Threads", by David R. Butenhof
 */
/**
 * @file
 * Bareos job queue routines.
 */

#ifndef BAREOS_DIRD_JOBQ_H_
#define BAREOS_DIRD_JOBQ_H_ 1

/**
 * Structure to keep track of job queue request
 */
struct jobq_item_t {
   dlink link;
   JobControlRecord *jcr;
};

/**
 * Structure describing a work queue
 */
struct jobq_t {
   pthread_mutex_t   mutex;           /* queue access control */
   pthread_cond_t    work;            /* wait for work */
   pthread_attr_t    attr;            /* create detached threads */
   dlist            *waiting_jobs;    /* list of jobs waiting */
   dlist            *running_jobs;    /* jobs running */
   dlist            *ready_jobs;      /* jobs ready to run */
   int               valid;           /* queue initialized */
   bool              quit;            /* jobq should quit */
   int               max_workers;     /* max threads */
   int               num_workers;     /* current threads */
   void             *(*engine)(void *arg); /* user engine */
};

#define JOBQ_VALID  0xdec1993

extern int JobqInit(
              jobq_t *wq,
              int     max_workers,        /* maximum threads */
              void   *(*engine)(void *)   /* engine routine */
                    );
extern int JobqDestroy(jobq_t *wq);
extern int JobqAdd(jobq_t *wq, JobControlRecord *jcr);
extern int JobqRemove(jobq_t *wq, JobControlRecord *jcr);

bool IncReadStore(JobControlRecord *jcr);
void DecReadStore(JobControlRecord *jcr);
#endif /* BAREOS_DIRD_JOBQ_H_ */
