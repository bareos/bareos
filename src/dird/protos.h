/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Director external function prototypes
 */

/* admin.c */
bool do_admin_init(JCR *jcr);
bool do_admin(JCR *jcr);
void admin_cleanup(JCR *jcr, int TermCode);

/* authenticate.c */
bool authenticate_storage_daemon(JCR *jcr, STORERES *store);
int authenticate_file_daemon(JCR *jcr);
int authenticate_user_agent(UAContext *ua);

/* autoprune.c */
void do_autoprune(JCR *jcr);
void prune_volumes(JCR *jcr, bool InChanger, MEDIA_DBR *mr,
                   STORERES *store);

/* autorecycle.c */
bool recycle_oldest_purged_volume(JCR *jcr, bool InChanger,
                                  MEDIA_DBR *mr, STORERES *store);
int recycle_volume(JCR *jcr, MEDIA_DBR *mr);
bool find_recycled_volume(JCR *jcr, bool InChanger,
                          MEDIA_DBR *mr, STORERES *store);

/* backup.c */
int wait_for_job_termination(JCR *jcr, int timeout = 0);
bool do_native_backup_init(JCR *jcr);
bool do_native_backup(JCR *jcr);
void native_backup_cleanup(JCR *jcr, int TermCode);
void update_bootstrap_file(JCR *jcr);
bool send_accurate_current_files(JCR *jcr);
void generate_backup_summary(JCR *jcr, MEDIA_DBR *mr, CLIENT_DBR *cr,
                             int msg_type, const char *term_msg);

/* vbackup.c */
bool do_native_vbackup_init(JCR *jcr);
bool do_native_vbackup(JCR *jcr);
void native_vbackup_cleanup(JCR *jcr, int TermCode);

/* bsr.c */
RBSR *new_bsr();
void free_bsr(RBSR *bsr);
bool complete_bsr(UAContext *ua, RBSR *bsr);
uint32_t write_bsr_file(UAContext *ua, RESTORE_CTX &rx);
void display_bsr_info(UAContext *ua, RESTORE_CTX &rx);
void add_findex(RBSR *bsr, uint32_t JobId, int32_t findex);
void add_findex_all(RBSR *bsr, uint32_t JobId);
RBSR_FINDEX *new_findex();
void make_unique_restore_filename(UAContext *ua, POOLMEM **fname);
void print_bsr(UAContext *ua, RESTORE_CTX &rx);
bool open_bootstrap_file(JCR *jcr, bootstrap_info &info);
bool send_bootstrap_file(JCR *jcr, BSOCK *sock, bootstrap_info &info);
void close_bootstrap_file(bootstrap_info &info);

/* catreq.c */
void catalog_request(JCR *jcr, BSOCK *bs);
void catalog_update(JCR *jcr, BSOCK *bs);
bool despool_attributes_from_file(JCR *jcr, const char *file);

/* dird_conf.c */
const char *level_to_str(int level);
extern "C" char *job_code_callback_director(JCR *jcr, const char*);

/* expand.c */
int variable_expansion(JCR *jcr, char *inp, POOLMEM **exp);

/* fd_cmds.c */
int connect_to_file_daemon(JCR *jcr, int retry_interval,
                                  int max_retry_time, int verbose);
bool send_include_list(JCR *jcr);
bool send_exclude_list(JCR *jcr);
bool send_level_command(JCR *jcr);
bool send_bwlimit(JCR *jcr, const char *Job);
int get_attributes_and_put_in_catalog(JCR *jcr);
void get_attributes_and_compare_to_catalog(JCR *jcr, JobId_t JobId);
int put_file_into_catalog(JCR *jcr, long file_index, char *fname,
                          char *link, char *attr, int stream);
int send_runscripts_commands(JCR *jcr);
bool send_restore_objects(JCR *jcr);
bool cancel_file_daemon_job(UAContext *ua, JCR *jcr);
void do_native_client_status(UAContext *ua, CLIENTRES *client, char *cmd);

/* getmsg.c */
bool response(JCR *jcr, BSOCK *fd, char *resp, const char *cmd, e_prtmsg prtmsg);

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
void copy_rwstorage(JCR *jcr, alist *storage, const char *where);
void set_rwstorage(JCR *jcr, USTORERES *store);
void free_rwstorage(JCR *jcr);
void copy_wstorage(JCR *jcr, alist *storage, const char *where);
void set_wstorage(JCR *jcr, USTORERES *store);
void free_wstorage(JCR *jcr);
void copy_rstorage(JCR *jcr, alist *storage, const char *where);
void set_rstorage(JCR *jcr, USTORERES *store);
void free_rstorage(JCR *jcr);
bool select_next_rstore(JCR *jcr, bootstrap_info &info);
void set_paired_storage(JCR *jcr);
void free_paired_storage(JCR *jcr);
bool has_paired_storage(JCR *jcr);
bool setup_job(JCR *jcr);
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

