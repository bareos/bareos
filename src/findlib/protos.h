/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
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
 * Prototypes for findlib directory of Bareos
 */

/* acl.c */
DLL_IMP_EXP bacl_exit_code send_acl_stream(JCR *jcr, acl_data_t *acl_data, int stream);
DLL_IMP_EXP bacl_exit_code build_acl_streams(JCR *jcr, acl_data_t *acl_data, FF_PKT *ff_pkt);
DLL_IMP_EXP bacl_exit_code parse_acl_streams(JCR *jcr, acl_data_t *acl_data,
                                 int stream, char *content, uint32_t content_length);

/* attribs.c */
DLL_IMP_EXP int encode_attribsEx(JCR *jcr, char *attribsEx, FF_PKT *ff_pkt);
DLL_IMP_EXP bool set_attributes(JCR *jcr, ATTR *attr, BFILE *ofd);
DLL_IMP_EXP int select_data_stream(FF_PKT *ff_pkt, bool compatible);

/* create_file.c */
DLL_IMP_EXP int create_file(JCR *jcr, ATTR *attr, BFILE *ofd, int replace);

/* find.c */
DLL_IMP_EXP FF_PKT *init_find_files();
DLL_IMP_EXP void set_find_options(FF_PKT *ff, bool incremental, time_t mtime);
DLL_IMP_EXP void set_find_changed_function(FF_PKT *ff, bool check_fct(JCR *jcr, FF_PKT *ff));
DLL_IMP_EXP int find_files(JCR *jcr, FF_PKT *ff, int file_sub(JCR *, FF_PKT *ff_pkt, bool),
               int plugin_sub(JCR *, FF_PKT *ff_pkt, bool));
DLL_IMP_EXP bool match_files(JCR *jcr, FF_PKT *ff, int sub(JCR *, FF_PKT *ff_pkt, bool));
DLL_IMP_EXP int term_find_files(FF_PKT *ff);
DLL_IMP_EXP bool is_in_fileset(FF_PKT *ff);
DLL_IMP_EXP bool accept_file(FF_PKT *ff);
DLL_IMP_EXP findINCEXE *allocate_new_incexe(void);
DLL_IMP_EXP findINCEXE *new_exclude(findFILESET *fileset);
DLL_IMP_EXP findINCEXE *new_include(findFILESET *fileset);
DLL_IMP_EXP findINCEXE *new_preinclude(findFILESET *fileset);
DLL_IMP_EXP findINCEXE *new_preexclude(findFILESET *fileset);
DLL_IMP_EXP findFOPTS *start_options(FF_PKT *ff);
DLL_IMP_EXP void new_options(FF_PKT *ff, findINCEXE *incexe);

/* match.c */
DLL_IMP_EXP void init_include_exclude_files(FF_PKT *ff);
DLL_IMP_EXP void term_include_exclude_files(FF_PKT *ff);
DLL_IMP_EXP void add_fname_to_include_list(FF_PKT *ff, int prefixed, const char *fname);
DLL_IMP_EXP void add_fname_to_exclude_list(FF_PKT *ff, const char *fname);
DLL_IMP_EXP bool file_is_excluded(FF_PKT *ff, const char *file);
DLL_IMP_EXP bool file_is_included(FF_PKT *ff, const char *file);
DLL_IMP_EXP struct s_included_file *get_next_included_file(FF_PKT *ff,
                                               struct s_included_file *inc);
DLL_IMP_EXP bool parse_size_match(const char *size_match_pattern,
                      struct s_sz_matching *size_matching);

/* find_one.c */
DLL_IMP_EXP int find_one_file(JCR *jcr, FF_PKT *ff,
                  int handle_file(JCR *jcr, FF_PKT *ff_pkt, bool top_level),
                  char *p, dev_t parent_device, bool top_level);
DLL_IMP_EXP int term_find_one(FF_PKT *ff);
DLL_IMP_EXP bool has_file_changed(JCR *jcr, FF_PKT *ff_pkt);
DLL_IMP_EXP bool check_changes(JCR *jcr, FF_PKT *ff_pkt);

/* get_priv.c */
DLL_IMP_EXP int enable_backup_privileges(JCR *jcr, int ignore_errors);

/* hardlink.c */
DLL_IMP_EXP CurLink *lookup_hardlink(JCR *jcr, FF_PKT *ff_pkt, ino_t ino, dev_t dev);
DLL_IMP_EXP CurLink *new_hardlink(JCR *jcr, FF_PKT *ff_pkt, char *fname, ino_t ino, dev_t dev);
DLL_IMP_EXP void ff_pkt_set_link_digest(FF_PKT *ff_pkt,
                            int32_t digest_stream,
                            const char *digest,
                            uint32_t len);

/* makepath.c */
DLL_IMP_EXP bool makepath(ATTR *attr, const char *path, mode_t mode,
              mode_t parent_mode, uid_t owner, gid_t group,
              bool keep_dir_modes);

/* fstype.c */
DLL_IMP_EXP bool fstype(const char *fname, char *fs, int fslen);
DLL_IMP_EXP bool fstype_equals(const char *fname, const char *fstypename);

/* drivetype.c */
DLL_IMP_EXP bool drivetype(const char *fname, char *fs, int fslen);

/* shadowing.c */
DLL_IMP_EXP void check_include_list_shadowing(JCR *jcr, findFILESET *fileset);

#if defined(HAVE_WIN32)
/* win32.c */
DLL_IMP_EXP bool win32_onefs_is_disabled(findFILESET *fileset);
DLL_IMP_EXP int get_win32_driveletters(findFILESET *fileset, char *szDrives);
DLL_IMP_EXP int get_win32_virtualmountpoints(findFILESET *fileset, dlist **szVmps);
DLL_IMP_EXP bool expand_win32_fileset(findFILESET *fileset);
DLL_IMP_EXP bool exclude_win32_not_to_backup_registry_entries(JCR *jcr, FF_PKT *ff);
DLL_IMP_EXP int win32_send_to_copy_thread(JCR *jcr, BFILE *bfd, char *data, const int32_t length);
DLL_IMP_EXP void win32_flush_copy_thread(JCR *jcr);
DLL_IMP_EXP void win32_cleanup_copy_thread(JCR *jcr);
#endif

/* xattr.c */
DLL_IMP_EXP bxattr_exit_code send_xattr_stream(JCR *jcr, xattr_data_t *xattr_data, int stream);
DLL_IMP_EXP void xattr_drop_internal_table(alist *xattr_value_list);
DLL_IMP_EXP uint32_t serialize_xattr_stream(JCR *jcr, xattr_data_t *xattr_data,
                                uint32_t expected_serialize_len, alist *xattr_value_list);
DLL_IMP_EXP bxattr_exit_code unserialize_xattr_stream(JCR *jcr, xattr_data_t *xattr_data, char *content,
                                          uint32_t content_length, alist *xattr_value_list);
DLL_IMP_EXP bxattr_exit_code build_xattr_streams(JCR *jcr, struct xattr_data_t *xattr_data, FF_PKT *ff_pkt);
DLL_IMP_EXP bxattr_exit_code parse_xattr_streams(JCR *jcr, struct xattr_data_t *xattr_data,
                                     int stream, char *content, uint32_t content_length);

/* bfile.c -- see bfile.h */
