/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
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
/*
 * Kern Sibbald, October MMII
 */
/**
 * @file
 * Encode and decode Extended attributes for Win32 and
 * other non-Unix systems, or Unix systems with ACLs, ...
 */

#include "bareos.h"
#include "jcr.h"
#include "find.h"
#include "ch.h"
#include "findlib/attribs.h"

static uid_t my_uid = 1;
static gid_t my_gid = 1;
static bool uid_set = false;

#if defined(HAVE_WIN32)

/* Imported Functions */
extern void unix_name_to_win32(POOLMEM *&win32_name, const char *name);

/* Forward referenced subroutines */
static bool set_win32_attributes(JobControlRecord *jcr, Attributes *attr, BareosWinFilePacket *ofd);
void win_error(JobControlRecord *jcr, const char *prefix, POOLMEM *ofile);
#endif /* HAVE_WIN32 */

/**
 * For old systems that don't have lchown() use chown()
 */

#ifndef HAVE_LCHOWN
#define lchown chown
#endif

/**
 * For old systems that don't have lchmod() use chmod()
 */
#ifndef HAVE_LCHMOD
#define lchmod chmod
#endif

/*=============================================================*/
/*                                                             */
/*             ***  A l l  S y s t e m s ***                   */
/*                                                             */
/*=============================================================*/

/**
 * Return the data stream that will be used
 */
int select_data_stream(FindFilesPacket *ff_pkt, bool compatible)
{
   int stream;

   /* This is a plugin special restore object */
   if (ff_pkt->type == FT_RESTORE_FIRST) {
      clear_all_bits(FO_MAX, ff_pkt->flags);
      return STREAM_FILE_DATA;
   }

   /*
    * Fix all incompatible options
    */

   /**
    * No sparse option for encrypted data
    */
   if (bit_is_set(FO_ENCRYPT, ff_pkt->flags)) {
      clear_bit(FO_SPARSE, ff_pkt->flags);
   }

   /*
    * Note, no sparse option for win32_data
    */
   if (!is_portable_backup(&ff_pkt->bfd)) {
      stream = STREAM_WIN32_DATA;
      clear_bit(FO_SPARSE, ff_pkt->flags);
   } else if (bit_is_set(FO_SPARSE, ff_pkt->flags)) {
      stream = STREAM_SPARSE_DATA;
   } else {
      stream = STREAM_FILE_DATA;
   }
   if (bit_is_set(FO_OFFSETS, ff_pkt->flags)) {
      stream = STREAM_SPARSE_DATA;
   }

   /*
    * Encryption is only supported for file data
    */
   if (stream != STREAM_FILE_DATA &&
       stream != STREAM_WIN32_DATA &&
       stream != STREAM_MACOS_FORK_DATA) {
      clear_bit(FO_ENCRYPT, ff_pkt->flags);
   }

   /*
    * Compression is not supported for Mac fork data
    */
   if (stream == STREAM_MACOS_FORK_DATA) {
      clear_bit(FO_COMPRESS, ff_pkt->flags);
   }

   /*
    * Handle compression and encryption options
    */
   if (bit_is_set(FO_COMPRESS, ff_pkt->flags)) {
      if (compatible && ff_pkt->Compress_algo == COMPRESS_GZIP) {
         switch (stream) {
         case STREAM_WIN32_DATA:
               stream = STREAM_WIN32_GZIP_DATA;
            break;
         case STREAM_SPARSE_DATA:
               stream = STREAM_SPARSE_GZIP_DATA;
            break;
         case STREAM_FILE_DATA:
               stream = STREAM_GZIP_DATA;
            break;
         default:
            /**
             * All stream types that do not support compression should clear out
             * FO_COMPRESS above, and this code block should be unreachable.
             */
            ASSERT(!bit_is_set(FO_COMPRESS, ff_pkt->flags));
            return STREAM_NONE;
         }
      } else {
         switch (stream) {
         case STREAM_WIN32_DATA:
            stream = STREAM_WIN32_COMPRESSED_DATA;
            break;
         case STREAM_SPARSE_DATA:
            stream = STREAM_SPARSE_COMPRESSED_DATA;
            break;
         case STREAM_FILE_DATA:
            stream = STREAM_COMPRESSED_DATA;
            break;
         default:
            /*
             * All stream types that do not support compression should clear out
             * FO_COMPRESS above, and this code block should be unreachable.
             */
            ASSERT(!bit_is_set(FO_COMPRESS, ff_pkt->flags));
            return STREAM_NONE;
         }
      }
   }

#ifdef HAVE_CRYPTO
   if (bit_is_set(FO_ENCRYPT, ff_pkt->flags)) {
      switch (stream) {
      case STREAM_WIN32_DATA:
         stream = STREAM_ENCRYPTED_WIN32_DATA;
         break;
      case STREAM_WIN32_GZIP_DATA:
         stream = STREAM_ENCRYPTED_WIN32_GZIP_DATA;
         break;
      case STREAM_WIN32_COMPRESSED_DATA:
         stream = STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA;
         break;
      case STREAM_FILE_DATA:
         stream = STREAM_ENCRYPTED_FILE_DATA;
         break;
      case STREAM_GZIP_DATA:
         stream = STREAM_ENCRYPTED_FILE_GZIP_DATA;
         break;
      case STREAM_COMPRESSED_DATA:
         stream = STREAM_ENCRYPTED_FILE_COMPRESSED_DATA;
         break;
      default:
         /*
          * All stream types that do not support encryption should clear out
          * FO_ENCRYPT above, and this code block should be unreachable.
          */
         ASSERT(!bit_is_set(FO_ENCRYPT, ff_pkt->flags));
         return STREAM_NONE;
      }
   }
#endif

   return stream;
}

