/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_SRC_INCLUDE_JOB_TYPES_H_
#define BAREOS_SRC_INCLUDE_JOB_TYPES_H_

enum JobTypes : char
{
  JT_BACKUP = 'B',       /**< Backup Job */
  JT_MIGRATED_JOB = 'M', /**< A previous backup job that was migrated */
  JT_VERIFY = 'V',       /**< Verify Job */
  JT_RESTORE = 'R',      /**< Restore Job */
  JT_CONSOLE = 'U',      /**< console program */
  JT_SYSTEM = 'I',       /**< internal system "job" */
  JT_ADMIN = 'D',        /**< admin job */
  JT_ARCHIVE = 'A',      /**< Archive Job */
  JT_JOB_COPY = 'C',     /**< Copy of a Job */
  JT_COPY = 'c',         /**< Copy Job */
  JT_MIGRATE = 'g',      /**< Migration Job */
  JT_SCAN = 'S',         /**< Scan Job */
  JT_CONSOLIDATE = 'O'   /**< Always Incremental Consolidate Job */
};

#endif  // BAREOS_SRC_INCLUDE_JOB_TYPES_H_
