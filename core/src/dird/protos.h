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
bool do_admin_init(JobControlRecord *jcr);
bool do_admin(JobControlRecord *jcr);
void admin_cleanup(JobControlRecord *jcr, int TermCode);

/* archive.c */
bool do_archive_init(JobControlRecord *jcr);
bool do_archive(JobControlRecord *jcr);
void archive_cleanup(JobControlRecord *jcr, int TermCode);

/* authenticate.c */
bool authenticate_with_storage_daemon(JobControlRecord *jcr, StoreResource *store);
bool authenticate_with_file_daemon(JobControlRecord *jcr);
bool authenticate_file_daemon(BareosSocket *fd, char *client_name);
bool authenticate_user_agent(UaContext *ua);

/* autoprune.c */
void do_autoprune(JobControlRecord *jcr);
void prune_volumes(JobControlRecord *jcr, bool InChanger, MediaDbRecord *mr,
                   StoreResource *store);

/* autorecycle.c */
bool find_recycled_volume(JobControlRecord *jcr, bool InChanger, MediaDbRecord *mr,
                          StoreResource *store, const char *unwanted_volumes);
bool recycle_oldest_purged_volume(JobControlRecord *jcr, bool InChanger, MediaDbRecord *mr,
                                  StoreResource *store, const char *unwanted_volumes);
bool recycle_volume(JobControlRecord *jcr, MediaDbRecord *mr);

/* backup.c */
int wait_for_job_termination(JobControlRecord *jcr, int timeout = 0);
bool do_native_backup_init(JobControlRecord *jcr);
bool do_native_backup(JobControlRecord *jcr);
void native_backup_cleanup(JobControlRecord *jcr, int TermCode);
void update_bootstrap_file(JobControlRecord *jcr);
bool send_accurate_current_files(JobControlRecord *jcr);
void generate_backup_summary(JobControlRecord *jcr, ClientDbRecord *cr, int msg_type,
                             const char *term_msg);

char* storage_address_to_contact(ClientResource *client, StoreResource *store);
char* client_address_to_contact(ClientResource *client, StoreResource *store);
char* storage_address_to_contact(StoreResource *rstore, StoreResource *wstore);

/* bsr.c */
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

/* catreq.c */
void catalog_request(JobControlRecord *jcr, BareosSocket *bs);
void catalog_update(JobControlRecord *jcr, BareosSocket *bs);
bool despool_attributes_from_file(JobControlRecord *jcr, const char *file);

/* consolidate.c */
bool do_consolidate_init(JobControlRecord *jcr);
bool do_consolidate(JobControlRecord *jcr);
void consolidate_cleanup(JobControlRecord *jcr, int TermCode);

/* dird_conf.c */
bool print_datatype_schema_json(PoolMem &buffer, int level, const int type,
                                ResourceItem items[], const bool last = false);
#ifdef HAVE_JANSSON
json_t *json_datatype(const int type, ResourceItem items[]);
#endif
const char *auth_protocol_to_str(uint32_t auth_protocol);
const char *level_to_str(int level);
extern "C" char *job_code_callback_director(JobControlRecord *jcr, const char*);
const char *get_configure_usage_string();
void destroy_configure_usage_string();
bool populate_defs();

/* expand.c */
int variable_expansion(JobControlRecord *jcr, char *inp, POOLMEM *&exp);

/* fd_cmds.c */
bool connect_to_file_daemon(JobControlRecord *jcr, int retry_interval, int max_retry_time, bool verbose);
int  send_job_info(JobControlRecord *jcr);
bool send_include_list(JobControlRecord *jcr);
bool send_exclude_list(JobControlRecord *jcr);
bool send_level_command(JobControlRecord *jcr);
bool send_bwlimit_to_fd(JobControlRecord *jcr, const char *Job);
bool send_secure_erase_req_to_fd(JobControlRecord *jcr);
bool send_previous_restore_objects(JobControlRecord *jcr);
int get_attributes_and_put_in_catalog(JobControlRecord *jcr);
void get_attributes_and_compare_to_catalog(JobControlRecord *jcr, JobId_t JobId);
int put_file_into_catalog(JobControlRecord *jcr, long file_index, char *fname,
                          char *link, char *attr, int stream);
