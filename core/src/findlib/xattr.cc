/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2008-2012 Free Software Foundation Europe e.V.
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
/*
 * Functions to handle Extended Attributes for bareos.
 *
 * Extended Attributes are so OS specific we only restore Extended Attributes if
 * they were saved using a filed on the same platform.
 *
 * Currently we support the following OSes:
 *   Written by Marco van Wieringen, November 2008
 *   Major overhaul January 2012 + June 2012
 *   Moved into findlib so it can be used from other programs, May 2012
 */
/**
 * @file
 * Functions to handle Extended Attributes for bareos.
 *
 * Extended Attributes are so OS specific we only restore Extended Attributes if
 * they were saved using a filed on the same platform.
 *
 * Currently we support the following OSes:
 *   - AIX (Extended Attributes)
 *   - Darwin (Extended Attributes)
 *   - FreeBSD (Extended Attributes)
 *   - GNU HURD (Extended Attributes)
 *   - IRIX (Extended Attributes)
 *   - Linux (Extended Attributes)
 *   - NetBSD (Extended Attributes)
 *   - OpenBSD (Extended Attributes)
 *     (As it seems either they never implemented xattr or they are removed
 *      the support as it stated it was in version 3.1 but the current syscall
 *      tabled shows the extattr_ functions are not implemented. So as such we
 *      might eventually support xattr on OpenBSD when they implemented them
 * using the same interface as FreeBSD and NetBSD.
 *   - Solaris (Extended Attributes and Extensible Attributes)
 *   - Tru64 (Extended Attributes)
 */

#include "include/bareos.h"
#include "include/streams.h"
#include "find.h"
#include "lib/berrno.h"
#include "lib/bsock.h"
#include "include/jcr.h"
#include "lib/serial.h"

[[maybe_unused]] static void warn_if_disabling_xattrs(JobControlRecord* jcr,
                                                      const char* file)
{
  constexpr const char* err_msg
      = "Disabling restore of XATTRs on this filesystem, not supported. "
        "Current file: \"%s\"\n";
  Mmsg(jcr->errmsg, err_msg, file);
  Dmsg1(100, err_msg, file);
}

#if !defined(HAVE_XATTR)
/**
 * Entry points when compiled without support for XATTRs or on an unsupported
 * platform.
 */
BxattrExitCode BuildXattrStreams(JobControlRecord*,
                                 XattrBuildData*,
                                 FindFilesPacket*)
{
  return BxattrExitCode::kErrorFatal;
}

BxattrExitCode ParseXattrStreams(JobControlRecord*,
                                 XattrData*,
                                 int,
                                 char*,
                                 uint32_t)
{
  return BxattrExitCode::kErrorFatal;
}

BxattrExitCode SendXattrStream(JobControlRecord*, XattrBuildData*, int)
{
  return BxattrExitCode::kErrorFatal;
}
#else
// Send a XATTR stream to the SD.
BxattrExitCode SendXattrStream(JobControlRecord* jcr,
                               XattrBuildData* xattr_data,
                               int stream)
{
  BareosSocket* sd = jcr->store_bsock;
  POOLMEM* msgsave;
#  ifdef FD_NO_SEND_TEST
  return BxattrExitCode::kSuccess;
#  endif

  // Sanity check
  if (xattr_data->content_length <= 0) { return BxattrExitCode::kSuccess; }

  // Send header
  if (!sd->fsend("%" PRIu32 " %" PRId32 " 0", jcr->JobFiles, stream)) {
    Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
          sd->bstrerror());
    return BxattrExitCode::kErrorFatal;
  }

  // Send the buffer to the storage deamon
  Dmsg1(400, "Backing up XATTR <%s>\n", xattr_data->content.c_str());
  msgsave = sd->msg;
  sd->msg = xattr_data->content.c_str();
  sd->message_length = xattr_data->content_length;
  if (!sd->send()) {
    sd->msg = msgsave;
    sd->message_length = 0;
    Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
          sd->bstrerror());
    return BxattrExitCode::kErrorFatal;
  }

  jcr->JobBytes += sd->message_length;
  sd->msg = msgsave;
  if (!sd->signal(BNET_EOD)) {
    Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
          sd->bstrerror());
    return BxattrExitCode::kErrorFatal;
  }
  Dmsg1(200, "XATTR of file: %s successfully backed up!\n",
        xattr_data->last_fname);
  return BxattrExitCode::kSuccess;
}

/**
 * First some generic functions for OSes that use the same xattr encoding
 * scheme. Currently for all OSes except for Solaris.
 */
void XattrDropInternalTable(alist<xattr_t*>* xattr_value_list)
{
  if (!xattr_value_list) { return; }
  // Walk the list of xattrs and free allocated memory on traversing.
  for (auto* current_xattr : xattr_value_list) {
    // See if we can shortcut.
    if (current_xattr == NULL || current_xattr->magic != XATTR_MAGIC) break;

    free(current_xattr->name);

    if (current_xattr->value_length > 0) free(current_xattr->value);

    free(current_xattr);
  }

  delete xattr_value_list;
}

/**
 * The xattr stream for OSX, FreeBSD, Linux and NetBSD is a serialized stream of
 * bytes which encodes one or more xattr_t structures.
 *
 * The Serialized stream consists of the following elements:
 *    magic - A magic string which makes it easy to detect any binary
 * incompatabilites name_length - The length of the following xattr name name -
 * The name of the extended attribute value_length - The length of the following
 * xattr data value - The actual content of the extended attribute
 *
 * This is repeated 1 or more times.
 *
 */
static bool SerializeXattrStream(JobControlRecord*,
                                 XattrBuildData* xattr_data,
                                 uint32_t expected_serialize_len,
                                 alist<xattr_t*>* xattr_value_list)
{
  ser_declare;

  /* Make sure the serialized stream fits in the poolmem buffer.
   * We allocate some more to be sure the stream is gonna fit. */
  xattr_data->content.check_size(expected_serialize_len + 10);
  SerBegin(xattr_data->content.c_str(), expected_serialize_len + 10);

  // Walk the list of xattrs and Serialize the data.
  for (auto* current_xattr : xattr_value_list) {
    // See if we can shortcut.
    if (current_xattr == NULL || current_xattr->magic != XATTR_MAGIC) break;

    ser_uint32(current_xattr->magic);
    ser_uint32(current_xattr->name_length);
    SerBytes(current_xattr->name, current_xattr->name_length);

    ser_uint32(current_xattr->value_length);
    if (current_xattr->value_length > 0 && current_xattr->value) {
      SerBytes(current_xattr->value, current_xattr->value_length);

      Dmsg3(100, "Backup xattr named %.*s, value %.*s\n",
            current_xattr->name_length, current_xattr->name,
            current_xattr->value_length, current_xattr->value);
    } else {
      Dmsg1(100, "Backup empty xattr named %.*s\n", current_xattr->name_length,
            current_xattr->name);
    }
  }

  SerEnd(xattr_data->content.c_str(), expected_serialize_len + 10);
  xattr_data->content_length = SerLength(xattr_data->content.c_str());

  return xattr_data->content_length == expected_serialize_len;
}

BxattrExitCode UnSerializeXattrStream(JobControlRecord* jcr,
                                      XattrData* xattr_data,
                                      char* content,
                                      uint32_t content_length,
                                      alist<xattr_t*>* xattr_value_list)
{
  unser_declare;
  xattr_t* current_xattr;

  /* Parse the stream and call restore_xattr_on_file for each extended
   * attribute.
   *
   * Start unserializing the data. We keep on looping while we have not
   * unserialized all bytes in the stream. */
  UnserBegin(content, content_length);
  while (UnserLength(content) < content_length) {
    /* First make sure the magic is present. This way we can easily catch
     * corruption. Any missing MAGIC is fatal we do NOT try to continue. */
    current_xattr = (xattr_t*)malloc(sizeof(xattr_t));
    unser_uint32(current_xattr->magic);
    if (current_xattr->magic != XATTR_MAGIC) {
      Mmsg1(jcr->errmsg,
            T_("Illegal xattr stream, no XATTR_MAGIC on file \"%s\"\n"),
            xattr_data->last_fname);
      Dmsg1(100, "Illegal xattr stream, no XATTR_MAGIC on file \"%s\"\n",
            xattr_data->last_fname);
      free(current_xattr);
      return BxattrExitCode::kError;
    }

    // Decode the valuepair. First decode the length of the name.
    unser_uint32(current_xattr->name_length);
    if (current_xattr->name_length == 0) {
      Mmsg1(jcr->errmsg,
            T_("Illegal xattr stream, xattr name length <= 0 on file \"%s\"\n"),
            xattr_data->last_fname);
      Dmsg1(100,
            "Illegal xattr stream, xattr name length <= 0 on file \"%s\"\n",
            xattr_data->last_fname);
      free(current_xattr);
      return BxattrExitCode::kError;
    }

    // Allocate room for the name and decode its content.
    current_xattr->name = (char*)malloc(current_xattr->name_length + 1);
    UnserBytes(current_xattr->name, current_xattr->name_length);

    // The xattr_name needs to be null terminated.
    current_xattr->name[current_xattr->name_length] = '\0';

    // Decode the value length.
    unser_uint32(current_xattr->value_length);

    if (current_xattr->value_length > 0) {
      // Allocate room for the value and decode its content.
      current_xattr->value = (char*)malloc(current_xattr->value_length);
      UnserBytes(current_xattr->value, current_xattr->value_length);

      Dmsg3(100, "Restoring xattr named %.*s, value %.*s\n",
            current_xattr->name_length, current_xattr->name,
            current_xattr->value_length, current_xattr->value);
    } else {
      current_xattr->value = NULL;
      Dmsg1(100, "Restoring empty xattr named %.*s\n",
            current_xattr->name_length, current_xattr->name);
    }

    xattr_value_list->append(current_xattr);
  }

  UnserEnd(content, content_length);
  return BxattrExitCode::kSuccess;
}

BxattrExitCode SerializeAndSendXattrStream(JobControlRecord* jcr,
                                           XattrBuildData* xattr_data,
                                           uint32_t expected_serialize_len,
                                           alist<xattr_t*>* xattr_value_list,
                                           int stream_type)
{
  // Serialize the datastream.
  if (!SerializeXattrStream(jcr, xattr_data, expected_serialize_len,
                            xattr_value_list)) {
    Mmsg1(jcr->errmsg,
          T_("Failed to Serialize extended attributes on file \"%s\" (expected "
             "len %" PRIu32 " != actual len %" PRIu32 ")\n"),
          xattr_data->last_fname, expected_serialize_len,
          xattr_data->content_length);
    Dmsg1(100,
          T_("Failed to Serialize extended attributes on file \"%s\" (expected "
             "len %" PRIu32 " != actual len %" PRIu32 ")\n"),
          xattr_data->last_fname, expected_serialize_len,
          xattr_data->content_length);
    return BxattrExitCode::kError;
  }
  return SendXattrStream(jcr, xattr_data, stream_type);
}
// This is a supported OS, See what kind of interface we should use.
#  if defined(HAVE_AIX_OS)

#    if (!defined(HAVE_AIX_EA)
#      error "Missing full support for the Extended Attributes (EA) functions."
#    endif

#    include <sys/ea.h>

// Define the supported XATTR streams for this OS
static int os_default_xattr_streams[1] = {STREAM_XATTR_AIX};

