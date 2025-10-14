/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2012 Free Software Foundation Europe e.V.
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
 *   Original written by Preben 'Peppe' Guldberg, December 2004
 *   Major rewrite by Marco van Wieringen, November 2008
 *   Major overhaul by Marco van Wieringen, January 2012
 *   Moved into findlib so it can be used from other programs, May 2012
 */
/**
 * @file
 * Functions to handle ACLs for bareos.
 *
 * Currently we support the following OSes:
 *   - AIX (pre-5.3 and post 5.3 acls, acl_get and aclx_get interface)
 *   - Darwin
 *   - FreeBSD (POSIX and NFSv4/ZFS acls)
 *   - GNU Hurd
 *   - HPUX
 *   - IRIX
 *   - Linux
 *   - Solaris (POSIX and NFSv4/ZFS acls)
 *   - Tru64
 *
 * Next to OS specific acls we support AFS acls using the pioctl interface.
 *
 * We handle two different types of ACLs: access and default ACLS.
 * On most systems that support default ACLs they only apply to directories.
 *
 * On some systems (eg. linux and FreeBSD) we must obtain the two ACLs
 * independently, while others (eg. Solaris) provide both in one call.
 *
 * The Filed saves ACLs in their native format and uses different streams
 * for all different platforms. Currently we only allow ACLs to be restored
 * which were saved in the native format of the platform they are extracted
 * on. Later on we might add conversion functions for mapping from one
 * platform to another or allow restores of systems that use the same
 * native format.
 *
 * Its also interesting to see what the exact format of acl text is on
 * certain platforms and if they use they same encoding we might allow
 * different platform streams to be decoded on another similar platform.
 */

#include "include/bareos.h"
#include "include/filetypes.h"
#include "include/streams.h"
#include "include/jcr.h"
#include "lib/berrno.h"
#include "lib/bsock.h"
#include "find.h"

#if !defined(HAVE_ACL) && !defined(HAVE_AFS_ACL)
/**
 * Entry points when compiled without support for ACLs or on an unsupported
 * platform.
 */
bacl_exit_code BuildAclStreams(JobControlRecord*,
                               AclBuildData*,
                               FindFilesPacket*)
{
  return bacl_exit_fatal;
}

bacl_exit_code parse_acl_streams(JobControlRecord*,
                                 AclData*,
                                 int,
                                 char*,
                                 uint32_t)
{
  return bacl_exit_fatal;
}
#else
// Send an ACL stream to the SD.
bacl_exit_code SendAclStream(JobControlRecord* jcr,
                             AclBuildData* acl_data,
                             int stream)
{
  BareosSocket* sd = jcr->store_bsock;
  POOLMEM* msgsave;
#  ifdef FD_NO_SEND_TEST
  return bacl_exit_ok;
#  endif

  // Sanity check
  if (acl_data->content_length <= 0) { return bacl_exit_ok; }

  // Send header
  if (!sd->fsend("%" PRIu32 " %" PRId32 " 0", jcr->JobFiles, stream)) {
    Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
          sd->bstrerror());
    return bacl_exit_fatal;
  }

  // Send the buffer to the storage daemon
  Dmsg1(400, "Backing up ACL <%s>\n", acl_data->content.c_str());
  msgsave = sd->msg;
  sd->msg = acl_data->content.c_str();
  sd->message_length = acl_data->content_length + 1;
  if (!sd->send()) {
    sd->msg = msgsave;
    sd->message_length = 0;
    Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
          sd->bstrerror());
    return bacl_exit_fatal;
  }

  jcr->JobBytes += sd->message_length;
  sd->msg = msgsave;
  if (!sd->signal(BNET_EOD)) {
    Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
          sd->bstrerror());
    return bacl_exit_fatal;
  }

  Dmsg1(200, "ACL of file: %s successfully backed up!\n", acl_data->last_fname);
  return bacl_exit_ok;
}

// First the native ACLs.
#  if defined(HAVE_ACL)
#    if defined(HAVE_AIX_OS)

#      if defined(HAVE_EXTENDED_ACL)

#        include <sys/access.h>
#        include <sys/acl.h>

static bool AclIsTrivial(struct acl* acl)
{
  return (acl_last(acl) != acl->acl_ext ? false : true);
}

static bool acl_nfs4_is_trivial(nfs4_acl_int_t* acl)
{
  int i;
  int count = acl->aclEntryN;
  nfs4_ace_int_t* ace;

  for (i = 0; i < count; i++) {
    ace = &acl->aclEntry[i];
    if (!((ace->flags & ACE4_ID_SPECIAL) != 0
          && (ace->aceWho.special_whoid == ACE4_WHO_OWNER
              || ace->aceWho.special_whoid == ACE4_WHO_GROUP
              || ace->aceWho.special_whoid == ACE4_WHO_EVERYONE)
          && ace->aceType == ACE4_ACCESS_ALLOWED_ACE_TYPE && ace->aceFlags == 0
          && (ace->aceMask
              & ~(ACE4_READ_DATA | ACE4_LIST_DIRECTORY | ACE4_WRITE_DATA
                  | ACE4_ADD_FILE | ACE4_EXECUTE))
                 == 0)) {
      return false;
    }
  }
  return true;
}

// Define the supported ACL streams for this OS
static int os_access_acl_streams[3]
    = {STREAM_ACL_AIX_TEXT, STREAM_ACL_AIX_AIXC, STREAM_ACL_AIX_NFS4};
static int os_default_acl_streams[1] = {-1};

static bacl_exit_code aix_build_acl_streams(JobControlRecord* jcr,
                                            AclBuildData* acl_data,
                                            FindFilesPacket* ff_pkt)
{
  mode_t mode;
  acl_type_t type;
  size_t aclsize, acltxtsize;
  bacl_exit_code retval = bacl_exit_error;
  POOLMEM* aclbuf = GetPoolMemory(PM_MESSAGE);

  // First see how big the buffers should be.
  memset(&type, 0, sizeof(acl_type_t));
  type.u64 = ACL_ANY;
  if (aclx_get(acl_data->last_fname,
#        if defined(GET_ACL_FOR_LINK)
               GET_ACLINFO_ONLY | GET_ACL_FOR_LINK,
#        else
               GET_ACLINFO_ONLY,
#        endif
               &type, NULL, &aclsize, &mode)
      < 0) {
    BErrNo be;

    switch (errno) {
      case ENOENT:
        retval = bacl_exit_ok;
        goto bail_out;
      case ENOSYS:
        /* If the filesystem reports it doesn't support ACLs we clear the
         * SaveNative flag so we skip ACL saves on all other files on the same
         * filesystem. The SaveNative flag gets set again when we change from
         * one filesystem to another. */
        acl_data->flags.SaveNative = false;
        retval = bacl_exit_ok;
        goto bail_out;
      default:
        Mmsg2(jcr->errmsg, T_("aclx_get error on file \"%s\": ERR=%s\n"),
              acl_data->last_fname, be.bstrerror());
        Dmsg2(100, "aclx_get error file=%s ERR=%s\n", acl_data->last_fname,
              be.bstrerror());
        goto bail_out;
    }
  }

  // Make sure the buffers are big enough.
  aclbuf = CheckPoolMemorySize(aclbuf, aclsize + 1);

  // Retrieve the ACL info.
  if (aclx_get(acl_data->last_fname,
#        if defined(GET_ACL_FOR_LINK)
               GET_ACL_FOR_LINK,
#        else
               0,
#        endif
               &type, aclbuf, &aclsize, &mode)
      < 0) {
    BErrNo be;

    switch (errno) {
      case ENOENT:
        retval = bacl_exit_ok;
        goto bail_out;
      default:
        Mmsg2(jcr->errmsg, T_("aclx_get error on file \"%s\": ERR=%s\n"),
              acl_data->last_fname, be.bstrerror());
        Dmsg2(100, "aclx_get error file=%s ERR=%s\n", acl_data->last_fname,
              be.bstrerror());
        goto bail_out;
    }
  }

  // See if the acl is non trivial.
  switch (type.u64) {
    case ACL_AIXC:
      if (AclIsTrivial((struct acl*)aclbuf)) {
        retval = bacl_exit_ok;
        goto bail_out;
      }
      break;
    case ACL_NFS4:
      if (acl_nfs4_is_trivial((nfs4_acl_int_t*)aclbuf)) {
        retval = bacl_exit_ok;
        goto bail_out;
      }
      break;
    default:
      Mmsg2(jcr->errmsg,
            T_("Unknown acl type encountered on file \"%s\": %ld\n"),
            acl_data->last_fname, type.u64);
      Dmsg2(100, "Unknown acl type encountered on file \"%s\": %ld\n",
            acl_data->last_fname, type.u64);
      goto bail_out;
  }

  // We have a non-trivial acl lets convert it into some ASCII form.
  acltxtsize = SizeofPoolMemory(acl_data->content);
  if (aclx_printStr(acl_data->content, &acltxtsize, aclbuf, aclsize, type,
                    acl_data->last_fname, 0)
      < 0) {
    switch (errno) {
      case ENOSPC:
        /* Our buffer is not big enough, acltxtsize should be updated with the
         * value the aclx_printStr really need. So we increase the buffer and
         * try again. */
        acl_data->content
            = CheckPoolMemorySize(acl_data->content, acltxtsize + 1);
        if (aclx_printStr(acl_data->content, &acltxtsize, aclbuf, aclsize, type,
                          acl_data->last_fname, 0)
            < 0) {
          Mmsg1(jcr->errmsg,
                T_("Failed to convert acl into text on file \"%s\"\n"),
                acl_data->last_fname);
          Dmsg2(100, "Failed to convert acl into text on file \"%s\": %ld\n",
                acl_data->last_fname, type.u64);
          goto bail_out;
        }
        break;
      default:
        Mmsg1(jcr->errmsg,
              T_("Failed to convert acl into text on file \"%s\"\n"),
              acl_data->last_fname);
        Dmsg2(100, "Failed to convert acl into text on file \"%s\": %ld\n",
              acl_data->last_fname, type.u64);
        goto bail_out;
    }
  }

  acl_data->content_length = strlen(acl_data->content.c_str()) + 1;
  switch (type.u64) {
    case ACL_AIXC:
      retval = SendAclStream(jcr, acl_data, STREAM_ACL_AIX_AIXC);
      break;
    case ACL_NFS4:
      retval = SendAclStream(jcr, acl_data, STREAM_ACL_AIX_NFS4);
      break;
  }

bail_out:
  FreePoolMemory(aclbuf);

  return retval;
}

