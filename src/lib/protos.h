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
 * Prototypes for lib directory of BAREOS
 */

#ifndef __LIBPROTOS_H
#define __LIBPROTOS_H

class JCR;

/* attr.c */
DLL_IMP_EXP ATTR *new_attr(JCR *jcr);
DLL_IMP_EXP void free_attr(ATTR *attr);
DLL_IMP_EXP int unpack_attributes_record(JCR *jcr, int32_t stream, char *rec, int32_t reclen, ATTR *attr);
DLL_IMP_EXP void build_attr_output_fnames(JCR *jcr, ATTR *attr);
DLL_IMP_EXP const char *attr_to_str(POOL_MEM &resultbuffer, JCR *jcr, ATTR *attr);
DLL_IMP_EXP void print_ls_output(JCR *jcr, ATTR *attr);

/* attribs.c */
DLL_IMP_EXP void encode_stat(char *buf, struct stat *statp, int stat_size, int32_t LinkFI, int data_stream);
DLL_IMP_EXP int decode_stat(char *buf, struct stat *statp, int stat_size, int32_t *LinkFI);
DLL_IMP_EXP int32_t decode_LinkFI(char *buf, struct stat *statp, int stat_size);

/* base64.c */
DLL_IMP_EXP void base64_init(void);
DLL_IMP_EXP int to_base64(int64_t value, char *where);
DLL_IMP_EXP int from_base64(int64_t *value, char *where);
DLL_IMP_EXP int bin_to_base64(char *buf, int buflen, char *bin, int binlen, bool compatible);
DLL_IMP_EXP int base64_to_bin(char *dest, int destlen, char *src, int srclen);

/* bget_msg.c */
DLL_IMP_EXP int bget_msg(BSOCK *sock);

/* bnet.c */
DLL_IMP_EXP int32_t bnet_recv(BSOCK *bsock);
DLL_IMP_EXP bool bnet_send(BSOCK *bsock);
DLL_IMP_EXP bool bnet_fsend(BSOCK *bs, const char *fmt, ...);
DLL_IMP_EXP bool bnet_set_buffer_size(BSOCK *bs, uint32_t size, int rw);
DLL_IMP_EXP bool bnet_sig(BSOCK *bs, int sig);
DLL_IMP_EXP bool bnet_tls_server(TLS_CONTEXT *ctx, BSOCK *bsock,
                     alist *verify_list);
DLL_IMP_EXP bool bnet_tls_client(TLS_CONTEXT *ctx, BSOCK *bsock,
                     bool verify_peer, alist *verify_list);
DLL_IMP_EXP int bnet_get_peer(BSOCK *bs, char *buf, socklen_t buflen);
DLL_IMP_EXP BSOCK *dup_bsock(BSOCK *bsock);
DLL_IMP_EXP const char *bnet_strerror(BSOCK *bsock);
DLL_IMP_EXP const char *bnet_sig_to_ascii(BSOCK *bsock);
DLL_IMP_EXP int bnet_wait_data(BSOCK *bsock, int sec);
DLL_IMP_EXP int bnet_wait_data_intr(BSOCK *bsock, int sec);
DLL_IMP_EXP bool is_bnet_stop(BSOCK *bsock);
DLL_IMP_EXP int is_bnet_error(BSOCK *bsock);
DLL_IMP_EXP void bnet_suppress_error_messages(BSOCK *bsock, bool flag);
DLL_IMP_EXP dlist *bnet_host2ipaddrs(const char *host, int family, const char **errstr);
DLL_IMP_EXP int bnet_set_blocking(BSOCK *sock);
DLL_IMP_EXP int bnet_set_nonblocking(BSOCK *sock);
DLL_IMP_EXP void bnet_restore_blocking(BSOCK *sock, int flags);
DLL_IMP_EXP int net_connect(int port);
DLL_IMP_EXP BSOCK *bnet_bind(int port);
DLL_IMP_EXP BSOCK *bnet_accept(BSOCK *bsock, char *who);

/* bnet_server_tcp.c */
DLL_IMP_EXP void cleanup_bnet_thread_server_tcp(alist *sockfds, workq_t *client_wq);
DLL_IMP_EXP void bnet_thread_server_tcp(dlist *addr_list,
                            int max_clients,
                            alist *sockfds,
                            workq_t *client_wq,
                            bool nokeepalive,
                            void *handle_client_request(void *bsock));
