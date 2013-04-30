/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Director external function prototypes
 */

/* admin.c */
extern bool do_admin_init(JCR *jcr);
extern bool do_admin(JCR *jcr);
extern void admin_cleanup(JCR *jcr, int TermCode);


/* authenticate.c */
extern bool authenticate_storage_daemon(JCR *jcr, STORE *store);
extern int authenticate_file_daemon(JCR *jcr);
extern int authenticate_user_agent(UAContext *ua);

/* autoprune.c */
extern void do_autoprune(JCR *jcr);
extern void prune_volumes(JCR *jcr, bool InChanger, MEDIA_DBR *mr,
              STORE *store);

/* autorecycle.c */
extern bool recycle_oldest_purged_volume(JCR *jcr, bool InChanger,
              MEDIA_DBR *mr, STORE *store);

extern int recycle_volume(JCR *jcr, MEDIA_DBR *mr);
extern bool find_recycled_volume(JCR *jcr, bool InChanger,
                MEDIA_DBR *mr, STORE *store);

/* backup.c */
extern int wait_for_job_termination(JCR *jcr, int timeout=0);
extern bool do_backup_init(JCR *jcr);
extern bool do_backup(JCR *jcr);
extern void backup_cleanup(JCR *jcr, int TermCode);
extern void update_bootstrap_file(JCR *jcr);
extern bool send_accurate_current_files(JCR *jcr);


/* vbackup.c */
extern bool do_vbackup_init(JCR *jcr);
extern bool do_vbackup(JCR *jcr);
extern void vbackup_cleanup(JCR *jcr, int TermCode);


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


/* catreq.c */
extern void catalog_request(JCR *jcr, BSOCK *bs);
extern void catalog_update(JCR *jcr, BSOCK *bs);
extern bool despool_attributes_from_file(JCR *jcr, const char *file);

/* dird_conf.c */
extern const char *level_to_str(int level);
extern "C" char *job_code_callback_director(JCR *jcr, const char*);

/* expand.c */
int variable_expansion(JCR *jcr, char *inp, POOLMEM **exp);


/* fd_cmds.c */
extern int connect_to_file_daemon(JCR *jcr, int retry_interval,
                                  int max_retry_time, int verbose);
extern bool send_include_list(JCR *jcr);
extern bool send_exclude_list(JCR *jcr);
extern bool send_level_command(JCR *jcr);
extern int get_attributes_and_put_in_catalog(JCR *jcr);
extern void get_attributes_and_compare_to_catalog(JCR *jcr, JobId_t JobId);
extern int put_file_into_catalog(JCR *jcr, long file_index, char *fname,
                          char *link, char *attr, int stream);
extern void get_level_since_time(JCR *jcr, char *since, int since_len);
extern int send_runscripts_commands(JCR *jcr);
extern bool send_restore_objects(JCR *jcr);

/* getmsg.c */
enum e_prtmsg {
   DISPLAY_ERROR,
   NO_DISPLAY
};
extern bool response(JCR *jcr, BSOCK *fd, char *resp, const char *cmd, e_prtmsg prtmsg);

/* job.c */
extern bool allow_duplicate_job(JCR *jcr);
extern void set_jcr_defaults(JCR *jcr, JOB *job);
extern void create_unique_job_name(JCR *jcr, const char *base_name);
extern void update_job_end_record(JCR *jcr);
extern bool get_or_create_client_record(JCR *jcr);
extern bool get_or_create_fileset_record(JCR *jcr);
extern DBId_t get_or_create_pool_record(JCR *jcr, char *pool_name);
extern void apply_pool_overrides(JCR *jcr);
extern JobId_t run_job(JCR *jcr);
extern bool cancel_job(UAContext *ua, JCR *jcr);
extern void get_job_storage(USTORE *store, JOB *job, RUN *run);
extern void init_jcr_job_record(JCR *jcr);
extern void update_job_end(JCR *jcr, int TermCode);
extern void copy_rwstorage(JCR *jcr, alist *storage, const char *where);
extern void set_rwstorage(JCR *jcr, USTORE *store);
extern void free_rwstorage(JCR *jcr);
extern void copy_wstorage(JCR *jcr, alist *storage, const char *where);
extern void set_wstorage(JCR *jcr, USTORE *store);
extern void free_wstorage(JCR *jcr);
extern void copy_rstorage(JCR *jcr, alist *storage, const char *where);
extern void set_rstorage(JCR *jcr, USTORE *store);
extern void free_rstorage(JCR *jcr);
extern bool setup_job(JCR *jcr);
extern void create_clones(JCR *jcr);
extern int create_restore_bootstrap_file(JCR *jcr);
extern void dird_free_jcr(JCR *jcr);
extern void dird_free_jcr_pointers(JCR *jcr);
extern void cancel_storage_daemon_job(JCR *jcr);
extern bool run_console_command(JCR *jcr, const char *cmd);
extern void sd_msg_thread_send_signal(JCR *jcr, int sig);

