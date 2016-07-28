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
 * Prototypes for lib directory of BAREOS
 */

#ifndef __LIBPROTOS_H
#define __LIBPROTOS_H

class JCR;

/* attr.c */
ATTR *new_attr(JCR *jcr);
void free_attr(ATTR *attr);
int unpack_attributes_record(JCR *jcr, int32_t stream, char *rec, int32_t reclen, ATTR *attr);
void build_attr_output_fnames(JCR *jcr, ATTR *attr);
void print_ls_output(JCR *jcr, ATTR *attr);

/* attribs.c */
void encode_stat(char *buf, struct stat *statp, int stat_size, int32_t LinkFI, int data_stream);
int decode_stat(char *buf, struct stat *statp, int stat_size, int32_t *LinkFI);
int32_t decode_LinkFI(char *buf, struct stat *statp, int stat_size);

/* base64.c */
void base64_init(void);
int to_base64(int64_t value, char *where);
int from_base64(int64_t *value, char *where);
int bin_to_base64(char *buf, int buflen, char *bin, int binlen, bool compatible);
int base64_to_bin(char *dest, int destlen, char *src, int srclen);

/* bget_msg.c */
int bget_msg(BSOCK *sock);

/* bnet.c */
int32_t bnet_recv(BSOCK *bsock);
bool bnet_send(BSOCK *bsock);
bool bnet_fsend(BSOCK *bs, const char *fmt, ...);
bool bnet_set_buffer_size(BSOCK *bs, uint32_t size, int rw);
bool bnet_sig(BSOCK *bs, int sig);
bool bnet_tls_server(TLS_CONTEXT *ctx, BSOCK *bsock,
                     alist *verify_list);
bool bnet_tls_client(TLS_CONTEXT *ctx, BSOCK *bsock,
                     bool verify_peer, alist *verify_list);
int bnet_get_peer(BSOCK *bs, char *buf, socklen_t buflen);
BSOCK *dup_bsock(BSOCK *bsock);
const char *bnet_strerror(BSOCK *bsock);
const char *bnet_sig_to_ascii(BSOCK *bsock);
int bnet_wait_data(BSOCK *bsock, int sec);
int bnet_wait_data_intr(BSOCK *bsock, int sec);
bool is_bnet_stop(BSOCK *bsock);
int is_bnet_error(BSOCK *bsock);
void bnet_suppress_error_messages(BSOCK *bsock, bool flag);
dlist *bnet_host2ipaddrs(const char *host, int family, const char **errstr);
int bnet_set_blocking(BSOCK *sock);
int bnet_set_nonblocking(BSOCK *sock);
void bnet_restore_blocking(BSOCK *sock, int flags);
int net_connect(int port);
BSOCK *bnet_bind(int port);
BSOCK *bnet_accept(BSOCK *bsock, char *who);

/* bnet_server_tcp.c */
void cleanup_bnet_thread_server_tcp(alist *sockfds, workq_t *client_wq);
void bnet_thread_server_tcp(dlist *addr_list,
                            int max_clients,
                            alist *sockfds,
                            workq_t *client_wq,
                            bool nokeepalive,
                            void *handle_client_request(void *bsock));
void bnet_stop_thread_server_tcp(pthread_t tid);

/* bpipe.c */
BPIPE *open_bpipe(char *prog, int wait, const char *mode,
                  bool dup_stderr = true);
int close_wpipe(BPIPE *bpipe);
int close_bpipe(BPIPE *bpipe);

