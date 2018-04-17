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
DLL_IMP_EXP bacl_exit_code send_acl_stream(JobControlRecord *jcr, acl_data_t *acl_data, int stream);
DLL_IMP_EXP bacl_exit_code build_acl_streams(JobControlRecord *jcr, acl_data_t *acl_data, FindFilesPacket *ff_pkt);
DLL_IMP_EXP bacl_exit_code parse_acl_streams(JobControlRecord *jcr, acl_data_t *acl_data,
                                 int stream, char *content, uint32_t content_length);

/* attribs.c */
DLL_IMP_EXP int encode_attribsEx(JobControlRecord *jcr, char *attribsEx, FindFilesPacket *ff_pkt);
DLL_IMP_EXP bool set_attributes(JobControlRecord *jcr, Attributes *attr, BareosWinFilePacket *ofd);
DLL_IMP_EXP int select_data_stream(FindFilesPacket *ff_pkt, bool compatible);

/* create_file.c */
DLL_IMP_EXP int create_file(JobControlRecord *jcr, Attributes *attr, BareosWinFilePacket *ofd, int replace);

/* find.c */
DLL_IMP_EXP FindFilesPacket *init_find_files();
DLL_IMP_EXP void set_find_options(FindFilesPacket *ff, bool incremental, time_t mtime);
DLL_IMP_EXP void set_find_changed_function(FindFilesPacket *ff, bool check_fct(JobControlRecord *jcr, FindFilesPacket *ff));
DLL_IMP_EXP int find_files(JobControlRecord *jcr, FindFilesPacket *ff, int file_sub(JobControlRecord *, FindFilesPacket *ff_pkt, bool),
               int plugin_sub(JobControlRecord *, FindFilesPacket *ff_pkt, bool));
DLL_IMP_EXP bool match_files(JobControlRecord *jcr, FindFilesPacket *ff, int sub(JobControlRecord *, FindFilesPacket *ff_pkt, bool));
DLL_IMP_EXP int term_find_files(FindFilesPacket *ff);
DLL_IMP_EXP bool is_in_fileset(FindFilesPacket *ff);
DLL_IMP_EXP bool accept_file(FindFilesPacket *ff);
DLL_IMP_EXP findIncludeExcludeItem *allocate_new_incexe(void);
DLL_IMP_EXP findIncludeExcludeItem *new_exclude(findFILESET *fileset);
DLL_IMP_EXP findIncludeExcludeItem *new_include(findFILESET *fileset);
DLL_IMP_EXP findIncludeExcludeItem *new_preinclude(findFILESET *fileset);
DLL_IMP_EXP findIncludeExcludeItem *new_preexclude(findFILESET *fileset);
DLL_IMP_EXP findFOPTS *start_options(FindFilesPacket *ff);
DLL_IMP_EXP void new_options(FindFilesPacket *ff, findIncludeExcludeItem *incexe);

/* match.c */
DLL_IMP_EXP void init_include_exclude_files(FindFilesPacket *ff);
DLL_IMP_EXP void term_include_exclude_files(FindFilesPacket *ff);
DLL_IMP_EXP void add_fname_to_include_list(FindFilesPacket *ff, int prefixed, const char *fname);
DLL_IMP_EXP void add_fname_to_exclude_list(FindFilesPacket *ff, const char *fname);
DLL_IMP_EXP bool file_is_excluded(FindFilesPacket *ff, const char *file);
DLL_IMP_EXP bool file_is_included(FindFilesPacket *ff, const char *file);
DLL_IMP_EXP struct s_included_file *get_next_included_file(FindFilesPacket *ff,
                                               struct s_included_file *inc);
DLL_IMP_EXP bool parse_size_match(const char *size_match_pattern,
                      struct s_sz_matching *size_matching);

/* find_one.c */
DLL_IMP_EXP int find_one_file(JobControlRecord *jcr, FindFilesPacket *ff,
                  int handle_file(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool top_level),
                  char *p, dev_t parent_device, bool top_level);
