/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2010 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, July 2002
 */
/**
 * @file
 * Bootstrap Record header file
 *
 * BootStrapRecord (bootstrap record) handling routines split from ua_restore.c July 2003
 * Bootstrap send handling routines split from restore.c July 2012
 */

/**
 * FileIndex entry in restore bootstrap record
 */
struct RestoreBootstrapRecordFileIndex {
   RestoreBootstrapRecordFileIndex *next;
   int32_t findex;
   int32_t findex2;
};

/**
 * Restore bootstrap record -- not the real one, but useful here
 *  The restore bsr is a chain of BootStrapRecord records (linked by next).
 *  Each BootStrapRecord represents a single JobId, and within it, it
 *    contains a linked list of file indexes for that JobId.
 *    The complete_bsr() routine, will then add all the volumes
 *    on which the Job is stored to the BootStrapRecord.
 */
struct RestoreBootstrapRecord {
   RestoreBootstrapRecord *next;                        /**< next JobId */
   JobId_t JobId;                     /**< JobId this bsr */
   uint32_t VolSessionId;
   uint32_t VolSessionTime;
   int      VolCount;                 /**< Volume parameter count */
   VolumeParameters *VolParams;             /**< Volume, start/end file/blocks */
   RestoreBootstrapRecordFileIndex *fi;                   /**< File indexes this JobId */
   char *fileregex;                   /**< Only restore files matching regex */
};

class UaContext;

/**
 * Open bootstrap file.
 */
struct bootstrap_info
{
   FILE *bs;
   UaContext *ua;
   char storage[MAX_NAME_LENGTH + 1];
};
