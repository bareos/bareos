/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.

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

extern const char CATS_IMP_EXP *get_restore_objects;
extern const char CATS_IMP_EXP *fill_jobhisto;
extern const char CATS_IMP_EXP *client_backups;
extern const char CATS_IMP_EXP *list_pool;
extern const char CATS_IMP_EXP *drop_deltabs[];
extern const char CATS_IMP_EXP *create_delindex;
extern const char CATS_IMP_EXP *cnt_File;
extern const char CATS_IMP_EXP *del_MAC;
extern const char CATS_IMP_EXP *sel_JobMedia;

extern const char CATS_IMP_EXP *cleanup_created_job;
extern const char CATS_IMP_EXP *cleanup_running_job;
extern const char CATS_IMP_EXP *uar_list_jobs;
extern const char CATS_IMP_EXP *uar_print_jobs;
extern const char CATS_IMP_EXP *uar_count_files;
extern const char CATS_IMP_EXP *uar_sel_files;
extern const char CATS_IMP_EXP *uar_del_temp;
extern const char CATS_IMP_EXP *uar_del_temp1;
extern const char CATS_IMP_EXP *uar_last_full;
extern const char CATS_IMP_EXP *uar_full;
extern const char CATS_IMP_EXP *uar_inc;
extern const char CATS_IMP_EXP *uar_list_temp;
extern const char CATS_IMP_EXP *uar_sel_all_temp1;
extern const char CATS_IMP_EXP *uar_sel_fileset;
extern const char CATS_IMP_EXP *uar_sel_filesetid;
extern const char CATS_IMP_EXP *uar_mediatype;
extern const char CATS_IMP_EXP *uar_jobid_fileindex;
extern const char CATS_IMP_EXP *uar_dif;
extern const char CATS_IMP_EXP *uar_sel_all_temp;
extern const char CATS_IMP_EXP *uar_count_files;
extern const char CATS_IMP_EXP *uar_jobids_fileindex;
extern const char CATS_IMP_EXP *uar_jobid_fileindex_from_table;
extern const char CATS_IMP_EXP *uar_sel_jobid_temp;

extern const char CATS_IMP_EXP *select_recent_version[];
extern const char CATS_IMP_EXP *select_recent_version_with_basejob[];
extern const char CATS_IMP_EXP *select_recent_version_with_basejob_and_delta[];
extern const char CATS_IMP_EXP *create_temp_accurate_jobids[];
extern const char CATS_IMP_EXP *create_temp_basefile[];
extern const char CATS_IMP_EXP *create_temp_new_basefile[];
extern const char CATS_IMP_EXP *create_deltabs[];
extern const char CATS_IMP_EXP *uap_upgrade_copies_oldest_job[];

extern const char CATS_IMP_EXP *uar_file[];
extern const char CATS_IMP_EXP *uar_create_temp[];
extern const char CATS_IMP_EXP *uar_create_temp1[];
extern const char CATS_IMP_EXP *uar_jobid_fileindex_from_dir[];
extern const char CATS_IMP_EXP *sql_media_order_most_recently_written[];
extern const char CATS_IMP_EXP *sql_get_max_connections[];
extern const char CATS_IMP_EXP *sql_bvfs_select[];
extern const char CATS_IMP_EXP *sql_bvfs_list_files[];

extern const char CATS_IMP_EXP *batch_lock_path_query[];
extern const char CATS_IMP_EXP *batch_lock_filename_query[];
extern const char CATS_IMP_EXP *batch_unlock_tables_query[];
extern const char CATS_IMP_EXP *batch_fill_path_query[];
extern const char CATS_IMP_EXP *batch_fill_filename_query[];
extern const char CATS_IMP_EXP *match_query[];
extern const char CATS_IMP_EXP *insert_counter_values[];
extern const char CATS_IMP_EXP *select_counter_values[];
extern const char CATS_IMP_EXP *update_counter_values[];