static BxattrExitCode aix_build_xattr_streams(JobControlRecord* jcr,
                                              XattrBuildData* xattr_data,
                                              FindFilesPacket* ff_pkt)
{
  char* bp;
  bool skip_xattr;
  char* xattr_list = NULL;
  int cnt, xattr_count = 0;
  uint32_t name_length;
  int32_t xattr_list_len, xattr_value_len;
  uint32_t expected_serialize_len = 0;
  xattr_t* current_xattr;
  alist<xattr_t*>* xattr_value_list = NULL;
  BxattrExitCode retval = BxattrExitCode::kError;

  // First get the length of the available list with extended attributes.
  xattr_list_len = llistea(xattr_data->last_fname, NULL, 0);
  switch (xattr_list_len) {
    case -1: {
      BErrNo be;

      switch (errno) {
        case ENOENT:
        case EFORMAT:
          retval = BxattrExitCode::kSuccess;
          goto bail_out;
        case ENOTSUP:
          /* If the filesystem reports it doesn't support XATTRs we clear the
           * BXATTR_FLAG_SAVE_NATIVE flag so we skip XATTR saves on all other
           * files on the same filesystem. The BXATTR_FLAG_SAVE_NATIVE flags
           * gets sets again when we change from one filesystem to another. */
          xattr_data->flags &= ~BXATTR_FLAG_SAVE_NATIVE;
          retval = BxattrExitCode::kWarning;
          warn_if_disabling_xattr(jcr, xattr_data->last_fname);
          goto bail_out;
        default:
          Mmsg2(jcr->errmsg, T_("llistea error on file \"%s\": ERR=%s\n"),
                xattr_data->last_fname, be.bstrerror());
          Dmsg2(100, "llistea error file=%s ERR=%s\n", xattr_data->last_fname,
                be.bstrerror());
          goto bail_out;
      }
      break;
    }
    case 0:
      retval = BxattrExitCode::kSuccess;
      goto bail_out;
    default:
      break;
  }

  // Allocate room for the extented attribute list.
  xattr_list = (char*)malloc(xattr_list_len + 1);
  memset(xattr_list, 0, xattr_list_len + 1);

  // Get the actual list of extended attributes names for a file.
  xattr_list_len = llistea(xattr_data->last_fname, xattr_list, xattr_list_len);
  switch (xattr_list_len) {
    case -1: {
      BErrNo be;

      switch (errno) {
        case ENOENT:
        case EFORMAT:
          retval = BxattrExitCode::kSuccess;
          goto bail_out;
        default:
          Mmsg2(jcr->errmsg, T_("llistea error on file \"%s\": ERR=%s\n"),
                xattr_data->last_fname, be.bstrerror());
          Dmsg2(100, "llistea error file=%s ERR=%s\n", xattr_data->last_fname,
                be.bstrerror());
          goto bail_out;
      }
      break;
    }
    default:
      break;
  }
  xattr_list[xattr_list_len] = '\0';

  /* Walk the list of extended attributes names and retrieve the data.
   * We already count the bytes needed for serializing the stream later on. */
  for (bp = xattr_list; (bp - xattr_list) + 1 < xattr_list_len;
       bp = strchr(bp, '\0') + 1) {
    skip_xattr = false;

    // We want to skip certain xattrs which start with a 0xF8 character on AIX.
    if (*bp == 0xF8) { skip_xattr = true; }

    name_length = strlen(bp);
    if (skip_xattr || name_length == 0) {
      Dmsg1(100, "Skipping xattr named %s\n", bp);
      continue;
    }

    // First see how long the value is for the extended attribute.
    xattr_value_len = lgetea(xattr_data->last_fname, bp, NULL, 0);
    switch (xattr_value_len) {
      case -1: {
        BErrNo be;

        switch (errno) {
          case ENOENT:
          case EFORMAT:
            retval = BxattrExitCode::kSuccess;
            goto bail_out;
          default:
            Mmsg2(jcr->errmsg, T_("lgetea error on file \"%s\": ERR=%s\n"),
                  xattr_data->last_fname, be.bstrerror());
            Dmsg2(100, "lgetea error file=%s ERR=%s\n", xattr_data->last_fname,
                  be.bstrerror());
            goto bail_out;
        }
        break;
      }
      default:
        break;
    }

    // Each xattr valuepair starts with a magic so we can parse it easier.
    current_xattr = (xattr_t*)malloc(sizeof(xattr_t));
    current_xattr->magic = XATTR_MAGIC;
    expected_serialize_len += sizeof(current_xattr->magic);

    // Allocate space for storing the name.
    current_xattr->name_length = name_length;
    current_xattr->name = (char*)malloc(current_xattr->name_length);
    memcpy(current_xattr->name, bp, current_xattr->name_length);

    expected_serialize_len
        += sizeof(current_xattr->name_length) + current_xattr->name_length;

    switch (xattr_value_len) {
      case 0:
        current_xattr->value = NULL;
        current_xattr->value_length = 0;
        expected_serialize_len += sizeof(current_xattr->value_length);
        break;
      default:
        // Allocate space for storing the value.
        current_xattr->value = (char*)malloc(xattr_value_len);
        memset(current_xattr->value, 0, xattr_value_len);

        xattr_value_len = lgetea(xattr_data->last_fname, bp,
                                 current_xattr->value, xattr_value_len);
        if (xattr_value_len < 0) {
          BErrNo be;

          switch (errno) {
            case ENOENT:
            case EFORMAT:
              retval = BxattrExitCode::kSuccess;
              break;
            default:
              Mmsg2(jcr->errmsg, T_("lgetea error on file \"%s\": ERR=%s\n"),
                    xattr_data->last_fname, be.bstrerror());
              Dmsg2(100, "lgetea error file=%s ERR=%s\n",
                    xattr_data->last_fname, be.bstrerror());
              break;
          }

          // Default failure path out when retrieval of attr fails.
          free(current_xattr->value);
          free(current_xattr->name);
          free(current_xattr);
          goto bail_out;
        }

        // Store the actual length of the value.
        current_xattr->value_length = xattr_value_len;
        expected_serialize_len += sizeof(current_xattr->value_length)
                                  + current_xattr->value_length;
        break;
    }

    if (xattr_value_list == NULL) {
      xattr_value_list = new alist<xattr_t*>(10, not_owned_by_alist);
    }

    xattr_value_list->append(current_xattr);
    xattr_count++;

    // Protect ourself against things getting out of hand.
    if (expected_serialize_len >= MAX_XATTR_STREAM) {
      Mmsg2(
          jcr->errmsg,
          T_("Xattr stream on file \"%s\" exceeds maximum size of %d bytes\n"),
          xattr_data->last_fname, MAX_XATTR_STREAM);
      goto bail_out;
    }
  }

  free(xattr_list);
  xattr_list = (char*)NULL;

  // If we found any xattr send them to the SD.
  if (xattr_count > 0) {
    retval = SerializeAndSendXattrStream(
        jcr, xattr_data, expected_serialize_len, xattr_value_list,
        os_default_xattr_streams[0]);
  } else {
    retval = BxattrExitCode::kSuccess;
  }

bail_out:
  if (xattr_list != NULL) { free(xattr_list); }
  if (xattr_value_list != NULL) { XattrDropInternalTable(xattr_value_list); }

  return retval;
}

static BxattrExitCode aix_parse_xattr_streams(JobControlRecord* jcr,
                                              XattrData* xattr_data,
                                              int stream,
                                              char* content,
                                              uint32_t content_length)
{
  xattr_t* current_xattr;
  alist<xattr_t*>* xattr_value_list;
  BxattrExitCode retval = BxattrExitCode::kError;

  xattr_value_list = new alist<xattr_t*>(10, not_owned_by_alist);

  if (UnSerializeXattrStream(jcr, xattr_data, content, content_length,
                             xattr_value_list)
      != BxattrExitCode::kSuccess) {
    goto bail_out;
  }

  for (auto* current_xattr : xattr_value_list) {
    if (lsetea(xattr_data->last_fname, current_xattr->name,
               current_xattr->value, current_xattr->value_length, 0)
        != 0) {
      BErrNo be;

      switch (errno) {
        case ENOENT:
        case EFORMAT:
          goto bail_out;
        case ENOTSUP:
          /* If the filesystem reports it doesn't support XATTRs we clear
           * the BXATTR_FLAG_RESTORE_NATIVE flag so we skip XATTR restores
           * on all other files on the same filesystem. The
           * BXATTR_FLAG_RESTORE_NATIVE flags gets sets again when we
           * change from one filesystem to another. */
          xattr_data->flags &= ~BXATTR_FLAG_RESTORE_NATIVE;
          retval = BxattrExitCode::kWarning;
          warn_if_disabling_xattrs(jcr, xattr_data->last_fname);
          goto bail_out;
        default:
          Mmsg2(jcr->errmsg, T_("lsetea error on file \"%s\": ERR=%s\n"),
                xattr_data->last_fname, be.bstrerror());
          Dmsg2(100, "lsetea error file=%s ERR=%s\n", xattr_data->last_fname,
                be.bstrerror());
          goto bail_out;
      }
    }
  }

  retval = BxattrExitCode::kSuccess;

bail_out:
  XattrDropInternalTable(xattr_value_list);

  return retval;
}

// Function pointers to the build and parse function to use for these xattrs.
static BxattrExitCode (*os_build_xattr_streams)(JobControlRecord* jcr,
                                                XattrBuildData* xattr_data,
                                                FindFilesPacket* ff_pkt)
    = aix_build_xattr_streams;
static BxattrExitCode (*os_parse_xattr_streams)(JobControlRecord* jcr,
                                                XattrData* xattr_data,
                                                int stream,
                                                char* content,
                                                uint32_t content_length)
    = aix_parse_xattr_streams;

#  elif defined(HAVE_DARWIN_OS) || defined(HAVE_LINUX_OS)

#    if (defined(HAVE_DARWIN_OS) && !defined(HAVE_DARWIN_XATTR)) \
        || (defined(HAVE_LINUX_OS) && !defined(HAVE_LINUX_XATTR))
#      error "Missing full support for the XATTR functions."
#    endif

#    include <sys/xattr.h>

// Define the supported XATTR streams for this OS
#    if defined(HAVE_DARWIN_OS)
static int os_default_xattr_streams[1] = {STREAM_XATTR_DARWIN};
static const char* xattr_acl_skiplist[2] = {"com.apple.system.Security", NULL};
static const char* xattr_skiplist[3]
    = {"com.apple.system.extendedsecurity", "com.apple.ResourceFork", NULL};
#    elif defined(HAVE_LINUX_OS)
static int os_default_xattr_streams[1] = {STREAM_XATTR_LINUX};
static const char* xattr_acl_skiplist[3]
    = {"system.posix_acl_access", "system.posix_acl_default", NULL};
static const char* xattr_skiplist[]
    = {"ceph.dir.entries",  "ceph.dir.files",    "ceph.dir.rbytes",
       "ceph.dir.rctime",   "ceph.dir.rentries", "ceph.dir.rfiles",
       "ceph.dir.rsubdirs", "ceph.dir.subdirs",  NULL};
#    endif

/**
 * OSX doesn't have llistxattr, lgetxattr and lsetxattr but has
 * listxattr, getxattr and setxattr with an extra options argument
 * which mimics the l variants of the functions when we specify
 * XATTR_NOFOLLOW as the options value.
 */
#    if defined(HAVE_DARWIN_OS)
#      define llistxattr(path, list, size) \
        listxattr((path), (list), (size), XATTR_NOFOLLOW)
#      define lgetxattr(path, name, value, size) \
        getxattr((path), (name), (value), (size), 0, XATTR_NOFOLLOW)
#      define lsetxattr(path, name, value, size, flags) \
        setxattr((path), (name), (value), (size), (flags), XATTR_NOFOLLOW)
#    endif

