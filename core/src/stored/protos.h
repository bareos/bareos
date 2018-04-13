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
/*
 * Kern Sibbald MM
 */
/**
 * @file
 * Protypes for stored
 */

/* stored.c */
uint32_t new_VolSessionId();

/* acquire.c */
DeviceControlRecord *acquire_device_for_append(DeviceControlRecord *dcr);
bool acquire_device_for_read(DeviceControlRecord *dcr);
bool release_device(DeviceControlRecord *dcr);
bool clean_device(DeviceControlRecord *dcr);
void setup_new_dcr_device(JobControlRecord *jcr, DeviceControlRecord *dcr, Device *dev, BlockSizes *blocksizes);
void free_dcr(DeviceControlRecord *dcr);

/* append.c */
bool do_append_data(JobControlRecord *jcr, BareosSocket *bs, const char *what);
bool send_attrs_to_dir(JobControlRecord *jcr, DeviceRecord *rec);

/* authenticate.c */
bool authenticate_director(JobControlRecord *jcr);
bool authenticate_storagedaemon(JobControlRecord *jcr);
bool authenticate_with_storagedaemon(JobControlRecord *jcr);
bool authenticate_filedaemon(JobControlRecord *jcr);
bool authenticate_with_filedaemon(JobControlRecord *jcr);

/* autochanger.c */
bool init_autochangers();
int autoload_device(DeviceControlRecord *dcr, int writing, BareosSocket *dir);
bool autochanger_cmd(DeviceControlRecord *dcr, BareosSocket *dir, const char *cmd);
bool autochanger_transfer_cmd(DeviceControlRecord *dcr, BareosSocket *dir,
                              slot_number_t src_slot, slot_number_t dst_slot);
bool unload_autochanger(DeviceControlRecord *dcr, slot_number_t loaded, bool lock_set = false);
bool unload_dev(DeviceControlRecord *dcr, Device *dev, bool lock_set = false);
slot_number_t get_autochanger_loaded_slot(DeviceControlRecord *dcr, bool lock_set = false);

/* block.c */
void dump_block(DeviceBlock *b, const char *msg);
DeviceBlock *new_block(Device *dev);
DeviceBlock *dup_block(DeviceBlock *eblock);
void init_block_write(DeviceBlock *block);
void empty_block(DeviceBlock *block);
void free_block(DeviceBlock *block);
void print_block_read_errors(JobControlRecord *jcr, DeviceBlock *block);
void ser_block_header(DeviceBlock *block);

/* butil.c -- utilities for SD tool programs */
void print_ls_output(const char *fname, const char *link, int type, struct stat *statp);
JobControlRecord *setup_jcr(const char *name, char *dev_name,
               BootStrapRecord *bsr, DirectorResource *director, DeviceControlRecord* dcr,
               const char *VolumeName, bool readonly);
void display_tape_error_status(JobControlRecord *jcr, Device *dev);

/* crc32.c */
uint32_t bcrc32(uint8_t *buf, int len);

/* dev.c */
Device *init_dev(JobControlRecord *jcr, DeviceResource *device);
bool can_open_mounted_dev(Device *dev);
bool load_dev(Device *dev);
int write_block(Device *dev);
void attach_jcr_to_device(Device *dev, JobControlRecord *jcr);
void detach_jcr_from_device(Device *dev, JobControlRecord *jcr);
JobControlRecord *next_attached_jcr(Device *dev, JobControlRecord *jcr);
void init_device_wait_timers(DeviceControlRecord *dcr);
void init_jcr_device_wait_timers(JobControlRecord *jcr);
bool double_dev_wait_time(Device *dev);