/**
 * See if a specific type of ACLs are supported on the filesystem
 * the file is located on.
 */
static inline bool aix_query_acl_support(JobControlRecord* jcr,
                                         AclData* acl_data,
                                         uint64_t aclType,
                                         acl_type_t* pacl_type_info)
{
  unsigned int i;
  acl_types_list_t acl_type_list;
  size_t acl_type_list_len = sizeof(acl_types_list_t);

  memset(&acl_type_list, 0, sizeof(acl_type_list));
  if (aclx_gettypes(acl_data->last_fname, &acl_type_list, &acl_type_list_len)) {
    return false;
  }

  for (i = 0; i < acl_type_list.num_entries; i++) {
    if (acl_type_list.entries[i].u64 == aclType) {
      memcpy(pacl_type_info, acl_type_list.entries + i, sizeof(acl_type_t));
      return true;
    }
  }
  return false;
}

static bacl_exit_code aix_parse_acl_streams(JobControlRecord* jcr,
                                            AclData* acl_data,
                                            int stream,
                                            char* content,
                                            uint32_t content_length)
{
  int cnt;
  acl_type_t type;
  size_t aclsize;
  bacl_exit_code retval = bacl_exit_error;
  POOLMEM* aclbuf = GetPoolMemory(PM_MESSAGE);

  switch (stream) {
    case STREAM_ACL_AIX_TEXT:
      // Handle the old stream using the old system call for now.
      if (acl_put(acl_data->last_fname, content, 0) != 0) {
        retval = bacl_exit_error;
        goto bail_out;
      }
      retval = bacl_exit_ok;
      goto bail_out;
    case STREAM_ACL_AIX_AIXC:
      if (!aix_query_acl_support(jcr, acl_data, ACL_AIXC, &type)) {
        Mmsg1(jcr->errmsg,
              T_("Trying to restore POSIX acl on file \"%s\" on filesystem "
                 "without AIXC acl support\n"),
              acl_data->last_fname);
        goto bail_out;
      }
      break;
    case STREAM_ACL_AIX_NFS4:
      if (!aix_query_acl_support(jcr, acl_data, ACL_NFS4, &type)) {
        Mmsg1(jcr->errmsg,
              T_("Trying to restore NFSv4 acl on file \"%s\" on filesystem "
                 "without NFS4 acl support\n"),
              acl_data->last_fname);
        goto bail_out;
      }
      break;
    default:
      goto bail_out;
  } /* end switch (stream) */

  /* Set the acl buffer to an initial size. For now we set it
   * to the same size as the ASCII representation. */
  aclbuf = CheckPoolMemorySize(aclbuf, content_length);
  aclsize = content_length;
  if (aclx_scanStr(content, aclbuf, &aclsize, type) < 0) {
    BErrNo be;

    switch (errno) {
      case ENOSPC:
        /* The buffer isn't big enough. The man page doesn't say that aclsize
         * is updated to the needed size as what is done with aclx_printStr.
         * So for now we try to increase the buffer a maximum of 3 times
         * and retry the conversion. */
        for (cnt = 0; cnt < 3; cnt++) {
          aclsize = 2 * aclsize;
          aclbuf = CheckPoolMemorySize(aclbuf, aclsize);

          if (aclx_scanStr(content, aclbuf, &aclsize, type) == 0) { break; }

          /* See why we failed this time, ENOSPC retry if max retries not met,
           * otherwise abort. */
          switch (errno) {
            case ENOSPC:
              if (cnt < 3) { continue; }
              // FALLTHROUGH
            default:
              Mmsg2(jcr->errmsg,
                    T_("aclx_scanStr error on file \"%s\": ERR=%s\n"),
                    acl_data->last_fname, be.bstrerror(errno));
              Dmsg2(100, "aclx_scanStr error file=%s ERR=%s\n",
                    acl_data->last_fname, be.bstrerror());
              goto bail_out;
          }
        }
        break;
      default:
        Mmsg2(jcr->errmsg, T_("aclx_scanStr error on file \"%s\": ERR=%s\n"),
              acl_data->last_fname, be.bstrerror());
        Dmsg2(100, "aclx_scanStr error file=%s ERR=%s\n", acl_data->last_fname,
              be.bstrerror());
    }
  }

  if (aclx_put(acl_data->last_fname, SET_ACL, type, aclbuf, aclsize, 0) < 0) {
    BErrNo be;

    switch (errno) {
      case ENOENT:
        retval = bacl_exit_ok;
        goto bail_out;
      case ENOSYS:
        /* If the filesystem reports it doesn't support ACLs we clear the
         * RestoreNative flag so we skip ACL restores on all other files on the
         * same filesystem. The RestoreNative flag gets set again when we change
         * from one filesystem to another. */
        acl_data->flags.RestoreNative = false;
        retval = bacl_exit_ok;
        goto bail_out;
      default:
        Mmsg2(jcr->errmsg, T_("aclx_put error on file \"%s\": ERR=%s\n"),
              acl_data->last_fname, be.bstrerror());
        Dmsg2(100, "aclx_put error file=%s ERR=%s\n", acl_data->last_fname,
              be.bstrerror());
        goto bail_out;
    }
  }

  retval = bacl_exit_ok;

bail_out:
  FreePoolMemory(aclbuf);

  return retval;
}

#      else /* HAVE_EXTENDED_ACL */

#        include <sys/access.h>

// Define the supported ACL streams for this OS
static int os_access_acl_streams[1] = {STREAM_ACL_AIX_TEXT};
static int os_default_acl_streams[1] = {-1};

static bacl_exit_code aix_build_acl_streams(JobControlRecord* jcr,
                                            AclBuildData* acl_data,
                                            FindFilesPacket* ff_pkt)
{
  char* acl_text;

  if ((acl_text = acl_get(acl_data->last_fname)) != NULL) {
    acl_data->content_length = PmStrcpy(acl_data->content, acl_text);
    free(acl_text);
    return SendAclStream(jcr, acl_data, STREAM_ACL_AIX_TEXT);
  }
  return bacl_exit_error;
}