static BxattrExitCode generic_build_xattr_streams(JobControlRecord* jcr,
                                                  XattrBuildData* xattr_data,
                                                  FindFilesPacket* ff_pkt)
{
  char* bp;
  bool skip_xattr;
  char* xattr_list = NULL;
  int cnt, xattr_count = 0;
  uint32_t name_length;
  int32_t xattr_list_len, xattr_value_len;
  uint32_t expected_serialize_len = 0;
  xattr_t* current_xattr;
  alist<xattr_t*>* xattr_value_list = NULL;
  BxattrExitCode retval = BxattrExitCode::kError;

  // First get the length of the available list with extended attributes.
  xattr_list_len = llistxattr(xattr_data->last_fname, NULL, 0);
  switch (xattr_list_len) {
    case -1: {
      BErrNo be;

      switch (errno) {
        case ENOENT:
          retval = BxattrExitCode::kSuccess;
          goto bail_out;
        case BXATTR_ENOTSUP:
          /* If the filesystem reports it doesn't support XATTRs we clear
           * the BXATTR_FLAG_RESTORE_NATIVE flag so we skip XATTR restores
           * on all other files on the same filesystem. The
           * BXATTR_FLAG_RESTORE_NATIVE flags gets sets again when we
           * change from one filesystem to another. */
          xattr_data->flags &= ~BXATTR_FLAG_SAVE_NATIVE;
          retval = BxattrExitCode::kWarning;
          warn_if_disabling_xattrs(jcr, xattr_data->last_fname);
          goto bail_out;
        default:
          Mmsg2(jcr->errmsg, T_("llistxattr error on file \"%s\": ERR=%s\n"),
                xattr_data->last_fname, be.bstrerror());
          Dmsg2(100, "llistxattr error file=%s ERR=%s\n",
                xattr_data->last_fname, be.bstrerror());
          goto bail_out;
      }
      break;
    }
    case 0:
      retval = BxattrExitCode::kSuccess;
      goto bail_out;
    default:
      break;
  }

  // Allocate room for the extented attribute list.
  xattr_list = (char*)malloc(xattr_list_len + 1);
  memset(xattr_list, 0, xattr_list_len + 1);

  // Get the actual list of extended attributes names for a file.
  xattr_list_len
      = llistxattr(xattr_data->last_fname, xattr_list, xattr_list_len);
  switch (xattr_list_len) {
    case -1: {
      BErrNo be;

      switch (errno) {
        case ENOENT:
          retval = BxattrExitCode::kSuccess;
          goto bail_out;
        default:
          Mmsg2(jcr->errmsg, T_("llistxattr error on file \"%s\": ERR=%s\n"),
                xattr_data->last_fname, be.bstrerror());
          Dmsg2(100, "llistxattr error file=%s ERR=%s\n",
                xattr_data->last_fname, be.bstrerror());
          goto bail_out;
      }
      break;
    }
    default:
      break;
  }
  xattr_list[xattr_list_len] = '\0';

  /* Walk the list of extended attributes names and retrieve the data.
   * We already count the bytes needed for serializing the stream later on. */
  for (bp = xattr_list; (bp - xattr_list) + 1 < xattr_list_len;
       bp = strchr(bp, '\0') + 1) {
    skip_xattr = false;

    /* On some OSes you also get the acls in the extented attribute list.
     * So we check if we are already backing up acls and if we do we
     * don't store the extended attribute with the same info. */
    if (BitIsSet(FO_ACL, ff_pkt->flags)) {
      for (cnt = 0; xattr_acl_skiplist[cnt] != NULL; cnt++) {
        if (bstrcmp(bp, xattr_acl_skiplist[cnt])) {
          skip_xattr = true;
          break;
        }
      }
    }

    /* On some OSes we want to skip certain xattrs which are in the
     * xattr_skiplist array. */
    if (!skip_xattr) {
      for (cnt = 0; xattr_skiplist[cnt] != NULL; cnt++) {
        if (bstrcmp(bp, xattr_skiplist[cnt])) {
          skip_xattr = true;
          break;
        }
      }
    }

    name_length = strlen(bp);
    if (skip_xattr || name_length == 0) {
      Dmsg1(100, "Skipping xattr named %s\n", bp);
      continue;
    }

    // First see how long the value is for the extended attribute.
    xattr_value_len = lgetxattr(xattr_data->last_fname, bp, NULL, 0);
    switch (xattr_value_len) {
      case -1: {
        BErrNo be;

        switch (errno) {
          case ENOENT:
            retval = BxattrExitCode::kSuccess;
            goto bail_out;
          default:
            Mmsg2(jcr->errmsg, T_("lgetxattr error on file \"%s\": ERR=%s\n"),
                  xattr_data->last_fname, be.bstrerror());
            Dmsg2(100, "lgetxattr error file=%s ERR=%s\n",
                  xattr_data->last_fname, be.bstrerror());
            goto bail_out;
        }
        break;
      }
      default:
        break;
    }

    // Each xattr valuepair starts with a magic so we can parse it easier.
    current_xattr = (xattr_t*)malloc(sizeof(xattr_t));
    current_xattr->magic = XATTR_MAGIC;
    current_xattr->value = NULL;
    expected_serialize_len += sizeof(current_xattr->magic);

    // Allocate space for storing the name.
    current_xattr->name_length = name_length;
    current_xattr->name = (char*)malloc(current_xattr->name_length);
    memcpy(current_xattr->name, bp, current_xattr->name_length);

    expected_serialize_len
        += sizeof(current_xattr->name_length) + current_xattr->name_length;

    switch (xattr_value_len) {
      case 0:
        current_xattr->value = NULL;
        current_xattr->value_length = 0;
        expected_serialize_len += sizeof(current_xattr->value_length);
        break;
      default:
        // Allocate space for storing the value.
        current_xattr->value = (char*)malloc(xattr_value_len);
        memset(current_xattr->value, 0, xattr_value_len);

        xattr_value_len = lgetxattr(xattr_data->last_fname, bp,
                                    current_xattr->value, xattr_value_len);
        if (xattr_value_len < 0) {
          BErrNo be;

          switch (errno) {
            case ENOENT:
              retval = BxattrExitCode::kSuccess;
              break;
            default:
              Mmsg2(jcr->errmsg, T_("lgetxattr error on file \"%s\": ERR=%s\n"),
                    xattr_data->last_fname, be.bstrerror());
              Dmsg2(100, "lgetxattr error file=%s ERR=%s\n",
                    xattr_data->last_fname, be.bstrerror());
              break;
          }

          // Default failure path out when retrieval of attr fails.
          free(current_xattr->value);
          free(current_xattr->name);
          free(current_xattr);
          goto bail_out;
        }
        // Store the actual length of the value.
        current_xattr->value_length = xattr_value_len;
        expected_serialize_len += sizeof(current_xattr->value_length)
                                  + current_xattr->value_length;
        break;
    }

    if (xattr_value_list == NULL) {
      xattr_value_list = new alist<xattr_t*>(10, not_owned_by_alist);
    }

    xattr_value_list->append(current_xattr);
    xattr_count++;

    // Protect ourself against things getting out of hand.
    if (expected_serialize_len >= MAX_XATTR_STREAM) {
      Mmsg2(
          jcr->errmsg,
          T_("Xattr stream on file \"%s\" exceeds maximum size of %d bytes\n"),
          xattr_data->last_fname, MAX_XATTR_STREAM);
      goto bail_out;
    }
  }

  free(xattr_list);
  xattr_list = (char*)NULL;

  // If we found any xattr send them to the SD.
  if (xattr_count > 0) {
    retval = SerializeAndSendXattrStream(
        jcr, xattr_data, expected_serialize_len, xattr_value_list,
        os_default_xattr_streams[0]);
  } else {
    retval = BxattrExitCode::kSuccess;
  }

bail_out:
  if (xattr_list != NULL) { free(xattr_list); }
  if (xattr_value_list != NULL) { XattrDropInternalTable(xattr_value_list); }

  return retval;
}

static BxattrExitCode generic_parse_xattr_streams(JobControlRecord* jcr,
                                                  XattrData* xattr_data,
                                                  int,
                                                  char* content,
                                                  uint32_t content_length)
{
  alist<xattr_t*>* xattr_value_list;
  BxattrExitCode retval = BxattrExitCode::kError;

  xattr_value_list = new alist<xattr_t*>(10, not_owned_by_alist);

  if (UnSerializeXattrStream(jcr, xattr_data, content, content_length,
                             xattr_value_list)
      != BxattrExitCode::kSuccess) {
    goto bail_out;
  }

  for (auto* current_xattr : xattr_value_list) {
    if (lsetxattr(xattr_data->last_fname, current_xattr->name,
                  current_xattr->value, current_xattr->value_length, 0)
        != 0) {
      BErrNo be;

      switch (errno) {
        case ENOENT:
          goto bail_out;
        case BXATTR_ENOTSUP:
          /* If the filesystem reports it doesn't support XATTRs we clear
           * the BXATTR_FLAG_RESTORE_NATIVE flag so we skip XATTR restores
           * on all other files on the same filesystem. The
           * BXATTR_FLAG_RESTORE_NATIVE flags gets sets again when we
           * change from one filesystem to another. */
          xattr_data->flags &= ~BXATTR_FLAG_RESTORE_NATIVE;
          retval = BxattrExitCode::kWarning;
          warn_if_disabling_xattrs(jcr, xattr_data->last_fname);
          goto bail_out;
        default:
          Mmsg2(jcr->errmsg, T_("lsetxattr error on file \"%s\": ERR=%s\n"),
                xattr_data->last_fname, be.bstrerror());
          Dmsg2(100, "lsetxattr error file=%s ERR=%s\n", xattr_data->last_fname,
                be.bstrerror());
          goto bail_out;
      }
    }
  }

  retval = BxattrExitCode::kSuccess;

bail_out:
  XattrDropInternalTable(xattr_value_list);

  return retval;
}

// Function pointers to the build and parse function to use for these xattrs.
static BxattrExitCode (*os_build_xattr_streams)(JobControlRecord* jcr,
                                                XattrBuildData* xattr_data,
                                                FindFilesPacket* ff_pkt)
    = generic_build_xattr_streams;
static BxattrExitCode (*os_parse_xattr_streams)(JobControlRecord* jcr,
                                                XattrData* xattr_data,
                                                int stream,
                                                char* content,
                                                uint32_t content_length)
    = generic_parse_xattr_streams;

#  elif defined(HAVE_FREEBSD_OS)

#    if !defined(HAVE_FREEBSD_EXTATTR)
#      error "Missing full support for the extattr functions."
#    endif

#    include <sys/extattr.h>

#    if __has_include(<libutil.h>)
#      include <libutil.h>
#    endif

static int os_default_xattr_streams[1] = {STREAM_XATTR_FREEBSD};
static int os_default_xattr_namespaces[2]
    = {EXTATTR_NAMESPACE_USER, EXTATTR_NAMESPACE_SYSTEM};
static const char* xattr_acl_skiplist[4]
    = {"system.posix1e.acl_access", "system.posix1e.acl_default",
       "system.nfs4.acl", NULL};
static const char* xattr_skiplist[1] = {NULL};