/* bsys.c */
char *bstrinlinecpy(char *dest, const char *src);
char *bstrncpy(char *dest, const char *src, int maxlen);
char *bstrncpy(char *dest, POOL_MEM &src, int maxlen);
char *bstrncat(char *dest, const char *src, int maxlen);
char *bstrncat(char *dest, POOL_MEM &src, int maxlen);
bool bstrcmp(const char *s1, const char *s2);
bool bstrncmp(const char *s1, const char *s2, int n);
bool bstrcasecmp(const char *s1, const char *s2);
bool bstrncasecmp(const char *s1, const char *s2, int n);
int cstrlen(const char *str);
void *b_malloc(const char *file, int line, size_t size);
#ifndef bmalloc
void *bmalloc(size_t size);
#endif
void bfree(void *buf);
void *brealloc(void *buf, size_t size);
void *bcalloc(size_t size1, size_t size2);
int bsnprintf(char *str, int32_t size, const char *format, ...);
int bvsnprintf(char *str, int32_t size, const char *format, va_list ap);
int pool_sprintf(char *pool_buf, const char *fmt, ...);
void create_pid_file(char *dir, const char *progname, int port);
int delete_pid_file(char *dir, const char *progname, int port);
void drop(char *uid, char *gid, bool keep_readall_caps);
int bmicrosleep(int32_t sec, int32_t usec);
char *bfgets(char *s, int size, FILE *fd);
char *bfgets(POOLMEM *&s, FILE *fd);
void make_unique_filename(POOLMEM *&name, int Id, char *what);
#ifndef HAVE_STRTOLL
long long int strtoll(const char *ptr, char **endptr, int base);
#endif
void read_state_file(char *dir, const char *progname, int port);
int b_strerror(int errnum, char *buf, size_t bufsiz);
char *escape_filename(const char *file_path);
int Zdeflate(char *in, int in_len, char *out, int &out_len);
int Zinflate(char *in, int in_len, char *out, int &out_len);
void stack_trace();
int safer_unlink(const char *pathname, const char *regex);
int secure_erase(JCR *jcr, const char *pathname);
void set_secure_erase_cmdline(const char *cmdline);
bool path_exists(const char *path);
bool path_exists(POOL_MEM &path);
bool path_is_directory(const char *path);
bool path_is_directory(POOL_MEM &path);
bool path_is_absolute(const char *path);
bool path_is_absolute(POOL_MEM &path);
bool path_get_directory(POOL_MEM &directory, POOL_MEM &path);
bool path_append(char *path, const char *extra, unsigned int max_path);
bool path_append(POOL_MEM &path, const char *extra);
bool path_append(POOL_MEM &path, POOL_MEM &extra);
bool path_create(const char *path, mode_t mode = 0750);
bool path_create(POOL_MEM &path, mode_t mode = 0750);

/* compression.c */
const char *cmprs_algo_to_text(uint32_t compression_algorithm);
bool setup_compression_buffers(JCR *jcr, bool compatible,
                               uint32_t compression_algorithm,
                               uint32_t *compress_buf_size);
bool setup_decompression_buffers(JCR *jcr, uint32_t *decompress_buf_size);
bool compress_data(JCR *jcr, uint32_t compression_algorithm, char *rbuf,
                   uint32_t rsize, unsigned char *cbuf,
                   uint32_t max_compress_len, uint32_t *compress_len);
bool decompress_data(JCR *jcr, const char *last_fname, int32_t stream,
                     char **data, uint32_t *length, bool want_data_stream);
void cleanup_compression(JCR *jcr);

/* cram-md5.c */
bool cram_md5_respond(BSOCK *bs, const char *password, int *tls_remote_need, bool *compatible);
bool cram_md5_challenge(BSOCK *bs, const char *password, int tls_local_need, bool compatible);
void hmac_md5(uint8_t *text, int text_len, uint8_t *key, int key_len, uint8_t *hmac);

/* crypto.c */
int init_crypto(void);
int cleanup_crypto(void);
DIGEST *crypto_digest_new(JCR *jcr, crypto_digest_t type);
bool crypto_digest_update(DIGEST *digest, const uint8_t *data, uint32_t length);
bool crypto_digest_finalize(DIGEST *digest, uint8_t *dest, uint32_t *length);
void crypto_digest_free(DIGEST *digest);
SIGNATURE *crypto_sign_new(JCR *jcr);
crypto_error_t crypto_sign_get_digest(SIGNATURE *sig, X509_KEYPAIR *keypair,
                                      crypto_digest_t &algorithm, DIGEST **digest);
