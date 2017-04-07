/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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
/**
 * @file
 * Director external function prototypes
 */

/* admin.c */
bool do_admin_init(JCR *jcr);
bool do_admin(JCR *jcr);
void admin_cleanup(JCR *jcr, int TermCode);

/* archive.c */
bool do_archive_init(JCR *jcr);
bool do_archive(JCR *jcr);
void archive_cleanup(JCR *jcr, int TermCode);

/* authenticate.c */
bool authenticate_with_storage_daemon(JCR *jcr, STORERES *store);
bool authenticate_with_file_daemon(JCR *jcr);
bool authenticate_file_daemon(BSOCK *fd, char *client_name);
bool authenticate_user_agent(UAContext *ua);

/* autoprune.c */
void do_autoprune(JCR *jcr);
void prune_volumes(JCR *jcr, bool InChanger, MEDIA_DBR *mr,
                   STORERES *store);

/* autorecycle.c */
bool find_recycled_volume(JCR *jcr, bool InChanger, MEDIA_DBR *mr,
                          STORERES *store, const char *unwanted_volumes);
bool recycle_oldest_purged_volume(JCR *jcr, bool InChanger, MEDIA_DBR *mr,
                                  STORERES *store, const char *unwanted_volumes);
bool recycle_volume(JCR *jcr, MEDIA_DBR *mr);

/* backup.c */
int wait_for_job_termination(JCR *jcr, int timeout = 0);
bool do_native_backup_init(JCR *jcr);
bool do_native_backup(JCR *jcr);
void native_backup_cleanup(JCR *jcr, int TermCode);
void update_bootstrap_file(JCR *jcr);
bool send_accurate_current_files(JCR *jcr);
void generate_backup_summary(JCR *jcr, CLIENT_DBR *cr, int msg_type,
                             const char *term_msg);

char* storage_address_to_contact(CLIENTRES *client, STORERES *store);
char* client_address_to_contact(CLIENTRES *client, STORERES *store);
char* storage_address_to_contact(STORERES *rstore, STORERES *wstore);

/* bsr.c */
RBSR *new_bsr();
void free_bsr(RBSR *bsr);
bool complete_bsr(UAContext *ua, RBSR *bsr);
uint32_t write_bsr_file(UAContext *ua, RESTORE_CTX &rx);
void display_bsr_info(UAContext *ua, RESTORE_CTX &rx);
uint32_t write_bsr(UAContext *ua, RESTORE_CTX &rx, POOL_MEM *buffer);
void add_findex(RBSR *bsr, uint32_t JobId, int32_t findex);
void add_findex_all(RBSR *bsr, uint32_t JobId);
RBSR_FINDEX *new_findex();
void make_unique_restore_filename(UAContext *ua, POOLMEM *&fname);
void print_bsr(UAContext *ua, RESTORE_CTX &rx);
bool open_bootstrap_file(JCR *jcr, bootstrap_info &info);
bool send_bootstrap_file(JCR *jcr, BSOCK *sock, bootstrap_info &info);
void close_bootstrap_file(bootstrap_info &info);

/* catreq.c */
void catalog_request(JCR *jcr, BSOCK *bs);
void catalog_update(JCR *jcr, BSOCK *bs);
bool despool_attributes_from_file(JCR *jcr, const char *file);

/* consolidate.c */
bool do_consolidate_init(JCR *jcr);
bool do_consolidate(JCR *jcr);
void consolidate_cleanup(JCR *jcr, int TermCode);

/* dird_conf.c */
bool print_datatype_schema_json(POOL_MEM &buffer, int level, const int type,
                                RES_ITEM items[], const bool last = false);
#ifdef HAVE_JANSSON
json_t *json_datatype(const int type, RES_ITEM items[]);
#endif
const char *auth_protocol_to_str(uint32_t auth_protocol);
const char *level_to_str(int level);
extern "C" char *job_code_callback_director(JCR *jcr, const char*);
const char *get_configure_usage_string();
void destroy_configure_usage_string();
bool populate_defs();