static BxattrExitCode bsd_build_xattr_streams(JobControlRecord* jcr,
                                              XattrBuildData* xattr_data,
                                              FindFilesPacket* ff_pkt)
{
  bool skip_xattr;
  char* xattr_list = NULL;
  int cnt, index, xattr_count = 0;
  int32_t xattr_list_len, xattr_value_len;
  uint32_t expected_serialize_len = 0;
  unsigned int namespace_index;
  int attrnamespace;
  char* current_attrnamespace = NULL;
  char current_attrname[XATTR_BUFSIZ], current_attrtuple[XATTR_BUFSIZ];
  xattr_t* current_xattr;
  alist<xattr_t*>* xattr_value_list = NULL;
  BxattrExitCode retval = BxattrExitCode::kError;

  // Loop over all available xattr namespaces.
  for (namespace_index = 0;
       namespace_index < sizeof(os_default_xattr_namespaces) / sizeof(int);
       namespace_index++) {
    attrnamespace = os_default_xattr_namespaces[namespace_index];

    /* First get the length of the available list with extended attributes.
     * If we get EPERM on system namespace, don't return error.
     * This is expected for normal users trying to archive the system
     * namespace on FreeBSD 6.2 and later. On NetBSD 3.1 and later,
     * they've decided to return EOPNOTSUPP instead. */
    xattr_list_len
        = extattr_list_link(xattr_data->last_fname, attrnamespace, NULL, 0);
    switch (xattr_list_len) {
      case -1: {
        BErrNo be;

        switch (errno) {
          case ENOENT:
            retval = BxattrExitCode::kSuccess;
            goto bail_out;
#    if defined(EOPNOTSUPP)
          case EOPNOTSUPP:
#    endif
          case EPERM:
            if (attrnamespace == EXTATTR_NAMESPACE_SYSTEM) { continue; }
            // FALLTHROUGH
          default:
            Mmsg2(jcr->errmsg,
                  T_("extattr_list_link error on file \"%s\": ERR=%s\n"),
                  xattr_data->last_fname, be.bstrerror());
            Dmsg2(100, "extattr_list_link error file=%s ERR=%s\n",
                  xattr_data->last_fname, be.bstrerror());
            goto bail_out;
        }
        break;
      }
      case 0:
        continue;
      default:
        break;
    }

    // Allocate room for the extented attribute list.
    xattr_list = (char*)malloc(xattr_list_len + 1);
    memset(xattr_list, 0, xattr_list_len + 1);

    // Get the actual list of extended attributes names for a file.
    xattr_list_len = extattr_list_link(xattr_data->last_fname, attrnamespace,
                                       xattr_list, xattr_list_len);
    switch (xattr_list_len) {
      case -1: {
        BErrNo be;

        switch (errno) {
          case ENOENT:
            retval = BxattrExitCode::kSuccess;
            goto bail_out;
          default:
            Mmsg2(jcr->errmsg,
                  T_("extattr_list_link error on file \"%s\": ERR=%s\n"),
                  xattr_data->last_fname, be.bstrerror());
            Dmsg2(100, "extattr_list_link error file=%s ERR=%s\n",
                  xattr_data->last_fname, be.bstrerror());
            goto bail_out;
        }
        break;
      }
      default:
        break;
    }
    xattr_list[xattr_list_len] = '\0';

    /* Convert the numeric attrnamespace into a string representation and make
     * a private copy of that string. The extattr_namespace_to_string functions
     * returns a strdupped string which we need to free. */
    if (extattr_namespace_to_string(attrnamespace, &current_attrnamespace)
        != 0) {
      Mmsg2(jcr->errmsg,
            T_("Failed to convert %d into namespace on file \"%s\"\n"),
            attrnamespace, xattr_data->last_fname);
      Dmsg2(100, "Failed to convert %d into namespace on file \"%s\"\n",
            attrnamespace, xattr_data->last_fname);
      goto bail_out;
    }

    /* Walk the list of extended attributes names and retrieve the data.
     * We already count the bytes needed for serializing the stream later on. */
    for (index = 0; index < xattr_list_len; index += xattr_list[index] + 1) {
      skip_xattr = false;

      /* Print the current name into the buffer as its not null terminated
       * we need to use the length encoded in the string for copying only
       * the needed bytes. */
      cnt = xattr_list[index];
      if (cnt > ((int)sizeof(current_attrname) - 1)) {
        cnt = ((int)sizeof(current_attrname) - 1);
      }
      strncpy(current_attrname, xattr_list + (index + 1), cnt);
      current_attrname[cnt] = '\0';

      /* First make a xattr tuple of the current namespace and the name of
       * the xattr. e.g. something like user.<attrname> or system.<attrname> */
      Bsnprintf(current_attrtuple, sizeof(current_attrtuple), "%s.%s",
                current_attrnamespace, current_attrname);

      /* On some OSes you also get the acls in the extented attribute list.
       * So we check if we are already backing up acls and if we do we
       * don't store the extended attribute with the same info. */
      if (BitIsSet(FO_ACL, ff_pkt->flags)) {
        for (cnt = 0; xattr_acl_skiplist[cnt] != NULL; cnt++) {
          if (bstrcmp(current_attrtuple, xattr_acl_skiplist[cnt])) {
            skip_xattr = true;
            break;
          }
        }
      }

      /* On some OSes we want to skip certain xattrs which are in the
       * xattr_skiplist array. */
      if (!skip_xattr) {
        for (cnt = 0; xattr_skiplist[cnt] != NULL; cnt++) {
          if (bstrcmp(current_attrtuple, xattr_skiplist[cnt])) {
            skip_xattr = true;
            break;
          }
        }
      }

      if (skip_xattr) {
        Dmsg1(100, "Skipping xattr named %s\n", current_attrname);
        continue;
      }

      // First see how long the value is for the extended attribute.
      xattr_value_len = extattr_get_link(xattr_data->last_fname, attrnamespace,
                                         current_attrname, NULL, 0);
      switch (xattr_value_len) {
        case -1: {
          BErrNo be;

          switch (errno) {
            case ENOENT:
              retval = BxattrExitCode::kSuccess;
              goto bail_out;
            default:
              Mmsg2(jcr->errmsg,
                    T_("extattr_get_link error on file \"%s\": ERR=%s\n"),
                    xattr_data->last_fname, be.bstrerror());
              Dmsg2(100, "extattr_get_link error file=%s ERR=%s\n",
                    xattr_data->last_fname, be.bstrerror());
              goto bail_out;
          }
          break;
        }
        default:
          break;
      }

      // Each xattr valuepair starts with a magic so we can parse it easier.
      current_xattr = (xattr_t*)malloc(sizeof(xattr_t));
      current_xattr->magic = XATTR_MAGIC;
      current_xattr->value = NULL;
      expected_serialize_len += sizeof(current_xattr->magic);

      // Allocate space for storing the name.
      current_xattr->name_length = strlen(current_attrtuple);
      current_xattr->name = (char*)malloc(current_xattr->name_length);
      memcpy(current_xattr->name, current_attrtuple,
             current_xattr->name_length);

      expected_serialize_len
          += sizeof(current_xattr->name_length) + current_xattr->name_length;

      switch (xattr_value_len) {
        case 0:
          current_xattr->value = NULL;
          current_xattr->value_length = 0;
          expected_serialize_len += sizeof(current_xattr->value_length);
          break;
        default:
          // Allocate space for storing the value.
          current_xattr->value = (char*)malloc(xattr_value_len);
          memset(current_xattr->value, 0, xattr_value_len);

          xattr_value_len = extattr_get_link(
              xattr_data->last_fname, attrnamespace, current_attrname,
              current_xattr->value, xattr_value_len);
          if (xattr_value_len < 0) {
            BErrNo be;

            switch (errno) {
              case ENOENT:
                retval = BxattrExitCode::kSuccess;
                break;
              default:
                Mmsg2(jcr->errmsg,
                      T_("extattr_get_link error on file \"%s\": ERR=%s\n"),
                      xattr_data->last_fname, be.bstrerror());
                Dmsg2(100, "extattr_get_link error file=%s ERR=%s\n",
                      xattr_data->last_fname, be.bstrerror());
                break;
            }

            // Default failure path out when retrieval of attr fails.
            free(current_xattr->value);
            free(current_xattr->name);
            free(current_xattr);
            goto bail_out;
          }

          // Store the actual length of the value.
          current_xattr->value_length = xattr_value_len;
          expected_serialize_len += sizeof(current_xattr->value_length)
                                    + current_xattr->value_length;
          break;
      }

      if (xattr_value_list == NULL) {
        xattr_value_list = new alist<xattr_t*>(10, not_owned_by_alist);
      }

      xattr_value_list->append(current_xattr);
      xattr_count++;

      // Protect ourself against things getting out of hand.
      if (expected_serialize_len >= MAX_XATTR_STREAM) {
        Mmsg2(jcr->errmsg,
              T_("Xattr stream on file \"%s\" exceeds maximum size of %d "
                 "bytes\n"),
              xattr_data->last_fname, MAX_XATTR_STREAM);
        goto bail_out;
      }
    }

    // Drop the local copy of the current_attrnamespace.
    free(current_attrnamespace);
    current_attrnamespace = NULL;

    // We are done with this xattr list.
    free(xattr_list);
    xattr_list = (char*)NULL;
  }

  // If we found any xattr send them to the SD.
  if (xattr_count > 0) {
    retval = SerializeAndSendXattrStream(
        jcr, xattr_data, expected_serialize_len, xattr_value_list,
        os_default_xattr_streams[0]);
  } else {
    retval = BxattrExitCode::kSuccess;
  }

bail_out:
  if (current_attrnamespace != NULL) { free(current_attrnamespace); }
  if (xattr_list != NULL) { free(xattr_list); }
  if (xattr_value_list != NULL) { XattrDropInternalTable(xattr_value_list); }

  return retval;
}

static BxattrExitCode bsd_parse_xattr_streams(JobControlRecord* jcr,
                                              XattrData* xattr_data,
                                              int,
                                              char* content,
                                              uint32_t content_length)
{
  alist<xattr_t*>* xattr_value_list;
  int current_attrnamespace, cnt;
  char *attrnamespace, *attrname;
  BxattrExitCode retval = BxattrExitCode::kError;

  xattr_value_list = new alist<xattr_t*>(10, not_owned_by_alist);

  if (UnSerializeXattrStream(jcr, xattr_data, content, content_length,
                             xattr_value_list)
      != BxattrExitCode::kSuccess) {
    goto bail_out;
  }

  for (auto* current_xattr : xattr_value_list) {
    /* Try splitting the xattr_name into a namespace and name part.
     * The splitting character is a . */
    attrnamespace = current_xattr->name;
    if ((attrname = strchr(attrnamespace, '.')) == (char*)NULL) {
      Mmsg2(jcr->errmsg,
            T_("Failed to split %s into namespace and name part on file "
               "\"%s\"\n"),
            current_xattr->name, xattr_data->last_fname);
      Dmsg2(100,
            "Failed to split %s into namespace and name part on file \"%s\"\n",
            current_xattr->name, xattr_data->last_fname);
      goto bail_out;
    }
    *attrname++ = '\0';

    // Make sure the attrnamespace makes sense.
    if (extattr_string_to_namespace(attrnamespace, &current_attrnamespace)
        != 0) {
      Mmsg2(jcr->errmsg,
            T_("Failed to convert %s into namespace on file \"%s\"\n"),
            attrnamespace, xattr_data->last_fname);
      Dmsg2(100, "Failed to convert %s into namespace on file \"%s\"\n",
            attrnamespace, xattr_data->last_fname);
      goto bail_out;
    }

    // Try restoring the extended attribute.
    cnt = extattr_set_link(xattr_data->last_fname, current_attrnamespace,
                           attrname, current_xattr->value,
                           current_xattr->value_length);
    if (cnt < 0 || cnt != (int)current_xattr->value_length) {
      BErrNo be;

      switch (errno) {
        case ENOENT:
          goto bail_out;
          break;
        default:
          Mmsg2(jcr->errmsg,
                T_("extattr_set_link error on file \"%s\": ERR=%s\n"),
                xattr_data->last_fname, be.bstrerror());
          Dmsg2(100, "extattr_set_link error file=%s ERR=%s\n",
                xattr_data->last_fname, be.bstrerror());
          goto bail_out;
          break;
      }
    }
  }

  retval = BxattrExitCode::kSuccess;

bail_out:
  XattrDropInternalTable(xattr_value_list);

  return retval;
}

// Function pointers to the build and parse function to use for these xattrs.
static BxattrExitCode (*os_build_xattr_streams)(JobControlRecord* jcr,
                                                XattrBuildData* xattr_data,
                                                FindFilesPacket* ff_pkt)
    = bsd_build_xattr_streams;
static BxattrExitCode (*os_parse_xattr_streams)(JobControlRecord* jcr,
                                                XattrData* xattr_data,
                                                int stream,
                                                char* content,
                                                uint32_t content_length)
    = bsd_parse_xattr_streams;

#  elif defined(HAVE_SUN_OS)
/**
 * Solaris extended attributes were introduced in Solaris 9
 * by PSARC 1999/209
 *
 * Solaris extensible attributes were introduced in OpenSolaris
 * by PSARC 2007/315 Solaris extensible attributes are also
 * sometimes called extended system attributes.
 *
 * man fsattr(5) on Solaris gives a wealth of info. The most
 * important bits are:
 *
 * Attributes are logically supported as files within the  file
 * system.   The  file  system  is  therefore augmented with an
 * orthogonal name space of file attributes. Any file  (includ-
 * ing  attribute files) can have an arbitrarily deep attribute
 * tree associated with it. Attribute values  are  accessed  by
 * file descriptors obtained through a special attribute inter-
 * face.  This logical view of "attributes as files" allows the
 * leveraging  of  existing file system interface functionality
 * to support the construction, deletion, and  manipulation  of
 * attributes.
 *
 * The special files  "."  and  ".."  retain  their  accustomed
 * semantics within the attribute hierarchy.  The "." attribute
 * file refers to the current directory and the ".."  attribute
 * file  refers to the parent directory.  The unnamed directory
 * at the head of each attribute tree is considered the "child"
 * of  the  file it is associated with and the ".." file refers
 * to the associated file.  For  any  non-directory  file  with
 * attributes,  the  ".." entry in the unnamed directory refers
 * to a file that is not a directory.
 *
 * Conceptually, the attribute model is fully general. Extended
 * attributes  can  be  any  type of file (doors, links, direc-
 * tories, and so forth) and can even have their own attributes
 * (fully  recursive).   As a result, the attributes associated
 * with a file could be an arbitrarily deep directory hierarchy
 * where each attribute could have an equally complex attribute
 * tree associated with it.  Not all implementations  are  able
 * to,  or  want to, support the full model. Implementation are
 * therefore permitted to reject operations that are  not  sup-
 * ported.   For  example,  the implementation for the UFS file
 * system allows only regular files as attributes (for example,
 * no sub-directories) and rejects attempts to place attributes
 * on attributes.
 *
 * The following list details the operations that are  rejected
 * in the current implementation:
 *
 * link                     Any attempt to create links between
 *                          attribute  and  non-attribute space
 *                          is rejected  to  prevent  security-
 *                          related   or   otherwise  sensitive
 *                          attributes from being exposed,  and
 *                          therefore  manipulable,  as regular
 *                          files.
 *
 * rename                   Any  attempt  to   rename   between
 *                          attribute  and  non-attribute space
 *                          is rejected to prevent  an  already
 *                          linked  file from being renamed and
 *                          thereby circumventing the link res-
 *                          triction above.
 *
 * mkdir, symlink, mknod    Any  attempt  to  create  a   "non-
 *                          regular" file in attribute space is
 *                          rejected to reduce the  functional-
 *                          ity,  and  therefore  exposure  and
 *                          risk, of  the  initial  implementa-
 *                          tion.
 *
 * The entire available name space has been allocated to  "gen-
 * eral use" to bring the implementation in line with the NFSv4
 * draft standard [NFSv4]. That standard defines "named  attri-
 * butes"  (equivalent  to Solaris Extended Attributes) with no
 * naming restrictions.  All Sun  applications  making  use  of
 * opaque extended attributes will use the prefix "SUNW".
 *
 */
