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
 * BootStrapRecord (bootstrap record) handling routines split from ua_restore.c
 * July 2003 Bootstrap send handling routines split from restore.c July 2012
 */

#ifndef BAREOS_DIRD_BSR_H_
#define BAREOS_DIRD_BSR_H_

#include "dird/ua.h"

struct VolumeParameters;

namespace directordaemon {

/**
 * FileIndex entry in restore bootstrap record
 */
class RestoreBootstrapRecordFileIndex {
 private:
  std::vector<int32_t> fileIds_;
  bool allFiles_ = false;
  bool sorted_ = false;
  void sort();

 public:
  void add(int32_t findex);
  void addAll();
  std::vector<std::pair<int32_t, int32_t>> getRanges();
  bool empty() const { return !allFiles_ && fileIds_.empty(); }
};

/**
 * Restore bootstrap record -- not the real one, but useful here
 *  The restore bsr is a chain of BootStrapRecord records (linked by next).
 *  Each BootStrapRecord represents a single JobId, and within it, it
 *    contains a linked list of file indexes for that JobId.
 *    The CompleteBsr() routine, will then add all the volumes
 *    on which the Job is stored to the BootStrapRecord.
 */
struct RestoreBootstrapRecord {
  std::unique_ptr<RestoreBootstrapRecord> next; /**< next JobId */
  JobId_t JobId = 0;                            /**< JobId this bsr */
  uint32_t VolSessionId = 0;
  uint32_t VolSessionTime = 0;
  int VolCount = 0;                      /**< Volume parameter count */
  VolumeParameters* VolParams = nullptr; /**< Volume, start/end file/blocks */
  std::unique_ptr<RestoreBootstrapRecordFileIndex>
      fi;                    /**< File indexes this JobId */
  char* fileregex = nullptr; /**< Only restore files matching regex */

  RestoreBootstrapRecord() : RestoreBootstrapRecord(0) {}
  RestoreBootstrapRecord(JobId_t JobId);
  virtual ~RestoreBootstrapRecord();
  RestoreBootstrapRecord(const RestoreBootstrapRecord&) = delete;
  RestoreBootstrapRecord& operator=(const RestoreBootstrapRecord&) = delete;
  RestoreBootstrapRecord(RestoreBootstrapRecord&&) = delete;
  RestoreBootstrapRecord& operator=(RestoreBootstrapRecord&&) = delete;
};

class UaContext;

/**
 * Open bootstrap file.
 */
struct bootstrap_info {
  FILE* bs;
  UaContext* ua;
  char storage[MAX_NAME_LENGTH + 1];
};

bool CompleteBsr(UaContext* ua, RestoreBootstrapRecord* bsr);
uint32_t WriteBsrFile(UaContext* ua, RestoreContext& rx);
void DisplayBsrInfo(UaContext* ua, RestoreContext& rx);
uint32_t WriteBsr(UaContext* ua, RestoreContext& rx, std::string& buffer);
void AddFindex(RestoreBootstrapRecord* bsr, uint32_t JobId, int32_t findex);
void AddFindexAll(RestoreBootstrapRecord* bsr, uint32_t JobId);
RestoreBootstrapRecordFileIndex* new_findex();
void MakeUniqueRestoreFilename(UaContext* ua, POOLMEM*& fname);
void PrintBsr(UaContext* ua, RestoreContext& rx);
bool OpenBootstrapFile(JobControlRecord* jcr, bootstrap_info& info);
bool SendBootstrapFile(JobControlRecord* jcr,
                       BareosSocket* sock,
                       bootstrap_info& info);
void CloseBootstrapFile(bootstrap_info& info);
uint32_t write_findex(RestoreBootstrapRecordFileIndex* fi,
                      int32_t FirstIndex,
                      int32_t LastIndex,
                      std::string& buffer);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_BSR_H_