/* ndmp_dma.c */
bool do_ndmp_backup_init(JCR *jcr);
bool do_ndmp_backup(JCR *jcr);
void ndmp_backup_cleanup(JCR *jcr, int TermCode);
bool do_ndmp_restore_init(JCR *jcr);
bool do_ndmp_restore(JCR *jcr);
void ndmp_restore_cleanup(JCR *jcr, int TermCode);
void do_ndmp_storage_status(UAContext *ua, STORERES *store, char *cmd);
void do_ndmp_client_status(UAContext *ua, CLIENTRES *client, char *cmd);

/* next_vol.c */
void set_storageid_in_mr(STORERES *store, MEDIA_DBR *mr);
int find_next_volume_for_append(JCR *jcr, MEDIA_DBR *mr, int index,
                                bool create, bool purge);
bool has_volume_expired(JCR *jcr, MEDIA_DBR *mr);
void check_if_volume_valid_or_recyclable(JCR *jcr, MEDIA_DBR *mr, const char **reason);
bool get_scratch_volume(JCR *jcr, bool InChanger, MEDIA_DBR *mr,
        STORERES *store);

/* newvol.c */
bool newVolume(JCR *jcr, MEDIA_DBR *mr, STORERES *store);

/* quota.c */
uint64_t quota_fetch_remaining_quota(JCR *jcr);
bool quota_check_hardquotas(JCR *jcr);
bool quota_check_softquotas(JCR *jcr);

/* restore.c */
bool do_native_restore(JCR *jcr);
bool do_native_restore_init(JCR *jcr);
void native_restore_cleanup(JCR *jcr, int TermCode);
void generate_restore_summary(JCR *jcr, int msg_type, const char *term_msg);

/* sd_cmds.c */
bool connect_to_storage_daemon(JCR *jcr, int retry_interval,
                               int max_retry_time, int verbose);
BSOCK *open_sd_bsock(UAContext *ua);
void close_sd_bsock(UAContext *ua);
char *get_volume_name_from_SD(UAContext *ua, int Slot, int drive);
dlist *get_vol_list_from_SD(UAContext *ua, STORERES *store, bool listall, bool scan);
void free_vol_list(dlist *vol_list);
int get_num_slots_from_SD(UAContext *ua);
int get_num_drives_from_SD(UAContext *ua);
bool cancel_storage_daemon_job(UAContext *ua, JCR *jcr, bool silent = false);
void cancel_storage_daemon_job(JCR *jcr);
void do_native_storage_status(UAContext *ua, STORERES *store, char *cmd);
bool transfer_volume(UAContext *ua, STORERES *store, int src_slot, int dst_slot);
bool do_autochanger_volume_operation(UAContext *ua, STORERES *store,
                                     const char *operation, int drive, int slot);

/* scheduler.c */
JCR *wait_for_next_job(char *one_shot_job_to_run);
bool is_doy_in_last_week(int year, int doy);
void term_scheduler();

/* ua_acl.c */
bool acl_access_ok(UAContext *ua, int acl, const char *item);
bool acl_access_ok(UAContext *ua, int acl, const char *item, int len);

/* ua_cmds.c */
bool do_a_command(UAContext *ua);
bool do_a_dot_command(UAContext *ua);
int qmessagescmd(UAContext *ua, const char *cmd);
bool open_new_client_db(UAContext *ua);
bool open_client_db(UAContext *ua, bool need_private = false);
bool open_db(UAContext *ua, bool need_private = false);
void close_db(UAContext *ua);
int create_pool(JCR *jcr, B_DB *db, POOLRES *pool, e_pool_op op);
void set_pool_dbr_defaults_in_media_dbr(MEDIA_DBR *mr, POOL_DBR *pr);
bool set_pooldbr_references(JCR *jcr, B_DB *db, POOL_DBR *pr, POOLRES *pool);
void set_pooldbr_from_poolres(POOL_DBR *pr, POOLRES *pool, e_pool_op op);
int update_pool_references(JCR *jcr, B_DB *db, POOLRES *pool);

/* ua_impexp.c */
int import_cmd(UAContext *ua, const char *cmd);
int export_cmd(UAContext *ua, const char *cmd);
int move_cmd(UAContext *ua, const char *cmd);

