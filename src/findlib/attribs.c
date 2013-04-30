/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.

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
/**
 *  Encode and decode standard Unix attributes and
 *   Extended attributes for Win32 and
 *   other non-Unix systems, or Unix systems with ACLs, ...
 *
 *    Kern Sibbald, October MMII
 *
 */

#include "bacula.h"
#include "find.h"
#include "ch.h"

static uid_t my_uid = 1;
static gid_t my_gid = 1;                        
static bool uid_set = false;


#if defined(HAVE_WIN32)
/* Forward referenced subroutines */
static bool set_win32_attributes(JCR *jcr, ATTR *attr, BFILE *ofd);
void unix_name_to_win32(POOLMEM **win32_name, char *name);
void win_error(JCR *jcr, const char *prefix, POOLMEM *ofile);
HANDLE bget_handle(BFILE *bfd);
#endif /* HAVE_WIN32 */

/* For old systems that don't have lchown() use chown() */
#ifndef HAVE_LCHOWN
#define lchown chown
#endif

/*=============================================================*/
/*                                                             */
/*             ***  A l l  S y s t e m s ***                   */
/*                                                             */
/*=============================================================*/

/**
 * Return the data stream that will be used
 */
int select_data_stream(FF_PKT *ff_pkt)
{
   int stream;

   /* This is a plugin special restore object */
   if (ff_pkt->type == FT_RESTORE_FIRST) {
      ff_pkt->flags = 0;
      return STREAM_FILE_DATA;
   }

   /**
    *  Fix all incompatible options
    */
   /** No sparse option for encrypted data */
   if (ff_pkt->flags & FO_ENCRYPT) {
      ff_pkt->flags &= ~FO_SPARSE;
   }

   /** Note, no sparse option for win32_data */
   if (!is_portable_backup(&ff_pkt->bfd)) {
      stream = STREAM_WIN32_DATA;
      ff_pkt->flags &= ~FO_SPARSE;
   } else if (ff_pkt->flags & FO_SPARSE) {
      stream = STREAM_SPARSE_DATA;
   } else {
      stream = STREAM_FILE_DATA;
   }
   if (ff_pkt->flags & FO_OFFSETS) {
      stream = STREAM_SPARSE_DATA;
   }

   /** Encryption is only supported for file data */
   if (stream != STREAM_FILE_DATA && stream != STREAM_WIN32_DATA &&
         stream != STREAM_MACOS_FORK_DATA) {
      ff_pkt->flags &= ~FO_ENCRYPT;
   }

   /** Compression is not supported for Mac fork data */
   if (stream == STREAM_MACOS_FORK_DATA) {
      ff_pkt->flags &= ~FO_COMPRESS;
   }

   /**
    * Handle compression and encryption options
    */
#if defined(HAVE_LIBZ) || defined(HAVE_LZO)
   if (ff_pkt->flags & FO_COMPRESS) {
      #ifdef HAVE_LIBZ
         if(ff_pkt->Compress_algo == COMPRESS_GZIP) {
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
               ASSERT(!(ff_pkt->flags & FO_COMPRESS));
               return STREAM_NONE;
            }
         }
      #endif
      #ifdef HAVE_LZO
         if(ff_pkt->Compress_algo == COMPRESS_LZO1X) {
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
               /**
                * All stream types that do not support compression should clear out
                * FO_COMPRESS above, and this code block should be unreachable.
                */
               ASSERT(!(ff_pkt->flags & FO_COMPRESS));
               return STREAM_NONE;
            }
         }
      #endif
   }
#endif
#ifdef HAVE_CRYPTO
   if (ff_pkt->flags & FO_ENCRYPT) {
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
         /* All stream types that do not support encryption should clear out
          * FO_ENCRYPT above, and this code block should be unreachable. */
         ASSERT(!(ff_pkt->flags & FO_ENCRYPT));
         return STREAM_NONE;
      }
   }
#endif

   return stream;
}


