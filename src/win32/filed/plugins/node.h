/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2008-2008 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is 
   listed in the file LICENSE.

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
 *  Written by James Harper, October 2008
 */

#define NODE_TYPE_UNKNOWN          0
#define NODE_TYPE_ROOT        1
#define NODE_TYPE_SERVICE          2
#define NODE_TYPE_STORAGE_GROUP    3
#define NODE_TYPE_STORE       4
#define NODE_TYPE_DATABASE_INFO    5
#define NODE_TYPE_FILE        6

class node_t {
public:
   int type;
   int state;
   node_t *parent;
   char *name;
   char *full_path;
   size_t size;
   int level;

   node_t(char *name, int type);
   node_t(char *name, int type, node_t *parent_node);
   virtual ~node_t();

   char *make_full_path();

   virtual bRC startBackupFile(exchange_fd_context_t *context, struct save_pkt *sp) = 0;
   virtual bRC endBackupFile(exchange_fd_context_t *context) = 0;

   virtual bRC createFile(exchange_fd_context_t *context, struct restore_pkt *rp) = 0;
   virtual bRC endRestoreFile(exchange_fd_context_t *context) = 0;

   virtual bRC pluginIoOpen(exchange_fd_context_t *context, struct io_pkt *io);
   virtual bRC pluginIoRead(exchange_fd_context_t *context, struct io_pkt *io);
   virtual bRC pluginIoWrite(exchange_fd_context_t *context, struct io_pkt *io);
   virtual bRC pluginIoClose(exchange_fd_context_t *context, struct io_pkt *io);
};

class file_node_t : public node_t {
public:
   WCHAR *filename;
   HCCX hccx;
   VOID *backup_file_handle;
   VOID *restore_file_handle;
   uint64_t section_size;
   bool restore_at_file_level;

   file_node_t(char *name, node_t *parent_node);
   virtual ~file_node_t();
   bRC startBackupFile(exchange_fd_context_t *context, struct save_pkt *sp);
   bRC endBackupFile(exchange_fd_context_t *context);

   bRC createFile(exchange_fd_context_t *context, struct restore_pkt *rp);
   bRC endRestoreFile(exchange_fd_context_t *context);

   bRC pluginIoOpen(exchange_fd_context_t *context, struct io_pkt *io);
   bRC pluginIoRead(exchange_fd_context_t *context, struct io_pkt *io);
   bRC pluginIoWrite(exchange_fd_context_t *context, struct io_pkt *io);
   bRC pluginIoClose(exchange_fd_context_t *context, struct io_pkt *io);
};

class dbi_node_t : public node_t {
public:
   DATABASE_BACKUP_INFO *dbi;
   char *buffer;
   uint32_t buffer_size;
   uint32_t buffer_pos;
   WCHAR *restore_display_name;
   GUID restore_guid;
   WCHAR *restore_input_streams;
   WCHAR *restore_output_streams;

   dbi_node_t(char *name, node_t *parent_node);
   virtual ~dbi_node_t();
   bRC startBackupFile(exchange_fd_context_t *context, struct save_pkt *sp);
   bRC endBackupFile(exchange_fd_context_t *context);

   bRC createFile(exchange_fd_context_t *context, struct restore_pkt *rp);
   bRC endRestoreFile(exchange_fd_context_t *context);

   bRC pluginIoOpen(exchange_fd_context_t *context, struct io_pkt *io);
   bRC pluginIoRead(exchange_fd_context_t *context, struct io_pkt *io);
   bRC pluginIoWrite(exchange_fd_context_t *context, struct io_pkt *io);
   bRC pluginIoClose(exchange_fd_context_t *context, struct io_pkt *io);
};

class store_node_t : public node_t {
public:
   HCCX hccx;
   DATABASE_BACKUP_INFO *dbi;
   WCHAR *stream_ptr;
   file_node_t *file_node;
   dbi_node_t *dbi_node;
   WCHAR *out_stream_ptr;

   store_node_t(char *name, node_t *parent_node);
   virtual ~store_node_t();
   bRC startBackupFile(exchange_fd_context_t *context, struct save_pkt *sp);
   bRC endBackupFile(exchange_fd_context_t *context);

   bRC createFile(exchange_fd_context_t *context, struct restore_pkt *rp);
   bRC endRestoreFile(exchange_fd_context_t *context);
};

class storage_group_node_t : public node_t {
public:
   HCCX hccx;
   INSTANCE_BACKUP_INFO *ibi;
   store_node_t *store_node;
   file_node_t *file_node;
   uint32_t current_dbi;
   WCHAR *logfiles;
   WCHAR *logfile_ptr;
   RESTORE_ENVIRONMENT *restore_environment;
   WCHAR *service_name;
   WCHAR *storage_group_name;
   WCHAR *saved_log_path;
   storage_group_node_t *next;

   storage_group_node_t(char *name, node_t *parent_node);
   virtual ~storage_group_node_t();
   bRC startBackupFile(exchange_fd_context_t *context, struct save_pkt *sp);
   bRC endBackupFile(exchange_fd_context_t *context);

   bRC createFile(exchange_fd_context_t *context, struct restore_pkt *rp);
   bRC endRestoreFile(exchange_fd_context_t *context);
};

class service_node_t : public node_t {
public:
   uint32_t ibi_count;
   INSTANCE_BACKUP_INFO *ibi;
   HCCX hccx;
   uint32_t current_ibi;
   storage_group_node_t *first_storage_group_node;

   service_node_t(char *name, node_t *parent_node);
   virtual ~service_node_t();
   bRC startBackupFile(exchange_fd_context_t *context, struct save_pkt *sp);
   bRC endBackupFile(exchange_fd_context_t *context);

   bRC createFile(exchange_fd_context_t *context, struct restore_pkt *rp);
   bRC endRestoreFile(exchange_fd_context_t *context);
};

class root_node_t : public node_t {
public:
   service_node_t *service_node;

   root_node_t(char *name);
   virtual ~root_node_t();
   bRC startBackupFile(exchange_fd_context_t *context, struct save_pkt *sp);
   bRC endBackupFile(exchange_fd_context_t *context);

   bRC createFile(exchange_fd_context_t *context, struct restore_pkt *rp);
   bRC endRestoreFile(exchange_fd_context_t *context);
};
