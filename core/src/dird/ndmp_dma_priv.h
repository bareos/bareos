/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2015 Planets Communications B.V.
   Copyright (C) 2015-2015 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, May 2015
 */
/**
 * @file
 * NDMP internal routines used by the different NDMP components.
 */

#ifndef BAREOS_DIRD_NDMP_DMA_PRIV_H_
#define BAREOS_DIRD_NDMP_DMA_PRIV_H_ 1

namespace directordaemon {

#ifdef NDMP_NEED_ENV_KEYWORDS
/**
 * Array used for storing fixed NDMP env keywords.
 * Anything special should go into a so called meta-tag in the fileset options.
 */
static char *ndmp_env_keywords[] = {
   (char *)"HIST",
   (char *)"TYPE",
   (char *)"DIRECT",
   (char *)"LEVEL",
   (char *)"UPDATE",
   (char *)"EXCLUDE",
   (char *)"INCLUDE",
   (char *)"FILESYSTEM",
   (char *)"PREFIX"
};

/**
 * Index values for above keyword.
 */
enum {
   NDMP_ENV_KW_HIST = 0,
   NDMP_ENV_KW_TYPE,
   NDMP_ENV_KW_DIRECT,
   NDMP_ENV_KW_LEVEL,
   NDMP_ENV_KW_UPDATE,
   NDMP_ENV_KW_EXCLUDE,
   NDMP_ENV_KW_INCLUDE,
   NDMP_ENV_KW_FILESYSTEM,
   NDMP_ENV_KW_PREFIX
};

/**
 * Array used for storing fixed NDMP env values.
 * Anything special should go into a so called meta-tag in the fileset options.
 */
static char *ndmp_env_values[] = {
   (char *)"n",
   (char *)"y"
};

/**
 * Index values for above values.
 */
enum {
   NDMP_ENV_VALUE_NO = 0,
   NDMP_ENV_VALUE_YES
};
#endif /* NDMP_NEED_ENV_KEYWORDS */

struct ndmp_backup_format_option {
   char *format;
   bool uses_file_history;
   bool uses_level;
   bool restore_prefix_relative;
   bool needs_namelist;
};

/**
 * Internal structure to keep track of private data.
 */
struct ndmp_internal_state {
   uint32_t LogLevel;
   JobControlRecord *jcr;
   UaContext *ua;
   char *filesystem;
   int32_t FileIndex;
   char *virtual_filename;
   bool save_filehist;
   int64_t filehist_size;
   void *fhdb_state;
};
typedef struct ndmp_internal_state NIS;

/*
 * Generic DMA functions.
 */
ndmp_backup_format_option *ndmp_lookup_backup_format_options(const char *backup_format);

bool NdmpValidateJob(JobControlRecord *jcr, struct ndm_job_param *job);
void NdmpParseMetaTag(struct ndm_env_table *env_tab, char *meta_tag);
int NativeToNdmpLoglevel(int NdmpLoglevel, int debuglevel, NIS *nis);
bool NdmpBuildClientJob(JobControlRecord *jcr, ClientResource *client, StorageResource *store, int operation,
                           struct ndm_job_param *job);
bool NdmpBuildStorageJob(JobControlRecord *jcr, StorageResource *store, bool init_tape, bool init_robot,
                           int operation, struct ndm_job_param *job);
bool NdmpBuildClientAndStorageJob(JobControlRecord *jcr, StorageResource *store, ClientResource *client,
                           bool init_tape, bool init_robot, int operation, struct ndm_job_param *job);

void NdmpLoghandler(struct ndmlog *log, char *tag, int level, char *msg);
void NdmpDoQuery(UaContext *ua, JobControlRecord *jcr,
      ndm_job_param *ndmp_job, int NdmpLoglevel, ndmca_query_callbacks* query_cbs);

/*
 * NDMP FHDB specific helpers.
 */
void NdmpStoreAttributeRecord(JobControlRecord *jcr, char *fname, char *linked_fname,
                                 char *attributes, int8_t FileType, uint64_t Node, uint64_t fhinfo);
void NdmpConvertFstat(ndmp9_file_stat *fstat, int32_t FileIndex,
                        int8_t *FileType, PoolMem &attribs);

/*
 * FHDB using LMDB.
 */
void NdmpFhdbLmdbRegister(struct ndmlog *ixlog);
void NdmpFhdbLmdbUnregister(struct ndmlog *ixlog);
void NdmpFhdbLmdbProcessDb(struct ndmlog *ixlog);

/*
 * FHDB using in memory tree.
 */
void NdmpFhdbMemRegister(struct ndmlog *ixlog);
void NdmpFhdbMemUnregister(struct ndmlog *ixlog);
void NdmpFhdbMemProcessDb(struct ndmlog *ixlog);

/*
 * NDMP Media Info in DB storage and retrieval
 */
bool StoreNdmmediaInfoInDatabase(ndmmedia *media, JobControlRecord  *jcr);
bool GetNdmmediaInfoFromDatabase(ndm_media_table *media_tab, JobControlRecord  *jcr);
extern "C" int BndmpFhdbAddFile(struct ndmlog *ixlog, int tagc, char *raw_name, ndmp9_file_stat *fstat);

} /* namespace directordaemon */
#endif /* BAREOS_DIRD_NDMP_DMA_PRIV_H_ */