DLL_IMP_EXP void bnet_stop_thread_server_tcp(pthread_t tid);

/* bpipe.c */
DLL_IMP_EXP BPIPE *open_bpipe(char *prog, int wait, const char *mode,
                  bool dup_stderr = true);
DLL_IMP_EXP int close_wpipe(BPIPE *bpipe);
DLL_IMP_EXP int close_bpipe(BPIPE *bpipe);

/* bsys.c */
DLL_IMP_EXP char *bstrinlinecpy(char *dest, const char *src);
DLL_IMP_EXP char *bstrncpy(char *dest, const char *src, int maxlen);
DLL_IMP_EXP char *bstrncpy(char *dest, POOL_MEM &src, int maxlen);
DLL_IMP_EXP char *bstrncat(char *dest, const char *src, int maxlen);
DLL_IMP_EXP char *bstrncat(char *dest, POOL_MEM &src, int maxlen);
DLL_IMP_EXP bool bstrcmp(const char *s1, const char *s2);
DLL_IMP_EXP bool bstrncmp(const char *s1, const char *s2, int n);
DLL_IMP_EXP bool bstrcasecmp(const char *s1, const char *s2);
DLL_IMP_EXP bool bstrncasecmp(const char *s1, const char *s2, int n);
DLL_IMP_EXP int cstrlen(const char *str);
DLL_IMP_EXP void *b_malloc(const char *file, int line, size_t size);
#ifndef bmalloc
DLL_IMP_EXP void *bmalloc(size_t size);
#endif
DLL_IMP_EXP void bfree(void *buf);
DLL_IMP_EXP void *brealloc(void *buf, size_t size);
DLL_IMP_EXP void *bcalloc(size_t size1, size_t size2);
DLL_IMP_EXP int bsnprintf(char *str, int32_t size, const char *format, ...);
DLL_IMP_EXP int bvsnprintf(char *str, int32_t size, const char *format, va_list ap);
DLL_IMP_EXP int pool_sprintf(char *pool_buf, const char *fmt, ...);
DLL_IMP_EXP void create_pid_file(char *dir, const char *progname, int port);
DLL_IMP_EXP int delete_pid_file(char *dir, const char *progname, int port);
DLL_IMP_EXP void drop(char *uid, char *gid, bool keep_readall_caps);
DLL_IMP_EXP int bmicrosleep(int32_t sec, int32_t usec);
DLL_IMP_EXP char *bfgets(char *s, int size, FILE *fd);
DLL_IMP_EXP char *bfgets(POOLMEM *&s, FILE *fd);
DLL_IMP_EXP void make_unique_filename(POOLMEM *&name, int Id, char *what);
#ifndef HAVE_STRTOLL
DLL_IMP_EXP long long int strtoll(const char *ptr, char **endptr, int base);
#endif
DLL_IMP_EXP void read_state_file(char *dir, const char *progname, int port);
DLL_IMP_EXP int b_strerror(int errnum, char *buf, size_t bufsiz);
DLL_IMP_EXP char *escape_filename(const char *file_path);
DLL_IMP_EXP int Zdeflate(char *in, int in_len, char *out, int &out_len);
DLL_IMP_EXP int Zinflate(char *in, int in_len, char *out, int &out_len);
DLL_IMP_EXP void stack_trace();
DLL_IMP_EXP int safer_unlink(const char *pathname, const char *regex);
DLL_IMP_EXP int secure_erase(JCR *jcr, const char *pathname);
DLL_IMP_EXP void set_secure_erase_cmdline(const char *cmdline);
DLL_IMP_EXP bool path_exists(const char *path);
DLL_IMP_EXP bool path_exists(POOL_MEM &path);
DLL_IMP_EXP bool path_is_directory(const char *path);
DLL_IMP_EXP bool path_is_directory(POOL_MEM &path);
DLL_IMP_EXP bool path_is_absolute(const char *path);
DLL_IMP_EXP bool path_is_absolute(POOL_MEM &path);
DLL_IMP_EXP bool path_get_directory(POOL_MEM &directory, POOL_MEM &path);
DLL_IMP_EXP bool path_append(char *path, const char *extra, unsigned int max_path);
DLL_IMP_EXP bool path_append(POOL_MEM &path, const char *extra);
DLL_IMP_EXP bool path_append(POOL_MEM &path, POOL_MEM &extra);
DLL_IMP_EXP bool path_create(const char *path, mode_t mode = 0750);
DLL_IMP_EXP bool path_create(POOL_MEM &path, mode_t mode = 0750);

