/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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
 * Database routines that are exported by the cats library for
 * use elsewhere in BAREOS (mainly the Director).
 */

#ifndef __SQL_PROTOS_H
#define __SQL_PROTOS_H

#include "cats.h"

/* Database prototypes */

/* cats_backends.c */
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
void db_set_backend_dirs(alist *new_backend_dirs);
#endif
void db_flush_backends(void);

/* sql.c */
bool db_open_batch_connection(JCR *jcr, B_DB *mdb);
char *db_strerror(B_DB *mdb);
int db_int64_handler(void *ctx, int num_fields, char **row);
int db_strtime_handler(void *ctx, int num_fields, char **row);
int db_list_handler(void *ctx, int num_fields, char **row);
void db_debug_print(JCR *jcr, FILE *fp);
int db_int_handler(void *ctx, int num_fields, char **row);

/* sql_create.c */
bool db_create_path_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar);
bool db_create_file_attributes_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar);
bool db_create_job_record(JCR *jcr, B_DB *db, JOB_DBR *jr);
bool db_create_media_record(JCR *jcr, B_DB *db, MEDIA_DBR *media_dbr);
bool db_create_client_record(JCR *jcr, B_DB *db, CLIENT_DBR *cr);
bool db_create_fileset_record(JCR *jcr, B_DB *db, FILESET_DBR *fsr);
bool db_create_pool_record(JCR *jcr, B_DB *db, POOL_DBR *pool_dbr);
bool db_create_jobmedia_record(JCR *jcr, B_DB *mdb, JOBMEDIA_DBR *jr);
bool db_create_counter_record(JCR *jcr, B_DB *mdb, COUNTER_DBR *cr);
bool db_create_device_record(JCR *jcr, B_DB *mdb, DEVICE_DBR *dr);
bool db_create_storage_record(JCR *jcr, B_DB *mdb, STORAGE_DBR *sr);
bool db_create_mediatype_record(JCR *jcr, B_DB *mdb, MEDIATYPE_DBR *mr);
bool db_write_batch_file_records(JCR *jcr);
bool db_create_attributes_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar);
bool db_create_restore_object_record(JCR *jcr, B_DB *mdb, ROBJECT_DBR *ar);
bool db_create_base_file_attributes_record(JCR *jcr, B_DB *mdb, ATTR_DBR *ar);
bool db_commit_base_file_attributes_record(JCR *jcr, B_DB *mdb);
bool db_create_base_file_list(JCR *jcr, B_DB *mdb, char *jobids);
bool db_create_quota_record(JCR *jcr, B_DB *mdb, CLIENT_DBR *cr);
bool db_create_ndmp_level_mapping(JCR *jcr, B_DB *mdb, JOB_DBR *jr, char *filesystem);
bool db_create_ndmp_environment_string(JCR *jcr, B_DB *mdb, JOB_DBR *jr, char *name, char *value);
bool db_create_job_statistics(JCR *jcr, B_DB *mdb, JOB_STATS_DBR *jsr);
bool db_create_device_statistics(JCR *jcr, B_DB *mdb, DEVICE_STATS_DBR *dsr);
bool db_create_tapealert_statistics(JCR *jcr, B_DB *mdb, TAPEALERT_STATS_DBR *tsr);

/* sql_delete.c */
bool db_delete_pool_record(JCR *jcr, B_DB *db, POOL_DBR *pool_dbr);
bool db_delete_media_record(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr);

/* sql_find.c */
bool db_find_last_job_start_time(JCR *jcr, B_DB *mdb, JOB_DBR *jr, POOLMEM **stime, char *job, int JobLevel);
bool db_find_job_start_time(JCR *jcr, B_DB *mdb, JOB_DBR *jr, POOLMEM **stime, char *job);
bool db_find_last_jobid(JCR *jcr, B_DB *mdb, const char *Name, JOB_DBR *jr);
int db_find_next_volume(JCR *jcr, B_DB *mdb, int index, bool InChanger, MEDIA_DBR *mr, const char *unwanted_volumes);
bool db_find_failed_job_since(JCR *jcr, B_DB *mdb, JOB_DBR *jr, POOLMEM *stime, int &JobLevel);

/* sql_get.c */
bool db_get_volume_jobids(JCR *jcr, B_DB *mdb,
                          MEDIA_DBR *mr, db_list_ctx *lst);