/**
 * Encode a stat structure into a base64 character string
 *   All systems must create such a structure.
 *   In addition, we tack on the LinkFI, which is non-zero in
 *   the case of a hard linked file that has no data.  This
 *   is a File Index pointing to the link that does have the
 *   data (always the first one encountered in a save).
 * You may piggyback attributes on this packet by encoding
 *   them in the encode_attribsEx() subroutine, but this is
 *   not recommended.
 */
void encode_stat(char *buf, struct stat *statp, int stat_size, int32_t LinkFI, int data_stream)
{
   char *p = buf;

   /*
    * We read the stat packet so make sure the caller's conception
    *  is the same as ours.  They can be different if LARGEFILE is not
    *  the same when compiling this library and the calling program.
    */
   ASSERT(stat_size == (int)sizeof(struct stat));
      
   /**
    *  Encode a stat packet.  I should have done this more intelligently
    *   with a length so that it could be easily expanded.
    */
   p += to_base64((int64_t)statp->st_dev, p);
   *p++ = ' ';                        /* separate fields with a space */
   p += to_base64((int64_t)statp->st_ino, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_mode, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_nlink, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_uid, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_gid, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_rdev, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_size, p);
   *p++ = ' ';
#ifndef HAVE_MINGW
   p += to_base64((int64_t)statp->st_blksize, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_blocks, p);
   *p++ = ' ';
#else
   p += to_base64((int64_t)0, p); /* output place holder */
   *p++ = ' ';
   p += to_base64((int64_t)0, p); /* output place holder */
   *p++ = ' ';
#endif
   p += to_base64((int64_t)statp->st_atime, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_mtime, p);
   *p++ = ' ';
   p += to_base64((int64_t)statp->st_ctime, p);
   *p++ = ' ';
   p += to_base64((int64_t)LinkFI, p);
   *p++ = ' ';

#ifdef HAVE_CHFLAGS
   /* FreeBSD function */
   p += to_base64((int64_t)statp->st_flags, p);  /* output st_flags */
#else
   p += to_base64((int64_t)0, p);     /* output place holder */
#endif
   *p++ = ' ';
   p += to_base64((int64_t)data_stream, p);
   *p = 0;
   return;
}


/* Do casting according to unknown type to keep compiler happy */
#ifdef HAVE_TYPEOF
  #define plug(st, val) st = (typeof st)val
#else
  #if !HAVE_GCC & HAVE_SUN_OS
    /* Sun compiler does not handle templates correctly */
    #define plug(st, val) st = val
  #elif __sgi
    #define plug(st, val) st = val
  #else
    /* Use templates to do the casting */
    template <class T> void plug(T &st, uint64_t val)
      { st = static_cast<T>(val); }
  #endif
#endif


/** Decode a stat packet from base64 characters */
int decode_stat(char *buf, struct stat *statp, int stat_size, int32_t *LinkFI)
{
   char *p = buf;
   int64_t val;

   /*
    * We store into the stat packet so make sure the caller's conception
    *  is the same as ours.  They can be different if LARGEFILE is not
    *  the same when compiling this library and the calling program.
    */
   ASSERT(stat_size == (int)sizeof(struct stat));

   p += from_base64(&val, p);
   plug(statp->st_dev, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_ino, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_mode, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_nlink, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_uid, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_gid, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_rdev, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_size, val);
   p++;
#ifndef HAVE_MINGW
   p += from_base64(&val, p);
   plug(statp->st_blksize, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_blocks, val);
   p++;
#else
   p += from_base64(&val, p);
//   plug(statp->st_blksize, val);
   p++;
   p += from_base64(&val, p);
//   plug(statp->st_blocks, val);
   p++;
#endif
   p += from_base64(&val, p);
   plug(statp->st_atime, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_mtime, val);
   p++;
   p += from_base64(&val, p);
   plug(statp->st_ctime, val);

   /* Optional FileIndex of hard linked file data */
   if (*p == ' ' || (*p != 0 && *(p+1) == ' ')) {
      p++;
      p += from_base64(&val, p);
      *LinkFI = (uint32_t)val;
   } else {
      *LinkFI = 0;
      return 0;
   }

   /* FreeBSD user flags */
   if (*p == ' ' || (*p != 0 && *(p+1) == ' ')) {
      p++;
      p += from_base64(&val, p);
#ifdef HAVE_CHFLAGS
      plug(statp->st_flags, val);
   } else {
      statp->st_flags  = 0;
#endif
   }

   /* Look for data stream id */
   if (*p == ' ' || (*p != 0 && *(p+1) == ' ')) {
      p++;
      p += from_base64(&val, p);
   } else {
      val = 0;
   }
   return (int)val;
}