static bacl_exit_code aix_parse_acl_streams(JobControlRecord* jcr,
                                            AclData* acl_data,
                                            int stream,
                                            char* content,
                                            uint32_t content_length)
{
  if (acl_put(acl_data->last_fname, content, 0) != 0) {
    return bacl_exit_error;
  }
  return bacl_exit_ok;
}
#      endif /* HAVE_EXTENDED_ACL */

/**
 * For this OS setup the build and parse function pointer to the OS specific
 * functions.
 */
static bacl_exit_code (*os_build_acl_streams)(JobControlRecord* jcr,
                                              AclBuildData* acl_data,
                                              FindFilesPacket* ff_pkt)
    = aix_build_acl_streams;
static bacl_exit_code (*os_parse_acl_streams)(JobControlRecord* jcr,
                                              AclData* acl_data,
                                              int stream,
                                              char* content,
                                              uint32_t content_length)
    = aix_parse_acl_streams;

#    elif defined(HAVE_DARWIN_OS) || defined(HAVE_FREEBSD_OS) \
        || defined(HAVE_LINUX_OS)

#      include <sys/types.h>

#      if __has_include(<sys/acl.h>)
#        include <sys/acl.h>
#      else
#        error "configure failed to detect availability of sys/acl.h"
#      endif

// On Linux we can get numeric and/or shorted ACLs
#      if defined(HAVE_LINUX_OS)
#        if defined(BACL_WANT_SHORT_ACLS) && defined(BACL_WANT_NUMERIC_IDS)
#          define BACL_ALTERNATE_TEXT (TEXT_ABBREVIATE | TEXT_NUMERIC_IDS)
#        elif defined(BACL_WANT_SHORT_ACLS)
#          define BACL_ALTERNATE_TEXT TEXT_ABBREVIATE
#        elif defined(BACL_WANT_NUMERIC_IDS)
#          define BACL_ALTERNATE_TEXT TEXT_NUMERIC_IDS
#        endif
#        ifdef BACL_ALTERNATE_TEXT
#          include <acl/libacl.h>
#          define acl_to_text(acl, len) \
            (acl_to_any_text((acl), NULL, ',', BACL_ALTERNATE_TEXT))
#        endif
#      endif

// On FreeBSD we can get numeric ACLs
#      if defined(HAVE_FREEBSD_OS)
#        if defined(BACL_WANT_NUMERIC_IDS)
#          define BACL_ALTERNATE_TEXT ACL_TEXT_NUMERIC_IDS
#        endif
#        ifdef BACL_ALTERNATE_TEXT
#          define acl_to_text(acl, len) \
            (acl_to_text_np((acl), (len), BACL_ALTERNATE_TEXT))
#        endif
#      endif

// Some generic functions used by multiple OSes.
static acl_type_t BacToOsAcltype(bacl_type acltype)
{
  acl_type_t ostype;

  switch (acltype) {
    case BACL_TYPE_ACCESS:
      ostype = ACL_TYPE_ACCESS;
      break;
    case BACL_TYPE_DEFAULT:
      ostype = ACL_TYPE_DEFAULT;
      break;
#      ifdef HAVE_ACL_TYPE_NFS4
      // FreeBSD has an additional acl type named ACL_TYPE_NFS4.
    case BACL_TYPE_NFS4:
      ostype = ACL_TYPE_NFS4;
      break;
#      endif
#      ifdef HAVE_ACL_TYPE_DEFAULT_DIR
    case BACL_TYPE_DEFAULT_DIR:
      // TRU64 has an additional acl type named ACL_TYPE_DEFAULT_DIR.
      ostype = ACL_TYPE_DEFAULT_DIR;
      break;
#      endif
#      ifdef HAVE_ACL_TYPE_EXTENDED
    case BACL_TYPE_EXTENDED:
      // MacOSX has an additional acl type named ACL_TYPE_EXTENDED.
      ostype = ACL_TYPE_EXTENDED;
      break;
#      endif
    default:
      /* This should never happen, as the per OS version function only tries acl
       * types supported on a certain platform. */
      ostype = ACL_TYPE_NONE;
      break;
  }
  return ostype;
}

static int AclCountEntries(acl_t acl)
{
  int count = 0;
#      if defined(HAVE_FREEBSD_OS) || defined(HAVE_LINUX_OS)
  acl_entry_t ace;
  int entry_available;

  entry_available = acl_get_entry(acl, ACL_FIRST_ENTRY, &ace);
  while (entry_available == 1) {
    count++;
    entry_available = acl_get_entry(acl, ACL_NEXT_ENTRY, &ace);
  }
#      elif defined(HAVE_DARWIN_OS)
  acl_entry_t ace;
  int entry_available;

  entry_available = acl_get_entry(acl, ACL_FIRST_ENTRY, &ace);
  while (entry_available == 0) {
    count++;
    entry_available = acl_get_entry(acl, ACL_NEXT_ENTRY, &ace);
  }
#      endif
  return count;
}

#      if !defined(HAVE_DARWIN_OS)
/**
 * See if an acl is a trivial one (e.g. just the stat bits encoded as acl.)
 * There is no need to store those acls as we already store the stat bits too.
 */
static bool AclIsTrivial(acl_t acl)
{
  /* acl is trivial if it has only the following entries:
   * "user::",
   * "group::",
   * "other::" */
  acl_entry_t ace;
  acl_tag_t tag;
#        if defined(HAVE_FREEBSD_OS) || defined(HAVE_LINUX_OS)
  int entry_available;

  entry_available = acl_get_entry(acl, ACL_FIRST_ENTRY, &ace);
  while (entry_available == 1) {
    /* Get the tag type of this acl entry.
     * If we fail to get the tagtype we call the acl non-trivial. */
    if (acl_get_tag_type(ace, &tag) < 0) return true;
    /* Anything other the ACL_USER_OBJ, ACL_GROUP_OBJ or ACL_OTHER breaks the
     * spell. */
    if (tag != ACL_USER_OBJ && tag != ACL_GROUP_OBJ && tag != ACL_OTHER)
      return false;
    entry_available = acl_get_entry(acl, ACL_NEXT_ENTRY, &ace);
  }
  return true;
#        endif
}
#      endif

// Generic wrapper around acl_get_file call.
static bacl_exit_code generic_get_acl_from_os(JobControlRecord* jcr,
                                              AclBuildData* acl_data,
                                              bacl_type acltype)
{
  acl_t acl;
  acl_type_t ostype;
  char* acl_text;
  bacl_exit_code retval = bacl_exit_ok;

  ostype = BacToOsAcltype(acltype);
  acl = acl_get_file(acl_data->last_fname, ostype);
  if (acl) {
    /* From observation, IRIX's acl_get_file() seems to return a
     * non-NULL acl with a count field of -1 when a file has no ACL
     * defined, while IRIX's acl_to_text() returns NULL when presented
     * with such an ACL.
     *
     * For all other implmentations we check if there are more then
     * zero entries in the acl returned. */
    if (AclCountEntries(acl) <= 0) { goto bail_out; }

    // Make sure this is not just a trivial ACL.
#      if !defined(HAVE_DARWIN_OS)
    if (acltype == BACL_TYPE_ACCESS && AclIsTrivial(acl)) {
      /* The ACLs simply reflect the (already known) standard permissions
       * So we don't send an ACL stream to the SD. */
      goto bail_out;
    }
#      endif
#      if defined(HAVE_FREEBSD_OS) && defined(_PC_ACL_NFS4)
    if (acltype == BACL_TYPE_NFS4) {
      int trivial;
      if (acl_is_trivial_np(acl, &trivial) == 0) {
        if (trivial == 1) {
          /* The ACLs simply reflect the (already known) standard permissions
           * So we don't send an ACL stream to the SD. */
          goto bail_out;
        }
      }
    }
#      endif

    // Convert the internal acl representation into a text representation.
    if ((acl_text = acl_to_text(acl, NULL)) != NULL) {
      acl_data->content_length = PmStrcpy(acl_data->content, acl_text);
      acl_free(acl);
      acl_free(acl_text);
      return bacl_exit_ok;
    }

    BErrNo be;
    Mmsg2(jcr->errmsg, T_("acl_to_text error on file \"%s\": ERR=%s\n"),
          acl_data->last_fname, be.bstrerror());
    Dmsg2(100, "acl_to_text error file=%s ERR=%s\n", acl_data->last_fname,
          be.bstrerror());

    retval = bacl_exit_error;
    goto bail_out;
  } else {
    BErrNo be;

    // Handle errors gracefully.
    switch (errno) {
#      if defined(BACL_ENOTSUP)
      case BACL_ENOTSUP:
        /* If the filesystem reports it doesn't support ACLs we clear the
         * SaveNative flag so we skip ACL saves on all other files
         * on the same filesystem. The SaveNative flag gets set again
         * when we change from one filesystem to another. */
        acl_data->flags.SaveNative = false;
        goto bail_out;
#      endif
      case ENOENT:
        goto bail_out;
      default:
        /* Some real error */
        Mmsg2(jcr->errmsg, T_("acl_get_file error on file \"%s\": ERR=%s\n"),
              acl_data->last_fname, be.bstrerror());
        Dmsg2(100, "acl_get_file error file=%s ERR=%s\n", acl_data->last_fname,
              be.bstrerror());

        retval = bacl_exit_error;
        goto bail_out;
    }
  }

bail_out:
  if (acl) { acl_free(acl); }
  PmStrcpy(acl_data->content, "");
  acl_data->content_length = 0;
  return retval;
}

