/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2010 Free Software Foundation Europe e.V.
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
// Kern Sibbald, April MMIII
/**
 * @file
 * Bareos low level File I/O routines.  This routine simulates
 * open(), read(), write(), and close(), but using native routines.
 * I.e. on Windows, we use Windows APIs.
 */

#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif
#include <netinet/in.h>

#include "include/fcntl_def.h"
#include "include/bareos.h"
#include "include/streams.h"
#include "find.h"
#include "lib/berrno.h"

const int debuglevel = 200;

int (*plugin_bopen)(BareosFilePacket* bfd,
                    const char* fname,
                    int flags,
                    mode_t mode)
    = NULL;
int (*plugin_bclose)(BareosFilePacket* bfd) = NULL;
ssize_t (*plugin_bread)(BareosFilePacket* bfd, void* buf, size_t count) = NULL;
ssize_t (*plugin_bwrite)(BareosFilePacket* bfd, void* buf, size_t count) = NULL;
boffset_t (*plugin_blseek)(BareosFilePacket* bfd, boffset_t offset, int whence)
    = NULL;

#ifdef HAVE_DARWIN_OS
#  include <sys/paths.h>
#endif

/* ===============================================================
 *
 *            U N I X   AND   W I N D O W S
 *
 * ===============================================================
 */

bool is_win32_stream(int stream)
{
  switch (stream) {
    case STREAM_WIN32_DATA:
    case STREAM_WIN32_GZIP_DATA:
    case STREAM_WIN32_COMPRESSED_DATA:
    case STREAM_ENCRYPTED_WIN32_DATA:
    case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
    case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
      return true;
  }
  return false;
}

const char* stream_to_ascii(int stream)
{
  static char buf[20];

  switch (stream & STREAMMASK_TYPE) {
    case STREAM_UNIX_ATTRIBUTES:
      return T_("Unix attributes");
    case STREAM_FILE_DATA:
      return T_("File data");
    case STREAM_MD5_DIGEST:
      return T_("MD5 digest");
    case STREAM_GZIP_DATA:
      return T_("GZIP data");
    case STREAM_COMPRESSED_DATA:
      return T_("Compressed data");
    case STREAM_UNIX_ATTRIBUTES_EX:
      return T_("Extended attributes");
    case STREAM_SPARSE_DATA:
      return T_("Sparse data");
    case STREAM_SPARSE_GZIP_DATA:
      return T_("GZIP sparse data");
    case STREAM_SPARSE_COMPRESSED_DATA:
      return T_("Compressed sparse data");
    case STREAM_PROGRAM_NAMES:
      return T_("Program names");
    case STREAM_PROGRAM_DATA:
      return T_("Program data");
    case STREAM_SHA1_DIGEST:
      return T_("SHA1 digest");
    case STREAM_WIN32_DATA:
      return T_("Win32 data");
    case STREAM_WIN32_GZIP_DATA:
      return T_("Win32 GZIP data");
    case STREAM_WIN32_COMPRESSED_DATA:
      return T_("Win32 compressed data");
    case STREAM_MACOS_FORK_DATA:
      return T_("MacOS Fork data");
    case STREAM_HFSPLUS_ATTRIBUTES:
      return T_("HFS+ attribs");
    case STREAM_UNIX_ACCESS_ACL:
      return T_("Standard Unix ACL attribs");
    case STREAM_UNIX_DEFAULT_ACL:
      return T_("Default Unix ACL attribs");
    case STREAM_SHA256_DIGEST:
      return T_("SHA256 digest");
    case STREAM_SHA512_DIGEST:
      return T_("SHA512 digest");
    case STREAM_XXH128_DIGEST:
      return T_("XXH128 digest");
    case STREAM_SIGNED_DIGEST:
      return T_("Signed digest");
    case STREAM_ENCRYPTED_FILE_DATA:
      return T_("Encrypted File data");
    case STREAM_ENCRYPTED_WIN32_DATA:
      return T_("Encrypted Win32 data");
    case STREAM_ENCRYPTED_SESSION_DATA:
      return T_("Encrypted session data");
    case STREAM_ENCRYPTED_FILE_GZIP_DATA:
      return T_("Encrypted GZIP data");
    case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
      return T_("Encrypted compressed data");
    case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
      return T_("Encrypted Win32 GZIP data");
    case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
      return T_("Encrypted Win32 Compressed data");
    case STREAM_ENCRYPTED_MACOS_FORK_DATA:
      return T_("Encrypted MacOS fork data");
    case STREAM_ACL_AIX_TEXT:
      return T_("AIX Specific ACL attribs");
    case STREAM_ACL_DARWIN_ACCESS_ACL:
      return T_("Darwin Specific ACL attribs");
    case STREAM_ACL_FREEBSD_DEFAULT_ACL:
      return T_("FreeBSD Specific Default ACL attribs");
    case STREAM_ACL_FREEBSD_ACCESS_ACL:
      return T_("FreeBSD Specific Access ACL attribs");
    case STREAM_ACL_HPUX_ACL_ENTRY:
      return T_("HPUX Specific ACL attribs");
    case STREAM_ACL_IRIX_DEFAULT_ACL:
      return T_("Irix Specific Default ACL attribs");
    case STREAM_ACL_IRIX_ACCESS_ACL:
      return T_("Irix Specific Access ACL attribs");
    case STREAM_ACL_LINUX_DEFAULT_ACL:
      return T_("Linux Specific Default ACL attribs");
    case STREAM_ACL_LINUX_ACCESS_ACL:
      return T_("Linux Specific Access ACL attribs");
    case STREAM_ACL_TRU64_DEFAULT_ACL:
      return T_("TRU64 Specific Default ACL attribs");
    case STREAM_ACL_TRU64_ACCESS_ACL:
      return T_("TRU64 Specific Access ACL attribs");
    case STREAM_ACL_SOLARIS_ACLENT:
      return T_("Solaris Specific POSIX ACL attribs");
    case STREAM_ACL_SOLARIS_ACE:
      return T_("Solaris Specific NFSv4/ZFS ACL attribs");
    case STREAM_ACL_AFS_TEXT:
      return T_("AFS Specific ACL attribs");
    case STREAM_ACL_AIX_AIXC:
      return T_("AIX Specific POSIX ACL attribs");
    case STREAM_ACL_AIX_NFS4:
      return T_("AIX Specific NFSv4 ACL attribs");
    case STREAM_ACL_FREEBSD_NFS4_ACL:
      return T_("FreeBSD Specific NFSv4/ZFS ACL attribs");
    case STREAM_ACL_HURD_DEFAULT_ACL:
      return T_("GNU Hurd Specific Default ACL attribs");
    case STREAM_ACL_HURD_ACCESS_ACL:
      return T_("GNU Hurd Specific Access ACL attribs");
    case STREAM_XATTR_HURD:
      return T_("GNU Hurd Specific Extended attribs");
    case STREAM_XATTR_IRIX:
      return T_("IRIX Specific Extended attribs");
    case STREAM_XATTR_TRU64:
      return T_("TRU64 Specific Extended attribs");
    case STREAM_XATTR_AIX:
      return T_("AIX Specific Extended attribs");
    case STREAM_XATTR_OPENBSD:
      return T_("OpenBSD Specific Extended attribs");
    case STREAM_XATTR_SOLARIS_SYS:
      return T_(
          "Solaris Specific Extensible attribs or System Extended attribs");
    case STREAM_XATTR_SOLARIS:
      return T_("Solaris Specific Extended attribs");
    case STREAM_XATTR_DARWIN:
      return T_("Darwin Specific Extended attribs");
    case STREAM_XATTR_FREEBSD:
      return T_("FreeBSD Specific Extended attribs");
    case STREAM_XATTR_LINUX:
      return T_("Linux Specific Extended attribs");
    case STREAM_XATTR_NETBSD:
      return T_("NetBSD Specific Extended attribs");
    default:
      snprintf(buf, 20, "%d", stream);
      return (const char*)buf;
  }
}