#    if __has_include(<sys/attr.h>)
#      include <sys/attr.h>
#    endif

#    if __has_include(<attr.h>)
#      include <attr.h>
#    endif

#    if __has_include(<sys/nvpair.h>)
#      include <sys/nvpair.h>
#    endif

#    if __has_include(<sys/acl.h>)
#      include <sys/acl.h>
#    endif

// Define the supported XATTR streams for this OS
#    if __has_include(<sys/nvpair.h>) && defined(_PC_SATTR_ENABLED)
static int os_default_xattr_streams[2]
    = {STREAM_XATTR_SOLARIS, STREAM_XATTR_SOLARIS_SYS};
#    else
static int os_default_xattr_streams[1] = {STREAM_XATTR_SOLARIS};
#    endif /* __has_include(<sys/nvpair.h>) && defined(_PC_SATTR_ENABLED) */

/**
 * This code creates a temporary cache with entries for each xattr which has
 * a link count > 1 (which indicates it has one or more hard linked
 * counterpart(s))
 */
static inline xattr_link_cache_entry_t* find_xattr_link_cache_entry(
    XattrBuildData* xattr_data,
    ino_t inum)
{
  if (xattr_data->link_cache) { return nullptr; }
  for (auto* ptr : xattr_data->link_cache) {
    if (ptr && ptr->inum == inum) { return ptr; }
  }
  return NULL;
}

static inline void add_xattr_link_cache_entry(XattrBuildData* xattr_data,
                                              ino_t inum,
                                              char* target)
{
  xattr_link_cache_entry_t* ptr;

  ptr = (xattr_link_cache_entry_t*)malloc(sizeof(xattr_link_cache_entry_t));
  memset(ptr, 0, sizeof(xattr_link_cache_entry_t));
  ptr->inum = inum;
  ptr->target = strdup(target);

  if (!xattr_data->link_cache) {
    xattr_data->link_cache = new alist<xattr_t*>(10, not_owned_by_alist);
  }
  xattr_data->link_cache->append(ptr);
}

static inline void DropXattrLinkCache(XattrBuildData* xattr_data)
{
  /* Walk the list of xattr link cache entries and free allocated memory on
   * traversing. */
  if (xattr_data) { return; }
  for (auto* ptr : xattr_data->link_cache) {
    free(ptr->target);
    free(ptr);
  }

  delete xattr_data->link_cache;
  xattr_data->link_cache = NULL;
}

#    if __has_include(<sys/nvpair.h>) && defined(_PC_SATTR_ENABLED)
/**
 * This function returns true if a non default extended system attribute
 * list is associated with fd and returns false when an error has occurred
 * or when only extended system attributes other than archive,
 * av_modified or crtime are set.
 *
 * The function returns true for the following cases:
 *
 * - any extended system attribute other than the default attributes
 *   ('archive', 'av_modified' and 'crtime') is set
 * - nvlist has NULL name string
 * - nvpair has data type of 'nvlist'
 * - default data type.
 */
static bool SolarisHasNonTransientExtensibleAttributes(int fd)
{
  boolean_t value;
  data_type_t type;
  nvlist_t* response;
  nvpair_t* pair;
  f_attr_t fattr;
  char* name;
  bool retval = false;

  if (fgetattr(fd, XATTR_VIEW_READWRITE, &response) != 0) { return false; }

  pair = NULL;
  while ((pair = nvlist_next_nvpair(response, pair)) != NULL) {
    name = nvpair_name(pair);

    if (name != NULL) {
      fattr = name_to_attr(name);
    } else {
      retval = true;
      goto bail_out;
    }

    type = nvpair_type(pair);
    switch (type) {
      case DATA_TYPE_BOOLEAN_VALUE:
        if (nvpair_value_boolean_value(pair, &value) != 0) { continue; }
        if (value && fattr != F_ARCHIVE && fattr != F_AV_MODIFIED) {
          retval = true;
          goto bail_out;
        }
        break;
      case DATA_TYPE_UINT64_ARRAY:
        if (fattr != F_CRTIME) {
          retval = true;
          goto bail_out;
        }
        break;
      case DATA_TYPE_NVLIST:
      default:
        retval = true;
        goto bail_out;
    }
  }

bail_out:
  if (response != NULL) { nvlist_free(response); }
  return retval;
}
#    endif /* __has_include(<sys/nvpair.h>) && _PC_SATTR_ENABLED */

#    if defined(HAVE_ACL) && !defined(HAVE_EXTENDED_ACL)
/**
 * See if an acl is a trivial one (e.g. just the stat bits encoded as acl.)
 * There is no need to store those acls as we already store the stat bits too.
 */
static bool AclIsTrivial(int count, aclent_t* entries)
{
  int n;
  aclent_t* ace;

  for (n = 0; n < count; n++) {
    ace = &entries[n];
    if (!(ace->a_type == USER_OBJ || ace->a_type == GROUP_OBJ
          || ace->a_type == OTHER_OBJ || ace->a_type == CLASS_OBJ))
      return false;
  }
  return true;
}
#    endif /* HAVE_ACL && !HAVE_EXTENDED_ACL */

static BxattrExitCode solaris_save_xattr_acl(JobControlRecord* jcr,
                                             XattrData* xattr_data,
                                             int fd,
                                             const char* attrname,
                                             char** acl_text)
{
  BxattrExitCode retval = BxattrExitCode::kError;
#    ifdef HAVE_ACL
#      ifdef HAVE_EXTENDED_ACL
  int flags;
  acl_t* aclp = NULL;

  // See if this attribute has an ACL
  if ((fd != -1 && fpathconf(fd, _PC_ACL_ENABLED) > 0)
      || pathconf(attrname, _PC_ACL_ENABLED) > 0) {
    // See if there is a non trivial acl on the file.
    if ((fd != -1 && facl_get(fd, ACL_NO_TRIVIAL, &aclp) != 0)
        || acl_get(attrname, ACL_NO_TRIVIAL, &aclp) != 0) {
      BErrNo be;

      switch (errno) {
        case ENOENT:
          retval = BxattrExitCode::kSuccess;
          goto bail_out;
        default:
          Mmsg3(jcr->errmsg,
                T_("Unable to get acl on xattr %s on file \"%s\": ERR=%s\n"),
                attrname, xattr_data->last_fname, be.bstrerror());
          Dmsg3(100, "facl_get/acl_get of xattr %s on \"%s\" failed: ERR=%s\n",
                attrname, xattr_data->last_fname, be.bstrerror());
          goto bail_out;
      }
    }

    if (aclp != NULL) {
#        if defined(ACL_SID_FMT)
      // New format flag added in newer Solaris versions.
      flags = ACL_APPEND_ID | ACL_COMPACT_FMT | ACL_SID_FMT;
#        else
      flags = ACL_APPEND_ID | ACL_COMPACT_FMT;
#        endif /* ACL_SID_FMT */

      *acl_text = acl_totext(aclp, flags);
      acl_free(aclp);
    } else {
      *acl_text = NULL;
    }
  } else {
    *acl_text = NULL;
  }
  retval = BxattrExitCode::kSuccess;
#      else    /* HAVE_EXTENDED_ACL */
  int n;
  aclent_t* acls = NULL;

  // See if this attribute has an ACL
  if (fd != -1) {
    n = facl(fd, GETACLCNT, 0, NULL);
  } else {
    n = acl(attrname, GETACLCNT, 0, NULL);
  }

  if (n >= MIN_ACL_ENTRIES) {
    acls = (aclent_t*)malloc(n * sizeof(aclent_t));
    if ((fd != -1 && facl(fd, GETACL, n, acls) != n)
        || acl(attrname, GETACL, n, acls) != n) {
      BErrNo be;

      switch (errno) {
        case ENOENT:
          free(acls);
          retval = BxattrExitCode::kSuccess;
          goto bail_out;
        default:
          Mmsg3(jcr->errmsg,
                T_("Unable to get acl on xattr %s on file \"%s\": ERR=%s\n"),
                attrname, xattr_data->last_fname, be.bstrerror());
          Dmsg3(100, "facl/acl of xattr %s on \"%s\" failed: ERR=%s\n",
                attrname, xattr_data->last_fname, be.bstrerror());
          free(acls);
          goto bail_out;
      }
    }

    // See if there is a non trivial acl on the file.
    if (!AclIsTrivial(n, acls)) {
      if ((*acl_text = acltotext(acls, n)) == NULL) {
        BErrNo be;

        Mmsg3(jcr->errmsg,
              T_("Unable to get acl text on xattr %s on file \"%s\": ERR=%s\n"),
              attrname, xattr_data->last_fname, be.bstrerror());
        Dmsg3(100, "acltotext of xattr %s on \"%s\" failed: ERR=%s\n", attrname,
              xattr_data->last_fname, be.bstrerror());
        free(acls);
        goto bail_out;
      }
    } else {
      *acl_text = NULL;
    }

    free(acls);
  } else {
    *acl_text = NULL;
  }
  retval = BxattrExitCode::kSuccess;
#      endif   /* HAVE_EXTENDED_ACL */

#    else  /* HAVE_ACL */
  retval = BxattrExitCode::kSuccess;
#    endif /* HAVE_ACL */

bail_out:
  return retval;
}

// Forward declaration for recursive function call.
static BxattrExitCode solaris_save_xattrs(JobControlRecord* jcr,
                                          XattrData* xattr_data,
                                          const char* xattr_namespace,
                                          const char* attr_parent);

/**
 * Save an extended or extensible attribute.
 * This is stored as an opaque stream of bytes with the following encoding:
 *
 * <xattr_name>\0<stat_buffer>\0<acl_string>\0<actual_xattr_data>
 *
 * or for a hardlinked or symlinked attribute
 *
 * <xattr_name>\0<stat_buffer>\0<xattr_link_source>\0
 *
 * xattr_name can be a subpath relative to the file the xattr is on.
 * stat_buffer is the string representation of the stat struct.
 * acl_string is an acl text when a non trivial acl is set on the xattr.
 * actual_xattr_data is the content of the xattr file.
 */