crypto_error_t crypto_sign_verify(SIGNATURE *sig, X509_KEYPAIR *keypair, DIGEST *digest);
int crypto_sign_add_signer(SIGNATURE *sig, DIGEST *digest, X509_KEYPAIR *keypair);
int crypto_sign_encode(SIGNATURE *sig, uint8_t *dest, uint32_t *length);
SIGNATURE *crypto_sign_decode(JCR *jcr, const uint8_t *sigData, uint32_t length);
void crypto_sign_free(SIGNATURE *sig);
CRYPTO_SESSION *crypto_session_new(crypto_cipher_t cipher, alist *pubkeys);
void crypto_session_free(CRYPTO_SESSION *cs);
bool crypto_session_encode(CRYPTO_SESSION *cs, uint8_t *dest, uint32_t *length);
crypto_error_t crypto_session_decode(const uint8_t *data, uint32_t length, alist *keypairs, CRYPTO_SESSION **session);
CRYPTO_SESSION *crypto_session_decode(const uint8_t *data, uint32_t length);
CIPHER_CONTEXT *crypto_cipher_new(CRYPTO_SESSION *cs, bool encrypt, uint32_t *blocksize);
bool crypto_cipher_update(CIPHER_CONTEXT *cipher_ctx, const uint8_t *data, uint32_t length, const uint8_t *dest, uint32_t *written);
bool crypto_cipher_finalize(CIPHER_CONTEXT *cipher_ctx, uint8_t *dest, uint32_t *written);
void crypto_cipher_free(CIPHER_CONTEXT *cipher_ctx);
X509_KEYPAIR *crypto_keypair_new(void);
X509_KEYPAIR *crypto_keypair_dup(X509_KEYPAIR *keypair);
int crypto_keypair_load_cert(X509_KEYPAIR *keypair, const char *file);
bool crypto_keypair_has_key(const char *file);
int crypto_keypair_load_key(X509_KEYPAIR *keypair, const char *file, CRYPTO_PEM_PASSWD_CB *pem_callback, const void *pem_userdata);
void crypto_keypair_free(X509_KEYPAIR *keypair);
int crypto_default_pem_callback(char *buf, int size, const void *userdata);
const char *crypto_digest_name(crypto_digest_t type);
const char *crypto_digest_name(DIGEST *digest);
crypto_digest_t crypto_digest_stream_type(int stream);
const char *crypto_strerror(crypto_error_t error);

/* crypto_openssl.c */
#ifdef HAVE_OPENSSL
void openssl_post_errors(int code, const char *errstring);
void openssl_post_errors(JCR *jcr, int code, const char *errstring);
int openssl_init_threads(void);
void openssl_cleanup_threads(void);
int openssl_seed_prng(void);
int openssl_save_prng(void);
#endif /* HAVE_OPENSSL */

/* crypto_wrap.c */
void aes_wrap(uint8_t *kek, int n, uint8_t *plain, uint8_t *cipher);
int aes_unwrap(uint8_t *kek, int n, uint8_t *cipher, uint8_t *plain);

/* daemon.c */
void daemon_start();

/* edit.c */
uint64_t str_to_uint64(const char *str);
int64_t str_to_int64(const char *str);
#define str_to_int16(str)((int16_t)str_to_int64(str))
#define str_to_int32(str)((int32_t)str_to_int64(str))
char *edit_uint64_with_commas(uint64_t val, char *buf);
char *edit_uint64_with_suffix(uint64_t val, char *buf);
char *add_commas(char *val, char *buf);
char *edit_uint64(uint64_t val, char *buf);
char *edit_int64(int64_t val, char *buf);
char *edit_int64_with_commas(int64_t val, char *buf);
bool duration_to_utime(char *str, utime_t *value);
bool size_to_uint64(char *str, uint64_t *value);
bool speed_to_uint64(char *str, uint64_t *value);
char *edit_utime(utime_t val, char *buf, int buf_len);
bool is_a_number(const char *num);
bool is_a_number_list(const char *n);
bool is_an_integer(const char *n);
bool is_name_valid(const char *name, POOLMEM *&msg);
bool is_name_valid(const char *name);

/* jcr.c (most definitions are in src/jcr.h) */
void init_last_jobs_list();
void term_last_jobs_list();
void lock_last_jobs_list();
void unlock_last_jobs_list();
bool read_last_jobs_list(int fd, uint64_t addr);
uint64_t write_last_jobs_list(int fd, uint64_t addr);
void write_state_file(char *dir, const char *progname, int port);
void job_end_push(JCR *jcr, void job_end_cb(JCR *jcr,void *), void *ctx);
void lock_jobs();
void unlock_jobs();
JCR *jcr_walk_start();
JCR *jcr_walk_next(JCR *prev_jcr);
void jcr_walk_end(JCR *jcr);
int job_count();
JCR *get_jcr_from_tsd();
void set_jcr_in_tsd(JCR *jcr);
void remove_jcr_from_tsd(JCR *jcr);
uint32_t get_jobid_from_tsd();
uint32_t get_jobid_from_tid(pthread_t tid);