/* compression.c */
DLL_IMP_EXP const char *cmprs_algo_to_text(uint32_t compression_algorithm);
DLL_IMP_EXP bool setup_compression_buffers(JCR *jcr, bool compatible,
                               uint32_t compression_algorithm,
                               uint32_t *compress_buf_size);
DLL_IMP_EXP bool setup_decompression_buffers(JCR *jcr, uint32_t *decompress_buf_size);
DLL_IMP_EXP bool compress_data(JCR *jcr, uint32_t compression_algorithm, char *rbuf,
                   uint32_t rsize, unsigned char *cbuf,
                   uint32_t max_compress_len, uint32_t *compress_len);
DLL_IMP_EXP bool decompress_data(JCR *jcr, const char *last_fname, int32_t stream,
                     char **data, uint32_t *length, bool want_data_stream);
DLL_IMP_EXP void cleanup_compression(JCR *jcr);

/* cram-md5.c */
DLL_IMP_EXP bool cram_md5_respond(BSOCK *bs, const char *password, int *tls_remote_need, bool *compatible);
DLL_IMP_EXP bool cram_md5_challenge(BSOCK *bs, const char *password, int tls_local_need, bool compatible);
DLL_IMP_EXP void hmac_md5(uint8_t *text, int text_len, uint8_t *key, int key_len, uint8_t *hmac);

/* crypto.c */
DLL_IMP_EXP int init_crypto(void);
DLL_IMP_EXP int cleanup_crypto(void);
DLL_IMP_EXP DIGEST *crypto_digest_new(JCR *jcr, crypto_digest_t type);
DLL_IMP_EXP bool crypto_digest_update(DIGEST *digest, const uint8_t *data, uint32_t length);
DLL_IMP_EXP bool crypto_digest_finalize(DIGEST *digest, uint8_t *dest, uint32_t *length);
DLL_IMP_EXP void crypto_digest_free(DIGEST *digest);
DLL_IMP_EXP SIGNATURE *crypto_sign_new(JCR *jcr);
DLL_IMP_EXP crypto_error_t crypto_sign_get_digest(SIGNATURE *sig, X509_KEYPAIR *keypair,
                                      crypto_digest_t &algorithm, DIGEST **digest);
DLL_IMP_EXP crypto_error_t crypto_sign_verify(SIGNATURE *sig, X509_KEYPAIR *keypair, DIGEST *digest);
DLL_IMP_EXP int crypto_sign_add_signer(SIGNATURE *sig, DIGEST *digest, X509_KEYPAIR *keypair);
DLL_IMP_EXP int crypto_sign_encode(SIGNATURE *sig, uint8_t *dest, uint32_t *length);
DLL_IMP_EXP SIGNATURE *crypto_sign_decode(JCR *jcr, const uint8_t *sigData, uint32_t length);
DLL_IMP_EXP void crypto_sign_free(SIGNATURE *sig);
DLL_IMP_EXP CRYPTO_SESSION *crypto_session_new(crypto_cipher_t cipher, alist *pubkeys);
DLL_IMP_EXP void crypto_session_free(CRYPTO_SESSION *cs);
DLL_IMP_EXP bool crypto_session_encode(CRYPTO_SESSION *cs, uint8_t *dest, uint32_t *length);
DLL_IMP_EXP crypto_error_t crypto_session_decode(const uint8_t *data, uint32_t length, alist *keypairs, CRYPTO_SESSION **session);
DLL_IMP_EXP CRYPTO_SESSION *crypto_session_decode(const uint8_t *data, uint32_t length);
DLL_IMP_EXP CIPHER_CONTEXT *crypto_cipher_new(CRYPTO_SESSION *cs, bool encrypt, uint32_t *blocksize);
DLL_IMP_EXP bool crypto_cipher_update(CIPHER_CONTEXT *cipher_ctx, const uint8_t *data, uint32_t length, const uint8_t *dest, uint32_t *written);
DLL_IMP_EXP bool crypto_cipher_finalize(CIPHER_CONTEXT *cipher_ctx, uint8_t *dest, uint32_t *written);
DLL_IMP_EXP void crypto_cipher_free(CIPHER_CONTEXT *cipher_ctx);
DLL_IMP_EXP X509_KEYPAIR *crypto_keypair_new(void);
DLL_IMP_EXP X509_KEYPAIR *crypto_keypair_dup(X509_KEYPAIR *keypair);
DLL_IMP_EXP int crypto_keypair_load_cert(X509_KEYPAIR *keypair, const char *file);
DLL_IMP_EXP bool crypto_keypair_has_key(const char *file);
DLL_IMP_EXP int crypto_keypair_load_key(X509_KEYPAIR *keypair, const char *file, CRYPTO_PEM_PASSWD_CB *pem_callback, const void *pem_userdata);
DLL_IMP_EXP void crypto_keypair_free(X509_KEYPAIR *keypair);
DLL_IMP_EXP int crypto_default_pem_callback(char *buf, int size, const void *userdata);
DLL_IMP_EXP const char *crypto_digest_name(crypto_digest_t type);
DLL_IMP_EXP const char *crypto_digest_name(DIGEST *digest);
DLL_IMP_EXP crypto_digest_t crypto_digest_stream_type(int stream);
DLL_IMP_EXP const char *crypto_strerror(crypto_error_t error);