/** Decode a LinkFI field of encoded stat packet */
int32_t decode_LinkFI(char *buf, struct stat *statp, int stat_size)
{
   char *p = buf;
   int64_t val;
   /*
    * We store into the stat packet so make sure the caller's conception
    *  is the same as ours.  They can be different if LARGEFILE is not
    *  the same when compiling this library and the calling program.
    */
   ASSERT(stat_size == (int)sizeof(struct stat));

   skip_nonspaces(&p);                /* st_dev */
   p++;                               /* skip space */
   skip_nonspaces(&p);                /* st_ino */
   p++;
   p += from_base64(&val, p);
   plug(statp->st_mode, val);         /* st_mode */
   p++;
   skip_nonspaces(&p);                /* st_nlink */
   p++;
   skip_nonspaces(&p);                /* st_uid */
   p++;
   skip_nonspaces(&p);                /* st_gid */
   p++;
   skip_nonspaces(&p);                /* st_rdev */
   p++;
   skip_nonspaces(&p);                /* st_size */
   p++;
   skip_nonspaces(&p);                /* st_blksize */
   p++;
   skip_nonspaces(&p);                /* st_blocks */
   p++;
   skip_nonspaces(&p);                /* st_atime */
   p++;
   skip_nonspaces(&p);                /* st_mtime */
   p++;
   skip_nonspaces(&p);                /* st_ctime */

   /* Optional FileIndex of hard linked file data */
   if (*p == ' ' || (*p != 0 && *(p+1) == ' ')) {
      p++;
      p += from_base64(&val, p);
      return (int32_t)val;
   }
   return 0;
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
bool set_attributes(JCR *jcr, ATTR *attr, BFILE *ofd)
{
   struct utimbuf ut;
   mode_t old_mask;
   bool ok = true;
   boffset_t fsize;
 
   if (uid_set) {
      my_uid = getuid();
      my_gid = getgid();
      uid_set = true;
   }

#if defined(HAVE_WIN32)
   if (attr->stream == STREAM_UNIX_ATTRIBUTES_EX &&
       set_win32_attributes(jcr, attr, ofd)) {
       if (is_bopen(ofd)) {
           bclose(ofd);
       }
       pm_strcpy(attr->ofname, "*none*");
       return true;
   }
   if (attr->data_stream == STREAM_WIN32_DATA ||
       attr->data_stream == STREAM_WIN32_GZIP_DATA ||
       attr->data_stream == STREAM_WIN32_COMPRESSED_DATA) {
      if (is_bopen(ofd)) {
         bclose(ofd);
      }
      pm_strcpy(attr->ofname, "*none*");
      return true;
   }


   /**
    * If Windows stuff failed, e.g. attempt to restore Unix file
    *  to Windows, simply fall through and we will do it the
    *  universal way.
    */
#endif

   old_mask = umask(0);
   if (is_bopen(ofd)) {
      char ec1[50], ec2[50];
      fsize = blseek(ofd, 0, SEEK_END);
      bclose(ofd);                    /* first close file */
      if (attr->type == FT_REG && fsize > 0 && attr->statp.st_size > 0 && 
                        fsize != (boffset_t)attr->statp.st_size) {
         Jmsg3(jcr, M_ERROR, 0, _("File size of restored file %s not correct. Original %s, restored %s.\n"),
            attr->ofname, edit_uint64(attr->statp.st_size, ec1),
            edit_uint64(fsize, ec2));
      }
   }

   /**
    * We do not restore sockets, so skip trying to restore their
    *   attributes.
    */
   if (attr->type == FT_SPEC && S_ISSOCK(attr->statp.st_mode)) {
      goto bail_out;
   }

   ut.actime = attr->statp.st_atime;
   ut.modtime = attr->statp.st_mtime;

   /* ***FIXME**** optimize -- don't do if already correct */
   /**
    * For link, change owner of link using lchown, but don't
    *   try to do a chmod as that will update the file behind it.
    */
   if (attr->type == FT_LNK) {
      /** Change owner of link, not of real file */
      if (lchown(attr->ofname, attr->statp.st_uid, attr->statp.st_gid) < 0 && my_uid == 0) {
         berrno be;
         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file owner %s: ERR=%s\n"),
            attr->ofname, be.bstrerror());
         ok = false;
      }
   } else {
      if (chown(attr->ofname, attr->statp.st_uid, attr->statp.st_gid) < 0 && my_uid == 0) {
         berrno be;
         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file owner %s: ERR=%s\n"),
            attr->ofname, be.bstrerror());
         ok = false;
      }
      if (chmod(attr->ofname, attr->statp.st_mode) < 0 && my_uid == 0) {
         berrno be;
         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file modes %s: ERR=%s\n"),
            attr->ofname, be.bstrerror());
         ok = false;
      }

      /**
       * Reset file times.
       */
      if (utime(attr->ofname, &ut) < 0 && my_uid == 0) {
         berrno be;
         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file times %s: ERR=%s\n"),
            attr->ofname, be.bstrerror());
         ok = false;
      }