/* json.c */
void initialize_json();

/* lex.c */
LEX *lex_close_file(LEX *lf);
LEX *lex_close_buffer(LEX *lf);
LEX *lex_open_file(LEX *lf,
                   const char *fname,
                   LEX_ERROR_HANDLER *scan_error,
                   LEX_WARNING_HANDLER *scan_warning);
LEX *lex_new_buffer(LEX *lf,
                    LEX_ERROR_HANDLER *scan_error,
                    LEX_WARNING_HANDLER *scan_warning);
int lex_get_char(LEX *lf);
void lex_unget_char(LEX *lf);
const char *lex_tok_to_str(int token);
int lex_get_token(LEX *lf, int expect);
void lex_set_default_error_handler(LEX *lf);
void lex_set_default_warning_handler(LEX *lf);
int lex_set_error_handler_error_type(LEX *lf, int err_type);

/* message.c */
void my_name_is(int argc, char *argv[], const char *name);
void init_msg(JCR *jcr, MSGSRES *msg, job_code_callback_t job_code_callback = NULL);
void term_msg(void);
void close_msg(JCR *jcr);
void add_msg_dest(MSGSRES *msg, int dest, int type,
                  char *where, char *mail_cmd, char *timestamp_format);
void rem_msg_dest(MSGSRES *msg, int dest, int type, char *where);
void Jmsg(JCR *jcr, int type, utime_t mtime, const char *fmt, ...);
void dispatch_message(JCR *jcr, int type, utime_t mtime, char *buf);
void init_console_msg(const char *wd);
void free_msgs_res(MSGSRES *msgs);
void dequeue_messages(JCR *jcr);
void set_trace(int trace_flag);
bool get_trace(void);
void set_hangup(int hangup_value);
bool get_hangup(void);
void set_timestamp(int timestamp_flag);
bool get_timestamp(void);
void set_db_type(const char *name);
void register_message_callback(void msg_callback(int type, char *msg));

/* passphrase.c */
char *generate_crypto_passphrase(uint16_t length);

/* path_list.c */
htable *path_list_init();
bool path_list_lookup(htable *path_list, const char *fname);
bool path_list_add(htable *path_list, uint32_t len, const char *fname);
void free_path_list(htable *path_list);

/* poll.c */
int wait_for_readable_fd(int fd, int sec, bool ignore_interupts);
int wait_for_writable_fd(int fd, int sec, bool ignore_interupts);

/* pythonlib.c */
int generate_daemon_event(JCR *jcr, const char *event);

/* scan.c */
void strip_leading_space(char *str);
void strip_trailing_junk(char *str);
void strip_trailing_newline(char *str);
void strip_trailing_slashes(char *dir);
bool skip_spaces(char **msg);
bool skip_nonspaces(char **msg);
int fstrsch(const char *a, const char *b);
char *next_arg(char **s);
int parse_args(POOLMEM *cmd, POOLMEM *&args, int *argc,
               char **argk, char **argv, int max_args);
int parse_args_only(POOLMEM *cmd, POOLMEM *&args, int *argc,
                    char **argk, char **argv, int max_args);
void split_path_and_filename(const char *fname, POOLMEM *&path,
                             int *pnl, POOLMEM *&file, int *fnl);
int bsscanf(const char *buf, const char *fmt, ...);

/* scsi_crypto.c */
bool clear_scsi_encryption_key(int fd, const char *device);
bool set_scsi_encryption_key(int fd, const char *device, char *encryption_key);
int get_scsi_drive_encryption_status(int fd, const char *device_name,
                                     POOLMEM *&status, int indent);
int get_scsi_volume_encryption_status(int fd, const char *device_name,
                                      POOLMEM *&status, int indent);
bool need_scsi_crypto_key(int fd, const char *device_name, bool use_drive_status);
bool is_scsi_encryption_enabled(int fd, const char *device_name);