// Generic wrapper around acl_set_file call.
static bacl_exit_code generic_set_acl_on_os(JobControlRecord* jcr,
                                            AclData* acl_data,
                                            bacl_type acltype,
                                            char* content,
                                            uint32_t)
{
  acl_t acl;
  acl_type_t ostype;

  // If we get empty default ACLs, clear ACLs now
  ostype = BacToOsAcltype(acltype);
  if (ostype == ACL_TYPE_DEFAULT && strlen(content) == 0) {
    if (acl_delete_def_file(acl_data->last_fname) == 0) { return bacl_exit_ok; }
    BErrNo be;

    switch (errno) {
      case ENOENT:
        return bacl_exit_ok;
#      if defined(BACL_ENOTSUP)
      case BACL_ENOTSUP:
        /* If the filesystem reports it doesn't support ACLs we clear the
         * RestoreNative flag so we skip ACL restores on all other
         * files on the same filesystem. The RestoreNative flag gets
         * set again when we change from one filesystem to another. */
        acl_data->flags.RestoreNative = false;
        Mmsg1(jcr->errmsg,
              T_("acl_delete_def_file error on file \"%s\": filesystem doesn't "
                 "support ACLs\n"),
              acl_data->last_fname);
        return bacl_exit_error;
#      endif
      default:
        Mmsg2(jcr->errmsg,
              T_("acl_delete_def_file error on file \"%s\": ERR=%s\n"),
              acl_data->last_fname, be.bstrerror());
        return bacl_exit_error;
    }
  }

  acl = acl_from_text(content);
  if (acl == NULL) {
    BErrNo be;

    Mmsg2(jcr->errmsg, T_("acl_from_text error on file \"%s\": ERR=%s\n"),
          acl_data->last_fname, be.bstrerror());
    Dmsg3(100, "acl_from_text error acl=%s file=%s ERR=%s\n", content,
          acl_data->last_fname, be.bstrerror());
    return bacl_exit_error;
  }

  /* Only validate POSIX acls the acl_valid interface is only implemented
   * for checking POSIX acls on most platforms. */
  switch (acltype) {
    case BACL_TYPE_NFS4:
      // Skip acl_valid tests on NFSv4 acls.
      break;
    default:
      if (acl_valid(acl) != 0) {
        BErrNo be;

        Mmsg2(jcr->errmsg, T_("acl_valid error on file \"%s\": ERR=%s\n"),
              acl_data->last_fname, be.bstrerror());
        Dmsg3(100, "acl_valid error acl=%s file=%s ERR=%s\n", content,
              acl_data->last_fname, be.bstrerror());
        acl_free(acl);
        return bacl_exit_error;
      }
      break;
  }

  /* Restore the ACLs, but don't complain about links which really should
   * not have attributes, and the file it is linked to may not yet be restored.
   * This is only true for the old acl streams as in the new implementation we
   * don't save acls of symlinks (which cannot have acls anyhow) */
  if (acl_set_file(acl_data->last_fname, ostype, acl) != 0
      && acl_data->filetype != FT_LNK) {
    BErrNo be;

    switch (errno) {
      case ENOENT:
        acl_free(acl);
        return bacl_exit_ok;
#      if defined(BACL_ENOTSUP)
      case BACL_ENOTSUP:
        /* If the filesystem reports it doesn't support ACLs we clear the
         * RestoreNative flag so we skip ACL restores on all other
         * files on the same filesystem. The RestoreNative flag gets
         * set again when we change from one filesystem to another. */
        acl_data->flags.RestoreNative = false;
        Mmsg1(
            jcr->errmsg,
            T_("acl_set_file error on file \"%s\": filesystem doesn't support "
               "ACLs\n"),
            acl_data->last_fname);
        Dmsg2(100,
              "acl_set_file error acl=%s file=%s filesystem doesn't support "
              "ACLs\n",
              content, acl_data->last_fname);
        acl_free(acl);
        return bacl_exit_error;
#      endif
      default:
        Mmsg2(jcr->errmsg, T_("acl_set_file error on file \"%s\": ERR=%s\n"),
              acl_data->last_fname, be.bstrerror());
        Dmsg3(100, "acl_set_file error acl=%s file=%s ERR=%s\n", content,
              acl_data->last_fname, be.bstrerror());
        acl_free(acl);
        return bacl_exit_error;
    }
  }
  acl_free(acl);
  return bacl_exit_ok;
}

// OS specific functions for handling different types of acl streams.
#      if defined(HAVE_DARWIN_OS)
// Define the supported ACL streams for this OS
static int os_access_acl_streams[1] = {STREAM_ACL_DARWIN_ACCESS_ACL};
static int os_default_acl_streams[1] = {-1};

static bacl_exit_code darwin_build_acl_streams(JobControlRecord* jcr,
                                               AclBuildData* acl_data,
                                               FindFilesPacket*)
{
#        if defined(HAVE_ACL_TYPE_EXTENDED)
  /* On MacOS X, acl_get_file (name, ACL_TYPE_ACCESS)
   * and acl_get_file (name, ACL_TYPE_DEFAULT)
   * always return NULL / EINVAL.  There is no point in making
   * these two useless calls.  The real ACL is retrieved through
   * acl_get_file (name, ACL_TYPE_EXTENDED).
   *
   * Read access ACLs for files, dirs and links */
  if (generic_get_acl_from_os(jcr, acl_data, BACL_TYPE_EXTENDED)
      == bacl_exit_fatal)
    return bacl_exit_fatal;
#        else
  // Read access ACLs for files, dirs and links
  if (generic_get_acl_from_os(jcr, acl_data, BACL_TYPE_ACCESS)
      == bacl_exit_fatal)
    return bacl_exit_fatal;
#        endif

  if (acl_data->content_length > 0) {
    return SendAclStream(jcr, acl_data, STREAM_ACL_DARWIN_ACCESS_ACL);
  }
  return bacl_exit_ok;
}

static bacl_exit_code darwin_parse_acl_streams(JobControlRecord* jcr,
                                               AclData* acl_data,
                                               int,
                                               char* content,
                                               uint32_t content_length)
{
#        if defined(HAVE_ACL_TYPE_EXTENDED)
  return generic_set_acl_on_os(jcr, acl_data, BACL_TYPE_EXTENDED, content,
                               content_length);
#        else
  return generic_set_acl_on_os(jcr, acl_data, BACL_TYPE_ACCESS, content,
                               content_length);
#        endif
}

/**
 * For this OS setup the build and parse function pointer to the OS specific
 * functions.
 */
static bacl_exit_code (*os_build_acl_streams)(JobControlRecord* jcr,
                                              AclBuildData* acl_data,
                                              FindFilesPacket* ff_pkt)
    = darwin_build_acl_streams;
static bacl_exit_code (*os_parse_acl_streams)(JobControlRecord* jcr,
                                              AclData* acl_data,
                                              int stream,
                                              char* content,
                                              uint32_t content_length)
    = darwin_parse_acl_streams;

