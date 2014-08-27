/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
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
 * Prototypes for findlib directory of Bareos
 */

/* acl.c */
bacl_exit_code build_acl_streams(JCR *jcr, acl_data_t *acl_data, FF_PKT *ff_pkt);
bacl_exit_code parse_acl_streams(JCR *jcr, acl_data_t *acl_data,
                                 int stream, char *content, uint32_t content_length);

/* attribs.c */
int encode_attribsEx(JCR *jcr, char *attribsEx, FF_PKT *ff_pkt);
bool set_attributes(JCR *jcr, ATTR *attr, BFILE *ofd);
int select_data_stream(FF_PKT *ff_pkt, bool compatible);

/* create_file.c */
int create_file(JCR *jcr, ATTR *attr, BFILE *ofd, int replace);

/* find.c */
FF_PKT *init_find_files();
void set_find_options(FF_PKT *ff, int incremental, time_t mtime);
void set_find_changed_function(FF_PKT *ff, bool check_fct(JCR *jcr, FF_PKT *ff));
int find_files(JCR *jcr, FF_PKT *ff, int file_sub(JCR *, FF_PKT *ff_pkt, bool),
               int plugin_sub(JCR *, FF_PKT *ff_pkt, bool));
bool match_files(JCR *jcr, FF_PKT *ff, int sub(JCR *, FF_PKT *ff_pkt, bool));
int term_find_files(FF_PKT *ff);
bool is_in_fileset(FF_PKT *ff);
bool accept_file(FF_PKT *ff);
findINCEXE *allocate_new_incexe(void);
findINCEXE *new_exclude(findFILESET *fileset);
findINCEXE *new_include(findFILESET *fileset);
findINCEXE *new_preinclude(findFILESET *fileset);
findINCEXE *new_preexclude(findFILESET *fileset);
findFOPTS *start_options(FF_PKT *ff);
void new_options(FF_PKT *ff, findINCEXE *incexe);

/* match.c */
void init_include_exclude_files(FF_PKT *ff);
void term_include_exclude_files(FF_PKT *ff);
void add_fname_to_include_list(FF_PKT *ff, int prefixed, const char *fname);
void add_fname_to_exclude_list(FF_PKT *ff, const char *fname);
bool file_is_excluded(FF_PKT *ff, const char *file);
bool file_is_included(FF_PKT *ff, const char *file);
struct s_included_file *get_next_included_file(FF_PKT *ff,
                                               struct s_included_file *inc);
bool parse_size_match(const char *size_match_pattern,
                      struct s_sz_matching *size_matching);

/* find_one.c */
int find_one_file(JCR *jcr, FF_PKT *ff,
                  int handle_file(JCR *jcr, FF_PKT *ff_pkt, bool top_level),
                  char *p, dev_t parent_device, bool top_level);
int term_find_one(FF_PKT *ff);
bool has_file_changed(JCR *jcr, FF_PKT *ff_pkt);
bool check_changes(JCR *jcr, FF_PKT *ff_pkt);

/* get_priv.c */
int enable_backup_privileges(JCR *jcr, int ignore_errors);

/* hardlink.c */
CurLink *lookup_hardlink(JCR *jcr, FF_PKT *ff_pkt, ino_t ino, dev_t dev);
CurLink *new_hardlink(JCR *jcr, FF_PKT *ff_pkt, char *fname, ino_t ino, dev_t dev);
void ff_pkt_set_link_digest(FF_PKT *ff_pkt,
                            int32_t digest_stream,
                            const char *digest,
                            uint32_t len);

/* makepath.c */
bool makepath(ATTR *attr, const char *path, mode_t mode,
              mode_t parent_mode, uid_t owner, gid_t group,
              bool keep_dir_modes);
void free_path_list(JCR *jcr);
bool path_list_lookup(JCR *jcr, char *fname);
bool path_list_add(JCR *jcr, uint32_t len, char *fname);

/* fstype.c */
bool fstype(const char *fname, char *fs, int fslen);
bool fstype_equals(const char *fname, const char *fstypename);

/* drivetype.c */
bool drivetype(const char *fname, char *fs, int fslen);

/* shadowing.c */
void check_include_list_shadowing(JCR *jcr, findFILESET *fileset);

#if defined(HAVE_WIN32)
/* win32.c */
int get_win32_driveletters(findFILESET *fileset, char *szDrives);
int get_win32_virtualmountpoints(findFILESET *fileset, dlist **szVmps);
bool expand_win32_fileset(findFILESET *fileset);
bool exclude_win32_not_to_backup_registry_entries(JCR *jcr, FF_PKT *ff);
int win32_send_to_copy_thread(JCR *jcr, BFILE *bfd, char *data, const int32_t length);
void win32_flush_copy_thread(JCR *jcr);
void win32_cleanup_copy_thread(JCR *jcr);
#endif

/* xattr.c */
bxattr_exit_code build_xattr_streams(JCR *jcr, struct xattr_data_t *xattr_data, FF_PKT *ff_pkt);
bxattr_exit_code parse_xattr_streams(JCR *jcr, struct xattr_data_t *xattr_data,
                                     int stream, char *content, uint32_t content_length);

/* bfile.c -- see bfile.h */