bool db_get_base_file_list(JCR *jcr, B_DB *mdb, bool use_md5,
                           DB_RESULT_HANDLER *result_handler,void *ctx);
int db_get_path_record(JCR *jcr, B_DB *mdb);
bool db_get_pool_record(JCR *jcr, B_DB *db, POOL_DBR *pdbr);
bool db_get_job_record(JCR *jcr, B_DB *mdb, JOB_DBR *jr);
int db_get_job_volume_names(JCR *jcr, B_DB *mdb, JobId_t JobId, POOLMEM **VolumeNames);
bool db_get_file_attributes_record(JCR *jcr, B_DB *mdb, char *fname, JOB_DBR *jr, FILE_DBR *fdbr);
int db_get_fileset_record(JCR *jcr, B_DB *mdb, FILESET_DBR *fsr);
bool db_get_media_record(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr);
int db_get_num_media_records(JCR *jcr, B_DB *mdb);
int db_get_num_pool_records(JCR *jcr, B_DB *mdb);
int db_get_pool_ids(JCR *jcr, B_DB *mdb, int *num_ids, DBId_t **ids);
bool db_get_client_ids(JCR *jcr, B_DB *mdb, int *num_ids, DBId_t **ids);
bool db_get_media_ids(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr, POOL_MEM &volumes, int *num_ids, uint32_t **ids);
int db_get_job_volume_parameters(JCR *jcr, B_DB *mdb, JobId_t JobId, VOL_PARAMS **VolParams);
bool db_get_client_record(JCR *jcr, B_DB *mdb, CLIENT_DBR *cdbr);
bool db_get_counter_record(JCR *jcr, B_DB *mdb, COUNTER_DBR *cr);
bool db_get_query_dbids(JCR *jcr, B_DB *mdb, POOL_MEM &query, dbid_list &ids);
bool db_get_file_list(JCR *jcr, B_DB *mdb, char *jobids,
                      bool use_md5, bool use_delta,
                      DB_RESULT_HANDLER *result_handler, void *ctx);
bool db_get_base_jobid(JCR *jcr, B_DB *mdb, JOB_DBR *jr, JobId_t *jobid);
bool db_accurate_get_jobids(JCR *jcr, B_DB *mdb, JOB_DBR *jr, db_list_ctx *jobids);
bool db_get_used_base_jobids(JCR *jcr, B_DB *mdb, POOLMEM *jobids, db_list_ctx *result);
bool db_get_quota_record(JCR *jcr, B_DB *mdb, CLIENT_DBR *cr);
bool db_get_quota_jobbytes(JCR *jcr, B_DB *mdb, JOB_DBR *jr, utime_t JobRetention);
bool db_get_quota_jobbytes_nofailed(JCR *jcr, B_DB *mdb, JOB_DBR *jr, utime_t JobRetention);
int db_get_ndmp_level_mapping(JCR *jcr, B_DB *mdb, JOB_DBR *jr, char *filesystem);
bool db_get_ndmp_environment_string(JCR *jcr, B_DB *mdb, JOB_DBR *jr,
                                    DB_RESULT_HANDLER *result_handler, void *ctx);

/* sql_list.c */
void db_list_pool_records(JCR *jcr, B_DB *db, POOL_DBR *pr,
                          OUTPUT_FORMATTER *sendit, e_list_type type);
void db_list_job_records(JCR *jcr, B_DB *db, JOB_DBR *jr, const char *range,
                         const char* clientname, int jobstatus, const char* volumename,
                         utime_t since_time, int last, int count,
                         OUTPUT_FORMATTER *sendit, e_list_type type);
void db_list_job_totals(JCR *jcr, B_DB *db, JOB_DBR *jr,
                        OUTPUT_FORMATTER *sendit);
void db_list_files_for_job(JCR *jcr, B_DB *db, uint32_t jobid,
                           OUTPUT_FORMATTER *sendit);
void db_list_filesets(JCR *jcr, B_DB *mdb, JOB_DBR *jr, const char *range,
                      OUTPUT_FORMATTER *sendit, e_list_type type);
void db_list_media_records(JCR *jcr, B_DB *mdb, MEDIA_DBR *mdbr,
                           OUTPUT_FORMATTER *sendit, e_list_type type);
void db_list_jobmedia_records(JCR *jcr, B_DB *mdb, JobId_t JobId,
                              OUTPUT_FORMATTER *sendit, e_list_type type);