/* crypto_openssl.c */
#ifdef HAVE_OPENSSL
DLL_IMP_EXP void openssl_post_errors(int code, const char *errstring);
DLL_IMP_EXP void openssl_post_errors(JCR *jcr, int code, const char *errstring);
DLL_IMP_EXP int openssl_init_threads(void);
DLL_IMP_EXP void openssl_cleanup_threads(void);
DLL_IMP_EXP int openssl_seed_prng(void);
DLL_IMP_EXP int openssl_save_prng(void);
#endif /* HAVE_OPENSSL */

/* crypto_wrap.c */
DLL_IMP_EXP void aes_wrap(uint8_t *kek, int n, uint8_t *plain, uint8_t *cipher);
DLL_IMP_EXP int aes_unwrap(uint8_t *kek, int n, uint8_t *cipher, uint8_t *plain);

/* daemon.c */
DLL_IMP_EXP void daemon_start();

/* edit.c */
DLL_IMP_EXP uint64_t str_to_uint64(const char *str);
DLL_IMP_EXP int64_t str_to_int64(const char *str);
#define str_to_int16(str)((int16_t)str_to_int64(str))
#define str_to_int32(str)((int32_t)str_to_int64(str))
DLL_IMP_EXP char *edit_uint64_with_commas(uint64_t val, char *buf);
DLL_IMP_EXP char *edit_uint64_with_suffix(uint64_t val, char *buf);
DLL_IMP_EXP char *add_commas(char *val, char *buf);
DLL_IMP_EXP char *edit_uint64(uint64_t val, char *buf);
DLL_IMP_EXP char *edit_int64(int64_t val, char *buf);
DLL_IMP_EXP char *edit_int64_with_commas(int64_t val, char *buf);
DLL_IMP_EXP bool duration_to_utime(char *str, utime_t *value);
DLL_IMP_EXP bool size_to_uint64(char *str, uint64_t *value);
DLL_IMP_EXP bool speed_to_uint64(char *str, uint64_t *value);
DLL_IMP_EXP char *edit_utime(utime_t val, char *buf, int buf_len);
DLL_IMP_EXP char *edit_pthread(pthread_t val, char *buf, int buf_len);
DLL_IMP_EXP bool is_a_number(const char *num);
DLL_IMP_EXP bool is_a_number_list(const char *n);
DLL_IMP_EXP bool is_an_integer(const char *n);
DLL_IMP_EXP bool is_name_valid(const char *name, POOLMEM *&msg);
DLL_IMP_EXP bool is_name_valid(const char *name);

