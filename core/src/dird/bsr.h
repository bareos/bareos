/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_BSR_H_
#define BAREOS_DIRD_BSR_H_

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

#include "dird/ua.h"

RestoreBootstrapRecord *new_bsr();
DLL_IMP_EXP void free_bsr(RestoreBootstrapRecord *bsr);
bool complete_bsr(UaContext *ua, RestoreBootstrapRecord *bsr);
uint32_t write_bsr_file(UaContext *ua, RestoreContext &rx);
void display_bsr_info(UaContext *ua, RestoreContext &rx);
uint32_t write_bsr(UaContext *ua, RestoreContext &rx, PoolMem *buffer);
void add_findex(RestoreBootstrapRecord *bsr, uint32_t JobId, int32_t findex);
void add_findex_all(RestoreBootstrapRecord *bsr, uint32_t JobId);
RestoreBootstrapRecordFileIndex *new_findex();
void make_unique_restore_filename(UaContext *ua, POOLMEM *&fname);
void print_bsr(UaContext *ua, RestoreContext &rx);
bool open_bootstrap_file(JobControlRecord *jcr, bootstrap_info &info);
bool send_bootstrap_file(JobControlRecord *jcr, BareosSocket *sock, bootstrap_info &info);
void close_bootstrap_file(bootstrap_info &info);

#endif // BAREOS_DIRD_BSR_H_