//  Convert a 64 bit little endian to a big endian
static inline void int64_LE2BE(int64_t* pBE, const int64_t v)
{
  /* convert little endian to big endian */
  if (htonl(1) != 1L) { /* no work if on little endian machine */
    memcpy(pBE, &v, sizeof(int64_t));
  } else {
    int i;
    uint8_t rv[sizeof(int64_t)];
    uint8_t* pv = (uint8_t*)&v;

    for (i = 0; i < 8; i++) { rv[i] = pv[7 - i]; }
    memcpy(pBE, &rv, sizeof(int64_t));
  }
}

//  Convert a 32 bit little endian to a big endian
static inline void int32_LE2BE(int32_t* pBE, const int32_t v)
{
  /* convert little endian to big endian */
  if (htonl(1) != 1L) { /* no work if on little endian machine */
    memcpy(pBE, &v, sizeof(int32_t));
  } else {
    int i;
    uint8_t rv[sizeof(int32_t)];
    uint8_t* pv = (uint8_t*)&v;

    for (i = 0; i < 4; i++) { rv[i] = pv[3 - i]; }
    memcpy(pBE, &rv, sizeof(int32_t));
  }
}

//  Read a BackupRead block and pull out the file data
bool processWin32BackupAPIBlock(BareosFilePacket* bfd,
                                void* pBuffer,
                                ssize_t dwSize)
{
  /* pByte contains the buffer
     dwSize the len to be processed.  function assumes to be
     called in successive incremental order over the complete
     BackupRead stream beginning at pos 0 and ending at the end.
   */

  PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT* plugin_private_context
      = &(bfd->win32Decomplugin_private_context);
  bool bContinue = false;
  int64_t dwDataOffset = 0;
  int64_t dwDataLen;

  /* Win32 Stream Header size without name of stream.
   * = sizeof (WIN32_STREAM_ID)- sizeof(wchar_t *);
   */
  int32_t dwSizeHeader = 20;

  do {
    if (plugin_private_context->liNextHeader >= dwSize) {
      dwDataLen = dwSize - dwDataOffset;
      bContinue = false; /* 1 iteration is enough */
    } else {
      dwDataLen = plugin_private_context->liNextHeader - dwDataOffset;
      bContinue = true; /* multiple iterations may be necessary */
    }

    /* flush */
    /* copy block of real DATA */
    if (plugin_private_context->bIsInData) {
      if (bwrite(bfd, ((char*)pBuffer) + dwDataOffset, dwDataLen)
          != (ssize_t)dwDataLen)
        return false;
    }

    if (plugin_private_context->liNextHeader
        < dwSize) { /* is a header in this block ? */
      int32_t dwOffsetTarget;
      int32_t dwOffsetSource;

      if (plugin_private_context->liNextHeader < 0) {
        /* start of header was before this block, so we
         * continue with the part in the current block
         */
        dwOffsetTarget = -plugin_private_context->liNextHeader;
        dwOffsetSource = 0;
      } else {
        /* start of header is inside of this block */
        dwOffsetTarget = 0;
        dwOffsetSource = plugin_private_context->liNextHeader;
      }

      int32_t dwHeaderPartLen = dwSizeHeader - dwOffsetTarget;
      bool bHeaderIsComplete;

      if (dwHeaderPartLen <= dwSize - dwOffsetSource) {
        /* header (or rest of header) is completely available
           in current block
         */
        bHeaderIsComplete = true;
      } else {
        /* header will continue in next block */
        bHeaderIsComplete = false;
        dwHeaderPartLen = dwSize - dwOffsetSource;
      }

      /* copy the available portion of header to persistent copy */
      memcpy(((char*)&plugin_private_context->header_stream) + dwOffsetTarget,
             ((char*)pBuffer) + dwOffsetSource, dwHeaderPartLen);

      /* recalculate position of next header */
      if (bHeaderIsComplete) {
        /* convert stream name size (32 bit little endian) to machine type */
        int32_t dwNameSize;
        int32_LE2BE(&dwNameSize,
                    plugin_private_context->header_stream.dwStreamNameSize);
        dwDataOffset
            = dwNameSize + plugin_private_context->liNextHeader + dwSizeHeader;

        /* convert stream size (64 bit little endian) to machine type */
        int64_LE2BE(&(plugin_private_context->liNextHeader),
                    plugin_private_context->header_stream.Size);
        plugin_private_context->liNextHeader += dwDataOffset;

        plugin_private_context->bIsInData
            = plugin_private_context->header_stream.dwStreamId
              == WIN32_BACKUP_DATA;
        if (dwDataOffset == dwSize) bContinue = false;
      } else {
        /* stop and continue with next block */
        bContinue = false;
        plugin_private_context->bIsInData = false;
      }
    }
  } while (bContinue);

  /* set "NextHeader" relative to the beginning of the next block */
  plugin_private_context->liNextHeader -= dwSize;

  return true;
}

