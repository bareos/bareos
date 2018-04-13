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

#ifndef BNDMP_DMA_PRIV_H
#define BNDMP_DMA_PRIV_H 1

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

bool ndmp_validate_job(JobControlRecord *jcr, struct ndm_job_param *job);
void ndmp_parse_meta_tag(struct ndm_env_table *env_tab, char *meta_tag);
int native_to_ndmp_loglevel(int NdmpLoglevel, int debuglevel, NIS *nis);
bool ndmp_build_client_job(JobControlRecord *jcr, ClientResource *client, StoreResource *store, int operation,
                           struct ndm_job_param *job);
bool ndmp_build_storage_job(JobControlRecord *jcr, StoreResource *store, bool init_tape, bool init_robot,
                           int operation, struct ndm_job_param *job);
bool ndmp_build_client_and_storage_job(JobControlRecord *jcr, StoreResource *store, ClientResource *client,
                           bool init_tape, bool init_robot, int operation, struct ndm_job_param *job);

extern "C" void ndmp_loghandler(struct ndmlog *log, char *tag, int level, char *msg);
void ndmp_do_query(UaContext *ua, ndm_job_param *ndmp_job, int NdmpLoglevel);

/*
 * NDMP FHDB specific helpers.
 */
void ndmp_store_attribute_record(JobControlRecord *jcr, char *fname, char *linked_fname,
                                 char *attributes, int8_t FileType, uint64_t Node, uint64_t fhinfo);
void ndmp_convert_fstat(ndmp9_file_stat *fstat, int32_t FileIndex,
                        int8_t *FileType, PoolMem &attribs);

/*
 * FHDB using LMDB.
 */
void ndmp_fhdb_lmdb_register(struct ndmlog *ixlog);
void ndmp_fhdb_lmdb_unregister(struct ndmlog *ixlog);
void ndmp_fhdb_lmdb_process_db(struct ndmlog *ixlog);

/*
 * FHDB using in memory tree.
 */
void ndmp_fhdb_mem_register(struct ndmlog *ixlog);
void ndmp_fhdb_mem_unregister(struct ndmlog *ixlog);
void ndmp_fhdb_mem_process_db(struct ndmlog *ixlog);

/*
 * NDMP Media Info in DB storage and retrieval
 */
bool store_ndmmedia_info_in_database(ndmmedia *media, JobControlRecord  *jcr);
bool get_ndmmedia_info_from_database(ndm_media_table *media_tab, JobControlRecord  *jcr);
extern "C" int bndmp_fhdb_add_file(struct ndmlog *ixlog, int tagc, char *raw_name, ndmp9_file_stat *fstat);
#endif
