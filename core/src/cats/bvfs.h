/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Planets Communications B.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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

#ifndef __BVFS_H_
#define __BVFS_H_ 1

/*
 * This object can be use to browse the catalog
 *
 * Bvfs fs;
 * fs.set_jobid(10);
 * fs.update_cache();
 * fs.ch_dir("/");
 * fs.ls_dirs();
 * fs.ls_files();
 */

/* Helper for result handler */
typedef enum {
   BVFS_FILE_RECORD  = 'F',
   BVFS_DIR_RECORD   = 'D',
   BVFS_FILE_VERSION = 'V'
} bvfs_handler_type;

typedef enum {
   BVFS_Type    = 0,            /**< Could be D, F, V */
   BVFS_PathId  = 1,

   BVFS_Name    = 2,
   BVFS_JobId   = 3,

   BVFS_LStat   = 4,            /**< Can be empty for missing directories */
   BVFS_FileId  = 5,            /**< Can be empty for missing directories */

   /* Only if File Version record */
   BVFS_Md5     = 6,
   BVFS_VolName = 7,
   BVFS_VolInchanger = 8
} bvfs_row_index;

class Bvfs {

public:
   Bvfs(JobControlRecord *j, BareosDb *mdb);
   virtual ~Bvfs();

   void set_jobid(JobId_t id);
   void set_jobids(char *ids);

   void set_limit(uint32_t max) {
      limit = max;
   }

   void set_offset(uint32_t nb) {
      offset = nb;
   }

   void set_pattern(char *p) {
      uint32_t len = strlen(p);
      pattern = check_pool_memory_size(pattern, len * 2 + 1);
      db->escape_string(jcr, pattern, p, len);
   }

   /* Get the root point */
   DBId_t get_root();

   /*
    * It's much better to access Path though their PathId, it
    * avoids mistakes with string encoding
    */
   void ch_dir(DBId_t pathid) {
      reset_offset();
      pwd_id = pathid;
   }

   /*
    * Returns true if the directory exists
    */
   bool ch_dir(const char *path);

   bool ls_files();             /* Returns true if we have more files to read */
   bool ls_dirs();              /* Returns true if we have more dir to read */
   void get_all_file_versions(const char *path, const char *fname, const char *client);
   void get_all_file_versions(DBId_t pathid, const char *fname, const char *client);

   void update_cache();

   void set_see_all_versions(bool val) {
      see_all_versions = val;
   }

   void set_see_copies(bool val) {
      see_copies = val;
   }

   void set_handler(DB_RESULT_HANDLER *h, void *ctx) {
      list_entries = h;
      user_data = ctx;
   }

   DBId_t get_pwd() {
      return pwd_id;
   }

   Attributes *get_attr() {
      return attr;
   }

   JobControlRecord *get_jcr() {
      return jcr;
   }

   void reset_offset() {
      offset=0;
   }

   void next_offset() {
      offset+=limit;
   }

   /* Clear all cache */
   void clear_cache();

   /* Compute restore list */
   bool compute_restore_list(char *fileid, char *dirid, char *hardlink,
                             char *output_table);

   /* Drop previous restore list */
   bool drop_restore_list(char *output_table);

   /* for internal use */
   int _handle_path(void *, int, char **);

private:
   Bvfs(const Bvfs &);               /* prohibit pass by value */
   Bvfs & operator = (const Bvfs &); /* prohibit class assignment */

   JobControlRecord *jcr;
   BareosDb *db;
   POOLMEM *jobids;
   uint32_t limit;
   uint32_t offset;
   uint32_t nb_record;          /* number of records of the last query */
   POOLMEM *pattern;
   DBId_t pwd_id;               /* Current pathid */
   POOLMEM *prev_dir; /* ls_dirs query returns all versions, take the 1st one */
   Attributes *attr;        /* Can be use by handler to call decode_stat() */

   bool see_all_versions;
   bool see_copies;

   DB_RESULT_HANDLER *list_entries;
   void *user_data;
};

#define bvfs_is_dir(row) ((row)[BVFS_Type][0] == BVFS_DIR_RECORD)
#define bvfs_is_file(row) ((row)[BVFS_Type][0] == BVFS_FILE_RECORD)
#define bvfs_is_version(row) ((row)[BVFS_Type][0] == BVFS_FILE_VERSION)

char *bvfs_parent_dir(char *path);

/*
 * Return the basename of the with the trailing / (update the given string)
 * TODO: see in the rest of bareos if we don't have this function already
 */
char *bvfs_basename_dir(char *path);

#endif /* __BVFS_H_ */