#ifdef HAVE_CHFLAGS
      /**
       * FreeBSD user flags
       *
       * Note, this should really be done before the utime() above,
       *  but if the immutable bit is set, it will make the utimes()
       *  fail.
       */
      if (chflags(attr->ofname, attr->statp.st_flags) < 0 && my_uid == 0) {
         berrno be;
         Jmsg2(jcr, M_ERROR, 0, _("Unable to set file flags %s: ERR=%s\n"),
            attr->ofname, be.bstrerror());
         ok = false;
      }
#endif
   }

bail_out:
   pm_strcpy(attr->ofname, "*none*");
   umask(old_mask);
   return ok;
}


/*=============================================================*/
/*                                                             */
/*                 * * *  U n i x * * * *                      */
/*                                                             */
/*=============================================================*/

#if !defined(HAVE_WIN32)

/**
 * It is possible to piggyback additional data e.g. ACLs on
 *   the encode_stat() data by returning the extended attributes
 *   here.  They must be "self-contained" (i.e. you keep track
 *   of your own length), and they must be in ASCII string
 *   format. Using this feature is not recommended.
 * The code below shows how to return nothing.  See the Win32
 *   code below for returning something in the attributes.
 */
int encode_attribsEx(JCR *jcr, char *attribsEx, FF_PKT *ff_pkt)
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
   if (ff_pkt->flags & FO_HFSPLUS) {
      p += to_base64((uint64_t)(ff_pkt->hfsinfo.rsrclength), p);
   }
   *p = 0;
#else
   *attribsEx = 0;                    /* no extended attributes */
#endif
   return STREAM_UNIX_ATTRIBUTES;
}

#endif



/*=============================================================*/
/*                                                             */
/*                 * * *  W i n 3 2 * * * *                    */
/*                                                             */
/*=============================================================*/

#if defined(HAVE_WIN32)

int encode_attribsEx(JCR *jcr, char *attribsEx, FF_PKT *ff_pkt)
{
   char *p = attribsEx;
   WIN32_FILE_ATTRIBUTE_DATA atts;
   ULARGE_INTEGER li;

   attribsEx[0] = 0;                  /* no extended attributes */

   if (jcr->cmd_plugin || ff_pkt->type == FT_DELETED) {
      return STREAM_UNIX_ATTRIBUTES;
   }

   unix_name_to_win32(&ff_pkt->sys_fname, ff_pkt->fname);

   /** try unicode version */
   if (p_GetFileAttributesExW)  {
      POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);   
      make_win32_path_UTF8_2_wchar(&pwszBuf, ff_pkt->fname);

      BOOL b=p_GetFileAttributesExW((LPCWSTR)pwszBuf, GetFileExInfoStandard, 
                                    (LPVOID)&atts);
      free_pool_memory(pwszBuf);

      if (!b) {
         win_error(jcr, "GetFileAttributesExW:", ff_pkt->sys_fname);
         return STREAM_UNIX_ATTRIBUTES;
      }
   }
   else {
      if (!p_GetFileAttributesExA)
         return STREAM_UNIX_ATTRIBUTES;      

      if (!p_GetFileAttributesExA(ff_pkt->sys_fname, GetFileExInfoStandard,
                              (LPVOID)&atts)) {
         win_error(jcr, "GetFileAttributesExA:", ff_pkt->sys_fname);
         return STREAM_UNIX_ATTRIBUTES;
      }
   }

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

