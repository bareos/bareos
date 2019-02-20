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
 *  Written by John Walker MM
 */
/**
 * @file
 * General purpose queue
 */

struct b_queue {
  struct b_queue* qnext; /* Next item in queue */
  struct b_queue* qprev; /* Previous item in queue */
};

typedef struct b_queue BQUEUE;

/**
 * Queue functions
 */
void qinsert(BQUEUE* qhead, BQUEUE* object);
BQUEUE* qnext(BQUEUE* qhead, BQUEUE* qitem);
BQUEUE* qdchain(BQUEUE* qitem);
BQUEUE* qremove(BQUEUE* qhead);