/* ===============================================================
 *
 *            W I N D O W S
 *
 * ===============================================================
 */

#if defined(HAVE_WIN32)

/* Imported Functions */
extern void unix_name_to_win32(POOLMEM*& win32_name, const char* name);
extern "C" HANDLE get_osfhandle(int fd);

void binit(BareosFilePacket* bfd)
{
  new (bfd) BareosFilePacket();
  bfd->fh = INVALID_HANDLE_VALUE;
  bfd->use_backup_api = have_win32_api();
}

/**
 * Enables using the Backup API (win32_data).
 *   Returns 1 if function worked
 *   Returns 0 if failed (i.e. do not have Backup API on this machine)
 */
bool set_win32_backup(BareosFilePacket* bfd)
{
  /* We enable if possible here */
  bfd->use_backup_api = have_win32_api();
  return bfd->use_backup_api;
}

bool SetPortableBackup(BareosFilePacket* bfd)
{
  bfd->use_backup_api = false;
  return true;
}

bool SetCmdPlugin(BareosFilePacket* bfd, JobControlRecord* jcr)
{
  bfd->cmd_plugin = true;
  bfd->jcr = jcr;
  return true;
}

/**
 * Return 1 if we are NOT using Win32 BackupWrite()
 * return 0 if are
 */
bool IsPortableBackup(BareosFilePacket* bfd) { return !bfd->use_backup_api; }

bool have_win32_api() { return p_BackupRead && p_BackupWrite; }

/**
 * Return true  if we support the stream
 *        false if we do not support the stream
 *
 *  This code is running under Win32, so we do not need #ifdef on MACOS ...
 */
bool IsRestoreStreamSupported(int stream)
{
  switch (stream) {
    // Streams known not to be supported
    case STREAM_MACOS_FORK_DATA:
    case STREAM_HFSPLUS_ATTRIBUTES:
    case STREAM_ENCRYPTED_MACOS_FORK_DATA:
      return false;

      // Known streams
    case STREAM_GZIP_DATA:
    case STREAM_SPARSE_GZIP_DATA:
    case STREAM_WIN32_GZIP_DATA:
    case STREAM_COMPRESSED_DATA:
    case STREAM_SPARSE_COMPRESSED_DATA:
    case STREAM_WIN32_COMPRESSED_DATA:
    case STREAM_WIN32_DATA:
    case STREAM_UNIX_ATTRIBUTES:
    case STREAM_FILE_DATA:
    case STREAM_MD5_DIGEST:
    case STREAM_UNIX_ATTRIBUTES_EX:
    case STREAM_SPARSE_DATA:
    case STREAM_PROGRAM_NAMES:
    case STREAM_PROGRAM_DATA:
    case STREAM_SHA1_DIGEST:
#  ifdef HAVE_SHA2
    case STREAM_SHA256_DIGEST:
    case STREAM_SHA512_DIGEST:
#  endif
    case STREAM_XXH128_DIGEST:
    case STREAM_SIGNED_DIGEST:
    case STREAM_ENCRYPTED_FILE_DATA:
    case STREAM_ENCRYPTED_FILE_GZIP_DATA:
    case STREAM_ENCRYPTED_WIN32_DATA:
    case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
    case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
    case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
    case 0: /* compatibility with old tapes */
      return true;
  }
  return false;
}