/** Define attributes that are legal to set with SetFileAttributes() */
#define SET_ATTRS ( \
         FILE_ATTRIBUTE_ARCHIVE| \
         FILE_ATTRIBUTE_HIDDEN| \
         FILE_ATTRIBUTE_NORMAL| \
         FILE_ATTRIBUTE_NOT_CONTENT_INDEXED| \
         FILE_ATTRIBUTE_OFFLINE| \
         FILE_ATTRIBUTE_READONLY| \
         FILE_ATTRIBUTE_SYSTEM| \
         FILE_ATTRIBUTE_TEMPORARY)


/**
 * Set Extended File Attributes for Win32
 *
 *  fname is the original filename
 *  ofile is the output filename (may be in a different directory)
 *
 * Returns:  true  on success
 *           false on failure
 */
static bool set_win32_attributes(JCR *jcr, ATTR *attr, BFILE *ofd)
{
   char *p = attr->attrEx;
   int64_t val;
   WIN32_FILE_ATTRIBUTE_DATA atts;
   ULARGE_INTEGER li;
   POOLMEM *win32_ofile;

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

   /** Convert to Windows path format */
   win32_ofile = get_pool_memory(PM_FNAME);
   unix_name_to_win32(&win32_ofile, attr->ofname);

   /** At this point, we have reconstructed the WIN32_FILE_ATTRIBUTE_DATA pkt */

   if (!is_bopen(ofd)) {
      Dmsg1(100, "File not open: %s\n", attr->ofname);
      bopen(ofd, attr->ofname, O_WRONLY|O_BINARY, 0);   /* attempt to open the file */
   }

   if (is_bopen(ofd)) {
      Dmsg1(100, "SetFileTime %s\n", attr->ofname);
      if (!SetFileTime(bget_handle(ofd),
                         &atts.ftCreationTime,
                         &atts.ftLastAccessTime,
                         &atts.ftLastWriteTime)) {
         win_error(jcr, "SetFileTime:", win32_ofile);
      }
      bclose(ofd);
   }

   Dmsg1(100, "SetFileAtts %s\n", attr->ofname);
   if (!(atts.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      if (p_SetFileAttributesW) {
         POOLMEM* pwszBuf = get_pool_memory(PM_FNAME);   
         make_win32_path_UTF8_2_wchar(&pwszBuf, attr->ofname);

         BOOL b=p_SetFileAttributesW((LPCWSTR)pwszBuf, atts.dwFileAttributes & SET_ATTRS);
         free_pool_memory(pwszBuf);
      
         if (!b) 
            win_error(jcr, "SetFileAttributesW:", win32_ofile); 
      }
      else {
         if (!p_SetFileAttributesA(win32_ofile, atts.dwFileAttributes & SET_ATTRS)) {
            win_error(jcr, "SetFileAttributesA:", win32_ofile);
         }
      }
   }
   free_pool_memory(win32_ofile);
   return true;
}

void win_error(JCR *jcr, const char *prefix, POOLMEM *win32_ofile)
{
   DWORD lerror = GetLastError();
   LPTSTR msg;
   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|
                 FORMAT_MESSAGE_FROM_SYSTEM,
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

void win_error(JCR *jcr, const char *prefix, DWORD lerror)
{
   LPTSTR msg;
   FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|
                 FORMAT_MESSAGE_FROM_SYSTEM,
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
#endif  /* HAVE_WIN32 */