static BxattrExitCode solaris_save_xattr(JobControlRecord* jcr,
                                         XattrBuildData* xattr_data,
                                         int fd,
                                         const char* xattr_namespace,
                                         const char* attrname,
                                         bool toplevel_hidden_dir,
                                         int stream)
{
  int cnt;
  int attrfd = -1;
  struct stat st;
  xattr_link_cache_entry_t* xlce;
  char target_attrname[PATH_MAX];
  char link_source[PATH_MAX];
  char* acl_text = NULL;
  char attribs[XATTR_BUFSIZ];
  char buffer[XATTR_BUFSIZ];
  BxattrExitCode retval = BxattrExitCode::kError;

  Bsnprintf(target_attrname, sizeof(target_attrname), "%s%s", xattr_namespace,
            attrname);

  // Get the stats of the extended or extensible attribute.
  if (fstatat(fd, attrname, &st, AT_SYMLINK_NOFOLLOW) < 0) {
    BErrNo be;

    switch (errno) {
      case ENOENT:
        retval = BxattrExitCode::kSuccess;
        goto bail_out;
      default:
        Mmsg3(jcr->errmsg,
              T_("Unable to get status on xattr %s on file \"%s\": ERR=%s\n"),
              target_attrname, xattr_data->last_fname, be.bstrerror());
        Dmsg3(100, "fstatat of xattr %s on \"%s\" failed: ERR=%s\n",
              target_attrname, xattr_data->last_fname, be.bstrerror());
        goto bail_out;
    }
  }

  /* Based on the filetype perform the correct action. We support most filetypes
   * here, more then the actual implementation on Solaris supports so some code
   * may never get executed due to limitations in the implementation. */
  switch (st.st_mode & S_IFMT) {
    case S_IFIFO:
    case S_IFCHR:
    case S_IFBLK:
      // Get any acl on the xattr.
      if (solaris_save_xattr_acl(jcr, xattr_data, attrfd, attrname, &acl_text)
          != BxattrExitCode::kSuccess)
        goto bail_out;

      /* The current implementation of xattr on Solaris doesn't support this,
       * but if it ever does we are prepared.
       * Encode the stat struct into an ASCII representation. */
      EncodeStat(attribs, &st, sizeof(st), 0, stream);
      cnt = Bsnprintf(buffer, sizeof(buffer), "%s%c%s%c%s%c", target_attrname,
                      0, attribs, 0, (acl_text) ? acl_text : "", 0);
      break;
    case S_IFDIR:
      // Get any acl on the xattr.
      if (solaris_save_xattr_acl(jcr, xattr_data, attrfd, attrname, &acl_text)
          != BxattrExitCode::kSuccess)
        goto bail_out;

      // See if this is the toplevel_hidden_dir being saved.
      if (toplevel_hidden_dir) {
        /* Save the data for later storage when we encounter a real xattr.
         * We store the data in the xattr_data->content buffer
         * and flush that just before sending out the first real xattr.
         * Encode the stat struct into an ASCII representation and jump
         * out of the function. */
        EncodeStat(attribs, &st, sizeof(st), 0, stream);
        cnt = Bsnprintf(buffer, sizeof(buffer), "%s%c%s%c%s%c", target_attrname,
                        0, attribs, 0, (acl_text) ? acl_text : "", 0);
        PmMemcpy(xattr_data->content, buffer, cnt);
        xattr_data->content_length = cnt;
        goto bail_out;
      } else {
        /* The current implementation of xattr on Solaris doesn't support this,
         * but if it ever does we are prepared.
         * Encode the stat struct into an ASCII representation. */
        EncodeStat(attribs, &st, sizeof(st), 0, stream);
        cnt = Bsnprintf(buffer, sizeof(buffer), "%s%c%s%c%s%c", target_attrname,
                        0, attribs, 0, (acl_text) ? acl_text : "", 0);
      }
      break;
    case S_IFREG:
      // If this is a hardlinked file check the inode cache for a hit.
      if (st.st_nlink > 1) {
        // See if the cache already knows this inode number.
        if ((xlce = find_xattr_link_cache_entry(xattr_data, st.st_ino))
            != NULL) {
          /* Generate a xattr encoding with the reference to the target in
           * there. */
          EncodeStat(attribs, &st, sizeof(st), st.st_ino, stream);
          cnt = Bsnprintf(buffer, sizeof(buffer), "%s%c%s%c%s%c",
                          target_attrname, 0, attribs, 0, xlce->target, 0);
          PmMemcpy(xattr_data->content, buffer, cnt);
          xattr_data->content_length = cnt;
          retval = SendXattrStream(jcr, xattr_data, stream);

          /* For a hard linked file we are ready now, no need to recursively
           * save the attributes. */
          goto bail_out;
        }

        /* Store this hard linked file in the cache.
         * Store the name relative to the top level xattr space. */
        add_xattr_link_cache_entry(xattr_data, st.st_ino, target_attrname + 1);
      }

      // Get any acl on the xattr.
      if (solaris_save_xattr_acl(jcr, xattr_data, attrfd, attrname, &acl_text)
          != BxattrExitCode::kSuccess) {
        goto bail_out;
      }

      // Encode the stat struct into an ASCII representation.
      EncodeStat(attribs, &st, sizeof(st), 0, stream);
      cnt = Bsnprintf(buffer, sizeof(buffer), "%s%c%s%c%s%c", target_attrname,
                      0, attribs, 0, (acl_text) ? acl_text : "", 0);

      // Open the extended or extensible attribute file.
      if ((attrfd = openat(fd, attrname, O_RDONLY)) < 0) {
        BErrNo be;

        switch (errno) {
          case ENOENT:
            retval = BxattrExitCode::kSuccess;
            goto bail_out;
          default:
            Mmsg3(jcr->errmsg,
                  T_("Unable to open xattr %s on \"%s\": ERR=%s\n"),
                  target_attrname, xattr_data->last_fname, be.bstrerror());
            Dmsg3(100, "openat of xattr %s on \"%s\" failed: ERR=%s\n",
                  target_attrname, xattr_data->last_fname, be.bstrerror());
            goto bail_out;
        }
      }
      break;
    case S_IFLNK:
      /* The current implementation of xattr on Solaris doesn't support this,
       * but if it ever does we are prepared. Encode the stat struct into an
       * ASCII representation. */
      if (readlink(attrname, link_source, sizeof(link_source)) < 0) {
        BErrNo be;

        switch (errno) {
          case ENOENT:
            retval = BxattrExitCode::kSuccess;
            goto bail_out;
          default:
            Mmsg3(jcr->errmsg,
                  T_("Unable to read symlin %s on \"%s\": ERR=%s\n"),
                  target_attrname, xattr_data->last_fname, be.bstrerror());
            Dmsg3(100, "readlink of xattr %s on \"%s\" failed: ERR=%s\n",
                  target_attrname, xattr_data->last_fname, be.bstrerror());
            goto bail_out;
        }
      }

      // Generate a xattr encoding with the reference to the target in there.
      EncodeStat(attribs, &st, sizeof(st), st.st_ino, stream);
      cnt = Bsnprintf(buffer, sizeof(buffer), "%s%c%s%c%s%c", target_attrname,
                      0, attribs, 0, link_source, 0);
      PmMemcpy(xattr_data->content, buffer, cnt);
      xattr_data->content_length = cnt;
      retval = SendXattrStream(jcr, xattr_data, stream);

      if (retval == BxattrExitCode::kSuccess) { xattr_data->nr_saved++; }

      /* For a soft linked file we are ready now, no need to recursively save
       * the attributes. */
      goto bail_out;
    default:
      goto bail_out;
  }

  /* See if this is the first real xattr being saved.
   * If it is save the toplevel_hidden_dir attributes first.
   * This is easy as its stored already in the
   * xattr_data->content buffer. */
  if (xattr_data->nr_saved == 0) {
    retval = SendXattrStream(jcr, xattr_data, STREAM_XATTR_SOLARIS);
    if (retval != BxattrExitCode::kSuccess) { goto bail_out; }
    xattr_data->nr_saved++;
  }

  PmMemcpy(xattr_data->content, buffer, cnt);
  xattr_data->content_length = cnt;

  // Only dump the content of regular files.
  switch (st.st_mode & S_IFMT) {
    case S_IFREG:
      if (st.st_size > 0) {
        // Protect ourself against things getting out of hand.
        if (st.st_size >= MAX_XATTR_STREAM) {
          Mmsg2(jcr->errmsg,
                T_("Xattr stream on file \"%s\" exceeds maximum size of %d "
                   "bytes\n"),
                xattr_data->last_fname, MAX_XATTR_STREAM);
          goto bail_out;
        }

        while ((cnt = read(attrfd, buffer, sizeof(buffer))) > 0) {
          xattr_data->content.check_size(xattr_data->content_length + cnt);
          memcpy(xattr_data->content.c_str() + xattr_data->content_length,
                 buffer, cnt);
          xattr_data->content_length += cnt;
        }

        if (cnt < 0) {
          Mmsg2(jcr->errmsg,
                T_("Unable to read content of xattr %s on file \"%s\"\n"),
                target_attrname, xattr_data->last_fname);
          Dmsg2(100, "read of data from xattr %s on \"%s\" failed\n",
                target_attrname, xattr_data->last_fname);
          goto bail_out;
        }
      }
      break;

    default:
      break;
  }

  // We build a new xattr stream send it to the SD.
  retval = SendXattrStream(jcr, xattr_data, stream);
  if (retval != BxattrExitCode::kSuccess) { goto bail_out; }
  xattr_data->nr_saved++;

  /* Recursivly call solaris_save_extended_attributes for archiving the
   * attributes available on this extended attribute. */
  retval = solaris_save_xattrs(jcr, xattr_data, xattr_namespace, attrname);

  /* The recursive call could change our working dir so change back to the
   * wanted workdir. */
  if (fchdir(fd) < 0) {
    BErrNo be;

    switch (errno) {
      case ENOENT:
        retval = BxattrExitCode::kSuccess;
        goto bail_out;
      default:
        Mmsg2(jcr->errmsg,
              T_("Unable to chdir to xattr space of file \"%s\": ERR=%s\n"),
              xattr_data->last_fname, be.bstrerror());
        Dmsg3(100,
              "Unable to fchdir to xattr space of file \"%s\" using fd %d: "
              "ERR=%s\n",
              xattr_data->last_fname, fd, be.bstrerror());
        goto bail_out;
    }
  }

bail_out:
  if (acl_text != NULL) { free(acl_text); }
  if (attrfd != -1) { close(attrfd); }
  return retval;
}

static BxattrExitCode solaris_save_xattrs(JobControlRecord* jcr,
                                          XattrBuildData* xattr_data,
                                          const char* xattr_namespace,
                                          const char* attr_parent)
{
  const char* name;
  int fd, filefd = -1, attrdirfd = -1;
  DIR* dirp;
  struct dirent* dp;
  char current_xattr_namespace[PATH_MAX];
  BxattrExitCode retval = BxattrExitCode::kError;

  /* Determine what argument to use. Use attr_parent when set
   * (recursive call) or xattr_data->last_fname for first call. Also save
   * the current depth of the xattr_space we are in. */
  if (attr_parent) {
    name = attr_parent;
    if (xattr_namespace) {
      Bsnprintf(current_xattr_namespace, sizeof(current_xattr_namespace),
                "%s%s/", xattr_namespace, attr_parent);
    } else {
      bstrncpy(current_xattr_namespace, "/", sizeof(current_xattr_namespace));
    }
  } else {
    name = xattr_data->last_fname;
    bstrncpy(current_xattr_namespace, "/", sizeof(current_xattr_namespace));
  }

  // Open the file on which to save the xattrs read-only.
  if ((filefd = open(name, O_RDONLY | O_NONBLOCK)) < 0) {
    BErrNo be;

    switch (errno) {
      case ENOENT:
        retval = BxattrExitCode::kSuccess;
        goto bail_out;
      default:
        Mmsg2(jcr->errmsg, T_("Unable to open file \"%s\": ERR=%s\n"),
              xattr_data->last_fname, be.bstrerror());
        Dmsg2(100, "Unable to open file \"%s\": ERR=%s\n",
              xattr_data->last_fname, be.bstrerror());
        goto bail_out;
    }
  }

  // Open the xattr naming space.
  if ((attrdirfd = openat(filefd, ".", O_RDONLY | O_XATTR)) < 0) {
    BErrNo be;

    switch (errno) {
      case EINVAL:
        /* Gentile way of the system saying this type of xattr layering is not
         * supported. Which is not problem we just forget about this this xattr.
         * But as this is not an error we return a positive return value. */
        retval = BxattrExitCode::kSuccess;
        goto bail_out;
      case ENOENT:
        retval = BxattrExitCode::kSuccess;
        goto bail_out;
      default:
        Mmsg3(jcr->errmsg,
              T_("Unable to open xattr space %s on file \"%s\": ERR=%s\n"),
              name, xattr_data->last_fname, be.bstrerror());
        Dmsg3(100, "Unable to open xattr space %s on file \"%s\": ERR=%s\n",
              name, xattr_data->last_fname, be.bstrerror());
        goto bail_out;
    }
  }

  /* We need to change into the attribute directory to determine if each of the
   * attributes should be saved. */
  if (fchdir(attrdirfd) < 0) {
    BErrNo be;

    Mmsg2(jcr->errmsg,
          T_("Unable to chdir to xattr space on file \"%s\": ERR=%s\n"),
          xattr_data->last_fname, be.bstrerror());
    Dmsg3(
        100,
        "Unable to fchdir to xattr space on file \"%s\" using fd %d: ERR=%s\n",
        xattr_data->last_fname, attrdirfd, be.bstrerror());
    goto bail_out;
  }

  /* Save the data of the toplevel xattr hidden_dir. We save this one before
   * anything else because the readdir returns "." entry after the extensible
   * attr entry. And as we want this entry before anything else we better just
   * save its data. */
  if (!attr_parent)
    solaris_save_xattr(jcr, xattr_data, attrdirfd, current_xattr_namespace, ".",
                       true, STREAM_XATTR_SOLARIS);

  if ((fd = dup(attrdirfd)) == -1 || (dirp = fdopendir(fd)) == (DIR*)NULL) {
    BErrNo be;

    Mmsg2(jcr->errmsg,
          T_("Unable to list the xattr space on file \"%s\": ERR=%s\n"),
          xattr_data->last_fname, be.bstrerror());
    Dmsg3(
        100,
        "Unable to fdopendir xattr space on file \"%s\" using fd %d: ERR=%s\n",
        xattr_data->last_fname, fd, be.bstrerror());

    goto bail_out;
  }

  // Walk the namespace.
  while ((dp = readdir(dirp)) != NULL) {
    // Skip only the toplevel . dir.
    if (!attr_parent && bstrcmp(dp->d_name, ".")) continue;

    // Skip all .. directories
    if (bstrcmp(dp->d_name, "..")) continue;

    Dmsg3(400, "processing extended attribute %s%s on file \"%s\"\n",
          current_xattr_namespace, dp->d_name, xattr_data->last_fname);

#    if __has_include(<sys/nvpair.h>) && defined(_PC_SATTR_ENABLED)
    // We are not interested in read-only extensible attributes.
    if (bstrcmp(dp->d_name, VIEW_READONLY)) {
      Dmsg3(400,
            "Skipping readonly extensible attributes %s%s on file \"%s\"\n",
            current_xattr_namespace, dp->d_name, xattr_data->last_fname);

      continue;
    }

    /* We are only interested in read-write extensible attributes
     * when they contain non-transient values. */
    if (bstrcmp(dp->d_name, VIEW_READWRITE)) {
      /* Determine if there are non-transient system attributes at the toplevel.
       * We need to provide a fd to the open file. */
      if (!SolarisHasNonTransientExtensibleAttributes(filefd)) {
        Dmsg3(400,
              "Skipping transient extensible attributes %s%s on file \"%s\"\n",
              current_xattr_namespace, dp->d_name, xattr_data->last_fname);
        continue;
      }

      // Save the xattr.
      solaris_save_xattr(jcr, xattr_data, attrdirfd, current_xattr_namespace,
                         dp->d_name, false, STREAM_XATTR_SOLARIS_SYS);
      continue;
    }
#    endif /* __has_include(<sys/nvpair.h>) && _PC_SATTR_ENABLED */

    // Save the xattr.
    solaris_save_xattr(jcr, xattr_data, attrdirfd, current_xattr_namespace,
                       dp->d_name, false, STREAM_XATTR_SOLARIS);
  }

  closedir(dirp);
  retval = BxattrExitCode::kSuccess;

bail_out:
  if (attrdirfd != -1) close(attrdirfd);
  if (filefd != -1) close(filefd);
  return retval;
}

