/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

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
 * Prototypes for finlib directory of Bacula
 *
 */

/* from attribs.c */
void    encode_stat       (char *buf, struct stat *statp, int stat_size, int32_t LinkFI, int data_stream);
int     decode_stat       (char *buf, struct stat *statp, int stat_size, int32_t *LinkFI);
int32_t decode_LinkFI     (char *buf, struct stat *statp, int stat_size);
int     encode_attribsEx  (JCR *jcr, char *attribsEx, FF_PKT *ff_pkt);
bool    set_attributes    (JCR *jcr, ATTR *attr, BFILE *ofd);
int     select_data_stream(FF_PKT *ff_pkt);

/* from create_file.c */
int    create_file       (JCR *jcr, ATTR *attr, BFILE *ofd, int replace);

/* From find.c */
FF_PKT *init_find_files();
void  set_find_options(FF_PKT *ff, int incremental, time_t mtime);
void set_find_changed_function(FF_PKT *ff, bool check_fct(JCR *jcr, FF_PKT *ff));
int   find_files(JCR *jcr, FF_PKT *ff, int file_sub(JCR *, FF_PKT *ff_pkt, bool),
                 int plugin_sub(JCR *, FF_PKT *ff_pkt, bool));
int   match_files(JCR *jcr, FF_PKT *ff, int sub(JCR *, FF_PKT *ff_pkt, bool));
int   term_find_files(FF_PKT *ff);
int   get_win32_driveletters(FF_PKT *ff, char* szDrives);
bool  is_in_fileset(FF_PKT *ff);
bool accept_file(FF_PKT *ff);

/* From match.c */
void  init_include_exclude_files(FF_PKT *ff);
void  term_include_exclude_files(FF_PKT *ff);
void  add_fname_to_include_list(FF_PKT *ff, int prefixed, const char *fname);
void  add_fname_to_exclude_list(FF_PKT *ff, const char *fname);
int   file_is_excluded(FF_PKT *ff, const char *file);
int   file_is_included(FF_PKT *ff, const char *file);
struct s_included_file *get_next_included_file(FF_PKT *ff,
                           struct s_included_file *inc);

/* From find_one.c */
int   find_one_file(JCR *jcr, FF_PKT *ff, 
               int handle_file(JCR *jcr, FF_PKT *ff_pkt, bool top_level),
               char *p, dev_t parent_device, bool top_level);
int   term_find_one(FF_PKT *ff);
bool  has_file_changed(JCR *jcr, FF_PKT *ff_pkt);
bool check_changes(JCR *jcr, FF_PKT *ff_pkt);
void ff_pkt_set_link_digest(FF_PKT *ff_pkt, 
                            int32_t digest_stream, const char *digest, uint32_t len);

/* From get_priv.c */
int enable_backup_privileges(JCR *jcr, int ignore_errors);


/* from makepath.c */
bool makepath(ATTR *attr, const char *path, mode_t mode,
           mode_t parent_mode, uid_t owner, gid_t group,
           int keep_dir_modes);
void free_path_list(JCR *jcr);
bool path_list_lookup(JCR *jcr, char *fname);
bool path_list_add(JCR *jcr, uint32_t len, char *fname);


/* from fstype.c */
bool fstype(const char *fname, char *fs, int fslen);
bool fstype_equals(const char *fname, const char *fstypename);

/* from drivetype.c */
bool drivetype(const char *fname, char *fs, int fslen);

/* from bfile.c -- see bfile.h */