/* device.c */
bool open_device(DeviceControlRecord *dcr);
bool first_open_device(DeviceControlRecord *dcr);
bool fixup_device_block_write_error(DeviceControlRecord *dcr, int retries = 4);
void set_start_vol_position(DeviceControlRecord *dcr);
void set_new_volume_parameters(DeviceControlRecord *dcr);
void set_new_file_parameters(DeviceControlRecord *dcr);
BootStrapRecord *position_device_to_first_file(JobControlRecord *jcr, DeviceControlRecord *dcr);
bool try_device_repositioning(JobControlRecord *jcr, DeviceRecord *rec, DeviceControlRecord *dcr);

/* dir_cmd.c */
void *handle_director_connection(BareosSocket *dir);

/* fd_cmds.c */
void *handle_filed_connection(BareosSocket *fd, char *job_name);
void run_job(JobControlRecord *jcr);
void do_fd_commands(JobControlRecord *jcr);

/* job.c */
void stored_free_jcr(JobControlRecord *jcr);

/* label.c */
int read_dev_volume_label(DeviceControlRecord *dcr);
void create_volume_label(Device *dev, const char *VolName, const char *PoolName);

#define ANSI_VOL_LABEL 0
#define ANSI_EOF_LABEL 1
#define ANSI_EOV_LABEL 2

bool write_ansi_ibm_labels(DeviceControlRecord *dcr, int type, const char *VolName);
int read_ansi_ibm_label(DeviceControlRecord *dcr);
bool write_session_label(DeviceControlRecord *dcr, int label);
void dump_volume_label(Device *dev);
void dump_label_record(Device *dev, DeviceRecord *rec, bool verbose);
bool unser_volume_label(Device *dev, DeviceRecord *rec);
bool unser_session_label(SESSION_LABEL *label, DeviceRecord *rec);
bool write_new_volume_label_to_dev(DeviceControlRecord *dcr, const char *VolName,
                                   const char *PoolName, bool relabel);

/* locks.c */
void _lock_device(const char *file, int line, Device *dev);
void _unlock_device(const char *file, int line, Device *dev);
void _block_device(const char *file, int line, Device *dev, int state);
void _unblock_device(const char *file, int line, Device *dev);
void _steal_device_lock(const char *file, int line, Device *dev, bsteal_lock_t *hold, int state);
void _give_back_device_lock(const char *file, int line, Device *dev, bsteal_lock_t *hold);

/* match_bsr.c */
int match_bsr(BootStrapRecord *bsr, DeviceRecord *rec, VOLUME_LABEL *volrec,
              SESSION_LABEL *sesrec, JobControlRecord *jcr);
int match_bsr_block(BootStrapRecord *bsr, DeviceBlock *block);
void position_bsr_block(BootStrapRecord *bsr, DeviceBlock *block);
BootStrapRecord *find_next_bsr(BootStrapRecord *root_bsr, Device *dev);
bool is_this_bsr_done(BootStrapRecord *bsr, DeviceRecord *rec);
uint64_t get_bsr_start_addr(BootStrapRecord *bsr,
                            uint32_t *file = NULL,
                            uint32_t *block = NULL);

/* mount.c */
bool mount_next_read_volume(DeviceControlRecord *dcr);

/* ndmp_tape.c */
void end_of_ndmp_backup(JobControlRecord *jcr);
void end_of_ndmp_restore(JobControlRecord *jcr);
int start_ndmp_thread_server(dlist *addr_list, int max_clients);
void stop_ndmp_thread_server();

/* parse_bsr.c */
BootStrapRecord *parse_bsr(JobControlRecord *jcr, char *lf);
void dump_bsr(BootStrapRecord *bsr, bool recurse);
DLL_IMP_EXP void free_bsr(BootStrapRecord *bsr);
void free_restore_volume_list(JobControlRecord *jcr);
void create_restore_volume_list(JobControlRecord *jcr);

/* status.c */
bool status_cmd(JobControlRecord *jcr);
bool dotstatus_cmd(JobControlRecord *jcr);
#if defined(HAVE_WIN32)
char *bareos_status(char *buf, int buf_len);
#endif