/* jobq.c */
extern bool inc_read_store(JCR *jcr);
extern void dec_read_store(JCR *jcr);

/* migration.c */
extern bool do_migration(JCR *jcr);
extern bool do_migration_init(JCR *jcr);
extern void migration_cleanup(JCR *jcr, int TermCode);
extern bool set_migration_wstorage(JCR *jcr, POOL *pool);


/* mountreq.c */
extern void mount_request(JCR *jcr, BSOCK *bs, char *buf);

/* msgchan.c */
extern bool connect_to_storage_daemon(JCR *jcr, int retry_interval,
                              int max_retry_time, int verbose);
extern bool start_storage_daemon_job(JCR *jcr, alist *rstore, alist *wstore,
              bool send_bsr=false);
extern bool start_storage_daemon_message_thread(JCR *jcr);
extern int bget_dirmsg(BSOCK *bs);
extern void wait_for_storage_daemon_termination(JCR *jcr);
extern bool send_bootstrap_file(JCR *jcr, BSOCK *sd);

/* next_vol.c */
void set_storageid_in_mr(STORE *store, MEDIA_DBR *mr);
int find_next_volume_for_append(JCR *jcr, MEDIA_DBR *mr, int index,
                                bool create, bool purge);
bool has_volume_expired(JCR *jcr, MEDIA_DBR *mr);
void check_if_volume_valid_or_recyclable(JCR *jcr, MEDIA_DBR *mr, const char **reason);
bool get_scratch_volume(JCR *jcr, bool InChanger, MEDIA_DBR *mr,
        STORE *store);

/* newvol.c */
bool newVolume(JCR *jcr, MEDIA_DBR *mr, STORE *store);

/* python.c */
int generate_job_event(JCR *jcr, const char *event);


/* restore.c */
extern bool do_restore(JCR *jcr);
extern bool do_restore_init(JCR *jcr);
extern void restore_cleanup(JCR *jcr, int TermCode);


/* ua_acl.c */
bool acl_access_ok(UAContext *ua, int acl, const char *item);
bool acl_access_ok(UAContext *ua, int acl, const char *item, int len);

/* ua_cmds.c */
bool do_a_command(UAContext *ua);
bool do_a_dot_command(UAContext *ua);
int qmessagescmd(UAContext *ua, const char *cmd);
bool open_new_client_db(UAContext *ua);
bool open_client_db(UAContext *ua);
bool open_db(UAContext *ua);
void close_db(UAContext *ua);
enum e_pool_op {
   POOL_OP_UPDATE,
   POOL_OP_CREATE
};
int create_pool(JCR *jcr, B_DB *db, POOL *pool, e_pool_op op);
void set_pool_dbr_defaults_in_media_dbr(MEDIA_DBR *mr, POOL_DBR *pr);
bool set_pooldbr_references(JCR *jcr, B_DB *db, POOL_DBR *pr, POOL *pool);
void set_pooldbr_from_poolres(POOL_DBR *pr, POOL *pool, e_pool_op op);
int update_pool_references(JCR *jcr, B_DB *db, POOL *pool);

/* ua_input.c */
int get_cmd(UAContext *ua, const char *prompt, bool subprompt=false);
bool get_pint(UAContext *ua, const char *prompt);
bool get_yesno(UAContext *ua, const char *prompt);
bool is_yesno(char *val, int *ret);
int get_enabled(UAContext *ua, const char *val);
void parse_ua_args(UAContext *ua);
bool is_comment_legal(UAContext *ua, const char *name);