/* expand.c */
int variable_expansion(JCR *jcr, char *inp, POOLMEM *&exp);

/* fd_cmds.c */
bool connect_to_file_daemon(JCR *jcr, int retry_interval, int max_retry_time, bool verbose);
int  send_job_info(JCR *jcr);
bool send_include_list(JCR *jcr);
bool send_exclude_list(JCR *jcr);
bool send_level_command(JCR *jcr);
bool send_bwlimit_to_fd(JCR *jcr, const char *Job);
bool send_secure_erase_req_to_fd(JCR *jcr);
bool send_previous_restore_objects(JCR *jcr);
int get_attributes_and_put_in_catalog(JCR *jcr);
void get_attributes_and_compare_to_catalog(JCR *jcr, JobId_t JobId);
int put_file_into_catalog(JCR *jcr, long file_index, char *fname,
                          char *link, char *attr, int stream);
int send_runscripts_commands(JCR *jcr);
bool send_plugin_options(JCR *jcr);
bool send_restore_objects(JCR *jcr, JobId_t JobId, bool send_global);
bool cancel_file_daemon_job(UAContext *ua, JCR *jcr);
void do_native_client_status(UAContext *ua, CLIENTRES *client, char *cmd);
void do_client_resolve(UAContext *ua, CLIENTRES *client);
void *handle_filed_connection(CONNECTION_POOL *connections, BSOCK *fd,
                              char *client_name, int fd_protocol_version);

CONNECTION_POOL *get_client_connections();
bool is_connecting_to_client_allowed(CLIENTRES *res);
bool is_connecting_to_client_allowed(JCR *jcr);
bool is_connect_from_client_allowed(CLIENTRES *res);
bool is_connect_from_client_allowed(JCR *jcr);
bool use_waiting_client(JCR *jcr_job, int timeout);

/* getmsg.c */
bool response(JCR *jcr, BSOCK *fd, char *resp, const char *cmd, e_prtmsg prtmsg);

/* inc_conf.c */
void find_used_compressalgos(POOL_MEM* compressalgos, JCR* jcr);
bool print_incexc_schema_json(POOL_MEM &buffer, int level,
                              const int type, const bool last = false);
bool print_options_schema_json(POOL_MEM &buffer, int level,
                               const int type, const bool last = false);
#ifdef HAVE_JANSSON
json_t *json_incexc(const int type);
json_t *json_options(const int type);
#endif


/* job.c */
bool allow_duplicate_job(JCR *jcr);
void set_jcr_defaults(JCR *jcr, JOBRES *job);
void create_unique_job_name(JCR *jcr, const char *base_name);
void update_job_end_record(JCR *jcr);
bool get_or_create_client_record(JCR *jcr);
bool get_or_create_fileset_record(JCR *jcr);
DBId_t get_or_create_pool_record(JCR *jcr, char *pool_name);
bool get_level_since_time(JCR *jcr);
void apply_pool_overrides(JCR *jcr, bool force = false);
JobId_t run_job(JCR *jcr);
bool cancel_job(UAContext *ua, JCR *jcr);
void get_job_storage(USTORERES *store, JOBRES *job, RUNRES *run);
void init_jcr_job_record(JCR *jcr);
void update_job_end(JCR *jcr, int TermCode);
bool setup_job(JCR *jcr, bool suppress_output = false);
void create_clones(JCR *jcr);
int create_restore_bootstrap_file(JCR *jcr);
void dird_free_jcr(JCR *jcr);
void dird_free_jcr_pointers(JCR *jcr);
void cancel_storage_daemon_job(JCR *jcr);
bool run_console_command(JCR *jcr, const char *cmd);
void sd_msg_thread_send_signal(JCR *jcr, int sig);

/* jobq.c */
bool inc_read_store(JCR *jcr);
void dec_read_store(JCR *jcr);