/* read.c */
DLL_IMP_EXP bool do_read_data(JobControlRecord *jcr);

/* read_record.c */
DLL_IMP_EXP READ_CTX *new_read_context(void);
DLL_IMP_EXP void free_read_context(READ_CTX *rctx);
DLL_IMP_EXP void read_context_set_record(DeviceControlRecord *dcr, READ_CTX *rctx);
DLL_IMP_EXP bool read_next_block_from_device(DeviceControlRecord *dcr,
                                 SESSION_LABEL *sessrec,
                                 bool record_cb(DeviceControlRecord *dcr, DeviceRecord *rec),
                                 bool mount_cb(DeviceControlRecord *dcr),
                                 bool *status);
DLL_IMP_EXP bool read_next_record_from_block(DeviceControlRecord *dcr,
                                 READ_CTX *rctx,
                                 bool *done);
DLL_IMP_EXP bool read_records(DeviceControlRecord *dcr,
                  bool record_cb(DeviceControlRecord *dcr, DeviceRecord *rec),
                  bool mount_cb(DeviceControlRecord *dcr));

/* record.c */
DLL_IMP_EXP const char *FI_to_ascii(char *buf, int fi);
DLL_IMP_EXP const char *stream_to_ascii(char *buf, int stream, int fi);
DLL_IMP_EXP const char *record_to_str(PoolMem &resultbuffer, JobControlRecord *jcr, const DeviceRecord *rec);
DLL_IMP_EXP void dump_record(const char *tag, const DeviceRecord *rec);
DLL_IMP_EXP bool write_record_to_block(DeviceControlRecord *dcr, DeviceRecord *rec);
DLL_IMP_EXP bool can_write_record_to_block(DeviceBlock *block, const DeviceRecord *rec);
DLL_IMP_EXP bool read_record_from_block(DeviceControlRecord *dcr, DeviceRecord *rec);
DLL_IMP_EXP DeviceRecord *new_record(bool with_data = true);
DLL_IMP_EXP void empty_record(DeviceRecord *rec);
DLL_IMP_EXP void copy_record_state(DeviceRecord *dst, DeviceRecord *src);
DLL_IMP_EXP void free_record(DeviceRecord *rec);
DLL_IMP_EXP uint64_t get_record_address(const DeviceRecord *rec);

/* reserve.c */
DLL_IMP_EXP void init_reservations_lock();
DLL_IMP_EXP void term_reservations_lock();
DLL_IMP_EXP void _lock_reservations(const char *file = "**Unknown**", int line = 0);
DLL_IMP_EXP void _unlock_reservations();
DLL_IMP_EXP void _lock_volumes(const char *file = "**Unknown**", int line = 0);
DLL_IMP_EXP void _unlock_volumes();
DLL_IMP_EXP void _lock_read_volumes(const char *file = "**Unknown**", int line = 0);
DLL_IMP_EXP void _unlock_read_volumes();
DLL_IMP_EXP void unreserve_device(DeviceControlRecord *dcr);
DLL_IMP_EXP bool find_suitable_device_for_job(JobControlRecord *jcr, ReserveContext &rctx);
DLL_IMP_EXP int search_res_for_device(ReserveContext &rctx);
DLL_IMP_EXP void release_reserve_messages(JobControlRecord *jcr);

#ifdef SD_DEBUG_LOCK
DLL_IMP_EXP extern int reservations_lock_count;
DLL_IMP_EXP extern int vol_list_lock_count;
DLL_IMP_EXP extern int read_vol_list_lock_count;