HANDLE BgetHandle(BareosFilePacket* bfd)
{
  return (bfd->mode == BF_CLOSED) ? INVALID_HANDLE_VALUE : bfd->fh;
}

// Windows flags for the OpenEncryptedFileRaw functions.
#  ifndef CREATE_FOR_EXPORT
#    define CREATE_FOR_EXPORT 0
#  endif
#  ifndef CREATE_FOR_IMPORT
#    define CREATE_FOR_IMPORT 1
#  endif
#  ifndef CREATE_FOR_DIR
#    define CREATE_FOR_DIR 2
#  endif
#  ifndef OVERWRITE_HIDDEN
#    define OVERWRITE_HIDDEN 4
#  endif

static inline int BopenEncrypted(BareosFilePacket* bfd,
                                 const char* fname,
                                 int flags,
                                 mode_t mode)
{
  bool is_dir;
  ULONG ulFlags = 0;

  if (!p_OpenEncryptedFileRawA && !p_OpenEncryptedFileRawW) {
    Dmsg0(50,
          "No p_OpenEncryptedFileRawA and no p_OpenEncryptedFileRawW!!!!!\n");
    return 0;
  }

  is_dir = S_ISDIR(mode);

  // Convert to Windows path format

  // See if we open the file for create or read-write.
  if ((flags & O_CREAT) || (flags & O_WRONLY)) {
    if (is_dir) {
      ulFlags |= CREATE_FOR_IMPORT | OVERWRITE_HIDDEN | CREATE_FOR_DIR;
    } else {
      ulFlags |= CREATE_FOR_IMPORT | OVERWRITE_HIDDEN;
    }
    bfd->mode = BF_WRITE;
  } else {
    ulFlags = CREATE_FOR_EXPORT;
    bfd->mode = BF_READ;
  }

  if (p_OpenEncryptedFileRawW && p_MultiByteToWideChar) {
    std::wstring utf16 = make_win32_path_UTF8_2_wchar(fname);

    // Unicode open.
    Dmsg1(100, "OpenEncryptedFileRawW=%s\n", FromUtf16(utf16).c_str());
    if (p_OpenEncryptedFileRawW(utf16.c_str(), ulFlags, &(bfd->pvContext))) {
      bfd->mode = BF_CLOSED;
    }
  } else {
    // ASCII open.
    PoolMem win32_fname(PM_FNAME);
    unix_name_to_win32(win32_fname.addr(), fname);
    Dmsg1(100, "OpenEncryptedFileRawA=%s\n", win32_fname.c_str());
    if (p_OpenEncryptedFileRawA(win32_fname.c_str(), ulFlags,
                                &(bfd->pvContext))) {
      bfd->mode = BF_CLOSED;
    }
  }

  bfd->encrypted = true;

  return bfd->mode == BF_CLOSED ? -1 : 1;
}