/* jcr.c (most definitions are in src/jcr.h) */
DLL_IMP_EXP void init_last_jobs_list();
DLL_IMP_EXP void term_last_jobs_list();
DLL_IMP_EXP void lock_last_jobs_list();
DLL_IMP_EXP void unlock_last_jobs_list();
DLL_IMP_EXP bool read_last_jobs_list(int fd, uint64_t addr);
DLL_IMP_EXP uint64_t write_last_jobs_list(int fd, uint64_t addr);
DLL_IMP_EXP void write_state_file(char *dir, const char *progname, int port);
DLL_IMP_EXP void register_job_end_callback(JCR *jcr, void job_end_cb(JCR *jcr,void *), void *ctx);
DLL_IMP_EXP void lock_jobs();
DLL_IMP_EXP void unlock_jobs();
DLL_IMP_EXP JCR *jcr_walk_start();
DLL_IMP_EXP JCR *jcr_walk_next(JCR *prev_jcr);
DLL_IMP_EXP void jcr_walk_end(JCR *jcr);
DLL_IMP_EXP int job_count();
DLL_IMP_EXP JCR *get_jcr_from_tsd();
DLL_IMP_EXP void set_jcr_in_tsd(JCR *jcr);
DLL_IMP_EXP void remove_jcr_from_tsd(JCR *jcr);
DLL_IMP_EXP uint32_t get_jobid_from_tsd();
DLL_IMP_EXP uint32_t get_jobid_from_tid(pthread_t tid);

/* json.c */
DLL_IMP_EXP void initialize_json();

/* lex.c */
DLL_IMP_EXP LEX *lex_close_file(LEX *lf);
DLL_IMP_EXP LEX *lex_open_file(LEX *lf,
                   const char *fname,
                   LEX_ERROR_HANDLER *scan_error,
                   LEX_WARNING_HANDLER *scan_warning);
DLL_IMP_EXP LEX *lex_new_buffer(LEX *lf,
                    LEX_ERROR_HANDLER *scan_error,
                    LEX_WARNING_HANDLER *scan_warning);
DLL_IMP_EXP int lex_get_char(LEX *lf);
DLL_IMP_EXP void lex_unget_char(LEX *lf);
DLL_IMP_EXP const char *lex_tok_to_str(int token);
DLL_IMP_EXP int lex_get_token(LEX *lf, int expect);
DLL_IMP_EXP void lex_set_default_error_handler(LEX *lf);
DLL_IMP_EXP void lex_set_default_warning_handler(LEX *lf);
DLL_IMP_EXP void lex_set_error_handler_error_type(LEX *lf, int err_type);

/* message.c */
DLL_IMP_EXP void my_name_is(int argc, char *argv[], const char *name);
DLL_IMP_EXP void init_msg(JCR *jcr, MSGSRES *msg, job_code_callback_t job_code_callback = NULL);
DLL_IMP_EXP void term_msg(void);
DLL_IMP_EXP void close_msg(JCR *jcr);
DLL_IMP_EXP void add_msg_dest(MSGSRES *msg, int dest, int type,
                  char *where, char *mail_cmd, char *timestamp_format);
DLL_IMP_EXP void rem_msg_dest(MSGSRES *msg, int dest, int type, char *where);
DLL_IMP_EXP void Jmsg(JCR *jcr, int type, utime_t mtime, const char *fmt, ...);
DLL_IMP_EXP void dispatch_message(JCR *jcr, int type, utime_t mtime, char *buf);
DLL_IMP_EXP void init_console_msg(const char *wd);
DLL_IMP_EXP void free_msgs_res(MSGSRES *msgs);
DLL_IMP_EXP void dequeue_messages(JCR *jcr);
DLL_IMP_EXP void set_trace(int trace_flag);
DLL_IMP_EXP bool get_trace(void);
DLL_IMP_EXP void set_hangup(int hangup_value);
DLL_IMP_EXP bool get_hangup(void);
DLL_IMP_EXP void set_timestamp(int timestamp_flag);
DLL_IMP_EXP bool get_timestamp(void);
DLL_IMP_EXP void set_db_type(const char *name);
DLL_IMP_EXP void register_message_callback(void msg_callback(int type, char *msg));

/* passphrase.c */
DLL_IMP_EXP char *generate_crypto_passphrase(uint16_t length);

/* path_list.c */
DLL_IMP_EXP htable *path_list_init();
DLL_IMP_EXP bool path_list_lookup(htable *path_list, const char *fname);
DLL_IMP_EXP bool path_list_add(htable *path_list, uint32_t len, const char *fname);
DLL_IMP_EXP void free_path_list(htable *path_list);