#      elif defined(HAVE_FREEBSD_OS)
// Define the supported ACL streams for these OSes
static int os_access_acl_streams[2]
    = {STREAM_ACL_FREEBSD_ACCESS_ACL, STREAM_ACL_FREEBSD_NFS4_ACL};
static int os_default_acl_streams[1] = {STREAM_ACL_FREEBSD_DEFAULT_ACL};

static bacl_exit_code freebsd_build_acl_streams(JobControlRecord* jcr,
                                                AclBuildData* acl_data,
                                                FindFilesPacket*)
{
  int acl_enabled = 0;
  bacl_type acltype = BACL_TYPE_NONE;

#        if defined(_PC_ACL_NFS4)
  // See if filesystem supports NFS4 acls.
  acl_enabled = pathconf(acl_data->last_fname, _PC_ACL_NFS4);
  switch (acl_enabled) {
    case -1: {
      BErrNo be;

      switch (errno) {
        case ENOENT:
          return bacl_exit_ok;
        default:
          Mmsg2(jcr->errmsg, T_("pathconf error on file \"%s\": ERR=%s\n"),
                acl_data->last_fname, be.bstrerror());
          Dmsg2(100, "pathconf error file=%s ERR=%s\n", acl_data->last_fname,
                be.bstrerror());
          return bacl_exit_error;
      }
    }
    case 0:
      break;
    default:
      acltype = BACL_TYPE_NFS4;
      break;
  }
#        endif

  if (acl_enabled == 0) {
    // See if filesystem supports POSIX acls.
    acl_enabled = pathconf(acl_data->last_fname, _PC_ACL_EXTENDED);
    switch (acl_enabled) {
      case -1: {
        BErrNo be;

        switch (errno) {
          case ENOENT:
            return bacl_exit_ok;
          default:
            Mmsg2(jcr->errmsg, T_("pathconf error on file \"%s\": ERR=%s\n"),
                  acl_data->last_fname, be.bstrerror());
            Dmsg2(100, "pathconf error file=%s ERR=%s\n", acl_data->last_fname,
                  be.bstrerror());
            return bacl_exit_error;
        }
      }
      case 0:
        break;
      default:
        acltype = BACL_TYPE_ACCESS;
        break;
    }
  }

  /* If the filesystem reports it doesn't support ACLs we clear the
   * SaveNative flag so we skip ACL saves on all other files
   * on the same filesystem. The SaveNative flag gets set again
   * when we change from one filesystem to another. */
  if (acl_enabled == 0) {
    acl_data->flags.SaveNative = false;
    PmStrcpy(acl_data->content, "");
    acl_data->content_length = 0;
    return bacl_exit_ok;
  }

  // Based on the supported ACLs retrieve and store them.
  switch (acltype) {
    case BACL_TYPE_NFS4:
      // Read NFS4 ACLs for files, dirs and links
      if (generic_get_acl_from_os(jcr, acl_data, BACL_TYPE_NFS4)
          == bacl_exit_fatal)
        return bacl_exit_fatal;

      if (acl_data->content_length > 0) {
        if (SendAclStream(jcr, acl_data, STREAM_ACL_FREEBSD_NFS4_ACL)
            == bacl_exit_fatal)
          return bacl_exit_fatal;
      }
      break;
    case BACL_TYPE_ACCESS:
      // Read access ACLs for files, dirs and links
      if (generic_get_acl_from_os(jcr, acl_data, BACL_TYPE_ACCESS)
          == bacl_exit_fatal)
        return bacl_exit_fatal;

      if (acl_data->content_length > 0) {
        if (SendAclStream(jcr, acl_data, STREAM_ACL_FREEBSD_ACCESS_ACL)
            == bacl_exit_fatal)
          return bacl_exit_fatal;
      }

      // Directories can have default ACLs too
      if (acl_data->filetype == FT_DIREND) {
        if (generic_get_acl_from_os(jcr, acl_data, BACL_TYPE_DEFAULT)
            == bacl_exit_fatal)
          return bacl_exit_fatal;
        if (acl_data->content_length > 0) {
          if (SendAclStream(jcr, acl_data, STREAM_ACL_FREEBSD_DEFAULT_ACL)
              == bacl_exit_fatal)
            return bacl_exit_fatal;
        }
      }
      break;
    default:
      break;
  }

  return bacl_exit_ok;
}

static bacl_exit_code freebsd_parse_acl_streams(JobControlRecord* jcr,
                                                AclData* acl_data,
                                                int stream,
                                                char* content,
                                                uint32_t content_length)
{
  int acl_enabled = 0;
  const char* acl_type_name;

  // First make sure the filesystem supports acls.
  switch (stream) {
    case STREAM_UNIX_ACCESS_ACL:
    case STREAM_ACL_FREEBSD_ACCESS_ACL:
    case STREAM_UNIX_DEFAULT_ACL:
    case STREAM_ACL_FREEBSD_DEFAULT_ACL:
      acl_enabled = pathconf(acl_data->last_fname, _PC_ACL_EXTENDED);
      acl_type_name = "POSIX";
      break;
    case STREAM_ACL_FREEBSD_NFS4_ACL:
#        if defined(_PC_ACL_NFS4)
      acl_enabled = pathconf(acl_data->last_fname, _PC_ACL_NFS4);
#        endif
      acl_type_name = "NFS4";
      break;
    default:
      acl_type_name = "unknown";
      break;
  }

  switch (acl_enabled) {
    case -1: {
      BErrNo be;

      switch (errno) {
        case ENOENT:
          return bacl_exit_ok;
        default:
          Mmsg2(jcr->errmsg, T_("pathconf error on file \"%s\": ERR=%s\n"),
                acl_data->last_fname, be.bstrerror());
          Dmsg3(100, "pathconf error acl=%s file=%s ERR=%s\n", content,
                acl_data->last_fname, be.bstrerror());
          return bacl_exit_error;
      }
    }
    case 0:
      /* If the filesystem reports it doesn't support ACLs we clear the
       * RestoreNative flag so we skip ACL restores on all other
       * files on the same filesystem. The RestoreNative flag gets
       * set again when we change from one filesystem to another. */
      acl_data->flags.SaveNative = false;
      Mmsg2(jcr->errmsg,
            T_("Trying to restore acl on file \"%s\" on filesystem without %s "
               "acl support\n"),
            acl_data->last_fname, acl_type_name);
      return bacl_exit_error;
    default:
      break;
  }

  // Restore the ACLs.
  switch (stream) {
    case STREAM_UNIX_ACCESS_ACL:
    case STREAM_ACL_FREEBSD_ACCESS_ACL:
      return generic_set_acl_on_os(jcr, acl_data, BACL_TYPE_ACCESS, content,
                                   content_length);
    case STREAM_UNIX_DEFAULT_ACL:
    case STREAM_ACL_FREEBSD_DEFAULT_ACL:
      return generic_set_acl_on_os(jcr, acl_data, BACL_TYPE_DEFAULT, content,
                                   content_length);
    case STREAM_ACL_FREEBSD_NFS4_ACL:
      return generic_set_acl_on_os(jcr, acl_data, BACL_TYPE_NFS4, content,
                                   content_length);
    default:
      break;
  }
  return bacl_exit_error;
}

/**
 * For this OSes setup the build and parse function pointer to the OS specific
 * functions.
 */
static bacl_exit_code (*os_build_acl_streams)(JobControlRecord* jcr,
                                              AclBuildData* acl_data,
                                              FindFilesPacket* ff_pkt)
    = freebsd_build_acl_streams;
static bacl_exit_code (*os_parse_acl_streams)(JobControlRecord* jcr,
                                              AclData* acl_data,
                                              int stream,
                                              char* content,
                                              uint32_t content_length)
    = freebsd_parse_acl_streams;

#      elif defined(HAVE_LINUX_OS)
// Define the supported ACL streams
static int os_access_acl_streams[1] = {STREAM_ACL_LINUX_ACCESS_ACL};
static int os_default_acl_streams[1] = {STREAM_ACL_LINUX_DEFAULT_ACL};

