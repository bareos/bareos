/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
// Kern Sibbald, June 2002
/**
 * @file
 * BootStrap record definition -- for restoring files.
 */
#ifndef BAREOS_STORED_BSR_H_
#define BAREOS_STORED_BSR_H_

#if __has_include(<regex.h>)
#  include <regex.h>
#else
#  include "lib/bregex.h"
#endif
#include "lib/attr.h"

namespace storagedaemon {

/**
 * List of Volume names to be read by Storage daemon.
 *  Formed by Storage daemon from BootStrapRecord
 */
struct VolumeList {
  VolumeList* next;
  char VolumeName[MAX_NAME_LENGTH];
  char MediaType[MAX_NAME_LENGTH];
  char device[MAX_NAME_LENGTH];
  int Slot;
  uint32_t start_file;
};

/**
 * !!!!!!!!!!!!!!!!!! NOTE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * !!!                                               !!!
 * !!!   All records must have a pointer to          !!!
 * !!!   the next item as the first item defined.    !!!
 * !!!                                               !!!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 */

struct BsrVolume {
  BsrVolume* next;
  char VolumeName[MAX_NAME_LENGTH];
  char MediaType[MAX_NAME_LENGTH];
  char device[MAX_NAME_LENGTH];
  int32_t Slot; /* Slot */
};

struct BsrClient {
  BsrClient* next;
  char ClientName[MAX_NAME_LENGTH];
};

struct BsrSessionId {
  BsrSessionId* next;
  uint32_t sessid;
  uint32_t sessid2;
};

struct BsrSessionTime {
  BsrSessionTime* next;
  uint32_t sesstime;
  bool done; /* local done */
};

struct BsrVolumeFile {
  BsrVolumeFile* next;
  uint32_t sfile; /* start file */
  uint32_t efile; /* end file */
  bool done;      /* local done */
};

struct BsrVolumeBlock {
  BsrVolumeBlock* next;
  uint32_t sblock; /* start block */
  uint32_t eblock; /* end block */
  bool done;       /* local done */
};

struct BsrVolumeAddress {
  BsrVolumeAddress* next;
  uint64_t saddr; /* start address */
  uint64_t eaddr; /* end address */
  bool done;      /* local done */
};

struct BsrFileIndex {
  BsrFileIndex* next;
  int32_t findex;  /* start file index */
  int32_t findex2; /* end file index */
  bool done;       /* local done */
};

struct BsrJobid {
  BsrJobid* next;
  uint32_t JobId;
  uint32_t JobId2;
};

struct BsrJobType {
  BsrJobType* next;
  uint32_t JobType;
};

struct BsrJoblevel {
  BsrJoblevel* next;
  uint32_t JobLevel;
};

struct BsrJob {
  BsrJob* next;
  char Job[MAX_NAME_LENGTH];
  bool done; /* local done */
};

struct BsrStream {
  BsrStream* next;
  int32_t stream; /* stream desired */
};

struct BootStrapRecord {
  /* NOTE!!! next must be the first item */
  BootStrapRecord* next;   /* pointer to next one */
  BootStrapRecord* prev;   /* pointer to previous one */
  BootStrapRecord* root;   /* root bsr */
  bool Reposition;         /* set when any bsr is marked done */
  bool mount_next_volume;  /* set when next volume should be mounted */
  bool done;               /* set when everything found for this bsr */
  bool use_fast_rejection; /* set if fast rejection can be used */
  bool use_positioning;    /* set if we can position the archive */
  bool skip_file;          /* skip all records for current file */
  BsrVolume* volume;
  uint32_t count; /* count of files to restore this bsr */
  uint32_t found; /* count of restored files this bsr */
  BsrVolumeFile* volfile;
  BsrVolumeBlock* volblock;
  BsrVolumeAddress* voladdr;
  BsrSessionTime* sesstime;
  BsrSessionId* sessid;
  BsrJobid* JobId;
  BsrJob* job;
  BsrClient* client;
  BsrFileIndex* FileIndex;
  BsrJobType* JobType;
  BsrJoblevel* JobLevel;
  BsrStream* stream;
  char* fileregex; /* set if restore is filtered on filename */
  regex_t* fileregex_re;
  Attributes* attr; /* scratch space for unpacking */
};


void CreateRestoreVolumeList(JobControlRecord* jcr);
void FreeRestoreVolumeList(JobControlRecord* jcr);

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_BSR_H_