int send_runscripts_commands(JobControlRecord *jcr);
bool send_plugin_options(JobControlRecord *jcr);
bool send_restore_objects(JobControlRecord *jcr, JobId_t JobId, bool send_global);
bool cancel_file_daemon_job(UaContext *ua, JobControlRecord *jcr);
void do_native_client_status(UaContext *ua, ClientResource *client, char *cmd);
void do_client_resolve(UaContext *ua, ClientResource *client);
void *handle_filed_connection(CONNECTION_POOL *connections, BareosSocket *fd,
                              char *client_name, int fd_protocol_version);

CONNECTION_POOL *get_client_connections();
bool is_connecting_to_client_allowed(ClientResource *res);
bool is_connecting_to_client_allowed(JobControlRecord *jcr);
bool is_connect_from_client_allowed(ClientResource *res);
bool is_connect_from_client_allowed(JobControlRecord *jcr);
bool use_waiting_client(JobControlRecord *jcr_job, int timeout);

/* getmsg.c */
bool response(JobControlRecord *jcr, BareosSocket *fd, char *resp, const char *cmd, e_prtmsg prtmsg);

/* inc_conf.c */
void find_used_compressalgos(PoolMem* compressalgos, JobControlRecord* jcr);
bool print_incexc_schema_json(PoolMem &buffer, int level,
                              const int type, const bool last = false);
bool print_options_schema_json(PoolMem &buffer, int level,
                               const int type, const bool last = false);
#ifdef HAVE_JANSSON
json_t *json_incexc(const int type);
json_t *json_options(const int type);
#endif


/* job.c */
bool allow_duplicate_job(JobControlRecord *jcr);
void set_jcr_defaults(JobControlRecord *jcr, JobResource *job);
void create_unique_job_name(JobControlRecord *jcr, const char *base_name);
void update_job_end_record(JobControlRecord *jcr);
bool get_or_create_client_record(JobControlRecord *jcr);
bool get_or_create_fileset_record(JobControlRecord *jcr);
DBId_t get_or_create_pool_record(JobControlRecord *jcr, char *pool_name);
bool get_level_since_time(JobControlRecord *jcr);
void apply_pool_overrides(JobControlRecord *jcr, bool force = false);
JobId_t run_job(JobControlRecord *jcr);
bool cancel_job(UaContext *ua, JobControlRecord *jcr);
void get_job_storage(UnifiedStoreResource *store, JobResource *job, RunResource *run);
void init_jcr_job_record(JobControlRecord *jcr);
void update_job_end(JobControlRecord *jcr, int TermCode);
bool setup_job(JobControlRecord *jcr, bool suppress_output = false);
void create_clones(JobControlRecord *jcr);
int create_restore_bootstrap_file(JobControlRecord *jcr);
void dird_free_jcr(JobControlRecord *jcr);
void dird_free_jcr_pointers(JobControlRecord *jcr);
void cancel_storage_daemon_job(JobControlRecord *jcr);
bool run_console_command(JobControlRecord *jcr, const char *cmd);
void sd_msg_thread_send_signal(JobControlRecord *jcr, int sig);

/* jobq.c */
bool inc_read_store(JobControlRecord *jcr);
void dec_read_store(JobControlRecord *jcr);

/* migration.c */
bool do_migration(JobControlRecord *jcr);
bool do_migration_init(JobControlRecord *jcr);
void migration_cleanup(JobControlRecord *jcr, int TermCode);
bool set_migration_wstorage(JobControlRecord *jcr, PoolResource *pool, PoolResource *next_pool, const char *where);

/* mountreq.c */
void mount_request(JobControlRecord *jcr, BareosSocket *bs, char *buf);

/* msgchan.c */
bool start_storage_daemon_job(JobControlRecord *jcr, alist *rstore, alist *wstore,
                              bool send_bsr = false);
bool start_storage_daemon_message_thread(JobControlRecord *jcr);
int bget_dirmsg(BareosSocket *bs, bool allow_any_msg = false);
void wait_for_storage_daemon_termination(JobControlRecord *jcr);

/* ndmp_dma_backup_common.c */
bool fill_backup_environment(JobControlRecord *jcr,
                             IncludeExcludeItem *ie,
                             char *filesystem,
                             struct ndm_job_param *job);