static inline int BopenNonencrypted(BareosFilePacket* bfd,
                                    const char* fname,
                                    int flags,
                                    mode_t mode)
{
  DWORD dwaccess, dwflags, dwshare;

  if (bfd->cmd_plugin && plugin_bopen) {
    int rtnstat;
    Dmsg1(50, "call plugin_bopen fname=%s\n", fname);
    rtnstat = plugin_bopen(bfd, fname, flags, mode);
    Dmsg1(50, "return from plugin_bopen status=%d\n", rtnstat);
    if (rtnstat >= 0) {
      if (flags & O_CREAT || flags & O_WRONLY) { /* Open existing for write */
        Dmsg1(50, "plugin_open for write OK file=%s.\n", fname);
        bfd->mode = BF_WRITE;
      } else {
        Dmsg1(50, "plugin_open for read OK file=%s.\n", fname);
        bfd->mode = BF_READ;
      }
    } else {
      bfd->mode = BF_CLOSED;
      Dmsg1(000, "==== plugin_bopen returned bad status=%d\n", rtnstat);
    }
    return bfd->mode == BF_CLOSED ? -1 : 1;
  }
  Dmsg0(50, "=== NO plugin\n");

  if (!(p_CreateFileA || p_CreateFileW)) {
    Dmsg0(50, "No CreateFileA and no CreateFileW!!!!!\n");
    return 0;
  }

  if (flags & O_CREAT) { /* Create */
    if (bfd->use_backup_api) {
      dwaccess = GENERIC_WRITE | FILE_ALL_ACCESS | WRITE_OWNER | WRITE_DAC
                 | ACCESS_SYSTEM_SECURITY;
      dwflags = FILE_FLAG_BACKUP_SEMANTICS;
    } else {
      dwaccess = GENERIC_WRITE;
      dwflags = 0;
    }

    // Unicode open for create write
    if (p_CreateFileW && p_MultiByteToWideChar) {
      std::wstring utf16 = make_win32_path_UTF8_2_wchar(fname);
      Dmsg1(100, "Create CreateFileW=%s\n", FromUtf16(utf16).c_str());
      bfd->fh = p_CreateFileW(utf16.c_str(), dwaccess, /* Requested access */
                              0,                       /* Shared mode */
                              NULL,                    /* SecurityAttributes */
                              CREATE_ALWAYS,           /* CreationDisposition */
                              dwflags, /* Flags and attributes */
                              NULL);   /* TemplateFile */
    } else {
      PoolMem ansi(PM_FNAME);
      unix_name_to_win32(ansi.addr(), fname);
      // ASCII open
      Dmsg1(100, "Create CreateFileA=%s\n", ansi.c_str());
      bfd->fh = p_CreateFileA(ansi.c_str(), dwaccess, /* Requested access */
                              0,                      /* Shared mode */
                              NULL,                   /* SecurityAttributes */
                              CREATE_ALWAYS,          /* CreationDisposition */
                              dwflags,                /* Flags and attributes */
                              NULL);                  /* TemplateFile */
    }

    bfd->mode = BF_WRITE;
  } else if (flags & O_WRONLY) {
    // Open existing for write
    if (bfd->use_backup_api) {
      dwaccess = GENERIC_READ | GENERIC_WRITE | WRITE_OWNER | WRITE_DAC;
      if (flags & O_NOFOLLOW) {
        dwflags = FILE_FLAG_BACKUP_SEMANTICS;
      } else {
        dwflags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT;
      }
    } else {
      dwaccess = GENERIC_READ | GENERIC_WRITE;
      dwflags = 0;
    }

    if (p_CreateFileW && p_MultiByteToWideChar) {
      // unicode open for open existing write
      std::wstring utf16 = make_win32_path_UTF8_2_wchar(fname);
      Dmsg1(100, "Write only CreateFileW=%s\n", FromUtf16(utf16).c_str());
      bfd->fh = p_CreateFileW(utf16.c_str(), dwaccess, /* Requested access */
                              0,                       /* Shared mode */
                              NULL,                    /* SecurityAttributes */
                              OPEN_EXISTING,           /* CreationDisposition */
                              dwflags, /* Flags and attributes */
                              NULL);   /* TemplateFile */
    } else {
      PoolMem ansi(PM_FNAME);
      unix_name_to_win32(ansi.addr(), fname);
      // ASCII open
      Dmsg1(100, "Write only CreateFileA=%s\n", ansi.c_str());
      bfd->fh = p_CreateFileA(ansi.c_str(), dwaccess, /* Requested access */
                              0,                      /* Shared mode */
                              NULL,                   /* SecurityAttributes */
                              OPEN_EXISTING,          /* CreationDisposition */
                              dwflags,                /* Flags and attributes */
                              NULL);                  /* TemplateFile */
    }

    bfd->mode = BF_WRITE;
  } else {
    // Open existing for read
    if (bfd->use_backup_api) {
      dwaccess = GENERIC_READ | READ_CONTROL | ACCESS_SYSTEM_SECURITY;
      if (flags & O_NOFOLLOW) {
        dwflags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN;
      } else {
        dwflags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN
                  | FILE_FLAG_OPEN_REPARSE_POINT;
      }
      dwshare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    } else {
      dwaccess = GENERIC_READ;
      dwflags = 0;
      dwshare = FILE_SHARE_READ | FILE_SHARE_WRITE;
    }

    if (p_CreateFileW && p_MultiByteToWideChar) {
      // Unicode open for open existing read
      std::wstring utf16 = make_win32_path_UTF8_2_wchar(fname);
      Dmsg1(100, "Read CreateFileW=%s\n", FromUtf16(utf16).c_str());
      bfd->fh = p_CreateFileW(utf16.c_str(), dwaccess, /* Requested access */
                              dwshare,                 /* Share modes */
                              NULL,                    /* SecurityAttributes */
                              OPEN_EXISTING,           /* CreationDisposition */
                              dwflags, /* Flags and attributes */
                              NULL);   /* TemplateFile */
    } else {
      PoolMem ansi(PM_FNAME);
      unix_name_to_win32(ansi.addr(), fname);
      // ASCII open
      Dmsg1(100, "Read CreateFileA=%s\n", ansi.c_str());
      bfd->fh = p_CreateFileA(ansi.c_str(), dwaccess, /* Requested access */
                              dwshare,                /* Share modes */
                              NULL,                   /* SecurityAttributes */
                              OPEN_EXISTING,          /* CreationDisposition */
                              dwflags,                /* Flags and attributes */
                              NULL);                  /* TemplateFile */
    }

    bfd->mode = BF_READ;
  }

  if (bfd->fh == INVALID_HANDLE_VALUE) {
    bfd->lerror = GetLastError();
    bfd->BErrNo = b_errno_win32;
    errno = b_errno_win32;
    bfd->mode = BF_CLOSED;
  }

  bfd->errmsg = NULL;
  bfd->lplugin_private_context = NULL;
  bfd->win32Decomplugin_private_context.bIsInData = false;
  bfd->win32Decomplugin_private_context.liNextHeader = 0;

  bfd->encrypted = false;

  return bfd->mode == BF_CLOSED ? -1 : 1;
}

int bopen(BareosFilePacket* bfd,
          const char* fname,
          int flags,
          mode_t mode,
          dev_t rdev)
{
  Dmsg4(100, "bopen: fname %s, flags %08o, mode %04o, rdev %u\n", fname, flags,
        (mode & ~S_IFMT), rdev);

  /* If the FILE_ATTRIBUTES_DEDUPED_ITEM bit is set this is a deduped file
   * we let the bopen function know it should open the file without the
   * FILE_FLAG_OPEN_REPARSE_POINT flag by setting in the O_NOFOLLOW open flag.
   */
  if (rdev & FILE_ATTRIBUTES_DEDUPED_ITEM) { flags |= O_NOFOLLOW; }

  /* If the FILE_ATTRIBUTE_ENCRYPTED bit is set this is an file on an EFS
   * filesystem. For that we need some special handling. */
  if (rdev & FILE_ATTRIBUTE_ENCRYPTED) {
    return BopenEncrypted(bfd, fname, flags, mode);
  } else {
    return BopenNonencrypted(bfd, fname, flags, mode);
  }
}