/**
 * Restore all file attributes like owner, mode and file times.
 */
static inline bool restore_file_attributes(JobControlRecord *jcr, Attributes *attr, BareosWinFilePacket *ofd)
{
   bool ok = true;
   bool suppress_errors;
#if defined(HAVE_FCHOWN) || defined(HAVE_FCHMOD) || defined(HAVE_FUTIMES) || defined(FUTIMENS)
   bool file_is_open;

   /*
    * Save if we are working on an open file.
    */
   file_is_open = is_bopen(ofd);
#endif

   /*
    * See if we want to print errors.
    */
   suppress_errors = (debug_level >= 100 || my_uid != 0);

   /*
    * Restore owner and group.
    */
#ifdef HAVE_FCHOWN
   if (file_is_open) {
      if (fchown(ofd->fid, attr->statp.st_uid, attr->statp.st_gid) < 0 && !suppress_errors) {
         berrno be;

         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file owner %s: ERR=%s\n"), attr->ofname, be.bstrerror());
         ok = false;
      }
   } else {
#else
   {
#endif
      if (lchown(attr->ofname, attr->statp.st_uid, attr->statp.st_gid) < 0 && !suppress_errors) {
         berrno be;

         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file owner %s: ERR=%s\n"), attr->ofname, be.bstrerror());
         ok = false;
      }
   }

   /*
    * Restore filemode.
    */
#ifdef HAVE_FCHMOD
   if (file_is_open) {
      if (fchmod(ofd->fid, attr->statp.st_mode) < 0 && !suppress_errors) {
         berrno be;

         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file modes %s: ERR=%s\n"), attr->ofname, be.bstrerror());
         ok = false;
      }
   } else {
#else
   {
#endif
#if defined(HAVE_WIN32)
      if (win32_chmod(attr->ofname, attr->statp.st_mode, attr->statp.st_rdev) < 0 && !suppress_errors) {
#else
      if (lchmod(attr->ofname, attr->statp.st_mode) < 0 && !suppress_errors) {
#endif
         berrno be;

         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file modes %s: ERR=%s\n"), attr->ofname, be.bstrerror());
         ok = false;
      }
   }

   /*
    * Reset file times.
    */
#if defined(HAVE_FUTIMES)
   if (file_is_open) {
      struct timeval restore_times[2];

      restore_times[0].tv_sec = attr->statp.st_atime;
      restore_times[0].tv_usec = 0;
      restore_times[1].tv_sec = attr->statp.st_mtime;
      restore_times[1].tv_usec = 0;

      if (futimes(ofd->fid, restore_times) < 0 && !suppress_errors) {
         berrno be;

         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file times %s: ERR=%s\n"), attr->ofname, be.bstrerror());
         ok = false;
      }
   } else {
#elif defined(HAVE_FUTIMENS)
   if (file_is_open) {
      struct timespec restore_times[2];

      restore_times[0].tv_sec = attr->statp.st_atime;
      restore_times[0].tv_nsec = 0;
      restore_times[1].tv_sec = attr->statp.st_mtime;
      restore_times[1].tv_nsec = 0;

      if (futimens(ofd->fid, restore_times) < 0 && !suppress_errors) {
         berrno be;

         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file times %s: ERR=%s\n"), attr->ofname, be.bstrerror());
         ok = false;
      }
   } else {
#else
   {
#endif
#if defined(HAVE_LUTIMES)
      struct timeval restore_times[2];

      restore_times[0].tv_sec = attr->statp.st_atime;
      restore_times[0].tv_usec = 0;
      restore_times[1].tv_sec = attr->statp.st_mtime;
      restore_times[1].tv_usec = 0;

      if (lutimes(attr->ofname, restore_times) < 0 && !suppress_errors) {
         berrno be;

         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file times %s: ERR=%s\n"), attr->ofname, be.bstrerror());
         ok = false;
      }
#elif defined(HAVE_UTIMES)
      struct timeval restore_times[2];

      restore_times[0].tv_sec = attr->statp.st_atime;
      restore_times[0].tv_usec = 0;
      restore_times[1].tv_sec = attr->statp.st_mtime;
      restore_times[1].tv_usec = 0;

      if (utimes(attr->ofname, restore_times) < 0 && !suppress_errors) {
         berrno be;

         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file times %s: ERR=%s\n"), attr->ofname, be.bstrerror());
         ok = false;
      }
#else
      struct utimbuf restore_times;

      restore_times.actime = attr->statp.st_atime;
      restore_times.modtime = attr->statp.st_mtime;

      if (utime(attr->ofname, &restore_times) < 0 && !suppress_errors) {
         berrno be;

         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file times %s: ERR=%s\n"), attr->ofname, be.bstrerror());
         ok = false;
      }
#endif /* HAVE_LUTIMES */
   }

   return ok;
}

/**
 * Set file modes, permissions and times
 *
 *  fname is the original filename
 *  ofile is the output filename (may be in a different directory)
 *
 * Returns:  true  on success
 *           false on failure
 */
bool set_attributes(JobControlRecord *jcr, Attributes *attr, BareosWinFilePacket *ofd)
{
   mode_t old_mask;
   bool ok = true;
   bool suppress_errors;

   if (uid_set) {
      my_uid = getuid();
      my_gid = getgid();
      uid_set = true;
   }

   /*
    * See if we want to print errors.
    */
   suppress_errors = (debug_level >= 100 || my_uid != 0);

#if defined(HAVE_WIN32)
   if (attr->stream == STREAM_UNIX_ATTRIBUTES_EX &&
       set_win32_attributes(jcr, attr, ofd)) {
       if (is_bopen(ofd)) {
           bclose(ofd);
       }
       pm_strcpy(attr->ofname, "*None*");
       return true;
   }

   if (attr->data_stream == STREAM_WIN32_DATA ||
       attr->data_stream == STREAM_WIN32_GZIP_DATA ||
       attr->data_stream == STREAM_WIN32_COMPRESSED_DATA) {
      if (is_bopen(ofd)) {
         bclose(ofd);
      }
      pm_strcpy(attr->ofname, "*None*");
      return true;
   }

   /**
    * If Windows stuff failed, e.g. attempt to restore Unix file to Windows, simply fall
    * through and we will do it the universal way.
    */
#endif

   old_mask = umask(0);
   if (is_bopen(ofd)) {
      boffset_t fsize;
      char ec1[50], ec2[50];

      fsize = blseek(ofd, 0, SEEK_END);
      if (attr->type == FT_REG &&
          fsize > 0 &&
          attr->statp.st_size > 0 &&
          fsize != (boffset_t)attr->statp.st_size) {
         Jmsg3(jcr, M_ERROR, 0, _("File size of restored file %s not correct. Original %s, restored %s.\n"),
               attr->ofname, edit_uint64(attr->statp.st_size, ec1), edit_uint64(fsize, ec2));
      }
   } else {
      struct stat st;
      char ec1[50], ec2[50];

      if (lstat(attr->ofname, &st) == 0) {
         if (attr->type == FT_REG &&
             st.st_size > 0 &&
             attr->statp.st_size > 0 &&
             st.st_size != attr->statp.st_size) {
            Jmsg3(jcr, M_ERROR, 0, _("File size of restored file %s not correct. Original %s, restored %s.\n"),
                  attr->ofname, edit_uint64(attr->statp.st_size, ec1), edit_uint64(st.st_size, ec2));
         }
      }
   }

   /**
    * We do not restore sockets, so skip trying to restore their attributes.
    */
   if (attr->type == FT_SPEC && S_ISSOCK(attr->statp.st_mode)) {
      goto bail_out;
   }

   /* ***FIXME**** optimize -- don't do if already correct */
   /**
    * For link, change owner of link using lchown, but don't try to do a chmod as that will update the file behind it.
    */
   if (attr->type == FT_LNK) {
      /*
       * Change owner of link, not of real file
       */
      if (lchown(attr->ofname, attr->statp.st_uid, attr->statp.st_gid) < 0 && !suppress_errors) {
         berrno be;

         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file owner %s: ERR=%s\n"), attr->ofname, be.bstrerror());
         ok = false;
      }

#ifdef HAVE_LCHMOD
      if (lchmod(attr->ofname, attr->statp.st_mode) < 0 && !suppress_errors) {
         berrno be;

         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file modes %s: ERR=%s\n"), attr->ofname, be.bstrerror());
         ok = false;
      }
#endif
   } else {
      if (!ofd->cmd_plugin) {
         ok = restore_file_attributes(jcr, attr, ofd);

#ifdef HAVE_CHFLAGS
         /**
          * FreeBSD user flags
          *
          * Note, this should really be done before the utime() above,
          * but if the immutable bit is set, it will make the utimes()
          * fail.
          */
         if (chflags(attr->ofname, attr->statp.st_flags) < 0 && !suppress_errors) {
            berrno be;
            Jmsg2(jcr, M_ERROR, 0, _("Unable to set file flags %s: ERR=%s\n"), attr->ofname, be.bstrerror());
            ok = false;
         }
#endif
      }
   }

bail_out:
   if (is_bopen(ofd)) {
      bclose(ofd);
   }

   pm_strcpy(attr->ofname, "*None*");
   umask(old_mask);

   return ok;
}

#if !defined(HAVE_WIN32)
/*=============================================================*/
/*                                                             */
/*                 * * *  U n i x * * * *                      */
/*                                                             */
/*=============================================================*/

/**
 * It is possible to piggyback additional data e.g. ACLs on
 *   the encode_stat() data by returning the extended attributes
 *   here.  They must be "self-contained" (i.e. you keep track
 *   of your own length), and they must be in ASCII string
 *   format. Using this feature is not recommended.
 * The code below shows how to return nothing.  See the Win32
 *   code below for returning something in the attributes.
 */
int encode_attribsEx(JobControlRecord *jcr, char *attribsEx, FindFilesPacket *ff_pkt)
{
#ifdef HAVE_DARWIN_OS
   /**
    * We save the Mac resource fork length so that on a
    * restore, we can be sure we put back the whole resource.
    */
   char *p;

   *attribsEx = 0;                 /* no extended attributes (yet) */
   if (jcr->cmd_plugin || ff_pkt->type == FT_DELETED) {
      return STREAM_UNIX_ATTRIBUTES;
   }
   p = attribsEx;
   if (bit_is_set(FO_HFSPLUS, ff_pkt->flags)) {
      p += to_base64((uint64_t)(ff_pkt->hfsinfo.rsrclength), p);
   }
   *p = 0;
#else
   *attribsEx = 0;                    /* no extended attributes */
#endif
   return STREAM_UNIX_ATTRIBUTES;
}
#else
/*=============================================================*/
/*                                                             */
/*                 * * *  W i n 3 2 * * * *                    */
/*                                                             */
/*=============================================================*/
int encode_attribsEx(JobControlRecord *jcr, char *attribsEx, FindFilesPacket *ff_pkt)
{
   char *p = attribsEx;
   WIN32_FILE_ATTRIBUTE_DATA atts;
   ULARGE_INTEGER li;

   attribsEx[0] = 0;                  /* no extended attributes */

   if (jcr->cmd_plugin || ff_pkt->type == FT_DELETED) {
      return STREAM_UNIX_ATTRIBUTES;
   }

   unix_name_to_win32(ff_pkt->sys_fname, ff_pkt->fname);
   if (p_GetFileAttributesExW)  {
      /**
       * Try unicode version
       */
      POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);
      make_win32_path_UTF8_2_wchar(pwszBuf, ff_pkt->fname);

      BOOL b=p_GetFileAttributesExW((LPCWSTR)pwszBuf, GetFileExInfoStandard,
                                    (LPVOID)&atts);
      free_pool_memory(pwszBuf);

      if (!b) {
         win_error(jcr, "GetFileAttributesExW:", ff_pkt->sys_fname);
         return STREAM_UNIX_ATTRIBUTES;
      }
   } else {
      if (!p_GetFileAttributesExA)
         return STREAM_UNIX_ATTRIBUTES;

      if (!p_GetFileAttributesExA(ff_pkt->sys_fname, GetFileExInfoStandard,
                              (LPVOID)&atts)) {
         win_error(jcr, "GetFileAttributesExA:", ff_pkt->sys_fname);
         return STREAM_UNIX_ATTRIBUTES;
      }
   }

   /*
    * Instead of using the current dwFileAttributes use the
    * ff_pkt->statp.st_rdev which contains the actual fileattributes we
    * want to save for this file.
    */
   atts.dwFileAttributes = ff_pkt->statp.st_rdev;

   p += to_base64((uint64_t)atts.dwFileAttributes, p);
   *p++ = ' ';                        /* separate fields with a space */
   li.LowPart = atts.ftCreationTime.dwLowDateTime;
   li.HighPart = atts.ftCreationTime.dwHighDateTime;
   p += to_base64((uint64_t)li.QuadPart, p);
   *p++ = ' ';
   li.LowPart = atts.ftLastAccessTime.dwLowDateTime;
   li.HighPart = atts.ftLastAccessTime.dwHighDateTime;
   p += to_base64((uint64_t)li.QuadPart, p);
   *p++ = ' ';
   li.LowPart = atts.ftLastWriteTime.dwLowDateTime;
   li.HighPart = atts.ftLastWriteTime.dwHighDateTime;
   p += to_base64((uint64_t)li.QuadPart, p);
   *p++ = ' ';
   p += to_base64((uint64_t)atts.nFileSizeHigh, p);
   *p++ = ' ';
   p += to_base64((uint64_t)atts.nFileSizeLow, p);
   *p = 0;

   return STREAM_UNIX_ATTRIBUTES_EX;
}

/**
 * Do casting according to unknown type to keep compiler happy
 */
#ifdef HAVE_TYPEOF
   #define plug(st, val) st = (typeof st)val
#else
  /*
   * Use templates to do the casting
   */
  template <class T> void plug(T &st, uint64_t val)
  { st = static_cast<T>(val); }
#endif

/**
 * Set Extended File Attributes for Win32
 *
 * fname is the original filename
 * ofile is the output filename (may be in a different directory)
 *
 * Returns:  true  on success
 *           false on failure
 */
static bool set_win32_attributes(JobControlRecord *jcr, Attributes *attr, BareosWinFilePacket *ofd)
{
   char *p = attr->attrEx;
   int64_t val;
   WIN32_FILE_ATTRIBUTE_DATA atts;
   ULARGE_INTEGER li;

   /** if we have neither Win ansi nor wchar API, get out */
   if (!(p_SetFileAttributesW || p_SetFileAttributesA)) {
      return false;
   }

   if (!p || !*p) {                   /* we should have attributes */
      Dmsg2(100, "Attributes missing. of=%s ofd=%d\n", attr->ofname, ofd->fid);
      if (is_bopen(ofd)) {
         bclose(ofd);
      }
      return false;
   } else {
      Dmsg2(100, "Attribs %s = %s\n", attr->ofname, attr->attrEx);
   }

   p += from_base64(&val, p);
   plug(atts.dwFileAttributes, val);
   p++;                               /* skip space */
   p += from_base64(&val, p);
   li.QuadPart = val;
   atts.ftCreationTime.dwLowDateTime = li.LowPart;
   atts.ftCreationTime.dwHighDateTime = li.HighPart;
   p++;                               /* skip space */
   p += from_base64(&val, p);
   li.QuadPart = val;
   atts.ftLastAccessTime.dwLowDateTime = li.LowPart;
   atts.ftLastAccessTime.dwHighDateTime = li.HighPart;
   p++;                               /* skip space */
   p += from_base64(&val, p);
   li.QuadPart = val;
   atts.ftLastWriteTime.dwLowDateTime = li.LowPart;
   atts.ftLastWriteTime.dwHighDateTime = li.HighPart;
   p++;
   p += from_base64(&val, p);
   plug(atts.nFileSizeHigh, val);
   p++;
   p += from_base64(&val, p);
   plug(atts.nFileSizeLow, val);

   /** At this point, we have reconstructed the WIN32_FILE_ATTRIBUTE_DATA pkt */

   if (!is_bopen(ofd)) {
      Dmsg1(100, "File not open: %s\n", attr->ofname);
      bopen(ofd, attr->ofname, O_WRONLY | O_BINARY, 0, 0); /* attempt to open the file */
   }

   /*
    * Restore file attributes and times on the restored file.
    */
   if (!win32_restore_file_attributes(attr->ofname, bget_handle(ofd), &atts)) {
      win_error(jcr, "win32_restore_file_attributes:", attr->ofname);
   }

   if (is_bopen(ofd)) {
      bclose(ofd);
   }

   return true;
}

void win_error(JobControlRecord *jcr, const char *prefix, POOLMEM *win32_ofile)
{
   DWORD lerror = GetLastError();
   LPTSTR msg;
   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                 NULL,
                 lerror,
                 0,
                 (LPTSTR)&msg,
                 0,
                 NULL);
   Dmsg3(100, "Error in %s on file %s: ERR=%s\n", prefix, win32_ofile, msg);
   strip_trailing_junk(msg);
   Jmsg3(jcr, M_ERROR, 0, _("Error in %s file %s: ERR=%s\n"), prefix, win32_ofile, msg);
   LocalFree(msg);
}

void win_error(JobControlRecord *jcr, const char *prefix, DWORD lerror)
{
   LPTSTR msg;
   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                 NULL,
                 lerror,
                 0,
                 (LPTSTR)&msg,
                 0,
                 NULL);
   strip_trailing_junk(msg);
   if (jcr) {
      Jmsg2(jcr, M_ERROR, 0, _("Error in %s: ERR=%s\n"), prefix, msg);
   } else {
      MessageBox(NULL, msg, prefix, MB_OK);
   }
   LocalFree(msg);
}
#endif /* HAVE_WIN32 */