/* migration.c */
bool do_migration(JCR *jcr);
bool do_migration_init(JCR *jcr);
void migration_cleanup(JCR *jcr, int TermCode);
bool set_migration_wstorage(JCR *jcr, POOLRES *pool, POOLRES *next_pool, const char *where);

/* mountreq.c */
void mount_request(JCR *jcr, BSOCK *bs, char *buf);

/* msgchan.c */
bool start_storage_daemon_job(JCR *jcr, alist *rstore, alist *wstore,
                              bool send_bsr = false);
bool start_storage_daemon_message_thread(JCR *jcr);
int bget_dirmsg(BSOCK *bs, bool allow_any_msg = false);
void wait_for_storage_daemon_termination(JCR *jcr);

/* ndmp_dma_backup.c */
bool do_ndmp_backup_init(JCR *jcr);
bool do_ndmp_backup(JCR *jcr);
void ndmp_backup_cleanup(JCR *jcr, int TermCode);

/* ndmp_dma_generic.c */
bool ndmp_validate_client(JCR *jcr);
bool ndmp_validate_storage(JCR *jcr);
void do_ndmp_client_status(UAContext *ua, CLIENTRES *client, char *cmd);

/* ndmp_dma_restore.c */
bool do_ndmp_restore_init(JCR *jcr);
bool do_ndmp_restore(JCR *jcr);
void ndmp_restore_cleanup(JCR *jcr, int TermCode);

/* ndmp_dma_storage.c */
void do_ndmp_storage_status(UAContext *ua, STORERES *store, char *cmd);
dlist *ndmp_get_vol_list(UAContext *ua, STORERES *store, bool listall, bool scan);
slot_number_t ndmp_get_num_slots(UAContext *ua, STORERES *store);
drive_number_t ndmp_get_num_drives(UAContext *ua, STORERES *store);
bool ndmp_transfer_volume(UAContext *ua, STORERES *store,
                          slot_number_t src_slot, slot_number_t dst_slot);
bool ndmp_autochanger_volume_operation(UAContext *ua, STORERES *store, const char *operation,
                                       drive_number_t drive, slot_number_t slot);
bool ndmp_send_label_request(UAContext *ua, STORERES *store, MEDIA_DBR *mr,
                             MEDIA_DBR *omr, POOL_DBR *pr, bool relabel,
                             drive_number_t drive, slot_number_t slot);

/* next_vol.c */
void set_storageid_in_mr(STORERES *store, MEDIA_DBR *mr);
int find_next_volume_for_append(JCR *jcr, MEDIA_DBR *mr, int index,
                                const char *unwanted_volumes, bool create, bool purge);
bool has_volume_expired(JCR *jcr, MEDIA_DBR *mr);
void check_if_volume_valid_or_recyclable(JCR *jcr, MEDIA_DBR *mr, const char **reason);
bool get_scratch_volume(JCR *jcr, bool InChanger, MEDIA_DBR *mr, STORERES *store);

/* newvol.c */
bool newVolume(JCR *jcr, MEDIA_DBR *mr, STORERES *store);

/* quota.c */
uint64_t fetch_remaining_quotas(JCR *jcr);
bool check_hardquotas(JCR *jcr);
bool check_softquotas(JCR *jcr);

/* restore.c */
bool do_native_restore(JCR *jcr);
bool do_native_restore_init(JCR *jcr);
void native_restore_cleanup(JCR *jcr, int TermCode);
void generate_restore_summary(JCR *jcr, int msg_type, const char *term_msg);

/* sd_cmds.c */
bool connect_to_storage_daemon(JCR *jcr, int retry_interval,
                               int max_retry_time, bool verbose);
