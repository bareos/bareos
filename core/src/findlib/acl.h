/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
 * Properties we use for getting and setting ACLs.
 */

#ifndef BAREOS_FINDLIB_ACL_H_
#define BAREOS_FINDLIB_ACL_H_

#include "include/config.h"
#include <cstdint>
#include "include/bc_types.h"
#include "lib/mem_pool.h"

#if __has_include(<sys/acl.h>)
#  include <sys/acl.h>
/**
 * This value is used as ostype when we encounter an invalid acl type.
 * The way the code is build this should never happen.
 */
static inline constexpr acl_type_t ACL_TYPE_NONE = static_cast<acl_type_t>(0);
#endif

class JobControlRecord;
struct FindFilesPacket;

// Number of acl errors to report per job.
static inline constexpr uint32_t ACL_REPORT_ERR_MAX_PER_JOB = 25;


// Return codes from acl subroutines.
typedef enum
{
  bacl_exit_fatal = -1,
  bacl_exit_error = 0,
  bacl_exit_ok = 1
} bacl_exit_code;

/* For shorter ACL strings when possible, define BACL_WANT_SHORT_ACLS */
/* #define BACL_WANT_SHORT_ACLS */

/* For numeric user/group ids when possible, define BACL_WANT_NUMERIC_IDS */
/* #define BACL_WANT_NUMERIC_IDS */

// We support the following types of ACLs
typedef enum
{
  BACL_TYPE_NONE = 0,
  BACL_TYPE_ACCESS = 1,
  BACL_TYPE_DEFAULT = 2,
  BACL_TYPE_DEFAULT_DIR = 3,
  BACL_TYPE_EXTENDED = 4,
  BACL_TYPE_NFS4 = 5
} bacl_type;


#if defined(HAVE_FREEBSD_OS) || defined(HAVE_DARWIN_OS) \
    || defined(HAVE_LINUX_OS)
#  define BACL_ENOTSUP EOPNOTSUPP
#endif

struct bacl_flags {
  bool SaveNative{false};
  bool SaveAfs{false};
  bool RestoreNative{false};
  bool RestoreAfs{false};
};

// Internal tracking data.
struct AclData {
  int filetype;
  POOLMEM* last_fname;
  bacl_flags flags{};
  uint32_t current_dev{0};
  bool first_dev{true};
  uint32_t nr_errors;
  virtual ~AclData() noexcept = default;
};

struct AclBuildData : public AclData {
  uint32_t content_length;
  PoolMem content{PM_MESSAGE};
};

bacl_exit_code SendAclStream(JobControlRecord* jcr,
                             AclBuildData* acl_data,
                             int stream);
bacl_exit_code BuildAclStreams(JobControlRecord* jcr,
                               AclBuildData* acl_data,
                               FindFilesPacket* ff_pkt);
bacl_exit_code parse_acl_streams(JobControlRecord* jcr,
                                 AclData* acl_data,
                                 int stream,
                                 char* content,
                                 uint32_t content_length);

#endif  // BAREOS_FINDLIB_ACL_H_