int native_to_ndmp_level(JobControlRecord *jcr, char *filesystem);
void register_callback_hooks(struct ndmlog *ixlog);
void unregister_callback_hooks(struct ndmlog *ixlog);
void process_fhdb(struct ndmlog *ixlog);
void ndmp_backup_cleanup(JobControlRecord *jcr, int TermCode);

/* ndmp_dma_backup.c */
bool do_ndmp_backup_init(JobControlRecord *jcr);
bool do_ndmp_backup(JobControlRecord *jcr);

/* ndmp_dma_backup_NATIVE_NDMP.c */
bool do_ndmp_backup_init_ndmp_native(JobControlRecord *jcr);
bool do_ndmp_backup_ndmp_native(JobControlRecord *jcr);


/* ndmp_dma_generic.c */
bool ndmp_validate_client(JobControlRecord *jcr);
bool ndmp_validate_storage(JobControlRecord *jcr);
void do_ndmp_client_status(UaContext *ua, ClientResource *client, char *cmd);

/* ndmp_dma_restore_common.c */
void add_to_namelist(struct ndm_job_param *job,
                                   char *filename,
                                   const char *restore_prefix,
                                   char *name,
                                   char *other_name,
                                   uint64_t node,
                                   uint64_t fhinfo);
int set_files_to_restore_ndmp_native(JobControlRecord *jcr,
                              struct ndm_job_param *job,
                              int32_t FileIndex,
                              const char *restore_prefix,
                              const char *ndmp_filesystem);
int ndmp_env_handler(void *ctx, int num_fields, char **row);
bool extract_post_restore_stats(JobControlRecord *jcr,
                                struct ndm_session *sess);
void ndmp_restore_cleanup(JobControlRecord *jcr, int TermCode);

/* ndmp_dma_restore_NDMP_BAREOS.c */
bool do_ndmp_restore_init(JobControlRecord *jcr);
bool do_ndmp_restore(JobControlRecord *jcr);


/* ndmp_dma_restore_NDMP_NATIVE.c */
bool do_ndmp_restore_ndmp_native(JobControlRecord *jcr);


/* ndmp_dma_storage.c */
void do_ndmp_storage_status(UaContext *ua, StoreResource *store, char *cmd);
dlist *ndmp_get_vol_list(UaContext *ua, StoreResource *store, bool listall, bool scan);
slot_number_t ndmp_get_num_slots(UaContext *ua, StoreResource *store);
drive_number_t ndmp_get_num_drives(UaContext *ua, StoreResource *store);
bool ndmp_transfer_volume(UaContext *ua, StoreResource *store,
                          slot_number_t src_slot, slot_number_t dst_slot);
bool ndmp_autochanger_volume_operation(UaContext *ua, StoreResource *store, const char *operation,
                                       drive_number_t drive, slot_number_t slot);
bool ndmp_send_label_request(UaContext *ua, StoreResource *store, MediaDbRecord *mr,
                             MediaDbRecord *omr, PoolDbRecord *pr, bool relabel,
                             drive_number_t drive, slot_number_t slot);
char *lookup_ndmp_drive(StoreResource *store, drive_number_t drive);
bool ndmp_update_storage_mappings(JobControlRecord* jcr, StoreResource *store);

/* next_vol.c */
void set_storageid_in_mr(StoreResource *store, MediaDbRecord *mr);
int find_next_volume_for_append(JobControlRecord *jcr, MediaDbRecord *mr, int index,
                                const char *unwanted_volumes, bool create, bool purge);
bool has_volume_expired(JobControlRecord *jcr, MediaDbRecord *mr);
void check_if_volume_valid_or_recyclable(JobControlRecord *jcr, MediaDbRecord *mr, const char **reason);
bool get_scratch_volume(JobControlRecord *jcr, bool InChanger, MediaDbRecord *mr, StoreResource *store);

/* newvol.c */
bool newVolume(JobControlRecord *jcr, MediaDbRecord *mr, StoreResource *store);

/* quota.c */
uint64_t fetch_remaining_quotas(JobControlRecord *jcr);
bool check_hardquotas(JobControlRecord *jcr);
bool check_softquotas(JobControlRecord *jcr);