BSOCK *open_sd_bsock(UAContext *ua);
void close_sd_bsock(UAContext *ua);
char *get_volume_name_from_SD(UAContext *ua, slot_number_t Slot, drive_number_t drive);
dlist *native_get_vol_list(UAContext *ua, STORERES *store, bool listall, bool scan);
slot_number_t native_get_num_slots(UAContext *ua, STORERES *store);
drive_number_t native_get_num_drives(UAContext *ua, STORERES *store);
bool cancel_storage_daemon_job(UAContext *ua, STORERES *store, char *JobId);
bool cancel_storage_daemon_job(UAContext *ua, JCR *jcr, bool interactive = true);
void cancel_storage_daemon_job(JCR *jcr);
void do_native_storage_status(UAContext *ua, STORERES *store, char *cmd);
bool native_transfer_volume(UAContext *ua, STORERES *store,
                            slot_number_t src_slot, slot_number_t dst_slot);
bool native_autochanger_volume_operation(UAContext *ua, STORERES *store, const char *operation,
                                         drive_number_t drive, slot_number_t slot);
bool send_bwlimit_to_sd(JCR *jcr, const char *Job);
bool send_secure_erase_req_to_sd(JCR *jcr);
bool do_storage_resolve(UAContext *ua, STORERES *store);
bool do_storage_plugin_options(JCR *jcr);

/* scheduler.c */
JCR *wait_for_next_job(char *one_shot_job_to_run);
bool is_doy_in_last_week(int year, int doy);
void term_scheduler();

/* socket_server.c */
void start_socket_server(dlist *addrs);
void stop_socket_server();

/* stats.c */
int start_statistics_thread(void);
void stop_statistics_thread();
void stats_job_started();

/* storage.c */
void copy_rwstorage(JCR *jcr, alist *storage, const char *where);
void set_rwstorage(JCR *jcr, USTORERES *store);
void free_rwstorage(JCR *jcr);
void copy_wstorage(JCR *jcr, alist *storage, const char *where);
void set_wstorage(JCR *jcr, USTORERES *store);
void free_wstorage(JCR *jcr);
void copy_rstorage(JCR *jcr, alist *storage, const char *where);
void set_rstorage(JCR *jcr, USTORERES *store);
void free_rstorage(JCR *jcr);
void set_paired_storage(JCR *jcr);
void free_paired_storage(JCR *jcr);
bool has_paired_storage(JCR *jcr);
bool select_next_rstore(JCR *jcr, bootstrap_info &info);
void storage_status(UAContext *ua, STORERES *store, char *cmd);
int storage_compare_vol_list_entry(void *e1, void *e2);
changer_vol_list_t *get_vol_list_from_storage(UAContext *ua, STORERES *store,
                                              bool listall, bool scan, bool cached = true);
slot_number_t get_num_slots(UAContext *ua, STORERES *store);
drive_number_t get_num_drives(UAContext *ua, STORERES *store);
bool transfer_volume(UAContext *ua, STORERES *store,
                     slot_number_t src_slot, slot_number_t dst_slot);
bool do_autochanger_volume_operation(UAContext *ua, STORERES *store,
                                     const char *operation,
                                     drive_number_t drive, slot_number_t slot);
vol_list_t *vol_is_loaded_in_drive(STORERES *store, changer_vol_list_t *vol_list, slot_number_t slot);
void storage_release_vol_list(STORERES *store, changer_vol_list_t *vol_list);
void storage_free_vol_list(STORERES *store, changer_vol_list_t *vol_list);
void invalidate_vol_list(STORERES *store);
int compare_storage_mapping(void *e1, void *e2);
slot_number_t lookup_storage_mapping(STORERES *store, slot_type slot_type,
                                     s_mapping_type map_type, slot_number_t slot);

/* ua_cmds.c */
bool do_a_command(UAContext *ua);
bool dot_messages_cmd(UAContext *ua, const char *cmd);