/* scsi_lli.c */
bool recv_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len);
bool send_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len);
bool check_scsi_at_eod(int fd);

/* scsi_tapealert.c */
bool get_tapealert_flags(int fd, const char *device_name, uint64_t *flags);

/* signal.c */
void init_signals(void terminate(int sig));
void init_stack_dump(void);

/* timers.c */
btimer_t *start_child_timer(JCR *jcr, pid_t pid, uint32_t wait);
void stop_child_timer(btimer_t *wid);
btimer_t *start_thread_timer(JCR *jcr, pthread_t tid, uint32_t wait);
void stop_thread_timer(btimer_t *wid);
btimer_t *start_bsock_timer(BSOCK *bs, uint32_t wait);
void stop_bsock_timer(btimer_t *wid);

/* tls.c */
TLS_CONTEXT *new_tls_context(const char *ca_certfile,
                             const char *ca_certdir,
                             const char *crlfile,
                             const char *certfile,
                             const char *keyfile,
                             CRYPTO_PEM_PASSWD_CB *pem_callback,
                             const void *pem_userdata,
                             const char *dhfile,
                             const char *cipherlist,
                             bool verify_peer);
void free_tls_context(TLS_CONTEXT *ctx);
#ifdef HAVE_TLS
bool tls_postconnect_verify_host(JCR *jcr, TLS_CONNECTION *tls_conn,
                                 const char *host);
bool tls_postconnect_verify_cn(JCR *jcr, TLS_CONNECTION *tls_conn,
                               alist *verify_list);
TLS_CONNECTION *new_tls_connection(TLS_CONTEXT *ctx, int fd, bool server);
bool tls_bsock_accept(BSOCK *bsock);
int tls_bsock_writen(BSOCK *bsock, char *ptr, int32_t nbytes);
int tls_bsock_readn(BSOCK *bsock, char *ptr, int32_t nbytes);
#endif /* HAVE_TLS */
bool tls_bsock_connect(BSOCK *bsock);
void tls_bsock_shutdown(BSOCK *bsock);
void free_tls_connection(TLS_CONNECTION *tls_conn);
bool get_tls_require(TLS_CONTEXT *ctx);
void set_tls_require(TLS_CONTEXT *ctx, bool value);
bool get_tls_enable(TLS_CONTEXT *ctx);
void set_tls_enable(TLS_CONTEXT *ctx, bool value);
bool get_tls_verify_peer(TLS_CONTEXT *ctx);

/* util.c */
void escape_string(POOL_MEM &snew, char *old, int len);
bool is_buf_zero(char *buf, int len);
void lcase(char *str);
void bash_spaces(char *str);
void bash_spaces(POOL_MEM &pm);
void unbash_spaces(char *str);
void unbash_spaces(POOL_MEM &pm);
char *encode_time(utime_t time, char *buf);
bool convert_timeout_to_timespec(timespec &timeout, int timeout_in_seconds);
char *encode_mode(mode_t mode, char *buf);
int do_shell_expansion(char *name, int name_len);
void jobstatus_to_ascii(int JobStatus, char *msg, int maxlen);
void jobstatus_to_ascii_gui(int JobStatus, char *msg, int maxlen);
int run_program(char *prog, int wait, POOLMEM *&results);
int run_program_full_output(char *prog, int wait, POOLMEM *&results);
char *action_on_purge_to_string(int aop, POOL_MEM &ret);
const char *job_type_to_str(int type);
const char *job_status_to_str(int stat);
const char *job_level_to_str(int level);
const char *volume_status_to_str(const char *status);
void make_session_key(char *key, char *seed, int mode);
void encode_session_key(char *encode, char *session, char *key, int maxlen);
void decode_session_key(char *decode, char *session, char *key, int maxlen);
POOLMEM *edit_job_codes(JCR *jcr, char *omsg, char *imsg, const char *to, job_code_callback_t job_code_callback = NULL);
void set_working_directory(char *wd);
const char *last_path_separator(const char *str);

/* watchdog.c */
int start_watchdog(void);
int stop_watchdog(void);
watchdog_t *new_watchdog(void);
bool register_watchdog(watchdog_t *wd);
bool unregister_watchdog(watchdog_t *wd);
bool is_watchdog();

#endif /* __LIBPROTOS_H */
