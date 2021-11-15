/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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

#ifndef BAREOS_FINDLIB_XATTR_H_
#define BAREOS_FINDLIB_XATTR_H_

// Return codes from xattr subroutines.
enum class BxattrExitCode
{
  kErrorFatal,
  kError,
  kWarning,
  kSuccess
};

#if defined(HAVE_LINUX_OS)
#  define BXATTR_ENOTSUP EOPNOTSUPP
#elif defined(HAVE_DARWIN_OS)
#  define BXATTR_ENOTSUP ENOTSUP
#elif defined(HAVE_HURD_OS)
#  define BXATTR_ENOTSUP ENOTSUP
#endif

/*
 * Magic used in the magic field of the xattr struct.
 * This way we can see we encounter a valid xattr struct.
 */
#define XATTR_MAGIC 0x5C5884

// Internal representation of an extended attribute.
struct xattr_t {
  uint32_t magic;
  uint32_t name_length;
  char* name;
  uint32_t value_length;
  char* value;
};

// Internal representation of an extended attribute hardlinked file.
struct xattr_link_cache_entry_t {
  uint32_t inum;
  char* target;
};

#define BXATTR_FLAG_SAVE_NATIVE 0x01
#define BXATTR_FLAG_RESTORE_NATIVE 0x02

struct xattr_build_data_t {
  uint32_t nr_errors;
  uint32_t nr_saved;
  POOLMEM* content;
  uint32_t content_length;
  alist<xattr_link_cache_entry_t*>* link_cache;
};

struct xattr_parse_data_t {
  uint32_t nr_errors;
};

// Internal tracking data.
struct XattrData {
  POOLMEM* last_fname;
  uint32_t flags{0}; /* See BXATTR_FLAG_* */
  uint32_t current_dev{0};
  bool first_dev{true};
  union {
    struct xattr_build_data_t* build;
    struct xattr_parse_data_t* parse;
  } u;
};

// Maximum size of the XATTR stream this prevents us from blowing up the filed.
#define MAX_XATTR_STREAM (1 * 1024 * 1024) /* 1 Mb */

// Upperlimit on a xattr internal buffer
#define XATTR_BUFSIZ 1024
template <typename T> class alist;

BxattrExitCode SendXattrStream(JobControlRecord* jcr,
                               XattrData* xattr_data,
                               int stream);
void XattrDropInternalTable(alist<xattr_t*>* xattr_value_list);
uint32_t SerializeXattrStream(JobControlRecord* jcr,
                              XattrData* xattr_data,
                              uint32_t expected_serialize_len,
                              alist<xattr_t*>* xattr_value_list);
BxattrExitCode UnSerializeXattrStream(JobControlRecord* jcr,
                                      XattrData* xattr_data,
                                      char* content,
                                      uint32_t content_length,
                                      alist<xattr_t*>* xattr_value_list);
BxattrExitCode BuildXattrStreams(JobControlRecord* jcr,
                                 struct XattrData* xattr_data,
                                 FindFilesPacket* ff_pkt);
BxattrExitCode ParseXattrStreams(JobControlRecord* jcr,
                                 struct XattrData* xattr_data,
                                 int stream,
                                 char* content,
                                 uint32_t content_length);


#endif  // BAREOS_FINDLIB_XATTR_H_