/**
 * Returns  0 on success
 *         -1 on error
 */
static inline int BcloseEncrypted(BareosFilePacket* bfd)
{
  if (bfd->mode == BF_CLOSED) {
    Dmsg0(50, "=== BFD already closed.\n");
    return 0;
  }

  if (!p_CloseEncryptedFileRaw) {
    Dmsg0(50, "No p_CloseEncryptedFileRaw !!!!!\n");
    return 0;
  }

  p_CloseEncryptedFileRaw(bfd->pvContext);
  bfd->pvContext = NULL;
  bfd->mode = BF_CLOSED;

  return 0;
}

/**
 * Returns  0 on success
 *         -1 on error
 */
static inline int BcloseNonencrypted(BareosFilePacket* bfd)
{
  int status = 0;

  if (bfd->mode == BF_CLOSED) {
    Dmsg0(50, "=== BFD already closed.\n");
    return 0;
  }

  if (bfd->cmd_plugin && plugin_bclose) {
    bfd->do_io_in_core = false;
    status = plugin_bclose(bfd);
    Dmsg0(50, "==== BFD closed!!!\n");
    goto all_done;
  }

  /* We need to tell the API to release the buffer it
   *  allocated in lplugin_private_context.  We do so by calling the
   *  API one more time, but with the Abort bit set. */
  if (bfd->use_backup_api && bfd->mode == BF_READ) {
    BYTE buf[10];
    if (bfd->lplugin_private_context
        && !p_BackupRead(bfd->fh, buf,                     /* buffer */
                         (DWORD)0,                         /* bytes to read */
                         &bfd->rw_bytes,                   /* bytes read */
                         1,                                /* Abort */
                         1,                                /* ProcessSecurity */
                         &bfd->lplugin_private_context)) { /* Read context */
      errno = b_errno_win32;
      status = -1;
    }
  } else if (bfd->use_backup_api && bfd->mode == BF_WRITE) {
    BYTE buf[10];
    if (bfd->lplugin_private_context
        && !p_BackupWrite(bfd->fh, buf,   /* buffer */
                          (DWORD)0,       /* bytes to read */
                          &bfd->rw_bytes, /* bytes written */
                          1,              /* Abort */
                          1,              /* ProcessSecurity */
                          &bfd->lplugin_private_context)) { /* Write context */
      errno = b_errno_win32;
      status = -1;
    }
  }
  if (!CloseHandle(bfd->fh)) {
    status = -1;
    errno = b_errno_win32;
  }

all_done:
  if (bfd->errmsg) {
    FreePoolMemory(bfd->errmsg);
    bfd->errmsg = NULL;
  }
  bfd->mode = BF_CLOSED;
  bfd->lplugin_private_context = NULL;
  bfd->cmd_plugin = false;
  bfd->do_io_in_core = false;

  return status;
}

/**
 * Returns  0 on success
 *         -1 on error
 */
int bclose(BareosFilePacket* bfd)
{
  if (bfd->encrypted) {
    return BcloseEncrypted(bfd);
  } else {
    return BcloseNonencrypted(bfd);
  }
}

/* Returns: bytes read on success
 *           0         on EOF
 *          -1         on error
 */
ssize_t bread(BareosFilePacket* bfd, void* buf, size_t count)
{
  bfd->rw_bytes = 0;

  if (bfd->cmd_plugin && plugin_bread) {
    // invalid filehandle -> plugin does read
    if (bfd->fh == INVALID_HANDLE_VALUE) {
      Dmsg1(400, "bread handled in plugin\n");
      return plugin_bread(bfd, buf, count);
    }
    Dmsg1(400, "bread handled in core via bfd->fh=%p\n", bfd->fh);
  }
  if (bfd->use_backup_api) {
    if (!p_BackupRead(bfd->fh, (BYTE*)buf, count, &bfd->rw_bytes,
                      0,                                /* no Abort */
                      1,                                /* Process Security */
                      &bfd->lplugin_private_context)) { /* Context */
      bfd->lerror = GetLastError();
      bfd->BErrNo = b_errno_win32;
      errno = b_errno_win32;
      return -1;
    }
  } else {
    if (!ReadFile(bfd->fh, buf, count, &bfd->rw_bytes, NULL)) {
      bfd->lerror = GetLastError();
      bfd->BErrNo = b_errno_win32;
      errno = b_errno_win32;
      return -1;
    }
  }

  return (ssize_t)bfd->rw_bytes;
}