void db_list_joblog_records(JCR *jcr, B_DB *mdb, JobId_t JobId,
                            OUTPUT_FORMATTER *sendit, e_list_type type);
void db_list_log_records(JCR *jcr, B_DB *mdb, const char *range, bool reverse,
                         OUTPUT_FORMATTER *sendit, e_list_type type);
bool db_list_sql_query(JCR *jcr, B_DB *mdb, const char *query,
                       OUTPUT_FORMATTER *sendit, e_list_type type,
                       bool verbose);
bool db_list_sql_query(JCR *jcr, B_DB *mdb, const char *query,
                       OUTPUT_FORMATTER *sendit, e_list_type type,
                       const char *description, bool verbose = false);
void db_list_client_records(JCR *jcr, B_DB *mdb, char *clientname,
                            OUTPUT_FORMATTER *sendit, e_list_type type);
void db_list_copies_records(JCR *jcr, B_DB *mdb, const char *range, char *jobids,
                            OUTPUT_FORMATTER *sendit, e_list_type type);
void db_list_base_files_for_job(JCR *jcr, B_DB *mdb, JobId_t jobid,
                                OUTPUT_FORMATTER *sendit);

/* sql_pooling.c */
bool db_sql_pool_initialize(const char *db_drivername,
                            const char *db_name,
                            const char *db_user,
                            const char *db_password,
                            const char *db_address,
                            int db_port,
                            const char *db_socket,
                            bool disable_batch_insert,
                            bool try_reconnect,
                            bool exit_on_fatal,
                            int min_connections,
                            int max_connections,
                            int increment_connections,
                            int idle_timeout,
                            int validate_timeout);
void db_sql_pool_destroy(void);
void db_sql_pool_flush(void);
B_DB *db_sql_get_non_pooled_connection(JCR *jcr,
                                       const char *db_drivername,
                                       const char *db_name,
                                       const char *db_user,
                                       const char *db_password,
                                       const char *db_address,
                                       int db_port,
                                       const char *db_socket,
                                       bool mult_db_connections,
                                       bool disable_batch_insert,
                                       bool try_reconnect,
                                       bool exit_on_fatal,
                                       bool need_private = false);
B_DB *db_sql_get_pooled_connection(JCR *jcr,
                                   const char *db_drivername,
                                   const char *db_name,
                                   const char *db_user,
                                   const char *db_password,
                                   const char *db_address,
                                   int db_port,
                                   const char *db_socket,
                                   bool mult_db_connections,
                                   bool disable_batch_insert,
                                   bool try_reconnect,
                                   bool exit_on_fatal,
                                   bool need_private = false);
void db_sql_close_pooled_connection(JCR *jcr, B_DB *mdb, bool abort = false);

/* sql_update.c */
bool db_update_job_start_record(JCR *jcr, B_DB *db, JOB_DBR *jr);
bool db_update_job_end_record(JCR *jcr, B_DB *db, JOB_DBR *jr);
bool db_update_client_record(JCR *jcr, B_DB *mdb, CLIENT_DBR *cr);
bool db_update_pool_record(JCR *jcr, B_DB *db, POOL_DBR *pr);
bool db_update_storage_record(JCR *jcr, B_DB *mdb, STORAGE_DBR *sr);
bool db_update_media_record(JCR *jcr, B_DB *db, MEDIA_DBR *mr);
bool db_update_media_defaults(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr);
bool db_update_counter_record(JCR *jcr, B_DB *mdb, COUNTER_DBR *cr);
bool db_update_quota_gracetime(JCR *jcr, B_DB *mdb, JOB_DBR *jr);
bool db_update_quota_softlimit(JCR *jcr, B_DB *mdb, JOB_DBR *jr);
bool db_reset_quota_record(JCR *jcr, B_DB *mdb, CLIENT_DBR *jr);
bool db_update_ndmp_level_mapping(JCR *jcr, B_DB *mdb, JOB_DBR *jr, char *filesystem, int level);
bool db_add_digest_to_file_record(JCR *jcr, B_DB *mdb, FileId_t FileId, char *digest, int type);
bool db_mark_file_record(JCR *jcr, B_DB *mdb, FileId_t FileId, JobId_t JobId);
void db_make_inchanger_unique(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr);
int db_update_stats(JCR *jcr, B_DB *mdb, utime_t age);

#endif /* __SQL_PROTOS_H */