/* poll.c */
DLL_IMP_EXP int wait_for_readable_fd(int fd, int sec, bool ignore_interupts);
DLL_IMP_EXP int wait_for_writable_fd(int fd, int sec, bool ignore_interupts);

/* pythonlib.c */
DLL_IMP_EXP int generate_daemon_event(JCR *jcr, const char *event);

/* scan.c */
DLL_IMP_EXP void strip_leading_space(char *str);
DLL_IMP_EXP void strip_trailing_junk(char *str);
DLL_IMP_EXP void strip_trailing_newline(char *str);
DLL_IMP_EXP void strip_trailing_slashes(char *dir);
DLL_IMP_EXP bool skip_spaces(char **msg);
DLL_IMP_EXP bool skip_nonspaces(char **msg);
DLL_IMP_EXP int fstrsch(const char *a, const char *b);
DLL_IMP_EXP char *next_arg(char **s);
DLL_IMP_EXP int parse_args(POOLMEM *cmd, POOLMEM *&args, int *argc,
               char **argk, char **argv, int max_args);
DLL_IMP_EXP int parse_args_only(POOLMEM *cmd, POOLMEM *&args, int *argc,
                    char **argk, char **argv, int max_args);
DLL_IMP_EXP void split_path_and_filename(const char *fname, POOLMEM *&path,
                             int *pnl, POOLMEM *&file, int *fnl);
DLL_IMP_EXP int bsscanf(const char *buf, const char *fmt, ...);

/* scsi_crypto.c */
DLL_IMP_EXP bool clear_scsi_encryption_key(int fd, const char *device);
DLL_IMP_EXP bool set_scsi_encryption_key(int fd, const char *device, char *encryption_key);
DLL_IMP_EXP int get_scsi_drive_encryption_status(int fd, const char *device_name,
                                     POOLMEM *&status, int indent);
DLL_IMP_EXP int get_scsi_volume_encryption_status(int fd, const char *device_name,
                                      POOLMEM *&status, int indent);
DLL_IMP_EXP bool need_scsi_crypto_key(int fd, const char *device_name, bool use_drive_status);
DLL_IMP_EXP bool is_scsi_encryption_enabled(int fd, const char *device_name);

/* scsi_lli.c */
DLL_IMP_EXP bool recv_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len);
DLL_IMP_EXP bool send_scsi_cmd_page(int fd, const char *device_name,
                        void *cdb, unsigned int cdb_len,
                        void *cmd_page, unsigned int cmd_page_len);
DLL_IMP_EXP bool check_scsi_at_eod(int fd);

/* scsi_tapealert.c */
DLL_IMP_EXP bool get_tapealert_flags(int fd, const char *device_name, uint64_t *flags);

/* signal.c */
DLL_IMP_EXP void init_signals(void terminate(int sig));
DLL_IMP_EXP void init_stack_dump(void);

/* timers.c */
DLL_IMP_EXP btimer_t *start_child_timer(JCR *jcr, pid_t pid, uint32_t wait);
DLL_IMP_EXP void stop_child_timer(btimer_t *wid);
DLL_IMP_EXP btimer_t *start_thread_timer(JCR *jcr, pthread_t tid, uint32_t wait);
DLL_IMP_EXP void stop_thread_timer(btimer_t *wid);
DLL_IMP_EXP btimer_t *start_bsock_timer(BSOCK *bs, uint32_t wait);
DLL_IMP_EXP void stop_bsock_timer(btimer_t *wid);

/* tls.c */
DLL_IMP_EXP TLS_CONTEXT *new_tls_context(const char *ca_certfile,
                             const char *ca_certdir,
                             const char *crlfile,
                             const char *certfile,
                             const char *keyfile,
                             CRYPTO_PEM_PASSWD_CB *pem_callback,
                             const void *pem_userdata,
                             const char *dhfile,
                             const char *cipherlist,
                             bool verify_peer);
DLL_IMP_EXP void free_tls_context(TLS_CONTEXT *ctx);
#ifdef HAVE_TLS
DLL_IMP_EXP bool tls_postconnect_verify_host(JCR *jcr, TLS_CONNECTION *tls_conn,
                                 const char *host);