/* restore.c */
bool do_native_restore(JobControlRecord *jcr);
bool do_native_restore_init(JobControlRecord *jcr);
void native_restore_cleanup(JobControlRecord *jcr, int TermCode);
void generate_restore_summary(JobControlRecord *jcr, int msg_type, const char *term_msg);

/* sd_cmds.c */
bool connect_to_storage_daemon(JobControlRecord *jcr, int retry_interval,
                               int max_retry_time, bool verbose);
BareosSocket *open_sd_bsock(UaContext *ua);
void close_sd_bsock(UaContext *ua);
char *get_volume_name_from_SD(UaContext *ua, slot_number_t Slot, drive_number_t drive);
dlist *native_get_vol_list(UaContext *ua, StoreResource *store, bool listall, bool scan);
slot_number_t native_get_num_slots(UaContext *ua, StoreResource *store);
drive_number_t native_get_num_drives(UaContext *ua, StoreResource *store);
bool cancel_storage_daemon_job(UaContext *ua, StoreResource *store, char *JobId);
bool cancel_storage_daemon_job(UaContext *ua, JobControlRecord *jcr, bool interactive = true);
void cancel_storage_daemon_job(JobControlRecord *jcr);
void do_native_storage_status(UaContext *ua, StoreResource *store, char *cmd);
bool native_transfer_volume(UaContext *ua, StoreResource *store,
                            slot_number_t src_slot, slot_number_t dst_slot);
bool native_autochanger_volume_operation(UaContext *ua, StoreResource *store, const char *operation,
                                         drive_number_t drive, slot_number_t slot);
bool send_bwlimit_to_sd(JobControlRecord *jcr, const char *Job);
bool send_secure_erase_req_to_sd(JobControlRecord *jcr);
bool do_storage_resolve(UaContext *ua, StoreResource *store);
bool do_storage_plugin_options(JobControlRecord *jcr);

/* scheduler.c */
JobControlRecord *wait_for_next_job(char *one_shot_job_to_run);
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
void copy_rwstorage(JobControlRecord *jcr, alist *storage, const char *where);
void set_rwstorage(JobControlRecord *jcr, UnifiedStoreResource *store);
void free_rwstorage(JobControlRecord *jcr);
void copy_wstorage(JobControlRecord *jcr, alist *storage, const char *where);
void set_wstorage(JobControlRecord *jcr, UnifiedStoreResource *store);
void free_wstorage(JobControlRecord *jcr);
void copy_rstorage(JobControlRecord *jcr, alist *storage, const char *where);
void set_rstorage(JobControlRecord *jcr, UnifiedStoreResource *store);
void free_rstorage(JobControlRecord *jcr);
void set_paired_storage(JobControlRecord *jcr);
void free_paired_storage(JobControlRecord *jcr);
bool has_paired_storage(JobControlRecord *jcr);
bool select_next_rstore(JobControlRecord *jcr, bootstrap_info &info);
void storage_status(UaContext *ua, StoreResource *store, char *cmd);
int storage_compare_vol_list_entry(void *e1, void *e2);
changer_vol_list_t *get_vol_list_from_storage(UaContext *ua, StoreResource *store,
                                              bool listall, bool scan, bool cached = true);
slot_number_t get_num_slots(UaContext *ua, StoreResource *store);
drive_number_t get_num_drives(UaContext *ua, StoreResource *store);
bool transfer_volume(UaContext *ua, StoreResource *store,
                     slot_number_t src_slot, slot_number_t dst_slot);
bool do_autochanger_volume_operation(UaContext *ua, StoreResource *store,
                                     const char *operation,
                                     drive_number_t drive, slot_number_t slot);
vol_list_t *vol_is_loaded_in_drive(StoreResource *store, changer_vol_list_t *vol_list, slot_number_t slot);
void storage_release_vol_list(StoreResource *store, changer_vol_list_t *vol_list);
void storage_free_vol_list(StoreResource *store, changer_vol_list_t *vol_list);
void invalidate_vol_list(StoreResource *store);
int compare_storage_mapping(void *e1, void *e2);
slot_number_t lookup_storage_mapping(StoreResource *store, slot_type slot_type,
                                     s_mapping_type map_type, slot_number_t slot);