ssize_t bwrite(BareosFilePacket* bfd, void* buf, size_t count)
{
  bfd->rw_bytes = 0;

  if (bfd->cmd_plugin && plugin_bwrite) {
    // invalid filehandle -> plugin does read
    if (bfd->fh == INVALID_HANDLE_VALUE) {
      return plugin_bwrite(bfd, buf, count);
    }
    Dmsg1(400, "bwrite handled in core via bfd->fh=%p\n", bfd->fh);
  }


  if (bfd->use_backup_api) {
    if (!p_BackupWrite(bfd->fh, (BYTE*)buf, count, &bfd->rw_bytes,
                       0,                                /* No abort */
                       1,                                /* Process Security */
                       &bfd->lplugin_private_context)) { /* Context */
      bfd->lerror = GetLastError();
      bfd->BErrNo = b_errno_win32;
      errno = b_errno_win32;
      return -1;
    }
  } else {
    if (!WriteFile(bfd->fh, buf, count, &bfd->rw_bytes, NULL)) {
      bfd->lerror = GetLastError();
      bfd->BErrNo = b_errno_win32;
      errno = b_errno_win32;
      return -1;
    }
  }

  return (ssize_t)bfd->rw_bytes;
}

bool IsBopen(BareosFilePacket* bfd) { return bfd->mode != BF_CLOSED; }

boffset_t blseek(BareosFilePacket* bfd, boffset_t offset, int whence)
{
  LONG offset_low = (LONG)offset;
  LONG offset_high = (LONG)(offset >> 32);
  DWORD dwResult;

  if (bfd->cmd_plugin && plugin_blseek) {
    return plugin_blseek(bfd, offset, whence);
  }

  dwResult = SetFilePointer(bfd->fh, offset_low, &offset_high, whence);

  if (dwResult == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
    return (boffset_t)-1;
  }

  return ((boffset_t)offset_high << 32) | dwResult;
}

#else /* Unix systems */

/* ===============================================================
 *
 *            U N I X
 *
 * ===============================================================
 */
void binit(BareosFilePacket* bfd) { bfd->filedes = kInvalidFiledescriptor; }

bool have_win32_api() { return false; /* no can do */ }

/**
 * Enables using the Backup API (win32_data).
 *   Returns true  if function worked
 *   Returns false if failed (i.e. do not have Backup API on this machine)
 */
bool set_win32_backup(BareosFilePacket*) { return false; /* no can do */ }

bool SetPortableBackup(BareosFilePacket*) { return true; /* no problem */ }

/**
 * Return true  if we are writing in portable format
 * return false if not
 */
bool IsPortableBackup(BareosFilePacket*)
{
  return true; /* portable by definition */
}

bool SetCmdPlugin(BareosFilePacket* bfd, JobControlRecord* jcr)
{
  bfd->cmd_plugin = true;
  bfd->jcr = jcr;
  return true;
}

// This code is running on a non-Win32 machine
bool IsRestoreStreamSupported(int stream)
{
  /* No Win32 backup on this machine */
  switch (stream) {
#  ifndef HAVE_DARWIN_OS
    case STREAM_MACOS_FORK_DATA:
    case STREAM_HFSPLUS_ATTRIBUTES:
#  endif
      return false;

      /* Known streams */
    case STREAM_GZIP_DATA:
    case STREAM_SPARSE_GZIP_DATA:
    case STREAM_WIN32_GZIP_DATA:
    case STREAM_COMPRESSED_DATA:
    case STREAM_SPARSE_COMPRESSED_DATA:
    case STREAM_WIN32_COMPRESSED_DATA:
    case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
    case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
    case STREAM_WIN32_DATA:
    case STREAM_UNIX_ATTRIBUTES:
    case STREAM_FILE_DATA:
    case STREAM_MD5_DIGEST:
    case STREAM_UNIX_ATTRIBUTES_EX:
    case STREAM_SPARSE_DATA:
    case STREAM_PROGRAM_NAMES:
    case STREAM_PROGRAM_DATA:
    case STREAM_SHA1_DIGEST:
#  ifdef HAVE_SHA2
    case STREAM_SHA256_DIGEST:
    case STREAM_SHA512_DIGEST:
#  endif
    case STREAM_XXH128_DIGEST:
    case STREAM_SIGNED_DIGEST:
    case STREAM_ENCRYPTED_FILE_DATA:
    case STREAM_ENCRYPTED_FILE_GZIP_DATA:
    case STREAM_ENCRYPTED_WIN32_DATA:
    case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
#  ifdef HAVE_DARWIN_OS
    case STREAM_MACOS_FORK_DATA:
    case STREAM_HFSPLUS_ATTRIBUTES:
    case STREAM_ENCRYPTED_MACOS_FORK_DATA:
#  endif /* HAVE_DARWIN_OS */
    case 0: /* compatibility with old tapes */
      return true;
  }
  return false;
}

