/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
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
 * Written by Kern Sibbald, MM
 */

/* accurate.c */
bool accurate_finish(JobControlRecord *jcr);
bool accurate_check_file(JobControlRecord *jcr, FindFilesPacket *ff_pkt);
bool accurate_mark_file_as_seen(JobControlRecord *jcr, char *fname);
bool accurate_unmark_file_as_seen(JobControlRecord *jcr, char *fname);
bool accurate_mark_all_files_as_seen(JobControlRecord *jcr);
bool accurate_unmark_all_files_as_seen(JobControlRecord *jcr);
void accurate_free(JobControlRecord *jcr);

/* authenticate.c */
bool authenticate_director(JobControlRecord *jcr);
bool authenticate_with_director(JobControlRecord *jcr, DirectorResource *director);
bool authenticate_storagedaemon(JobControlRecord *jcr);
bool authenticate_with_storagedaemon(JobControlRecord *jcr);

/* backup.c */
bool blast_data_to_storage_daemon(JobControlRecord *jcr, char *addr, crypto_cipher_t cipher);
bool encode_and_send_attributes(JobControlRecord *jcr, FindFilesPacket *ff_pkt, int &data_stream);
void strip_path(FindFilesPacket *ff_pkt);
void unstrip_path(FindFilesPacket *ff_pkt);

/* compression.c */
bool adjust_compression_buffers(JobControlRecord *jcr);
bool adjust_decompression_buffers(JobControlRecord *jcr);
bool setup_compression_context(b_ctx &bctx);

/* crypto.c */
bool crypto_session_start(JobControlRecord *jcr, crypto_cipher_t cipher);
void crypto_session_end(JobControlRecord *jcr);
bool crypto_session_send(JobControlRecord *jcr, BareosSocket *sd);
bool verify_signature(JobControlRecord *jcr, r_ctx &rctx);
bool flush_cipher(JobControlRecord *jcr, BareosWinFilePacket *bfd, uint64_t *addr, char *flags, int32_t stream,
                  RestoreCipherContext *cipher_ctx);
void deallocate_cipher(r_ctx &rctx);
void deallocate_fork_cipher(r_ctx &rctx);
bool setup_encryption_context(b_ctx &bctx);
bool setup_decryption_context(r_ctx &rctx, RestoreCipherContext &rcctx);
bool encrypt_data(b_ctx *bctx, bool *need_more_data);
bool decrypt_data(JobControlRecord *jcr, char **data, uint32_t *length, RestoreCipherContext *cipher_ctx);

/* dir_cmd.c */
JobControlRecord *create_new_director_session(BareosSocket *dir);
void *process_director_commands(JobControlRecord *jcr, BareosSocket *dir);
void *handle_director_connection(BareosSocket *dir);
bool start_connect_to_director_threads();
bool stop_connect_to_director_threads(bool wait=false);

/* estimate.c */
int make_estimate(JobControlRecord *jcr);

/* fileset.c */
bool init_fileset(JobControlRecord *jcr);
void add_file_to_fileset(JobControlRecord *jcr, const char *fname, bool is_file);
findIncludeExcludeItem *get_incexe(JobControlRecord *jcr);
void set_incexe(JobControlRecord *jcr, findIncludeExcludeItem *incexe);
int add_regex_to_fileset(JobControlRecord *jcr, const char *item, int type);
int add_wild_to_fileset(JobControlRecord *jcr, const char *item, int type);
int add_options_to_fileset(JobControlRecord *jcr, const char *item);
void add_fileset(JobControlRecord *jcr, const char *item);
bool term_fileset(JobControlRecord *jcr);

/* heartbeat.c */
void start_heartbeat_monitor(JobControlRecord *jcr);
void stop_heartbeat_monitor(JobControlRecord *jcr);
void start_dir_heartbeat(JobControlRecord *jcr);
void stop_dir_heartbeat(JobControlRecord *jcr);

/* restore.c */
void do_restore(JobControlRecord *jcr);
void free_session(r_ctx &rctx);
int do_file_digest(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool top_level);
bool sparse_data(JobControlRecord *jcr, BareosWinFilePacket *bfd, uint64_t *addr, char **data, uint32_t *length);
bool store_data(JobControlRecord *jcr, BareosWinFilePacket *bfd, char *data, const int32_t length, bool win32_decomp);

/* sd_cmds.c */
void *handle_stored_connection(BareosSocket *sd);

/* socket_server.c */
void start_socket_server(dlist *addrs);
void stop_socket_server(bool wait=false);

/* verify.c */
int digest_file(JobControlRecord *jcr, FindFilesPacket *ff_pkt, DIGEST *digest);
void do_verify(JobControlRecord *jcr);
void do_verify_volume(JobControlRecord *jcr);
bool calculate_and_compare_file_chksum(JobControlRecord *jcr, FindFilesPacket *ff_pkt,
                                       const char *fname, const char *chksum);