static bacl_exit_code generic_build_acl_streams(JobControlRecord* jcr,
                                                AclBuildData* acl_data,
                                                FindFilesPacket*)
{
  // Read access ACLs for files, dirs and links
  if (generic_get_acl_from_os(jcr, acl_data, BACL_TYPE_ACCESS)
      == bacl_exit_fatal)
    return bacl_exit_fatal;

  if (acl_data->content_length > 0) {
    if (SendAclStream(jcr, acl_data, os_access_acl_streams[0])
        == bacl_exit_fatal)
      return bacl_exit_fatal;
  }

  // Directories can have default ACLs too
  if (acl_data->filetype == FT_DIREND) {
    if (generic_get_acl_from_os(jcr, acl_data, BACL_TYPE_DEFAULT)
        == bacl_exit_fatal)
      return bacl_exit_fatal;
    if (acl_data->content_length > 0) {
      if (SendAclStream(jcr, acl_data, os_default_acl_streams[0])
          == bacl_exit_fatal)
        return bacl_exit_fatal;
    }
  }
  return bacl_exit_ok;
}

static bacl_exit_code generic_parse_acl_streams(JobControlRecord* jcr,
                                                AclData* acl_data,
                                                int stream,
                                                char* content,
                                                uint32_t content_length)
{
  unsigned int cnt;

  switch (stream) {
    case STREAM_UNIX_ACCESS_ACL:
      return generic_set_acl_on_os(jcr, acl_data, BACL_TYPE_ACCESS, content,
                                   content_length);
    case STREAM_UNIX_DEFAULT_ACL:
      return generic_set_acl_on_os(jcr, acl_data, BACL_TYPE_DEFAULT, content,
                                   content_length);
    default:
      // See what type of acl it is.
      for (cnt = 0; cnt < sizeof(os_access_acl_streams) / sizeof(int); cnt++) {
        if (os_access_acl_streams[cnt] == stream) {
          return generic_set_acl_on_os(jcr, acl_data, BACL_TYPE_ACCESS, content,
                                       content_length);
        }
      }
      for (cnt = 0; cnt < sizeof(os_default_acl_streams) / sizeof(int); cnt++) {
        if (os_default_acl_streams[cnt] == stream) {
          return generic_set_acl_on_os(jcr, acl_data, BACL_TYPE_DEFAULT,
                                       content, content_length);
        }
      }
      break;
  }
  return bacl_exit_error;
}

/**
 * For this OSes setup the build and parse function pointer to the OS specific
 * functions.
 */
static bacl_exit_code (*os_build_acl_streams)(JobControlRecord* jcr,
                                              AclBuildData* acl_data,
                                              FindFilesPacket* ff_pkt)
    = generic_build_acl_streams;
static bacl_exit_code (*os_parse_acl_streams)(JobControlRecord* jcr,
                                              AclData* acl_data,
                                              int stream,
                                              char* content,
                                              uint32_t content_length)
    = generic_parse_acl_streams;

#      endif

#    elif defined(HAVE_SUN_OS)
#      if __has_include(<sys/acl.h>)
#        include <sys/acl.h>
#      else
#        error "configure failed to detect availability of sys/acl.h"
#      endif

#      if defined(HAVE_EXTENDED_ACL)
/**
 * We define some internals of the Solaris acl libs here as those
 * are not exposed yet. Probably because they want us to see the
 * acls as opague data. But as we need to support different platforms
 * and versions of Solaris we need to expose some data to be able
 * to determine the type of acl used to stuff it into the correct
 * data stream. I know this is far from portable, but maybe the
 * proper interface is exposed later on and we can get ride of
 * this kludge. Newer versions of Solaris include sys/acl_impl.h
 * which has implementation details of acls, if thats included we
 * don't have to define it ourself.
 */
#        if !defined(_SYS_ACL_IMPL_H)
typedef enum acl_type
{
  ACLENT_T = 0,
  ACE_T = 1
} acl_type_t;
#        endif

/**
 * Two external references to functions in the libsec library function not in
 * current include files.
 */
extern "C" {
int acl_type(acl_t*);
char* acl_strerror(int);
}

// Define the supported ACL streams for this OS
static int os_access_acl_streams[2]
    = {STREAM_ACL_SOLARIS_ACLENT, STREAM_ACL_SOLARIS_ACE};
static int os_default_acl_streams[1] = {-1};

/**
 * As the new libsec interface with acl_totext and acl_fromtext also handles
 * the old format from acltotext we can use the new functions even
 * for acls retrieved and stored in the database with older fd versions. If the
 * new interface is not defined (Solaris 9 and older we fall back to the old
 * code)
 */
static bacl_exit_code solaris_build_acl_streams(JobControlRecord* jcr,
                                                AclBuildData* acl_data,
                                                FindFilesPacket*)
{
  int acl_enabled, flags;
  acl_t* aclp;
  char* acl_text;
  bacl_exit_code stream_status = bacl_exit_error;

  // See if filesystem supports acls.
  acl_enabled = pathconf(acl_data->last_fname, _PC_ACL_ENABLED);
  switch (acl_enabled) {
    case 0:
      /* If the filesystem reports it doesn't support ACLs we clear the
       * SaveNative flag so we skip ACL saves on all other files
       * on the same filesystem. The SaveNative flag gets set again
       * when we change from one filesystem to another. */
      acl_data->flags.SaveNative = false;
      PmStrcpy(acl_data->content, "");
      acl_data->content_length = 0;
      return bacl_exit_ok;
    case -1: {
      BErrNo be;

      switch (errno) {
        case ENOENT:
          return bacl_exit_ok;
        default:
          Mmsg2(jcr->errmsg, T_("pathconf error on file \"%s\": ERR=%s\n"),
                acl_data->last_fname, be.bstrerror());
          Dmsg2(100, "pathconf error file=%s ERR=%s\n", acl_data->last_fname,
                be.bstrerror());
          return bacl_exit_error;
      }
    }
    default:
      break;
  }

  // Get ACL info: don't bother allocating space if there is only a trivial ACL.
  if (acl_get(acl_data->last_fname, ACL_NO_TRIVIAL, &aclp) != 0) {
    BErrNo be;

    switch (errno) {
      case ENOENT:
        return bacl_exit_ok;
      default:
        Mmsg2(jcr->errmsg, T_("acl_get error on file \"%s\": ERR=%s\n"),
              acl_data->last_fname, acl_strerror(errno));
        Dmsg2(100, "acl_get error file=%s ERR=%s\n", acl_data->last_fname,
              acl_strerror(errno));
        return bacl_exit_error;
    }
  }

  if (!aclp) {
    /* The ACLs simply reflect the (already known) standard permissions
     * So we don't send an ACL stream to the SD. */
    PmStrcpy(acl_data->content, "");
    acl_data->content_length = 0;
    return bacl_exit_ok;
  }

#        if defined(ACL_SID_FMT)
  // New format flag added in newer Solaris versions.
  flags = ACL_APPEND_ID | ACL_COMPACT_FMT | ACL_SID_FMT;
#        else
  flags = ACL_APPEND_ID | ACL_COMPACT_FMT;
#        endif /* ACL_SID_FMT */

  if ((acl_text = acl_totext(aclp, flags)) != NULL) {
    acl_data->content_length = PmStrcpy(acl_data->content, acl_text);
    free(acl_text);

    switch (acl_type(aclp)) {
      case ACLENT_T:
        stream_status = SendAclStream(jcr, acl_data, STREAM_ACL_SOLARIS_ACLENT);
        break;
      case ACE_T:
        stream_status = SendAclStream(jcr, acl_data, STREAM_ACL_SOLARIS_ACE);
        break;
      default:
        break;
    }

    acl_free(aclp);
  }
  return stream_status;
}