DLL_IMP_EXP bool tls_postconnect_verify_cn(JCR *jcr, TLS_CONNECTION *tls_conn,
                               alist *verify_list);
DLL_IMP_EXP TLS_CONNECTION *new_tls_connection(TLS_CONTEXT *ctx, int fd, bool server);
DLL_IMP_EXP bool tls_bsock_accept(BSOCK *bsock);
DLL_IMP_EXP int tls_bsock_writen(BSOCK *bsock, char *ptr, int32_t nbytes);
DLL_IMP_EXP int tls_bsock_readn(BSOCK *bsock, char *ptr, int32_t nbytes);
#endif /* HAVE_TLS */
DLL_IMP_EXP bool tls_bsock_connect(BSOCK *bsock);
DLL_IMP_EXP void tls_bsock_shutdown(BSOCK *bsock);
DLL_IMP_EXP void free_tls_connection(TLS_CONNECTION *tls_conn);
DLL_IMP_EXP bool get_tls_require(TLS_CONTEXT *ctx);
DLL_IMP_EXP void set_tls_require(TLS_CONTEXT *ctx, bool value);
DLL_IMP_EXP bool get_tls_enable(TLS_CONTEXT *ctx);
DLL_IMP_EXP void set_tls_enable(TLS_CONTEXT *ctx, bool value);
DLL_IMP_EXP bool get_tls_verify_peer(TLS_CONTEXT *ctx);

/* util.c */
DLL_IMP_EXP void escape_string(POOL_MEM &snew, char *old, int len);
DLL_IMP_EXP bool is_buf_zero(char *buf, int len);
DLL_IMP_EXP void lcase(char *str);
DLL_IMP_EXP void bash_spaces(char *str);
DLL_IMP_EXP void bash_spaces(POOL_MEM &pm);
DLL_IMP_EXP void unbash_spaces(char *str);
DLL_IMP_EXP void unbash_spaces(POOL_MEM &pm);
DLL_IMP_EXP const char* indent_multiline_string(POOL_MEM &resultbuffer, const char *multilinestring, const char *separator);
DLL_IMP_EXP char *encode_time(utime_t time, char *buf);
DLL_IMP_EXP bool convert_timeout_to_timespec(timespec &timeout, int timeout_in_seconds);
DLL_IMP_EXP char *encode_mode(mode_t mode, char *buf);
DLL_IMP_EXP int do_shell_expansion(char *name, int name_len);
DLL_IMP_EXP void jobstatus_to_ascii(int JobStatus, char *msg, int maxlen);
DLL_IMP_EXP void jobstatus_to_ascii_gui(int JobStatus, char *msg, int maxlen);
DLL_IMP_EXP int run_program(char *prog, int wait, POOLMEM *&results);
DLL_IMP_EXP int run_program_full_output(char *prog, int wait, POOLMEM *&results);
DLL_IMP_EXP char *action_on_purge_to_string(int aop, POOL_MEM &ret);
DLL_IMP_EXP const char *job_type_to_str(int type);
DLL_IMP_EXP const char *job_status_to_str(int stat);
DLL_IMP_EXP const char *job_level_to_str(int level);
DLL_IMP_EXP const char *volume_status_to_str(const char *status);
DLL_IMP_EXP void make_session_key(char *key, char *seed, int mode);
DLL_IMP_EXP void encode_session_key(char *encode, char *session, char *key, int maxlen);
DLL_IMP_EXP void decode_session_key(char *decode, char *session, char *key, int maxlen);
DLL_IMP_EXP POOLMEM *edit_job_codes(JCR *jcr, char *omsg, char *imsg, const char *to, job_code_callback_t job_code_callback = NULL);
DLL_IMP_EXP void set_working_directory(char *wd);
DLL_IMP_EXP const char *last_path_separator(const char *str);

/* watchdog.c */
DLL_IMP_EXP int start_watchdog(void);
DLL_IMP_EXP int stop_watchdog(void);
DLL_IMP_EXP watchdog_t *new_watchdog(void);
DLL_IMP_EXP bool register_watchdog(watchdog_t *wd);
DLL_IMP_EXP bool unregister_watchdog(watchdog_t *wd);
DLL_IMP_EXP bool is_watchdog();

#endif /* __LIBPROTOS_H */