DLL_IMP_EXP int term_find_one(FindFilesPacket *ff);
DLL_IMP_EXP bool has_file_changed(JobControlRecord *jcr, FindFilesPacket *ff_pkt);
DLL_IMP_EXP bool check_changes(JobControlRecord *jcr, FindFilesPacket *ff_pkt);

/* get_priv.c */
DLL_IMP_EXP int enable_backup_privileges(JobControlRecord *jcr, int ignore_errors);

/* hardlink.c */
DLL_IMP_EXP CurLink *lookup_hardlink(JobControlRecord *jcr, FindFilesPacket *ff_pkt, ino_t ino, dev_t dev);
DLL_IMP_EXP CurLink *new_hardlink(JobControlRecord *jcr, FindFilesPacket *ff_pkt, char *fname, ino_t ino, dev_t dev);
DLL_IMP_EXP void ff_pkt_set_link_digest(FindFilesPacket *ff_pkt,
                            int32_t digest_stream,
                            const char *digest,
                            uint32_t len);

/* makepath.c */
DLL_IMP_EXP bool makepath(Attributes *attr, const char *path, mode_t mode,
              mode_t parent_mode, uid_t owner, gid_t group,
              bool keep_dir_modes);

/* fstype.c */
DLL_IMP_EXP bool fstype(const char *fname, char *fs, int fslen);
DLL_IMP_EXP bool fstype_equals(const char *fname, const char *fstypename);

/* drivetype.c */
DLL_IMP_EXP bool drivetype(const char *fname, char *fs, int fslen);

/* shadowing.c */
DLL_IMP_EXP void check_include_list_shadowing(JobControlRecord *jcr, findFILESET *fileset);

#if defined(HAVE_WIN32)
/* win32.c */
DLL_IMP_EXP bool win32_onefs_is_disabled(findFILESET *fileset);
DLL_IMP_EXP int get_win32_driveletters(findFILESET *fileset, char *szDrives);
DLL_IMP_EXP int get_win32_virtualmountpoints(findFILESET *fileset, dlist **szVmps);
DLL_IMP_EXP bool expand_win32_fileset(findFILESET *fileset);
DLL_IMP_EXP bool exclude_win32_not_to_backup_registry_entries(JobControlRecord *jcr, FindFilesPacket *ff);
DLL_IMP_EXP int win32_send_to_copy_thread(JobControlRecord *jcr, BareosWinFilePacket *bfd, char *data, const int32_t length);
DLL_IMP_EXP void win32_flush_copy_thread(JobControlRecord *jcr);
DLL_IMP_EXP void win32_cleanup_copy_thread(JobControlRecord *jcr);
#endif

/* xattr.c */
DLL_IMP_EXP bxattr_exit_code send_xattr_stream(JobControlRecord *jcr, xattr_data_t *xattr_data, int stream);
DLL_IMP_EXP void xattr_drop_internal_table(alist *xattr_value_list);
DLL_IMP_EXP uint32_t serialize_xattr_stream(JobControlRecord *jcr, xattr_data_t *xattr_data,
                                uint32_t expected_serialize_len, alist *xattr_value_list);
DLL_IMP_EXP bxattr_exit_code unserialize_xattr_stream(JobControlRecord *jcr, xattr_data_t *xattr_data, char *content,
                                          uint32_t content_length, alist *xattr_value_list);
DLL_IMP_EXP bxattr_exit_code build_xattr_streams(JobControlRecord *jcr, struct xattr_data_t *xattr_data, FindFilesPacket *ff_pkt);
DLL_IMP_EXP bxattr_exit_code parse_xattr_streams(JobControlRecord *jcr, struct xattr_data_t *xattr_data,
                                     int stream, char *content, uint32_t content_length);

/* bfile.c -- see bfile.h */