/* ua_label.c */
bool is_volume_name_legal(UAContext *ua, const char *name);
int get_num_drives_from_SD(UAContext *ua);
void update_slots(UAContext *ua);

/* ua_update.c */
void update_vol_pool(UAContext *ua, char *val, MEDIA_DBR *mr, POOL_DBR *opr);

/* ua_output.c */
void prtit(void *ctx, const char *msg);
bool complete_jcr_for_job(JCR *jcr, JOB *job, POOL *pool);
RUN *find_next_run(RUN *run, JOB *job, utime_t &runtime, int ndays);

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
STORE   *select_storage_resource(UAContext *ua);
JOB     *select_job_resource(UAContext *ua);
JOB     *select_enable_disable_job_resource(UAContext *ua, bool enable);
JOB     *select_restore_job_resource(UAContext *ua);
CLIENT  *select_client_resource(UAContext *ua);
FILESET *select_fileset_resource(UAContext *ua);
int     select_pool_and_media_dbr(UAContext *ua, POOL_DBR *pr, MEDIA_DBR *mr);
int     select_media_dbr(UAContext *ua, MEDIA_DBR *mr);
bool    select_pool_dbr(UAContext *ua, POOL_DBR *pr, const char *argk="pool");
bool    select_client_dbr(UAContext *ua, CLIENT_DBR *cr);

void    start_prompt(UAContext *ua, const char *msg);
void    add_prompt(UAContext *ua, const char *prompt);
int     do_prompt(UAContext *ua, const char *automsg, const char *msg, char *prompt, int max_prompt);
CAT    *get_catalog_resource(UAContext *ua);
STORE  *get_storage_resource(UAContext *ua, bool use_default);
int     get_storage_drive(UAContext *ua, STORE *store);
int     get_storage_slot(UAContext *ua, STORE *store);
int     get_media_type(UAContext *ua, char *MediaType, int max_media);
bool    get_pool_dbr(UAContext *ua, POOL_DBR *pr, const char *argk="pool");
bool    get_client_dbr(UAContext *ua, CLIENT_DBR *cr);
POOL   *get_pool_resource(UAContext *ua);
JOB    *get_restore_job(UAContext *ua);
POOL   *select_pool_resource(UAContext *ua);
JCR *select_running_job(UAContext *ua, const char *reason);
CLIENT *get_client_resource(UAContext *ua);
int     get_job_dbr(UAContext *ua, JOB_DBR *jr);

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
int prune_files(UAContext *ua, CLIENT *client, POOL *pool);
int prune_jobs(UAContext *ua, CLIENT *client, POOL *pool, int JobType);
int prune_stats(UAContext *ua, utime_t retention);
bool prune_volume(UAContext *ua, MEDIA_DBR *mr);
int job_delete_handler(void *ctx, int num_fields, char **row);
int del_count_handler(void *ctx, int num_fields, char **row);
int file_delete_handler(void *ctx, int num_fields, char **row);
int get_prune_list_for_volume(UAContext *ua, MEDIA_DBR *mr, del_ctx *del);
int exclude_running_jobs_from_list(del_ctx *prune_list);

/* ua_purge.c */
bool is_volume_purged(UAContext *ua, MEDIA_DBR *mr, bool force=false);
bool mark_media_purged(UAContext *ua, MEDIA_DBR *mr);
void purge_files_from_volume(UAContext *ua, MEDIA_DBR *mr);
bool purge_jobs_from_volume(UAContext *ua, MEDIA_DBR *mr, bool force=false);
void purge_files_from_jobs(UAContext *ua, char *jobs);
void purge_jobs_from_catalog(UAContext *ua, char *jobs);
void purge_job_list_from_catalog(UAContext *ua, del_ctx &del);
void purge_files_from_job_list(UAContext *ua, del_ctx &del);


/* ua_run.c */
extern int run_cmd(UAContext *ua, const char *cmd);

/* verify.c */
extern bool do_verify(JCR *jcr);
extern bool do_verify_init(JCR *jcr);
extern void verify_cleanup(JCR *jcr, int TermCode);