static bacl_exit_code solaris_parse_acl_streams(JobControlRecord* jcr,
                                                AclData* acl_data,
                                                int stream,
                                                char* content,
                                                uint32_t)
{
  acl_t* aclp;
  int acl_enabled, error;

  switch (stream) {
    case STREAM_UNIX_ACCESS_ACL:
    case STREAM_ACL_SOLARIS_ACLENT:
    case STREAM_ACL_SOLARIS_ACE:
      // First make sure the filesystem supports acls.
      acl_enabled = pathconf(acl_data->last_fname, _PC_ACL_ENABLED);
      switch (acl_enabled) {
        case 0:
          /* If the filesystem reports it doesn't support ACLs we clear the
           * RestoreNative flag so we skip ACL restores on all other
           * files on the same filesystem. The RestoreNative flag
           * gets set again when we change from one filesystem to another. */
          acl_data->flags.RestoreNative = false;
          Mmsg1(jcr->errmsg,
                T_("Trying to restore acl on file \"%s\" on filesystem without "
                   "acl support\n"),
                acl_data->last_fname);
          return bacl_exit_error;
        case -1: {
          BErrNo be;

          switch (errno) {
            case ENOENT:
              return bacl_exit_ok;
            default:
              Mmsg2(jcr->errmsg, T_("pathconf error on file \"%s\": ERR=%s\n"),
                    acl_data->last_fname, be.bstrerror());
              Dmsg3(100, "pathconf error acl=%s file=%s ERR=%s\n", content,
                    acl_data->last_fname, be.bstrerror());
              return bacl_exit_error;
          }
        }
        default:
          /* On a filesystem with ACL support make sure this particular ACL type
           * can be restored. */
          switch (stream) {
            case STREAM_ACL_SOLARIS_ACLENT:
              /* An aclent can be restored on filesystems with
               * _ACL_ACLENT_ENABLED or _ACL_ACE_ENABLED support. */
              if ((acl_enabled & (_ACL_ACLENT_ENABLED | _ACL_ACE_ENABLED))
                  == 0) {
                Mmsg1(jcr->errmsg,
                      T_("Trying to restore POSIX acl on file \"%s\" on "
                         "filesystem without aclent acl support\n"),
                      acl_data->last_fname);
                return bacl_exit_error;
              }
              break;
            case STREAM_ACL_SOLARIS_ACE:
              /* An ace can only be restored on a filesystem with
               * _ACL_ACE_ENABLED support. */
              if ((acl_enabled & _ACL_ACE_ENABLED) == 0) {
                Mmsg1(jcr->errmsg,
                      T_("Trying to restore NFSv4 acl on file \"%s\" on "
                         "filesystem without ace acl support\n"),
                      acl_data->last_fname);
                return bacl_exit_error;
              }
              break;
            default:
              /* Stream id which doesn't describe the type of acl which is
               * encoded. */
              break;
          }
          break;
      }

      if ((error = acl_fromtext(content, &aclp)) != 0) {
        Mmsg2(jcr->errmsg, T_("acl_fromtext error on file \"%s\": ERR=%s\n"),
              acl_data->last_fname, acl_strerror(error));
        Dmsg3(100, "acl_fromtext error acl=%s file=%s ERR=%s\n", content,
              acl_data->last_fname, acl_strerror(error));
        return bacl_exit_error;
      }

      // Validate that the conversion gave us the correct acl type.
      switch (stream) {
        case STREAM_ACL_SOLARIS_ACLENT:
          if (acl_type(aclp) != ACLENT_T) {
            Mmsg1(
                jcr->errmsg,
                T_("wrong encoding of acl type in acl stream on file \"%s\"\n"),
                acl_data->last_fname);
            return bacl_exit_error;
          }
          break;
        case STREAM_ACL_SOLARIS_ACE:
          if (acl_type(aclp) != ACE_T) {
            Mmsg1(
                jcr->errmsg,
                T_("wrong encoding of acl type in acl stream on file \"%s\"\n"),
                acl_data->last_fname);
            return bacl_exit_error;
          }
          break;
        default:
          // Stream id which doesn't describe the type of acl which is encoded.
          break;
      }

      /* Restore the ACLs, but don't complain about links which really should
       * not have attributes, and the file it is linked to may not yet be
       * restored. This is only true for the old acl streams as in the new
       * implementation we don't save acls of symlinks (which cannot have acls
       * anyhow) */
      if ((error = acl_set(acl_data->last_fname, aclp)) == -1
          && acl_data->filetype != FT_LNK) {
        switch (errno) {
          case ENOENT:
            acl_free(aclp);
            return bacl_exit_ok;
          default:
            Mmsg2(jcr->errmsg, T_("acl_set error on file \"%s\": ERR=%s\n"),
                  acl_data->last_fname, acl_strerror(error));
            Dmsg3(100, "acl_set error acl=%s file=%s ERR=%s\n", content,
                  acl_data->last_fname, acl_strerror(error));
            acl_free(aclp);
            return bacl_exit_error;
        }
      }

      acl_free(aclp);
      return bacl_exit_ok;
    default:
      return bacl_exit_error;
  } /* end switch (stream) */
}

#      else  /* HAVE_EXTENDED_ACL */

// Define the supported ACL streams for this OS
static int os_access_acl_streams[1] = {STREAM_ACL_SOLARIS_ACLENT};
static int os_default_acl_streams[1] = {-1};

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

// OS specific functions for handling different types of acl streams.
static bacl_exit_code solaris_build_acl_streams(JobControlRecord* jcr,
                                                AclBuildData* acl_data,
                                                FindFilesPacket* ff_pkt)
{
  int n;
  aclent_t* acls;
  char* acl_text;

  n = acl(acl_data->last_fname, GETACLCNT, 0, NULL);
  if (n < MIN_ACL_ENTRIES) { return bacl_exit_error; }

  acls = (aclent_t*)malloc(n * sizeof(aclent_t));
  if (acl(acl_data->last_fname, GETACL, n, acls) == n) {
    if (AclIsTrivial(n, acls)) {
      /* The ACLs simply reflect the (already known) standard permissions
       * So we don't send an ACL stream to the SD. */
      free(acls);
      PmStrcpy(acl_data->content, "");
      acl_data->content_length = 0;
      return bacl_exit_ok;
    }

    if ((acl_text = acltotext(acls, n)) != NULL) {
      acl_data->content_length = PmStrcpy(acl_data->content, acl_text);
      free(acl_text);
      free(acls);
      return SendAclStream(jcr, acl_data, STREAM_ACL_SOLARIS_ACLENT);
    }

    BErrNo be;
    Mmsg2(jcr->errmsg, T_("acltotext error on file \"%s\": ERR=%s\n"),
          acl_data->last_fname, be.bstrerror());
    Dmsg3(100, "acltotext error acl=%s file=%s ERR=%s\n", acl_data->content,
          acl_data->last_fname, be.bstrerror());
  }

  free(acls);
  return bacl_exit_error;
}

static bacl_exit_code solaris_parse_acl_streams(JobControlRecord* jcr,
                                                AclData* acl_data,
                                                int stream,
                                                char* content,
                                                uint32_t content_length)
{
  int n;
  aclent_t* acls;

  acls = aclfromtext(content, &n);
  if (!acls) {
    BErrNo be;

    Mmsg2(jcr->errmsg, T_("aclfromtext error on file \"%s\": ERR=%s\n"),
          acl_data->last_fname, be.bstrerror());
    Dmsg3(100, "aclfromtext error acl=%s file=%s ERR=%s\n", content,
          acl_data->last_fname, be.bstrerror());
    return bacl_exit_error;
  }

  /* Restore the ACLs, but don't complain about links which really should
   * not have attributes, and the file it is linked to may not yet be restored.
   */
  if (acl(acl_data->last_fname, SETACL, n, acls) == -1
      && acl_data->filetype != FT_LNK) {
    BErrNo be;

    switch (errno) {
      case ENOENT:
        free(acls);
        return bacl_exit_ok;
      default:
        Mmsg2(jcr->errmsg, T_("acl(SETACL) error on file \"%s\": ERR=%s\n"),
              acl_data->last_fname, be.bstrerror());
        Dmsg3(100, "acl(SETACL) error acl=%s file=%s ERR=%s\n", content,
              acl_data->last_fname, be.bstrerror());
        free(acls);
        return bacl_exit_error;
    }
  }
  free(acls);
  return bacl_exit_ok;
}
#      endif /* HAVE_EXTENDED_ACL */

/**
 * For this OS setup the build and parse function pointer to the OS specific
 * functions.
 */
static bacl_exit_code (*os_build_acl_streams)(JobControlRecord* jcr,
                                              AclBuildData* acl_data,
                                              FindFilesPacket* ff_pkt)
    = solaris_build_acl_streams;
static bacl_exit_code (*os_parse_acl_streams)(JobControlRecord* jcr,
                                              AclData* acl_data,
                                              int stream,
                                              char* content,
                                              uint32_t content_length)
    = solaris_parse_acl_streams;

#    endif /* HAVE_SUN_OS */
#  endif   /* HAVE_ACL */

#  if defined(HAVE_AFS_ACL)
#    include <afs/afsint.h>
#    include <afs/venus.h>