/* ua_db.c */
bool open_client_db(UAContext *ua, bool use_private = false);
bool open_db(UAContext *ua, bool use_private = false);
void close_db(UAContext *ua);
int create_pool(JCR *jcr, B_DB *db, POOLRES *pool, e_pool_op op);
void set_pool_dbr_defaults_in_media_dbr(MEDIA_DBR *mr, POOL_DBR *pr);
bool set_pooldbr_references(JCR *jcr, B_DB *db, POOL_DBR *pr, POOLRES *pool);
void set_pooldbr_from_poolres(POOL_DBR *pr, POOLRES *pool, e_pool_op op);
int update_pool_references(JCR *jcr, B_DB *db, POOLRES *pool);

/* ua_impexp.c */
bool import_cmd(UAContext *ua, const char *cmd);
bool export_cmd(UAContext *ua, const char *cmd);
bool move_cmd(UAContext *ua, const char *cmd);

/* ua_input.c */
bool get_cmd(UAContext *ua, const char *prompt, bool subprompt = false);
bool get_pint(UAContext *ua, const char *prompt);
bool get_yesno(UAContext *ua, const char *prompt);
bool is_yesno(char *val, bool *ret);
bool get_confirmation(UAContext *ua, const char *prompt = _("Confirm (yes/no): "));
int get_enabled(UAContext *ua, const char *val);
void parse_ua_args(UAContext *ua);
bool is_comment_legal(UAContext *ua, const char *name);

/* ua_label.c */
bool is_volume_name_legal(UAContext *ua, const char *name);
bool send_label_request(UAContext *ua,
                        STORERES *store, MEDIA_DBR *mr, MEDIA_DBR *omr, POOL_DBR *pr,
                        bool media_record_exists, bool relabel,
                        drive_number_t drive, slot_number_t slot);

/* ua_update.c */
void update_vol_pool(UAContext *ua, char *val, MEDIA_DBR *mr, POOL_DBR *opr);
void update_slots_from_vol_list(UAContext *ua, STORERES *store, changer_vol_list_t *vol_list, char *slot_list);
void update_inchanger_for_export(UAContext *ua, STORERES *store, changer_vol_list_t *vol_list, char *slot_list);

/* ua_output.c */
void bsendmsg(void *ua_ctx, const char *fmt, ...);
of_filter_state filterit(void *ctx, void *data, of_filter_tuple *tuple);
bool printit(void *ctx, const char *msg);
bool complete_jcr_for_job(JCR *jcr, JOBRES *job, POOLRES *pool);
RUNRES *find_next_run(RUNRES *run, JOBRES *job, utime_t &runtime, int ndays);

/* ua_restore.c */
void find_storage_resource(UAContext *ua, RESTORE_CTX &rx, char *Storage, char *MediaType);

/* ua_server.c */
void *handle_UA_client_request(BSOCK *user);
UAContext *new_ua_context(JCR *jcr);
JCR *new_control_jcr(const char *base_name, int job_type);
void free_ua_context(UAContext *ua);

/* ua_select.c */
STORERES *select_storage_resource(UAContext *ua, bool autochanger_only = false);
JOBRES *select_job_resource(UAContext *ua);
JOBRES *select_enable_disable_job_resource(UAContext *ua, bool enable);
JOBRES *select_restore_job_resource(UAContext *ua);
CLIENTRES *select_client_resource(UAContext *ua);
CLIENTRES *select_enable_disable_client_resource(UAContext *ua, bool enable);
FILESETRES *select_fileset_resource(UAContext *ua);
SCHEDRES *select_enable_disable_schedule_resource(UAContext *ua, bool enable);
bool select_pool_and_media_dbr(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr);
bool select_media_dbr(UAContext *ua, MEDIA_DBR *mr);
bool select_pool_dbr(UAContext *ua, POOL_DBR *pr, const char *argk = "pool");
bool select_storage_dbr(UAContext *ua, STORAGE_DBR *pr, const char *argk = "storage");
bool select_client_dbr(UAContext *ua, CLIENT_DBR *cr);