int bopen(BareosFilePacket* bfd,
          const char* fname,
          int flags,
          mode_t mode,
          dev_t rdev)
{
  Dmsg4(100, "bopen: fname %s, flags %08o, mode %04o, rdev %llu\n", fname,
        flags, (mode & ~S_IFMT), static_cast<long long unsigned>(rdev));

  if (bfd->cmd_plugin && plugin_bopen) {
    Dmsg1(400, "call plugin_bopen fname=%s\n", fname);
    int retval = plugin_bopen(bfd, fname, flags, mode);
    Dmsg1(400, "Plugin bopen stat=%d\n", retval);
    return retval;
  }

  /* Normal file open */
  Dmsg1(debuglevel, "open file %s\n", fname);

  /* We use fnctl to set O_NOATIME if requested to avoid open error */
  bfd->filedes = open(fname, flags & ~O_NOATIME, mode);

  /* Set O_NOATIME if possible */
  if (bfd->filedes != -1 && flags & O_NOATIME) {
    int oldflags = fcntl(bfd->filedes, F_GETFL, 0);
    if (oldflags == -1) {
      bfd->BErrNo = errno;
      close(bfd->filedes);
      bfd->filedes = -1;
    } else {
      int ret = fcntl(bfd->filedes, F_SETFL, oldflags | O_NOATIME);
      /* EPERM means setting O_NOATIME was not allowed  */
      if (ret == -1 && errno != EPERM) {
        bfd->BErrNo = errno;
        close(bfd->filedes);
        bfd->filedes = -1;
      }
    }
  }
  bfd->BErrNo = errno;
  bfd->flags_ = flags;
  Dmsg1(400, "Open file %d\n", bfd->filedes);
  errno = bfd->BErrNo;

  bfd->win32Decomplugin_private_context.bIsInData = false;
  bfd->win32Decomplugin_private_context.liNextHeader = 0;

#  if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
  /* If not RDWR or WRONLY must be Read Only */
  if (bfd->filedes != -1 && !(flags & (O_RDWR | O_WRONLY))) {
    int status = posix_fadvise(bfd->filedes, 0, 0, POSIX_FADV_WILLNEED);
    Dmsg3(400, "Did posix_fadvise WILLNEED on %s filedes=%d status=%d\n", fname,
          bfd->filedes, status);
  }
#  endif

  return bfd->filedes;
}

#  ifdef HAVE_DARWIN_OS
// Open the resource fork of a file.
int BopenRsrc(BareosFilePacket* bfd, const char* fname, int flags, mode_t mode)
{
  POOLMEM* rsrc_fname;

  rsrc_fname = GetPoolMemory(PM_FNAME);
  PmStrcpy(rsrc_fname, fname);
  PmStrcat(rsrc_fname, _PATH_RSRCFORKSPEC);
  bopen(bfd, rsrc_fname, flags, mode, 0);
  FreePoolMemory(rsrc_fname);

  return bfd->filedes;
}
#  else
int BopenRsrc(BareosFilePacket*, const char*, int, mode_t) { return -1; }
#  endif

int bclose(BareosFilePacket* bfd)
{
  int status;

  if (bfd->filedes == -1) { return 0; }

  Dmsg1(400, "Close file %d\n", bfd->filedes);

  if (bfd->cmd_plugin && plugin_bclose) {
    status = plugin_bclose(bfd);
    bfd->filedes = -1;
    bfd->do_io_in_core = false;
    bfd->cmd_plugin = false;
  } else {
#  if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_DONTNEED)
    /* If not RDWR or WRONLY must be Read Only */
    if (!(bfd->flags_ & (O_RDWR | O_WRONLY))) {
      /* Tell OS we don't need it any more */
      posix_fadvise(bfd->filedes, 0, 0, POSIX_FADV_DONTNEED);
      Dmsg1(400, "Did posix_fadvise DONTNEED on filedes=%d\n", bfd->filedes);
    }
#  endif

    /* Close normal file */
    status = close(bfd->filedes);
    bfd->BErrNo = errno;
    bfd->filedes = -1;
    bfd->cmd_plugin = false;
    bfd->do_io_in_core = false;
  }

  return status;
}

ssize_t bread(BareosFilePacket* bfd, void* buf, size_t count)
{
  if (bfd->cmd_plugin && plugin_bread)
    // plugin does read/write
    if (!bfd->do_io_in_core) { return plugin_bread(bfd, buf, count); }

  char* ptr = static_cast<char*>(buf);

  Dmsg1(400, "bread handled in core via bfd->filedes=%d\n", bfd->filedes);
  ASSERT(static_cast<ssize_t>(count) >= 0);
  ssize_t bytes_read = 0;
  while (bytes_read < static_cast<ssize_t>(count)) {
    ssize_t status = read(bfd->filedes, ptr + bytes_read, count - bytes_read);
    if (status < 0) {
      // error while reading
      bytes_read = status;
      break;
    } else if (status == 0) {
      // no more data left
      break;
    } else {
      bytes_read += status;
    }
  }
  bfd->BErrNo = errno;
  return bytes_read;
}

ssize_t bwrite(BareosFilePacket* bfd, void* buf, size_t count)
{
  if (bfd->cmd_plugin && plugin_bwrite)
    // plugin does read/write
    if (!bfd->do_io_in_core) { return plugin_bwrite(bfd, buf, count); }

  const char* ptr = static_cast<const char*>(buf);

  Dmsg1(400, "bwrite handled in core via bfd->filedes=%d\n", bfd->filedes);
  ASSERT(static_cast<ssize_t>(count) >= 0);
  ssize_t bytes_written = 0;
  while (bytes_written < static_cast<ssize_t>(count)) {
    ssize_t status
        = write(bfd->filedes, ptr + bytes_written, count - bytes_written);
    if (status < 0) {
      // error while reading
      bytes_written = status;
      break;
    } else if (status == 0) {
      // no more data left
      break;
    } else {
      bytes_written += status;
    }
  }
  bfd->BErrNo = errno;
  return bytes_written;
}

bool IsBopen(BareosFilePacket* bfd) { return bfd->filedes >= 0; }

boffset_t blseek(BareosFilePacket* bfd, boffset_t offset, int whence)
{
  boffset_t pos;

  if (bfd->cmd_plugin && plugin_bwrite) {
    return plugin_blseek(bfd, offset, whence);
  }
  pos = (boffset_t)lseek(bfd->filedes, offset, whence);
  bfd->BErrNo = errno;
  return pos;
}
#endif