/* ua_cmds.c */
bool do_a_command(UaContext *ua);
bool dot_messages_cmd(UaContext *ua, const char *cmd);

/* ua_db.c */
bool open_client_db(UaContext *ua, bool use_private = false);
bool open_db(UaContext *ua, bool use_private = false);
void close_db(UaContext *ua);
int create_pool(JobControlRecord *jcr, BareosDb *db, PoolResource *pool, e_pool_op op);
void set_pool_dbr_defaults_in_media_dbr(MediaDbRecord *mr, PoolDbRecord *pr);
bool set_pooldbr_references(JobControlRecord *jcr, BareosDb *db, PoolDbRecord *pr, PoolResource *pool);
void set_pooldbr_from_poolres(PoolDbRecord *pr, PoolResource *pool, e_pool_op op);
int update_pool_references(JobControlRecord *jcr, BareosDb *db, PoolResource *pool);

/* ua_impexp.c */
bool import_cmd(UaContext *ua, const char *cmd);
bool export_cmd(UaContext *ua, const char *cmd);
bool move_cmd(UaContext *ua, const char *cmd);

/* ua_input.c */
bool get_cmd(UaContext *ua, const char *prompt, bool subprompt = false);
bool get_pint(UaContext *ua, const char *prompt);
bool get_yesno(UaContext *ua, const char *prompt);
bool is_yesno(char *val, bool *ret);
bool get_confirmation(UaContext *ua, const char *prompt = _("Confirm (yes/no): "));
int get_enabled(UaContext *ua, const char *val);
void parse_ua_args(UaContext *ua);
bool is_comment_legal(UaContext *ua, const char *name);

/* ua_label.c */
bool is_volume_name_legal(UaContext *ua, const char *name);
bool send_label_request(UaContext *ua,
                        StoreResource *store, MediaDbRecord *mr, MediaDbRecord *omr, PoolDbRecord *pr,
                        bool media_record_exists, bool relabel,
                        drive_number_t drive, slot_number_t slot);

/* ua_update.c */
void update_vol_pool(UaContext *ua, char *val, MediaDbRecord *mr, PoolDbRecord *opr);
void update_slots_from_vol_list(UaContext *ua, StoreResource *store, changer_vol_list_t *vol_list, char *slot_list);
void update_inchanger_for_export(UaContext *ua, StoreResource *store, changer_vol_list_t *vol_list, char *slot_list);

/* ua_output.c */
void bsendmsg(void *ua_ctx, const char *fmt, ...);
of_filter_state filterit(void *ctx, void *data, of_filter_tuple *tuple);
bool printit(void *ctx, const char *msg);
bool complete_jcr_for_job(JobControlRecord *jcr, JobResource *job, PoolResource *pool);
RunResource *find_next_run(RunResource *run, JobResource *job, utime_t &runtime, int ndays);

/* ua_restore.c */
void find_storage_resource(UaContext *ua, RestoreContext &rx, char *Storage, char *MediaType);

/* ua_server.c */
void *handle_UA_client_request(BareosSocket *user);
UaContext *new_ua_context(JobControlRecord *jcr);
JobControlRecord *new_control_jcr(const char *base_name, int job_type);
void free_ua_context(UaContext *ua);

/* ua_select.c */
StoreResource *select_storage_resource(UaContext *ua, bool autochanger_only = false);
JobResource *select_job_resource(UaContext *ua);
JobResource *select_enable_disable_job_resource(UaContext *ua, bool enable);
JobResource *select_restore_job_resource(UaContext *ua);
ClientResource *select_client_resource(UaContext *ua);
ClientResource *select_enable_disable_client_resource(UaContext *ua, bool enable);
FilesetResource *select_fileset_resource(UaContext *ua);
ScheduleResource *select_enable_disable_schedule_resource(UaContext *ua, bool enable);
bool select_pool_and_media_dbr(UaContext *ua, PoolDbRecord *pr, MediaDbRecord *mr);
bool select_media_dbr(UaContext *ua, MediaDbRecord *mr);
bool select_pool_dbr(UaContext *ua, PoolDbRecord *pr, const char *argk = "pool");
bool select_storage_dbr(UaContext *ua, StorageDbRecord *pr, const char *argk = "storage");
bool select_client_dbr(UaContext *ua, ClientDbRecord *cr);