#    ifdef HAVE_ACL
static BxattrExitCode solaris_restore_xattr_acl(JobControlRecord* jcr,
                                                XattrData* xattr_data,
                                                int fd,
                                                const char* attrname,
                                                char* acl_text)
{
#      ifdef HAVE_EXTENDED_ACL
  int error;
  acl_t* aclp = NULL;

  if ((error = acl_fromtext(acl_text, &aclp)) != 0) {
    Mmsg1(jcr->errmsg, T_("Unable to convert acl from text on file \"%s\"\n"),
          xattr_data->last_fname);
    return BxattrExitCode::kError;
  }

  if ((fd != -1 && facl_set(fd, aclp) != 0) || acl_set(attrname, aclp) != 0) {
    BErrNo be;

    Mmsg3(jcr->errmsg,
          T_("Unable to restore acl of xattr %s on file \"%s\": ERR=%s\n"),
          attrname, xattr_data->last_fname, be.bstrerror());
    Dmsg3(100, "Unable to restore acl of xattr %s on file \"%s\": ERR=%s\n",
          attrname, xattr_data->last_fname, be.bstrerror());
    return BxattrExitCode::kError;
  }

  if (aclp) { acl_free(aclp); }
  return BxattrExitCode::kSuccess;

#      else /* HAVE_EXTENDED_ACL */
  int n;
  aclent_t* acls = NULL;

  acls = aclfromtext(acl_text, &n);
  if (!acls) {
    if ((fd != -1 && facl(fd, SETACL, n, acls) != 0)
        || acl(attrname, SETACL, n, acls) != 0) {
      BErrNo be;

      Mmsg3(jcr->errmsg,
            T_("Unable to restore acl of xattr %s on file \"%s\": ERR=%s\n"),
            attrname, xattr_data->last_fname, be.bstrerror());
      Dmsg3(100, "Unable to restore acl of xattr %s on file \"%s\": ERR=%s\n",
            attrname, xattr_data->last_fname, be.bstrerror());
      return BxattrExitCode::kError;
    }
  }

  if (acls) { free(acls); }
  return BxattrExitCode::kSuccess;

#      endif /* HAVE_EXTENDED_ACL */
}
#    endif   /* HAVE_ACL */

static BxattrExitCode solaris_restore_xattrs(JobControlRecord* jcr,
                                             XattrData* xattr_data,
                                             bool is_extensible,
                                             char* content,
                                             uint32_t content_length)

{
  int fd, filefd = -1, attrdirfd = -1, attrfd = -1;
  int used_bytes, cnt;
  char *bp, *target_attrname, *attribs;
  char* linked_target = NULL;
  char* acl_text = NULL;
  char* data = NULL;
  int32_t inum;
  struct stat st;
  struct timeval times[2];
  BxattrExitCode retval = BxattrExitCode::kError;

  // Parse the xattr stream. First the part that is the same for all xattrs.
  used_bytes = 0;

  /* The name of the target xattr has a leading / we are not interested
   * in that so skip it when decoding the string. We always start a the /
   * of the xattr space anyway. */
  target_attrname = content + 1;
  if ((bp = strchr(target_attrname, '\0')) == (char*)NULL
      || (used_bytes = (bp - content)) >= (int32_t)(content_length - 1)) {
    goto parse_error;
  }
  attribs = ++bp;

  // Open the file on which to restore the xattrs read-only.
  if ((filefd = open(xattr_data->last_fname, O_RDONLY | O_NONBLOCK)) < 0) {
    BErrNo be;

    Mmsg2(jcr->errmsg, T_("Unable to open file \"%s\": ERR=%s\n"),
          xattr_data->last_fname, be.bstrerror());
    Dmsg2(100, "Unable to open file \"%s\": ERR=%s\n", xattr_data->last_fname,
          be.bstrerror());
    goto bail_out;
  }

  // Open the xattr naming space and make it the current working dir.
  if ((attrdirfd = openat(filefd, ".", O_RDONLY | O_XATTR)) < 0) {
    BErrNo be;

    Mmsg2(jcr->errmsg,
          T_("Unable to open xattr space on file \"%s\": ERR=%s\n"),
          xattr_data->last_fname, be.bstrerror());
    Dmsg2(100, "Unable to open xattr space on file \"%s\": ERR=%s\n",
          xattr_data->last_fname, be.bstrerror());
    goto bail_out;
  }

  if (fchdir(attrdirfd) < 0) {
    BErrNo be;

    Mmsg2(jcr->errmsg,
          T_("Unable to chdir to xattr space on file \"%s\": ERR=%s\n"),
          xattr_data->last_fname, be.bstrerror());
    Dmsg3(
        100,
        "Unable to fchdir to xattr space on file \"%s\" using fd %d: ERR=%s\n",
        xattr_data->last_fname, attrdirfd, be.bstrerror());
    goto bail_out;
  }

  /* Try to open the correct xattr subdir based on the target_attrname given.
   * e.g. check if its a subdir attrname. Each / in the string makes us go
   * one level deeper. */
  while ((bp = strchr(target_attrname, '/')) != (char*)NULL) {
    *bp = '\0';

    if ((fd = open(target_attrname, O_RDONLY | O_NONBLOCK)) < 0) {
      BErrNo be;

      Mmsg3(jcr->errmsg, T_("Unable to open xattr %s on file \"%s\": ERR=%s\n"),
            target_attrname, xattr_data->last_fname, be.bstrerror());
      Dmsg3(100, "Unable to open xattr %s on file \"%s\": ERR=%s\n",
            target_attrname, xattr_data->last_fname, be.bstrerror());
      goto bail_out;
    }

    close(filefd);
    filefd = fd;

    // Open the xattr naming space.
    if ((fd = openat(filefd, ".", O_RDONLY | O_XATTR)) < 0) {
      BErrNo be;

      Mmsg3(jcr->errmsg,
            T_("Unable to open xattr space %s on file \"%s\": ERR=%s\n"),
            target_attrname, xattr_data->last_fname, be.bstrerror());
      Dmsg3(100, "Unable to open xattr space %s on file \"%s\": ERR=%s\n",
            target_attrname, xattr_data->last_fname, be.bstrerror());
      goto bail_out;
    }

    close(attrdirfd);
    attrdirfd = fd;

    // Make the xattr space our current workingdir.
    if (fchdir(attrdirfd) < 0) {
      BErrNo be;

      Mmsg3(jcr->errmsg,
            T_("Unable to chdir to xattr space %s on file \"%s\": ERR=%s\n"),
            target_attrname, xattr_data->last_fname, be.bstrerror());
      Dmsg4(100,
            "Unable to fchdir to xattr space %s on file \"%s\" using fd %d: "
            "ERR=%s\n",
            target_attrname, xattr_data->last_fname, attrdirfd, be.bstrerror());
      goto bail_out;
    }

    target_attrname = ++bp;
  }

  // Decode the attributes from the stream.
  DecodeStat(attribs, &st, sizeof(st), &inum);

  // Decode the next field (acl_text).
  if ((bp = strchr(attribs, '\0')) == (char*)NULL
      || (used_bytes = (bp - content)) >= (int32_t)(content_length - 1)) {
    goto parse_error;
  }
  acl_text = ++bp;

  /* Based on the filetype perform the correct action. We support most filetypes
   * here, more then the actual implementation on Solaris supports so some code
   * may never get executed due to limitations in the implementation. */
  switch (st.st_mode & S_IFMT) {
    case S_IFIFO:
      /* The current implementation of xattr on Solaris doesn't support this,
       * but if it ever does we are prepared. */
      unlinkat(attrdirfd, target_attrname, 0);
      if (mkfifo(target_attrname, st.st_mode) < 0) {
        BErrNo be;

        Mmsg3(jcr->errmsg,
              T_("Unable to mkfifo xattr %s on file \"%s\": ERR=%s\n"),
              target_attrname, xattr_data->last_fname, be.bstrerror());
        Dmsg3(100, "Unable to mkfifo xattr %s on file \"%s\": ERR=%s\n",
              target_attrname, xattr_data->last_fname, be.bstrerror());
        goto bail_out;
      }
      break;
    case S_IFCHR:
    case S_IFBLK:
      /* The current implementation of xattr on Solaris doesn't support this,
       * but if it ever does we are prepared. */
      unlinkat(attrdirfd, target_attrname, 0);
      if (mknod(target_attrname, st.st_mode, st.st_rdev) < 0) {
        BErrNo be;

        Mmsg3(jcr->errmsg,
              T_("Unable to mknod xattr %s on file \"%s\": ERR=%s\n"),
              target_attrname, xattr_data->last_fname, be.bstrerror());
        Dmsg3(100, "Unable to mknod xattr %s on file \"%s\": ERR=%s\n",
              target_attrname, xattr_data->last_fname, be.bstrerror());
        goto bail_out;
      }
      break;
    case S_IFDIR:
      /* If its not the hidden_dir create the entry.
       * The current implementation of xattr on Solaris doesn't support this,
       * but if it ever does we are prepared. */
      if (!bstrcmp(target_attrname, ".")) {
        unlinkat(attrdirfd, target_attrname, AT_REMOVEDIR);
        if (mkdir(target_attrname, st.st_mode) < 0) {
          BErrNo be;

          Jmsg3(jcr, M_WARNING, 0,
                T_("Unable to mkdir xattr %s on file \"%s\": ERR=%s\n"),
                target_attrname, xattr_data->last_fname, be.bstrerror());
          Dmsg3(100, "Unable to mkdir xattr %s on file \"%s\": ERR=%s\n",
                target_attrname, xattr_data->last_fname, be.bstrerror());
          goto bail_out;
        }
      }
      break;
    case S_IFREG:
      // See if this is a hard linked file. e.g. inum != 0
      if (inum != 0) {
        linked_target = bp;

        unlinkat(attrdirfd, target_attrname, 0);
        if (link(linked_target, target_attrname) < 0) {
          BErrNo be;

          Mmsg4(jcr->errmsg,
                T_("Unable to link xattr %s to %s on file \"%s\": ERR=%s\n"),
                target_attrname, linked_target, xattr_data->last_fname,
                be.bstrerror());
          Dmsg4(100, "Unable to link xattr %s to %s on file \"%s\": ERR=%s\n",
                target_attrname, linked_target, xattr_data->last_fname,
                be.bstrerror());
          goto bail_out;
        }

        // Successfully restored xattr.
        retval = BxattrExitCode::kSuccess;
        goto bail_out;
      } else {
        if ((bp = strchr(acl_text, '\0')) == (char*)NULL
            || (used_bytes = (bp - content)) >= (int32_t)content_length) {
          goto parse_error;
        }

        if (used_bytes < (int32_t)(content_length - 1)) data = ++bp;

        // Restore the actual xattr.
        if (!is_extensible) { unlinkat(attrdirfd, target_attrname, 0); }

        if ((attrfd = openat(attrdirfd, target_attrname,
                             O_RDWR | O_CREAT | O_TRUNC, st.st_mode))
            < 0) {
          BErrNo be;

          Mmsg3(jcr->errmsg,
                T_("Unable to open xattr %s on file \"%s\": ERR=%s\n"),
                target_attrname, xattr_data->last_fname, be.bstrerror());
          Dmsg3(100, "Unable to open xattr %s on file \"%s\": ERR=%s\n",
                target_attrname, xattr_data->last_fname, be.bstrerror());
          goto bail_out;
        }
      }

      // Restore the actual data.
      if (st.st_size > 0) {
        used_bytes = (data - content);
        cnt = content_length - used_bytes;

        /* Do a sanity check, the st.st_size should be the same as the number of
         * bytes we have available as data of the stream. */
        if (cnt != st.st_size) {
          Mmsg2(jcr->errmsg,
                T_("Unable to restore data of xattr %s on file \"%s\": Not all "
                   "data available in xattr stream\n"),
                target_attrname, xattr_data->last_fname);
          Dmsg2(100,
                "Unable to restore data of xattr %s on file \"%s\": Not all "
                "data available in xattr stream\n",
                target_attrname, xattr_data->last_fname);
          goto bail_out;
        }

        while (cnt > 0) {
          cnt = write(attrfd, data, cnt);
          if (cnt < 0) {
            BErrNo be;

            Mmsg3(jcr->errmsg,
                  T_("Unable to restore data of xattr %s on file \"%s\": "
                     "ERR=%s\n"),
                  target_attrname, xattr_data->last_fname, be.bstrerror());
            Dmsg3(100,
                  "Unable to restore data of xattr %s on file \"%s\": ERR=%s\n",
                  target_attrname, xattr_data->last_fname, be.bstrerror());
            goto bail_out;
          }

          used_bytes += cnt;
          data += cnt;
          cnt = content_length - used_bytes;
        }
      }
      break;
    case S_IFLNK:
      /* The current implementation of xattr on Solaris doesn't support this,
       * but if it ever does we are prepared. */
      linked_target = bp;

      if (symlink(linked_target, target_attrname) < 0) {
        BErrNo be;

        Mmsg4(jcr->errmsg,
              T_("Unable to symlink xattr %s to %s on file \"%s\": ERR=%s\n"),
              target_attrname, linked_target, xattr_data->last_fname,
              be.bstrerror());
        Dmsg4(100, "Unable to symlink xattr %s to %s on file \"%s\": ERR=%s\n",
              target_attrname, linked_target, xattr_data->last_fname,
              be.bstrerror());
        goto bail_out;
      }

      // Successfully restored xattr.
      retval = BxattrExitCode::kSuccess;
      goto bail_out;
    default:
      goto bail_out;
  }

  // Restore owner and acl for non extensible attributes.
  if (!is_extensible) {
    if (fchownat(attrdirfd, target_attrname, st.st_uid, st.st_gid,
                 AT_SYMLINK_NOFOLLOW)
        < 0) {
      BErrNo be;

      switch (errno) {
        case EINVAL:
          /* Gentile way of the system saying this type of xattr layering is not
           * supported. But as this is not an error we return a positive return
           * value. */
          retval = BxattrExitCode::kSuccess;
          break;
        case ENOENT:
          retval = BxattrExitCode::kSuccess;
          break;
        default:
          Mmsg3(jcr->errmsg,
                T_("Unable to restore owner of xattr %s on file \"%s\": "
                   "ERR=%s\n"),
                target_attrname, xattr_data->last_fname, be.bstrerror());
          Dmsg3(100,
                "Unable to restore owner of xattr %s on file \"%s\": ERR=%s\n",
                target_attrname, xattr_data->last_fname, be.bstrerror());
      }
      goto bail_out;
    }
  }

#    ifdef HAVE_ACL
  if (acl_text && *acl_text)
    if (solaris_restore_xattr_acl(jcr, xattr_data, attrfd, target_attrname,
                                  acl_text)
        != BxattrExitCode::kSuccess)
      goto bail_out;
#    endif /* HAVE_ACL */

  /* For a non extensible attribute restore access and modification time on the
   * xattr. */
  if (!is_extensible) {
    times[0].tv_sec = st.st_atime;
    times[0].tv_usec = 0;
    times[1].tv_sec = st.st_mtime;
    times[1].tv_usec = 0;

    if (futimesat(attrdirfd, target_attrname, times) < 0) {
      BErrNo be;

      Mmsg3(jcr->errmsg,
            T_("Unable to restore filetimes of xattr %s on file \"%s\": "
               "ERR=%s\n"),
            target_attrname, xattr_data->last_fname, be.bstrerror());
      Dmsg3(100,
            "Unable to restore filetimes of xattr %s on file \"%s\": ERR=%s\n",
            target_attrname, xattr_data->last_fname, be.bstrerror());
      goto bail_out;
    }
  }

  // Successfully restored xattr.
  retval = BxattrExitCode::kSuccess;
  goto bail_out;

parse_error:
  Mmsg1(
      jcr->errmsg,
      T_("Illegal xattr stream, failed to parse xattr stream on file \"%s\"\n"),
      xattr_data->last_fname);
  Dmsg1(100,
        "Illegal xattr stream, failed to parse xattr stream on file \"%s\"\n",
        xattr_data->last_fname);

bail_out:
  if (attrfd != -1) { close(attrfd); }
  if (attrdirfd != -1) { close(attrdirfd); }
  if (filefd != -1) { close(filefd); }
  return retval;
}