void start_prompt(UAContext *ua, const char *msg);
void add_prompt(UAContext *ua, const char *prompt);
int do_prompt(UAContext *ua, const char *automsg, const char *msg, char *prompt, int max_prompt);
CATRES *get_catalog_resource(UAContext *ua);
STORERES *get_storage_resource(UAContext *ua, bool use_default = false,
                               bool autochanger_only = false);
drive_number_t get_storage_drive(UAContext *ua, STORERES *store);
slot_number_t get_storage_slot(UAContext *ua, STORERES *store);
int get_media_type(UAContext *ua, char *MediaType, int max_media);
bool get_pool_dbr(UAContext *ua, POOL_DBR *pr, const char *argk = "pool");
bool get_storage_dbr(UAContext *ua, STORAGE_DBR *sr, const char *argk = "storage");
bool get_client_dbr(UAContext *ua, CLIENT_DBR *cr);
POOLRES *get_pool_resource(UAContext *ua);
JOBRES *get_restore_job(UAContext *ua);
POOLRES *select_pool_resource(UAContext *ua);
alist *select_jobs(UAContext *ua, const char *reason);
CLIENTRES *get_client_resource(UAContext *ua);
int get_job_dbr(UAContext *ua, JOB_DBR *jr);
bool get_user_slot_list(UAContext *ua, char *slot_list, const char *argument, int num_slots);
bool get_user_job_type_selection(UAContext *ua, int *jobtype);
bool get_user_job_status_selection(UAContext *ua, int *jobstatus);

int find_arg_keyword(UAContext *ua, const char **list);
int find_arg(UAContext *ua, const char *keyword);
int find_arg_with_value(UAContext *ua, const char *keyword);
int do_keyword_prompt(UAContext *ua, const char *msg, const char **list);
bool confirm_retention(UAContext *ua, utime_t *ret, const char *msg);
bool get_level_from_name(JCR *jcr, const char *level_name);

/* ua_status.c */
void list_dir_status_header(UAContext *ua);

/* ua_tree.c */
bool user_select_files_from_tree(TREE_CTX *tree);
int insert_tree_handler(void *ctx, int num_fields, char **row);

/* ua_prune.c */
bool prune_files(UAContext *ua, CLIENTRES *client, POOLRES *pool);
bool prune_jobs(UAContext *ua, CLIENTRES *client, POOLRES *pool, int JobType);
bool prune_volume(UAContext *ua, MEDIA_DBR *mr);
int job_delete_handler(void *ctx, int num_fields, char **row);
int del_count_handler(void *ctx, int num_fields, char **row);
int file_delete_handler(void *ctx, int num_fields, char **row);
int get_prune_list_for_volume(UAContext *ua, MEDIA_DBR *mr, del_ctx *del);
int exclude_running_jobs_from_list(del_ctx *prune_list);

/* ua_purge.c */
bool is_volume_purged(UAContext *ua, MEDIA_DBR *mr, bool force = false);
bool mark_media_purged(UAContext *ua, MEDIA_DBR *mr);
void purge_files_from_volume(UAContext *ua, MEDIA_DBR *mr);
bool purge_jobs_from_volume(UAContext *ua, MEDIA_DBR *mr, bool force = false);
void purge_files_from_jobs(UAContext *ua, char *jobs);
void purge_jobs_from_catalog(UAContext *ua, char *jobs);
void purge_job_list_from_catalog(UAContext *ua, del_ctx &del);
void purge_files_from_job_list(UAContext *ua, del_ctx &del);

/* ua_run.c */
bool rerun_cmd(UAContext *ua, const char *cmd);
bool run_cmd(UAContext *ua, const char *cmd);
int do_run_cmd(UAContext *ua, const char *cmd);

/* vbackup.c */
bool do_native_vbackup_init(JCR *jcr);
bool do_native_vbackup(JCR *jcr);
void native_vbackup_cleanup(JCR *jcr, int TermCode, int JobLevel = L_FULL);

/* verify.c */
bool do_verify(JCR *jcr);
bool do_verify_init(JCR *jcr);
void verify_cleanup(JCR *jcr, int TermCode);
