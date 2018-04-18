/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2009 Free Software Foundation Europe e.V.
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

#ifndef __XATTR_H
#define __XATTR_H

/*
 * Number of xattr errors to report per job.
 */
#define XATTR_REPORT_ERR_MAX_PER_JOB    25

/*
 * Return codes from xattr subroutines.
 */
typedef enum {
   bxattr_exit_fatal = -1,
   bxattr_exit_error = 0,
   bxattr_exit_ok = 1
} bxattr_exit_code;

#if defined(HAVE_LINUX_OS)
#define BXATTR_ENOTSUP EOPNOTSUPP
#elif defined(HAVE_DARWIN_OS)
#define BXATTR_ENOTSUP ENOTSUP
#elif defined(HAVE_HURD_OS)
#define BXATTR_ENOTSUP ENOTSUP
#endif

/*
 * Magic used in the magic field of the xattr struct.
 * This way we can see we encounter a valid xattr struct.
 */
#define XATTR_MAGIC 0x5C5884

/*
 * Internal representation of an extended attribute.
 */
struct xattr_t {
   uint32_t magic;
   uint32_t name_length;
   char *name;
   uint32_t value_length;
   char *value;
};

/*
 * Internal representation of an extended attribute hardlinked file.
 */
struct xattr_link_cache_entry_t {
   uint32_t inum;
   char *target;
};

#define BXATTR_FLAG_SAVE_NATIVE    0x01
#define BXATTR_FLAG_RESTORE_NATIVE 0x02

struct xattr_build_data_t {
   uint32_t nr_errors;
   uint32_t nr_saved;
   POOLMEM *content;
   uint32_t content_length;
   alist *link_cache;
};

struct xattr_parse_data_t {
   uint32_t nr_errors;
};

/*
 * Internal tracking data.
 */
struct xattr_data_t {
   POOLMEM *last_fname;
   uint32_t flags;              /* See BXATTR_FLAG_* */
   uint32_t current_dev;
   union {
      struct xattr_build_data_t *build;
      struct xattr_parse_data_t *parse;
   } u;
};

/*
 * Maximum size of the XATTR stream this prevents us from blowing up the filed.
 */
#define MAX_XATTR_STREAM  (1 * 1024 * 1024) /* 1 Mb */

/*
 * Upperlimit on a xattr internal buffer
 */
#define XATTR_BUFSIZ	1024

DLL_IMP_EXP bxattr_exit_code send_xattr_stream(JobControlRecord *jcr, xattr_data_t *xattr_data, int stream);
DLL_IMP_EXP void xattr_drop_internal_table(alist *xattr_value_list);
DLL_IMP_EXP uint32_t serialize_xattr_stream(JobControlRecord *jcr, xattr_data_t *xattr_data,
                                uint32_t expected_serialize_len, alist *xattr_value_list);
DLL_IMP_EXP bxattr_exit_code unserialize_xattr_stream(JobControlRecord *jcr, xattr_data_t *xattr_data, char *content,
                                          uint32_t content_length, alist *xattr_value_list);
DLL_IMP_EXP bxattr_exit_code build_xattr_streams(JobControlRecord *jcr, struct xattr_data_t *xattr_data, FindFilesPacket *ff_pkt);
DLL_IMP_EXP bxattr_exit_code parse_xattr_streams(JobControlRecord *jcr, struct xattr_data_t *xattr_data,
                                     int stream, char *content, uint32_t content_length);


#endif