void start_prompt(UaContext *ua, const char *msg);
void add_prompt(UaContext *ua, const char *prompt);
int do_prompt(UaContext *ua, const char *automsg, const char *msg, char *prompt, int max_prompt);
CatalogResource *get_catalog_resource(UaContext *ua);
StoreResource *get_storage_resource(UaContext *ua, bool use_default = false,
                               bool autochanger_only = false);
drive_number_t get_storage_drive(UaContext *ua, StoreResource *store);
slot_number_t get_storage_slot(UaContext *ua, StoreResource *store);
int get_media_type(UaContext *ua, char *MediaType, int max_media);
bool get_pool_dbr(UaContext *ua, PoolDbRecord *pr, const char *argk = "pool");
bool get_storage_dbr(UaContext *ua, StorageDbRecord *sr, const char *argk = "storage");
bool get_client_dbr(UaContext *ua, ClientDbRecord *cr);
PoolResource *get_pool_resource(UaContext *ua);
JobResource *get_restore_job(UaContext *ua);
PoolResource *select_pool_resource(UaContext *ua);
alist *select_jobs(UaContext *ua, const char *reason);
ClientResource *get_client_resource(UaContext *ua);
int get_job_dbr(UaContext *ua, JobDbRecord *jr);
bool get_user_slot_list(UaContext *ua, char *slot_list, const char *argument, int num_slots);
bool get_user_job_type_selection(UaContext *ua, int *jobtype);
bool get_user_job_status_selection(UaContext *ua, int *jobstatus);
bool get_user_job_level_selection(UaContext *ua, int *joblevel);

int find_arg_keyword(UaContext *ua, const char **list);
int find_arg(UaContext *ua, const char *keyword);
int find_arg_with_value(UaContext *ua, const char *keyword);
int do_keyword_prompt(UaContext *ua, const char *msg, const char **list);
bool confirm_retention(UaContext *ua, utime_t *ret, const char *msg);
bool get_level_from_name(JobControlRecord *jcr, const char *level_name);

/* ua_status.c */
void list_dir_status_header(UaContext *ua);

/* ua_tree.c */
bool user_select_files_from_tree(TreeContext *tree);
int insert_tree_handler(void *ctx, int num_fields, char **row);

/* ua_prune.c */
bool prune_files(UaContext *ua, ClientResource *client, PoolResource *pool);
bool prune_jobs(UaContext *ua, ClientResource *client, PoolResource *pool, int JobType);
bool prune_volume(UaContext *ua, MediaDbRecord *mr);
int job_delete_handler(void *ctx, int num_fields, char **row);
int del_count_handler(void *ctx, int num_fields, char **row);
int file_delete_handler(void *ctx, int num_fields, char **row);
int get_prune_list_for_volume(UaContext *ua, MediaDbRecord *mr, del_ctx *del);
int exclude_running_jobs_from_list(del_ctx *prune_list);

/* ua_purge.c */
bool is_volume_purged(UaContext *ua, MediaDbRecord *mr, bool force = false);
bool mark_media_purged(UaContext *ua, MediaDbRecord *mr);
void purge_files_from_volume(UaContext *ua, MediaDbRecord *mr);
bool purge_jobs_from_volume(UaContext *ua, MediaDbRecord *mr, bool force = false);
void purge_files_from_jobs(UaContext *ua, char *jobs);
void purge_jobs_from_catalog(UaContext *ua, char *jobs);
void purge_job_list_from_catalog(UaContext *ua, del_ctx &del);
void purge_files_from_job_list(UaContext *ua, del_ctx &del);

/* ua_run.c */
bool rerun_cmd(UaContext *ua, const char *cmd);
bool run_cmd(UaContext *ua, const char *cmd);
int do_run_cmd(UaContext *ua, const char *cmd);

/* vbackup.c */
bool do_native_vbackup_init(JobControlRecord *jcr);
bool do_native_vbackup(JobControlRecord *jcr);
void native_vbackup_cleanup(JobControlRecord *jcr, int TermCode, int JobLevel = L_FULL);

/* verify.c */
bool do_verify(JobControlRecord *jcr);
bool do_verify_init(JobControlRecord *jcr);
void verify_cleanup(JobControlRecord *jcr, int TermCode);