#define lock_reservations() \
         do { Dmsg3(sd_dbglvl, "lock_reservations at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    reservations_lock_count); \
              _lock_reservations(__FILE__, __LINE__); \
              Dmsg0(sd_dbglvl, "lock_reservations: got lock\n"); \
         } while (0)
#define unlock_reservations() \
         do { Dmsg3(sd_dbglvl, "unlock_reservations at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    reservations_lock_count); \
              _unlock_reservations(); } while (0)
#define lock_volumes() \
         do { Dmsg3(sd_dbglvl, "lock_volumes at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    vol_list_lock_count); \
              _lock_volumes(__FILE__, __LINE__); \
              Dmsg0(sd_dbglvl, "lock_volumes: got lock\n"); \
         } while (0)
#define unlock_volumes() \
         do { Dmsg3(sd_dbglvl, "unlock_volumes at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    vol_list_lock_count); \
              _unlock_volumes(); } while (0)
#define lock_read_volumes() \
         do { Dmsg3(sd_dbglvl, "lock_read_volumes at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    read_vol_list_lock_count); \
              _lock_read_volumes(__FILE__, __LINE__); \
              Dmsg0(sd_dbglvl, "lock_read_volumes: got lock\n"); \
         } while (0)
#define unlock_read_volumes() \
         do { Dmsg3(sd_dbglvl, "unlock_read_volumes at %s:%d precnt=%d\n", \
                    __FILE__, __LINE__, \
                    read_vol_list_lock_count); \
              _unlock_read_volumes(); } while (0)
#else
#define lock_reservations() _lock_reservations(__FILE__, __LINE__)
#define unlock_reservations() _unlock_reservations()
#define lock_volumes() _lock_volumes(__FILE__, __LINE__)
#define unlock_volumes() _unlock_volumes()
#define lock_read_volumes() _lock_read_volumes(__FILE__, __LINE__)
#define unlock_read_volumes() _unlock_read_volumes()
#endif

/* sd_backends.c */
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
void sd_set_backend_dirs(alist *new_backend_dirs);
Device *init_backend_dev(JobControlRecord *jcr, int device_type);
void dev_flush_backends();
#endif

/* sd_cmds.c */
void *handle_stored_connection(BareosSocket *sd, char *job_name);
bool do_listen_run(JobControlRecord *jcr);

/* sd_plugins.c */
char *edit_device_codes(DeviceControlRecord *dcr, POOLMEM *&omsg, const char *imsg, const char *cmd);

/* sd_stats.c */
int start_statistics_thread(void);
void stop_statistics_thread();
void update_device_tapealert(const char *devname, uint64_t flags, utime_t now);
void update_job_statistics(JobControlRecord *jcr, utime_t now);

/* socket_server.c */
void start_socket_server(dlist *addrs);
void stop_socket_server();

/* spool.c */
bool begin_data_spool (DeviceControlRecord *dcr);
bool discard_data_spool (DeviceControlRecord *dcr);
bool commit_data_spool (DeviceControlRecord *dcr);
bool are_attributes_spooled (JobControlRecord *jcr);
bool begin_attribute_spool (JobControlRecord *jcr);
bool discard_attribute_spool (JobControlRecord *jcr);
bool commit_attribute_spool (JobControlRecord *jcr);
bool write_block_to_spool_file (DeviceControlRecord *dcr);
void list_spool_stats (void sendit(const char *msg, int len, void *sarg), void *arg);

/* vol_mgr.c */
void init_vol_list_lock();
void term_vol_list_lock();
VolumeReservationItem *reserve_volume(DeviceControlRecord *dcr, const char *VolumeName);
bool free_volume(Device *dev);
bool is_vol_list_empty();
dlist *dup_vol_list(JobControlRecord *jcr);
void free_temp_vol_list(dlist *temp_vol_list);
bool volume_unused(DeviceControlRecord *dcr);
void create_volume_lists();
void free_volume_lists();
bool is_volume_in_use(DeviceControlRecord *dcr);
void add_read_volume(JobControlRecord *jcr, const char *VolumeName);
void remove_read_volume(JobControlRecord *jcr, const char *VolumeName);

/* wait.c */
int wait_for_sysop(DeviceControlRecord *dcr);
bool wait_for_device(JobControlRecord *jcr, int &retries);
void release_device_cond();