static BxattrExitCode solaris_build_xattr_streams(JobControlRecord* jcr,
                                                  XattrBuildData* xattr_data,
                                                  FindFilesPacket* ff_pkt)
{
  char cwd[PATH_MAX];
  BxattrExitCode retval = BxattrExitCode::kSuccess;

  /* First see if extended attributes or extensible attributes are present.
   * If not just pretend things went ok. */
  if (pathconf(xattr_data->last_fname, _PC_XATTR_EXISTS) > 0) {
    xattr_data->nr_saved = 0;

    /* As we change the cwd in the save function save the current cwd
     * for restore after return from the solaris_save_xattrs function. */
    getcwd(cwd, sizeof(cwd));
    retval = solaris_save_xattrs(jcr, xattr_data, NULL, NULL);
    chdir(cwd);
    if (xattr_data->link_cache) { DropXattrLinkCache(xattr_data); }
  }
  return retval;
}

static BxattrExitCode solaris_parse_xattr_streams(JobControlRecord* jcr,
                                                  XattrData* xattr_data,
                                                  int stream,
                                                  char* content,
                                                  uint32_t content_length)
{
  char cwd[PATH_MAX];
  bool is_extensible = false;
  BxattrExitCode retval = BxattrExitCode::kError;

  // First make sure we can restore xattr on the filesystem.
  switch (stream) {
#    if __has_include(<sys/nvpair.h>) && defined(_PC_SATTR_ENABLED)
    case STREAM_XATTR_SOLARIS_SYS:
      if (pathconf(xattr_data->last_fname, _PC_SATTR_ENABLED) <= 0) {
        Mmsg1(jcr->errmsg,
              T_("Failed to restore extensible attributes on file \"%s\"\n"),
              xattr_data->last_fname);
        Dmsg1(100,
              "Unable to restore extensible attributes on file \"%s\", "
              "filesystem doesn't support this\n",
              xattr_data->last_fname);
        goto bail_out;
      }

      is_extensible = true;
      break;
#    endif
    case STREAM_XATTR_SOLARIS:
      if (pathconf(xattr_data->last_fname, _PC_XATTR_ENABLED) <= 0) {
        Mmsg1(jcr->errmsg,
              T_("Failed to restore extended attributes on file \"%s\"\n"),
              xattr_data->last_fname);
        Dmsg1(100,
              "Unable to restore extended attributes on file \"%s\", "
              "filesystem doesn't support this\n",
              xattr_data->last_fname);
        goto bail_out;
      }
      break;
    default:
      goto bail_out;
  }

  /* As we change the cwd in the restore function save the current cwd
   * for restore after return from the solaris_restore_xattrs function. */
  getcwd(cwd, sizeof(cwd));
  retval = solaris_restore_xattrs(jcr, xattr_data, is_extensible, content,
                                  content_length);
  chdir(cwd);

bail_out:
  return retval;
}


// Function pointers to the build and parse function to use for these xattrs.
static BxattrExitCode (*os_build_xattr_streams)(JobControlRecord* jcr,
                                                XattrBuildData* xattr_data,
                                                FindFilesPacket* ff_pkt)
    = solaris_build_xattr_streams;
static BxattrExitCode (*os_parse_xattr_streams)(JobControlRecord* jcr,
                                                XattrData* xattr_data,
                                                int stream,
                                                char* content,
                                                uint32_t content_length)
    = solaris_parse_xattr_streams;

#  endif /* defined(HAVE_SUN_OS) */

// Entry points when compiled with support for XATTRs on a supported platform.
BxattrExitCode BuildXattrStreams(JobControlRecord* jcr,
                                 XattrBuildData* xattr_data,
                                 FindFilesPacket* ff_pkt)
{
  /* See if we are changing from one device to another.
   * We save the current device we are scanning and compare
   * it with the current st_dev in the last stat performed on
   * the file we are currently storing. */
  Dmsg0(1000, "BuildXattrStreams(): Enter\n");
  if (xattr_data->first_dev
      || xattr_data->current_dev != ff_pkt->statp.st_dev) {
    xattr_data->flags = BXATTR_FLAG_SAVE_NATIVE;
    xattr_data->first_dev = false;
    xattr_data->current_dev = ff_pkt->statp.st_dev;
  }

  if ((xattr_data->flags & BXATTR_FLAG_SAVE_NATIVE) && os_build_xattr_streams) {
    return os_build_xattr_streams(jcr, xattr_data, ff_pkt);
  } else {
    return BxattrExitCode::kSuccess;
  }
}

BxattrExitCode ParseXattrStreams(JobControlRecord* jcr,
                                 XattrData* xattr_data,
                                 int stream,
                                 char* content,
                                 uint32_t content_length)
{
  int ret;
  struct stat st;
  unsigned int cnt;
  BxattrExitCode retval = BxattrExitCode::kError;

  Dmsg0(1000, "ParseXattrStreams(): Enter\n");
  /* See if we are changing from one device to another.
   * We save the current device we are restoring to and compare
   * it with the current st_dev in the last stat performed on
   * the file we are currently restoring. */
  ret = lstat(xattr_data->last_fname, &st);
  switch (ret) {
    case -1: {
      BErrNo be;

      switch (errno) {
        case ENOENT:
          retval = BxattrExitCode::kSuccess;
          goto bail_out;
        default:
          Mmsg2(jcr->errmsg, T_("Unable to stat file \"%s\": ERR=%s\n"),
                xattr_data->last_fname, be.bstrerror());
          Dmsg2(100, "Unable to stat file \"%s\": ERR=%s\n",
                xattr_data->last_fname, be.bstrerror());
          goto bail_out;
      }
      break;
    }
    case 0:
      break;
  }
  if (xattr_data->first_dev || xattr_data->current_dev != st.st_dev) {
    xattr_data->flags = BXATTR_FLAG_RESTORE_NATIVE;
    xattr_data->first_dev = false;
    xattr_data->current_dev = st.st_dev;
  }

  // See if we are still restoring native xattr to this filesystem.
  if ((xattr_data->flags & BXATTR_FLAG_RESTORE_NATIVE)
      && os_parse_xattr_streams) {
    // See if we can parse this stream, and ifso give it a try.
    for (cnt = 0; cnt < sizeof(os_default_xattr_streams) / sizeof(int); cnt++) {
      if (os_default_xattr_streams[cnt] == stream) {
        retval = os_parse_xattr_streams(jcr, xattr_data, stream, content,
                                        content_length);
        goto bail_out;
      }
    }
  } else {
    /* Increment error count but don't log an error again for the same
     * filesystem. */
    xattr_data->nr_errors++;
    retval = BxattrExitCode::kSuccess;
    goto bail_out;
  }

  // Issue a warning and discard the message. But pretend the restore was ok.
  Jmsg2(
      jcr, M_WARNING, 0,
      T_("Can't restore Extended Attributes of %s - incompatible xattr stream "
         "encountered - %d\n"),
      xattr_data->last_fname, stream);

bail_out:
  return retval;
}
#endif