/* ua_input.c */
int get_cmd(UAContext *ua, const char *prompt, bool subprompt = false);
bool get_pint(UAContext *ua, const char *prompt);
bool get_yesno(UAContext *ua, const char *prompt);
bool is_yesno(char *val, int *ret);
int get_enabled(UAContext *ua, const char *val);
void parse_ua_args(UAContext *ua);
bool is_comment_legal(UAContext *ua, const char *name);

/* ua_label.c */
bool is_volume_name_legal(UAContext *ua, const char *name);

/* ua_update.c */
void update_vol_pool(UAContext *ua, char *val, MEDIA_DBR *mr, POOL_DBR *opr);
void update_slots_from_vol_list(UAContext *ua, STORERES *store, dlist *vol_list, char *slot_list);
void update_inchanger_for_export(UAContext *ua, STORERES *store, dlist *vol_list, char *slot_list);

/* ua_output.c */
void prtit(void *ctx, const char *msg);
bool complete_jcr_for_job(JCR *jcr, JOBRES *job, POOLRES *pool);
RUNRES *find_next_run(RUNRES *run, JOBRES *job, utime_t &runtime, int ndays);

/* ua_restore.c */
void find_storage_resource(UAContext *ua, RESTORE_CTX &rx, char *Storage, char *MediaType);

/* ua_server.c */
void bsendmsg(void *ua_ctx, const char *fmt, ...);
void berrormsg(void *ua_ctx, const char *fmt, ...);
void bwarningmsg(void *ua_ctx, const char *fmt, ...);
void binfomsg(void *ua_ctx, const char *fmt, ...);
UAContext *new_ua_context(JCR *jcr);
JCR *new_control_jcr(const char *base_name, int job_type);
void free_ua_context(UAContext *ua);

/* ua_select.c */
STORERES *select_storage_resource(UAContext *ua);
JOBRES *select_job_resource(UAContext *ua);
JOBRES *select_enable_disable_job_resource(UAContext *ua, bool enable);
JOBRES *select_restore_job_resource(UAContext *ua);
CLIENTRES *select_client_resource(UAContext *ua);
FILESETRES *select_fileset_resource(UAContext *ua);
int select_pool_and_media_dbr(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr);
int select_media_dbr(UAContext *ua, MEDIA_DBR *mr);
bool select_pool_dbr(UAContext *ua, POOL_DBR *pr, const char *argk = "pool");
bool select_client_dbr(UAContext *ua, CLIENT_DBR *cr);

void start_prompt(UAContext *ua, const char *msg);
void add_prompt(UAContext *ua, const char *prompt);
int do_prompt(UAContext *ua, const char *automsg, const char *msg, char *prompt, int max_prompt);
CATRES *get_catalog_resource(UAContext *ua);
STORERES *get_storage_resource(UAContext *ua, bool use_default);
int get_storage_drive(UAContext *ua, STORERES *store);
int get_storage_slot(UAContext *ua, STORERES *store);
int get_media_type(UAContext *ua, char *MediaType, int max_media);
bool get_pool_dbr(UAContext *ua, POOL_DBR *pr, const char *argk = "pool");
bool get_client_dbr(UAContext *ua, CLIENT_DBR *cr);
POOLRES *get_pool_resource(UAContext *ua);
JOBRES *get_restore_job(UAContext *ua);
POOLRES *select_pool_resource(UAContext *ua);
JCR *select_running_job(UAContext *ua, const char *reason);
CLIENTRES *get_client_resource(UAContext *ua);
int get_job_dbr(UAContext *ua, JOB_DBR *jr);
bool get_user_slot_list(UAContext *ua, char *slot_list, const char *argument, int num_slots);

int find_arg_keyword(UAContext *ua, const char **list);
int find_arg(UAContext *ua, const char *keyword);
int find_arg_with_value(UAContext *ua, const char *keyword);
int do_keyword_prompt(UAContext *ua, const char *msg, const char **list);
int confirm_retention(UAContext *ua, utime_t *ret, const char *msg);
bool get_level_from_name(JCR *jcr, const char *level_name);

/* ua_status.c */
void list_dir_status_header(UAContext *ua);

/* ua_tree.c */
bool user_select_files_from_tree(TREE_CTX *tree);
int insert_tree_handler(void *ctx, int num_fields, char **row);

/* ua_prune.c */
int prune_files(UAContext *ua, CLIENTRES *client, POOLRES *pool);
int prune_jobs(UAContext *ua, CLIENTRES *client, POOLRES *pool, int JobType);
int prune_stats(UAContext *ua, utime_t retention);
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
int rerun_cmd(UAContext *ua, const char *cmd);
int run_cmd(UAContext *ua, const char *cmd);

/* verify.c */
bool do_verify(JCR *jcr);
bool do_verify_init(JCR *jcr);
void verify_cleanup(JCR *jcr, int TermCode);
