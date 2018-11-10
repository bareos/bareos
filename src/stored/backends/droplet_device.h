/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2017 Planets Communications B.V.
   Copyright (C) 2014-2018 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
 * Object Storage API device abstraction.
 *
 * Marco van Wieringen, February 2014
 */

#ifndef OBJECTSTORAGE_DEVICE_H
#define OBJECTSTORAGE_DEVICE_H

#include <droplet.h>
#include <droplet/vfs.h>

/*
 * Generic callback for the droplet_device::walk_directory() function.
 *
 * Returns DPL_SUCCESS - success
 *         other dpl_status_t value: failure
 */
typedef dpl_status_t (*t_dpl_walk_directory_call_back)(dpl_dirent_t *dirent, dpl_ctx_t *ctx,
                                                       const char *dirname, void *data);
typedef dpl_status_t (*t_dpl_walk_chunks_call_back)(dpl_sysmd_t *sysmd, dpl_ctx_t *ctx, const char *chunkpath, void *data);




class droplet_device: public chunked_device {
private:
   /*
    * Private Members
    */
   /* maximun number of chunks in a volume (0000 to 9999) */
   const int m_max_chunks = 10000;
   char *m_configstring;
   const char *m_profile;
   const char *m_location;
   const char *m_canned_acl;
   const char *m_storage_class;
   const char *m_bucketname;
   dpl_ctx_t *m_ctx;
   dpl_sysmd_t m_sysmd;

   /*
    * Private Methods
    */
   bool initialize();
   dpl_status_t check_path(const char *path);

   /*
    * Interface from chunked_device
    */
   bool check_remote();
   bool remote_chunked_volume_exists();
   bool flush_remote_chunk(chunk_io_request *request);
   bool read_remote_chunk(chunk_io_request *request);
   ssize_t chunked_remote_volume_size();
   bool truncate_remote_chunked_volume(DCR *dcr);

   bool walk_directory(const char *dirname, t_dpl_walk_directory_call_back callback, void *data);
   bool walk_chunks(const char *dirname, t_dpl_walk_chunks_call_back callback, void *data, bool ignore_gaps = false);

public:
   /*
    * Public Methods
    */
   droplet_device();
   ~droplet_device();

   /*
    * Interface from DEVICE
    */
   int d_close(int fd);
   int d_open(const char *pathname, int flags, int mode);
   int d_ioctl(int fd, ioctl_req_t request, char *mt = NULL);
   boffset_t d_lseek(DCR *dcr, boffset_t offset, int whence);
   ssize_t d_read(int fd, void *buffer, size_t count);
   ssize_t d_write(int fd, const void *buffer, size_t count);
   bool d_truncate(DCR *dcr);
   bool d_flush(DCR *dcr);
};
#endif /* OBJECTSTORE_DEVICE_H */
