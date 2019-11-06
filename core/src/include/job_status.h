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

#ifndef BAREOS_SRC_INCLUDE_JOB_STATUS_H_
#define BAREOS_SRC_INCLUDE_JOB_STATUS_H_

enum JobStatus : char
{
  JS_Canceled = 'A',        /**< Canceled by user */
  JS_Blocked = 'B',         /**< Blocked */
  JS_Created = 'C',         /**< Created but not yet running */
  JS_Differences = 'D',     /**< Verify differences */
  JS_ErrorTerminated = 'E', /**< Job terminated in error */
  JS_WaitFD = 'F',          /**< Waiting on File daemon */
  JS_Incomplete = 'I',      /**< Incomplete Job */
  JS_DataCommitting = 'L',  /**< Committing data (last despool) */
  JS_WaitMount = 'M',       /**< Waiting for Mount */
  JS_Running = 'R',         /**< Running */
  JS_WaitSD = 'S',          /**< Waiting on the Storage daemon */
  JS_Terminated = 'T',      /**< Terminated normally */
  JS_Warnings = 'W',        /**< Terminated normally with warnings */

  JS_AttrDespooling = 'a', /**< SD despooling attributes */
  JS_WaitClientRes = 'c',  /**< Waiting for Client resource */
  JS_WaitMaxJobs = 'd',    /**< Waiting for maximum jobs */
  JS_Error = 'e',          /**< Non-fatal error */
  JS_FatalError = 'f',     /**< Fatal error */
  JS_AttrInserting = 'i',  /**< Doing batch insert file records */
  JS_WaitJobRes = 'j',     /**< Waiting for job resource */
  JS_DataDespooling = 'l', /**< Doing data despooling */
  JS_WaitMedia = 'm',      /**< Waiting for new media */
  JS_WaitPriority = 'p',   /**< Waiting for higher priority jobs to finish */
  JS_WaitDevice = 'q',     /**< Queued waiting for device */
  JS_WaitStoreRes = 's',   /**< Waiting for storage resource */
  JS_WaitStartTime = 't'   /**< Waiting for start time */
};

#endif  // BAREOS_SRC_INCLUDE_JOB_STATUS_H_