/**
 * External references to functions in the libsys library function not in
 * current include files.
 */
extern "C" {
long pioctl(char* pathp, long opcode, struct ViceIoctl* blobp, int follow);
}

static bacl_exit_code afs_build_acl_streams(JobControlRecord* jcr,
                                            AclBuildData* acl_data,
                                            FindFilesPacket* ff_pkt)
{
  int error;
  struct ViceIoctl vip;
  char acl_text[BUFSIZ];

  /* AFS ACLs can only be set on a directory, so no need to try to
   * request them for anything other then that. */
  if (ff_pkt->type != FT_DIREND) { return bacl_exit_ok; }

  vip.in = NULL;
  vip.in_size = 0;
  vip.out = acl_text;
  vip.out_size = sizeof(acl_text);
  memset((caddr_t)acl_text, 0, sizeof(acl_text));

  if ((error = pioctl(acl_data->last_fname, VIOCGETAL, &vip, 0)) < 0) {
    BErrNo be;

    Mmsg2(jcr->errmsg, T_("pioctl VIOCGETAL error on file \"%s\": ERR=%s\n"),
          acl_data->last_fname, be.bstrerror());
    Dmsg2(100, "pioctl VIOCGETAL error file=%s ERR=%s\n", acl_data->last_fname,
          be.bstrerror());
    return bacl_exit_error;
  }
  acl_data->content_length = PmStrcpy(acl_data->content, acl_text);
  return SendAclStream(jcr, acl_data, STREAM_ACL_AFS_TEXT);
}

static bacl_exit_code afs_parse_acl_stream(JobControlRecord* jcr,
                                           AclData* acl_data,
                                           int stream,
                                           char* content,
                                           uint32_t content_length)
{
  int error;
  struct ViceIoctl vip;

  vip.in = content;
  vip.in_size = content_length;
  vip.out = NULL;
  vip.out_size = 0;

  if ((error = pioctl(acl_data->last_fname, VIOCSETAL, &vip, 0)) < 0) {
    BErrNo be;

    Mmsg2(jcr->errmsg, T_("pioctl VIOCSETAL error on file \"%s\": ERR=%s\n"),
          acl_data->last_fname, be.bstrerror());
    Dmsg2(100, "pioctl VIOCSETAL error file=%s ERR=%s\n", acl_data->last_fname,
          be.bstrerror());

    return bacl_exit_error;
  }
  return bacl_exit_ok;
}
#  endif /* HAVE_AFS_ACL */

// Entry points when compiled with support for ACLs on a supported platform.

// Read and send an ACL for the last encountered file.
bacl_exit_code BuildAclStreams(JobControlRecord* jcr,
                               AclBuildData* acl_data,
                               FindFilesPacket* ff_pkt)
{
  /* See if we are changing from one device to another.
   * We save the current device we are scanning and compare
   * it with the current st_dev in the last stat performed on
   * the file we are currently storing. */
  if (acl_data->first_dev || acl_data->current_dev != ff_pkt->statp.st_dev) {
    acl_data->flags = {};
    acl_data->first_dev = false;

#  if defined(HAVE_AFS_ACL)
    /* AFS is a non OS specific filesystem so see if this path is on an AFS
     * filesystem Set the SaveAfs flag if it is. If not set the
     * SaveNative flag. */
    if (FstypeEquals(acl_data->last_fname, "afs")) {
      acl_data->flags.SaveAfs = true;
    } else {
      acl_data->flags.SaveNative = true;
    }
#  else
    acl_data->flags.SaveNative = true;
#  endif

    // Save that we started scanning a new filesystem.
    acl_data->current_dev = ff_pkt->statp.st_dev;
  }

#  if defined(HAVE_AFS_ACL)
  /* See if the SaveAfs flag is set which lets us know if we should
   * save AFS ACLs. */
  if (acl_data->flags.SaveAfs) {
    return afs_build_acl_streams(jcr, acl_data, ff_pkt);
  }
#  endif
#  if defined(HAVE_ACL)
  /* See if the SaveNative flag is set which lets us know if we
   * should save native ACLs. */
  if (acl_data->flags.SaveNative) {
    // Call the appropriate function.
    if (os_build_acl_streams) {
      return os_build_acl_streams(jcr, acl_data, ff_pkt);
    }
  } else {
    return bacl_exit_ok;
  }
#  endif
  return bacl_exit_error;
}

bacl_exit_code parse_acl_streams(JobControlRecord* jcr,
                                 AclData* acl_data,
                                 int stream,
                                 char* content,
                                 uint32_t content_length)
{
  int ret;
  struct stat st;
  unsigned int cnt;

  /* See if we are changing from one device to another.
   * We save the current device we are restoring to and compare
   * it with the current st_dev in the last stat performed on
   * the file we are currently restoring. */
  ret = lstat(acl_data->last_fname, &st);
  switch (ret) {
    case -1: {
      BErrNo be;

      switch (errno) {
        case ENOENT:
          return bacl_exit_ok;
        default:
          Mmsg2(jcr->errmsg, T_("Unable to stat file \"%s\": ERR=%s\n"),
                acl_data->last_fname, be.bstrerror());
          Dmsg2(100, "Unable to stat file \"%s\": ERR=%s\n",
                acl_data->last_fname, be.bstrerror());
          return bacl_exit_error;
      }
      break;
    }
    case 0:
      break;
  }
  if (acl_data->first_dev || acl_data->current_dev != st.st_dev) {
    acl_data->flags = {};
    acl_data->first_dev = false;

#  if defined(HAVE_AFS_ACL)
    /* AFS is a non OS specific filesystem so see if this path is on an AFS
     * filesystem Set the RestoreAfs flag if it is. If not set the
     * RestoreNative flag. */
    if (FstypeEquals(acl_data->last_fname, "afs")) {
      acl_data->flags.RestoreAfs = true;
    } else {
      acl_data->flags.RestoreNative = true;
    }
#  else
    acl_data->flags.RestoreNative = true;
#  endif

    // Save that we started restoring to a new filesystem.
    acl_data->current_dev = st.st_dev;
  }

  switch (stream) {
#  if defined(HAVE_AFS_ACL)
    case STREAM_ACL_AFS_TEXT:
      if (acl_data->flags.RestoreAfs) {
        return afs_parse_acl_stream(jcr, acl_data, stream, content,
                                    content_length);
      } else {
        /* Increment error count but don't log an error again for the same
         * filesystem. */
        acl_data->u.parse->nr_errors++;
        return bacl_exit_ok;
      }
#  endif
#  if defined(HAVE_ACL)
    case STREAM_UNIX_ACCESS_ACL:
    case STREAM_UNIX_DEFAULT_ACL:
      // Handle legacy ACL streams.
      if (acl_data->flags.RestoreNative && os_parse_acl_streams) {
        return os_parse_acl_streams(jcr, acl_data, stream, content,
                                    content_length);
      } else {
        /* Increment error count but don't log an error again for the same
         * filesystem. */
        acl_data->nr_errors++;
        return bacl_exit_ok;
      }
      break;
    default:
      if (acl_data->flags.RestoreNative && os_parse_acl_streams) {
        /* Walk the os_access_acl_streams array with the supported Access ACL
         * streams for this OS. */
        for (cnt = 0; cnt < sizeof(os_access_acl_streams) / sizeof(int);
             cnt++) {
          if (os_access_acl_streams[cnt] == stream) {
            return os_parse_acl_streams(jcr, acl_data, stream, content,
                                        content_length);
          }
        }
        /* Walk the os_default_acl_streams array with the supported Default ACL
         * streams for this OS. */
        for (cnt = 0; cnt < sizeof(os_default_acl_streams) / sizeof(int);
             cnt++) {
          if (os_default_acl_streams[cnt] == stream) {
            return os_parse_acl_streams(jcr, acl_data, stream, content,
                                        content_length);
          }
        }
      } else {
        /* Increment error count but don't log an error again for the same
         * filesystem. */
        acl_data->nr_errors++;
        return bacl_exit_ok;
      }
      break;
#  else
    default:
      break;
#  endif
  }
  Qmsg2(jcr, M_WARNING, 0,
        T_("Can't restore ACLs of %s - incompatible acl stream encountered - "
           "%d\n"),
        acl_data->last_fname, stream);
  return bacl_exit_error;
}
#endif
