/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2010 Free Software Foundation Europe e.V.
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
/*
 * Kern Sibbald, April MMIII
 */
/**
 * @file
 * Bareos low level File I/O routines.  This routine simulates
 * open(), read(), write(), and close(), but using native routines.
 * I.e. on Windows, we use Windows APIs.
 */

#include "include/bareos.h"
#include "find.h"
#include "lib/berrno.h"

const int debuglevel = 200;

int (*plugin_bopen)(BareosWinFilePacket* bfd,
                    const char* fname,
                    int flags,
                    mode_t mode) = NULL;
int (*plugin_bclose)(BareosWinFilePacket* bfd) = NULL;
ssize_t (*plugin_bread)(BareosWinFilePacket* bfd,
                        void* buf,
                        size_t count) = NULL;
ssize_t (*plugin_bwrite)(BareosWinFilePacket* bfd,
                         void* buf,
                         size_t count) = NULL;
boffset_t (*plugin_blseek)(BareosWinFilePacket* bfd,
                           boffset_t offset,
                           int whence) = NULL;

#ifdef HAVE_DARWIN_OS
#include <sys/paths.h>
#endif

#if !defined(HAVE_FDATASYNC)
#define fdatasync(fd)
#endif

#ifdef HAVE_WIN32
void PauseMsg(const char* file, const char* func, int line, const char* msg)
{
  char buf[1000];
  if (msg) {
    Bsnprintf(buf, sizeof(buf), "%s:%s:%d %s", file, func, line, msg);
  } else {
    Bsnprintf(buf, sizeof(buf), "%s:%s:%d", file, func, line);
  }
  MessageBox(NULL, buf, "Pause", MB_OK);
}
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
      return _("Unix attributes");
    case STREAM_FILE_DATA:
      return _("File data");
    case STREAM_MD5_DIGEST:
      return _("MD5 digest");
    case STREAM_GZIP_DATA:
      return _("GZIP data");
    case STREAM_COMPRESSED_DATA:
      return _("Compressed data");
    case STREAM_UNIX_ATTRIBUTES_EX:
      return _("Extended attributes");
    case STREAM_SPARSE_DATA:
      return _("Sparse data");
    case STREAM_SPARSE_GZIP_DATA:
      return _("GZIP sparse data");
    case STREAM_SPARSE_COMPRESSED_DATA:
      return _("Compressed sparse data");
    case STREAM_PROGRAM_NAMES:
      return _("Program names");
    case STREAM_PROGRAM_DATA:
      return _("Program data");
    case STREAM_SHA1_DIGEST:
      return _("SHA1 digest");
    case STREAM_WIN32_DATA:
      return _("Win32 data");
    case STREAM_WIN32_GZIP_DATA:
      return _("Win32 GZIP data");
    case STREAM_WIN32_COMPRESSED_DATA:
      return _("Win32 compressed data");
    case STREAM_MACOS_FORK_DATA:
      return _("MacOS Fork data");
    case STREAM_HFSPLUS_ATTRIBUTES:
      return _("HFS+ attribs");
    case STREAM_UNIX_ACCESS_ACL:
      return _("Standard Unix ACL attribs");
    case STREAM_UNIX_DEFAULT_ACL:
      return _("Default Unix ACL attribs");
    case STREAM_SHA256_DIGEST:
      return _("SHA256 digest");
    case STREAM_SHA512_DIGEST:
      return _("SHA512 digest");
    case STREAM_SIGNED_DIGEST:
      return _("Signed digest");
    case STREAM_ENCRYPTED_FILE_DATA:
      return _("Encrypted File data");
    case STREAM_ENCRYPTED_WIN32_DATA:
      return _("Encrypted Win32 data");
    case STREAM_ENCRYPTED_SESSION_DATA:
      return _("Encrypted session data");
    case STREAM_ENCRYPTED_FILE_GZIP_DATA:
      return _("Encrypted GZIP data");
    case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
      return _("Encrypted compressed data");
    case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
      return _("Encrypted Win32 GZIP data");
    case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
      return _("Encrypted Win32 Compressed data");
    case STREAM_ENCRYPTED_MACOS_FORK_DATA:
      return _("Encrypted MacOS fork data");
    case STREAM_ACL_AIX_TEXT:
      return _("AIX Specific ACL attribs");
    case STREAM_ACL_DARWIN_ACCESS_ACL:
      return _("Darwin Specific ACL attribs");
    case STREAM_ACL_FREEBSD_DEFAULT_ACL:
      return _("FreeBSD Specific Default ACL attribs");
    case STREAM_ACL_FREEBSD_ACCESS_ACL:
      return _("FreeBSD Specific Access ACL attribs");
    case STREAM_ACL_HPUX_ACL_ENTRY:
      return _("HPUX Specific ACL attribs");
    case STREAM_ACL_IRIX_DEFAULT_ACL:
      return _("Irix Specific Default ACL attribs");
    case STREAM_ACL_IRIX_ACCESS_ACL:
      return _("Irix Specific Access ACL attribs");
    case STREAM_ACL_LINUX_DEFAULT_ACL:
      return _("Linux Specific Default ACL attribs");
    case STREAM_ACL_LINUX_ACCESS_ACL:
      return _("Linux Specific Access ACL attribs");
    case STREAM_ACL_TRU64_DEFAULT_ACL:
      return _("TRU64 Specific Default ACL attribs");
    case STREAM_ACL_TRU64_ACCESS_ACL:
      return _("TRU64 Specific Access ACL attribs");
    case STREAM_ACL_SOLARIS_ACLENT:
      return _("Solaris Specific POSIX ACL attribs");
    case STREAM_ACL_SOLARIS_ACE:
      return _("Solaris Specific NFSv4/ZFS ACL attribs");
    case STREAM_ACL_AFS_TEXT:
      return _("AFS Specific ACL attribs");
    case STREAM_ACL_AIX_AIXC:
      return _("AIX Specific POSIX ACL attribs");
    case STREAM_ACL_AIX_NFS4:
      return _("AIX Specific NFSv4 ACL attribs");
    case STREAM_ACL_FREEBSD_NFS4_ACL:
      return _("FreeBSD Specific NFSv4/ZFS ACL attribs");
    case STREAM_ACL_HURD_DEFAULT_ACL:
      return _("GNU Hurd Specific Default ACL attribs");
    case STREAM_ACL_HURD_ACCESS_ACL:
      return _("GNU Hurd Specific Access ACL attribs");
    case STREAM_XATTR_HURD:
      return _("GNU Hurd Specific Extended attribs");
    case STREAM_XATTR_IRIX:
      return _("IRIX Specific Extended attribs");
    case STREAM_XATTR_TRU64:
      return _("TRU64 Specific Extended attribs");
    case STREAM_XATTR_AIX:
      return _("AIX Specific Extended attribs");
    case STREAM_XATTR_OPENBSD:
      return _("OpenBSD Specific Extended attribs");
    case STREAM_XATTR_SOLARIS_SYS:
      return _(
          "Solaris Specific Extensible attribs or System Extended attribs");
    case STREAM_XATTR_SOLARIS:
      return _("Solaris Specific Extended attribs");
    case STREAM_XATTR_DARWIN:
      return _("Darwin Specific Extended attribs");
    case STREAM_XATTR_FREEBSD:
      return _("FreeBSD Specific Extended attribs");
    case STREAM_XATTR_LINUX:
      return _("Linux Specific Extended attribs");
    case STREAM_XATTR_NETBSD:
      return _("NetBSD Specific Extended attribs");
    default:
      sprintf(buf, "%d", stream);
      return (const char*)buf;
  }
}

/**
 *  Convert a 64 bit little endian to a big endian
 */
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

/**
 *  Convert a 32 bit little endian to a big endian
 */
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

/**
 *  Read a BackupRead block and pull out the file data
 */
bool processWin32BackupAPIBlock(BareosWinFilePacket* bfd,
                                void* pBuffer,
                                ssize_t dwSize)
{
  /* pByte contains the buffer
     dwSize the len to be processed.  function assumes to be
     called in successive incremental order over the complete
     BackupRead stream beginning at pos 0 and ending at the end.
   */

  PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT* pContext = &(bfd->win32DecompContext);
  bool bContinue = false;
  int64_t dwDataOffset = 0;
  int64_t dwDataLen;

  /* Win32 Stream Header size without name of stream.
   * = sizeof (WIN32_STREAM_ID)- sizeof(wchar_t *);
   */
  int32_t dwSizeHeader = 20;

  do {
    if (pContext->liNextHeader >= dwSize) {
      dwDataLen = dwSize - dwDataOffset;
      bContinue = false; /* 1 iteration is enough */
    } else {
      dwDataLen = pContext->liNextHeader - dwDataOffset;
      bContinue = true; /* multiple iterations may be necessary */
    }

    /* flush */
    /* copy block of real DATA */
    if (pContext->bIsInData) {
      if (bwrite(bfd, ((char*)pBuffer) + dwDataOffset, dwDataLen) !=
          (ssize_t)dwDataLen)
        return false;
    }

    if (pContext->liNextHeader < dwSize) { /* is a header in this block ? */
      int32_t dwOffsetTarget;
      int32_t dwOffsetSource;

      if (pContext->liNextHeader < 0) {
        /* start of header was before this block, so we
         * continue with the part in the current block
         */
        dwOffsetTarget = -pContext->liNextHeader;
        dwOffsetSource = 0;
      } else {
        /* start of header is inside of this block */
        dwOffsetTarget = 0;
        dwOffsetSource = pContext->liNextHeader;
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
      memcpy(((char*)&pContext->header_stream) + dwOffsetTarget,
             ((char*)pBuffer) + dwOffsetSource, dwHeaderPartLen);

      /* recalculate position of next header */
      if (bHeaderIsComplete) {
        /* convert stream name size (32 bit little endian) to machine type */
        int32_t dwNameSize;
        int32_LE2BE(&dwNameSize, pContext->header_stream.dwStreamNameSize);
        dwDataOffset = dwNameSize + pContext->liNextHeader + dwSizeHeader;

        /* convert stream size (64 bit little endian) to machine type */
        int64_LE2BE(&(pContext->liNextHeader), pContext->header_stream.Size);
        pContext->liNextHeader += dwDataOffset;

        pContext->bIsInData =
            pContext->header_stream.dwStreamId == WIN32_BACKUP_DATA;
        if (dwDataOffset == dwSize) bContinue = false;
      } else {
        /* stop and continue with next block */
        bContinue = false;
        pContext->bIsInData = false;
      }
    }
  } while (bContinue);

  /* set "NextHeader" relative to the beginning of the next block */
  pContext->liNextHeader -= dwSize;

  return TRUE;
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

void binit(BareosWinFilePacket* bfd)
{
  memset(bfd, 0, sizeof(BareosWinFilePacket));
  bfd->fid = -1;
  bfd->mode = BF_CLOSED;
  bfd->use_backup_api = have_win32_api();
  bfd->cmd_plugin = false;
  bfd->pvContext = NULL;
}

/**
 * Enables using the Backup API (win32_data).
 *   Returns 1 if function worked
 *   Returns 0 if failed (i.e. do not have Backup API on this machine)
 */
bool set_win32_backup(BareosWinFilePacket* bfd)
{
  /* We enable if possible here */
  bfd->use_backup_api = have_win32_api();
  return bfd->use_backup_api;
}

bool SetPortableBackup(BareosWinFilePacket* bfd)
{
  bfd->use_backup_api = false;
  return true;
}

bool SetCmdPlugin(BareosWinFilePacket* bfd, JobControlRecord* jcr)
{
  bfd->cmd_plugin = true;
  bfd->jcr = jcr;
  return true;
}

/**
 * Return 1 if we are NOT using Win32 BackupWrite()
 * return 0 if are
 */
bool IsPortableBackup(BareosWinFilePacket* bfd) { return !bfd->use_backup_api; }

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
    /*
     * Streams known not to be supported
     */
#ifndef HAVE_LIBZ
    case STREAM_GZIP_DATA:
    case STREAM_SPARSE_GZIP_DATA:
    case STREAM_WIN32_GZIP_DATA:
#endif
    case STREAM_MACOS_FORK_DATA:
    case STREAM_HFSPLUS_ATTRIBUTES:
    case STREAM_ENCRYPTED_MACOS_FORK_DATA:
      return false;

      /*
       * Known streams
       */
#ifdef HAVE_LIBZ
    case STREAM_GZIP_DATA:
    case STREAM_SPARSE_GZIP_DATA:
    case STREAM_WIN32_GZIP_DATA:
#endif
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
#ifdef HAVE_SHA2
    case STREAM_SHA256_DIGEST:
    case STREAM_SHA512_DIGEST:
#endif
#ifdef HAVE_CRYPTO
    case STREAM_SIGNED_DIGEST:
    case STREAM_ENCRYPTED_FILE_DATA:
    case STREAM_ENCRYPTED_FILE_GZIP_DATA:
    case STREAM_ENCRYPTED_WIN32_DATA:
    case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
    case STREAM_ENCRYPTED_FILE_COMPRESSED_DATA:
    case STREAM_ENCRYPTED_WIN32_COMPRESSED_DATA:
#endif      /* !HAVE_CRYPTO */
    case 0: /* compatibility with old tapes */
      return true;
  }
  return false;
}

HANDLE BgetHandle(BareosWinFilePacket* bfd)
{
  return (bfd->mode == BF_CLOSED) ? INVALID_HANDLE_VALUE : bfd->fh;
}

/**
 * Windows flags for the OpenEncryptedFileRaw functions.
 */
#ifndef CREATE_FOR_EXPORT
#define CREATE_FOR_EXPORT 0
#endif
#ifndef CREATE_FOR_IMPORT
#define CREATE_FOR_IMPORT 1
#endif
#ifndef CREATE_FOR_DIR
#define CREATE_FOR_DIR 2
#endif
#ifndef OVERWRITE_HIDDEN
#define OVERWRITE_HIDDEN 4
#endif

static inline int BopenEncrypted(BareosWinFilePacket* bfd,
                                 const char* fname,
                                 int flags,
                                 mode_t mode)
{
  bool is_dir;
  ULONG ulFlags = 0;
  POOLMEM* win32_fname;
  POOLMEM* win32_fname_wchar;

  if (!p_OpenEncryptedFileRawA && !p_OpenEncryptedFileRawW) {
    Dmsg0(50,
          "No p_OpenEncryptedFileRawA and no p_OpenEncryptedFileRawW!!!!!\n");
    return 0;
  }

  is_dir = S_ISDIR(mode);

  /*
   * Convert to Windows path format
   */
  win32_fname = GetPoolMemory(PM_FNAME);
  win32_fname_wchar = GetPoolMemory(PM_FNAME);

  unix_name_to_win32(win32_fname, (char*)fname);
  if (p_OpenEncryptedFileRawW && p_MultiByteToWideChar) {
    make_win32_path_UTF8_2_wchar(win32_fname_wchar, fname);
  }

  /*
   * See if we open the file for create or read-write.
   */
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
    /*
     * Unicode open.
     */
    Dmsg1(100, "OpenEncryptedFileRawW=%s\n", win32_fname);
    if (p_OpenEncryptedFileRawW((LPCWSTR)win32_fname_wchar, ulFlags,
                                &(bfd->pvContext))) {
      bfd->mode = BF_CLOSED;
    }
  } else {
    /*
     * ASCII open.
     */
    Dmsg1(100, "OpenEncryptedFileRawA=%s\n", win32_fname);
    if (p_OpenEncryptedFileRawA(win32_fname_wchar, ulFlags,
                                &(bfd->pvContext))) {
      bfd->mode = BF_CLOSED;
    }
  }

  FreePoolMemory(win32_fname_wchar);
  FreePoolMemory(win32_fname);

  bfd->encrypted = true;

  return bfd->mode == BF_CLOSED ? -1 : 1;
}

static inline int BopenNonencrypted(BareosWinFilePacket* bfd,
                                    const char* fname,
                                    int flags,
                                    mode_t mode)
{
  POOLMEM* win32_fname;
  POOLMEM* win32_fname_wchar;
  DWORD dwaccess, dwflags, dwshare;

  /*
   * Convert to Windows path format
   */
  win32_fname = GetPoolMemory(PM_FNAME);
  win32_fname_wchar = GetPoolMemory(PM_FNAME);

  unix_name_to_win32(win32_fname, (char*)fname);
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
    FreePoolMemory(win32_fname_wchar);
    FreePoolMemory(win32_fname);
    return bfd->mode == BF_CLOSED ? -1 : 1;
  }
  Dmsg0(50, "=== NO plugin\n");

  if (!(p_CreateFileA || p_CreateFileW)) {
    Dmsg0(50, "No CreateFileA and no CreateFileW!!!!!\n");
    return 0;
  }

  if (p_CreateFileW && p_MultiByteToWideChar) {
    make_win32_path_UTF8_2_wchar(win32_fname_wchar, fname);
  }

  if (flags & O_CREAT) { /* Create */
    if (bfd->use_backup_api) {
      dwaccess = GENERIC_WRITE | FILE_ALL_ACCESS | WRITE_OWNER | WRITE_DAC |
                 ACCESS_SYSTEM_SECURITY;
      dwflags = FILE_FLAG_BACKUP_SEMANTICS;
    } else {
      dwaccess = GENERIC_WRITE;
      dwflags = 0;
    }

    /*
     * Unicode open for create write
     */
    if (p_CreateFileW && p_MultiByteToWideChar) {
      Dmsg1(100, "Create CreateFileW=%s\n", win32_fname);
      bfd->fh = p_CreateFileW((LPCWSTR)win32_fname_wchar,
                              dwaccess,      /* Requested access */
                              0,             /* Shared mode */
                              NULL,          /* SecurityAttributes */
                              CREATE_ALWAYS, /* CreationDisposition */
                              dwflags,       /* Flags and attributes */
                              NULL);         /* TemplateFile */
    } else {
      /*
       * ASCII open
       */
      Dmsg1(100, "Create CreateFileA=%s\n", win32_fname);
      bfd->fh = p_CreateFileA(win32_fname, dwaccess, /* Requested access */
                              0,                     /* Shared mode */
                              NULL,                  /* SecurityAttributes */
                              CREATE_ALWAYS,         /* CreationDisposition */
                              dwflags,               /* Flags and attributes */
                              NULL);                 /* TemplateFile */
    }

    bfd->mode = BF_WRITE;
  } else if (flags & O_WRONLY) {
    /*
     * Open existing for write
     */
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
      /*
       * unicode open for open existing write
       */
      Dmsg1(100, "Write only CreateFileW=%s\n", win32_fname);
      bfd->fh = p_CreateFileW((LPCWSTR)win32_fname_wchar,
                              dwaccess,      /* Requested access */
                              0,             /* Shared mode */
                              NULL,          /* SecurityAttributes */
                              OPEN_EXISTING, /* CreationDisposition */
                              dwflags,       /* Flags and attributes */
                              NULL);         /* TemplateFile */
    } else {
      /*
       * ASCII open
       */
      Dmsg1(100, "Write only CreateFileA=%s\n", win32_fname);
      bfd->fh = p_CreateFileA(win32_fname, dwaccess, /* Requested access */
                              0,                     /* Shared mode */
                              NULL,                  /* SecurityAttributes */
                              OPEN_EXISTING,         /* CreationDisposition */
                              dwflags,               /* Flags and attributes */
                              NULL);                 /* TemplateFile */
    }

    bfd->mode = BF_WRITE;
  } else {
    /*
     * Open existing for read
     */
    if (bfd->use_backup_api) {
      dwaccess = GENERIC_READ | READ_CONTROL | ACCESS_SYSTEM_SECURITY;
      if (flags & O_NOFOLLOW) {
        dwflags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN;
      } else {
        dwflags = FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN |
                  FILE_FLAG_OPEN_REPARSE_POINT;
      }
      dwshare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    } else {
      dwaccess = GENERIC_READ;
      dwflags = 0;
      dwshare = FILE_SHARE_READ | FILE_SHARE_WRITE;
    }

    if (p_CreateFileW && p_MultiByteToWideChar) {
      /*
       * Unicode open for open existing read
       */
      Dmsg1(100, "Read CreateFileW=%s\n", win32_fname);
      bfd->fh = p_CreateFileW((LPCWSTR)win32_fname_wchar,
                              dwaccess,      /* Requested access */
                              dwshare,       /* Share modes */
                              NULL,          /* SecurityAttributes */
                              OPEN_EXISTING, /* CreationDisposition */
                              dwflags,       /* Flags and attributes */
                              NULL);         /* TemplateFile */
    } else {
      /*
       * ASCII open for open existing read
       */
      Dmsg1(100, "Read CreateFileA=%s\n", win32_fname);
      bfd->fh = p_CreateFileA(win32_fname, dwaccess, /* Requested access */
                              dwshare,               /* Share modes */
                              NULL,                  /* SecurityAttributes */
                              OPEN_EXISTING,         /* CreationDisposition */
                              dwflags,               /* Flags and attributes */
                              NULL);                 /* TemplateFile */
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
  bfd->lpContext = NULL;
  bfd->win32DecompContext.bIsInData = false;
  bfd->win32DecompContext.liNextHeader = 0;
  FreePoolMemory(win32_fname_wchar);
  FreePoolMemory(win32_fname);

  bfd->encrypted = false;

  return bfd->mode == BF_CLOSED ? -1 : 1;
}

int bopen(BareosWinFilePacket* bfd,
          const char* fname,
          int flags,
          mode_t mode,
          dev_t rdev)
{
  Dmsg4(100, "bopen: fname %s, flags %08o, mode %04o, rdev %u\n", fname, flags,
        (mode & ~S_IFMT), rdev);

  /*
   * If the FILE_ATTRIBUTES_DEDUPED_ITEM bit is set this is a deduped file
   * we let the bopen function know it should open the file without the
   * FILE_FLAG_OPEN_REPARSE_POINT flag by setting in the O_NOFOLLOW open flag.
   */
  if (rdev & FILE_ATTRIBUTES_DEDUPED_ITEM) { flags |= O_NOFOLLOW; }

  /*
   * If the FILE_ATTRIBUTE_ENCRYPTED bit is set this is an file on an EFS
   * filesystem. For that we need some special handling.
   */
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
static inline int BcloseEncrypted(BareosWinFilePacket* bfd)
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
static inline int BcloseNonencrypted(BareosWinFilePacket* bfd)
{
  int status = 0;

  if (bfd->mode == BF_CLOSED) {
    Dmsg0(50, "=== BFD already closed.\n");
    return 0;
  }

  if (bfd->cmd_plugin && plugin_bclose) {
    status = plugin_bclose(bfd);
    Dmsg0(50, "==== BFD closed!!!\n");
    goto all_done;
  }

  /*
   * We need to tell the API to release the buffer it
   *  allocated in lpContext.  We do so by calling the
   *  API one more time, but with the Abort bit set.
   */
  if (bfd->use_backup_api && bfd->mode == BF_READ) {
    BYTE buf[10];
    if (bfd->lpContext && !p_BackupRead(bfd->fh, buf,   /* buffer */
                                        (DWORD)0,       /* bytes to read */
                                        &bfd->rw_bytes, /* bytes read */
                                        1,              /* Abort */
                                        1,              /* ProcessSecurity */
                                        &bfd->lpContext)) { /* Read context */
      errno = b_errno_win32;
      status = -1;
    }
  } else if (bfd->use_backup_api && bfd->mode == BF_WRITE) {
    BYTE buf[10];
    if (bfd->lpContext && !p_BackupWrite(bfd->fh, buf,   /* buffer */
                                         (DWORD)0,       /* bytes to read */
                                         &bfd->rw_bytes, /* bytes written */
                                         1,              /* Abort */
                                         1,              /* ProcessSecurity */
                                         &bfd->lpContext)) { /* Write context */
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
  bfd->lpContext = NULL;
  bfd->cmd_plugin = false;

  return status;
}

/**
 * Returns  0 on success
 *         -1 on error
 */
int bclose(BareosWinFilePacket* bfd)
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
ssize_t bread(BareosWinFilePacket* bfd, void* buf, size_t count)
{
  bfd->rw_bytes = 0;

  if (bfd->cmd_plugin && plugin_bread) { return plugin_bread(bfd, buf, count); }

  if (bfd->use_backup_api) {
    if (!p_BackupRead(bfd->fh, (BYTE*)buf, count, &bfd->rw_bytes,
                      0,                  /* no Abort */
                      1,                  /* Process Security */
                      &bfd->lpContext)) { /* Context */
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

ssize_t bwrite(BareosWinFilePacket* bfd, void* buf, size_t count)
{
  bfd->rw_bytes = 0;

  if (bfd->cmd_plugin && plugin_bwrite) {
    return plugin_bwrite(bfd, buf, count);
  }

  if (bfd->use_backup_api) {
    if (!p_BackupWrite(bfd->fh, (BYTE*)buf, count, &bfd->rw_bytes,
                       0,                  /* No abort */
                       1,                  /* Process Security */
                       &bfd->lpContext)) { /* Context */
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

bool IsBopen(BareosWinFilePacket* bfd) { return bfd->mode != BF_CLOSED; }

boffset_t blseek(BareosWinFilePacket* bfd, boffset_t offset, int whence)
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
void binit(BareosWinFilePacket* bfd)
{
  memset(bfd, 0, sizeof(BareosWinFilePacket));
  bfd->fid = -1;
}

bool have_win32_api() { return false; /* no can do */ }

/**
 * Enables using the Backup API (win32_data).
 *   Returns true  if function worked
 *   Returns false if failed (i.e. do not have Backup API on this machine)
 */
bool set_win32_backup(BareosWinFilePacket* bfd)
{
  return false; /* no can do */
}

bool SetPortableBackup(BareosWinFilePacket* bfd)
{
  return true; /* no problem */
}

/**
 * Return true  if we are writing in portable format
 * return false if not
 */
bool IsPortableBackup(BareosWinFilePacket* bfd)
{
  return true; /* portable by definition */
}

bool set_prog(BareosWinFilePacket* bfd, char* prog, JobControlRecord* jcr)
{
  return false;
}

bool SetCmdPlugin(BareosWinFilePacket* bfd, JobControlRecord* jcr)
{
  bfd->cmd_plugin = true;
  bfd->jcr = jcr;
  return true;
}

/**
 * This code is running on a non-Win32 machine
 */
bool IsRestoreStreamSupported(int stream)
{
  /* No Win32 backup on this machine */
  switch (stream) {
#ifndef HAVE_LIBZ
    case STREAM_GZIP_DATA:
    case STREAM_SPARSE_GZIP_DATA:
    case STREAM_WIN32_GZIP_DATA:
#endif
#ifndef HAVE_DARWIN_OS
    case STREAM_MACOS_FORK_DATA:
    case STREAM_HFSPLUS_ATTRIBUTES:
#endif
      return false;

      /* Known streams */
#ifdef HAVE_LIBZ
    case STREAM_GZIP_DATA:
    case STREAM_SPARSE_GZIP_DATA:
    case STREAM_WIN32_GZIP_DATA:
#endif
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
#ifdef HAVE_SHA2
    case STREAM_SHA256_DIGEST:
    case STREAM_SHA512_DIGEST:
#endif
#ifdef HAVE_CRYPTO
    case STREAM_SIGNED_DIGEST:
    case STREAM_ENCRYPTED_FILE_DATA:
    case STREAM_ENCRYPTED_FILE_GZIP_DATA:
    case STREAM_ENCRYPTED_WIN32_DATA:
    case STREAM_ENCRYPTED_WIN32_GZIP_DATA:
#endif
#ifdef HAVE_DARWIN_OS
    case STREAM_MACOS_FORK_DATA:
    case STREAM_HFSPLUS_ATTRIBUTES:
#ifdef HAVE_CRYPTO
    case STREAM_ENCRYPTED_MACOS_FORK_DATA:
#endif /* HAVE_CRYPTO */
#endif /* HAVE_DARWIN_OS */
    case 0: /* compatibility with old tapes */
      return true;
  }
  return false;
}

int bopen(BareosWinFilePacket* bfd,
          const char* fname,
          int flags,
          mode_t mode,
          dev_t rdev)
{
  Dmsg4(100, "bopen: fname %s, flags %08o, mode %04o, rdev %u\n", fname, flags,
        (mode & ~S_IFMT), rdev);

  if (bfd->cmd_plugin && plugin_bopen) {
    Dmsg1(400, "call plugin_bopen fname=%s\n", fname);
    bfd->fid = plugin_bopen(bfd, fname, flags, mode);
    Dmsg1(400, "Plugin bopen stat=%d\n", bfd->fid);
    return bfd->fid;
  }

  /* Normal file open */
  Dmsg1(debuglevel, "open file %s\n", fname);

  /* We use fnctl to set O_NOATIME if requested to avoid open error */
  bfd->fid = open(fname, flags & ~O_NOATIME, mode);

  /* Set O_NOATIME if possible */
  if (bfd->fid != -1 && flags & O_NOATIME) {
    int oldflags = fcntl(bfd->fid, F_GETFL, 0);
    if (oldflags == -1) {
      bfd->BErrNo = errno;
      close(bfd->fid);
      bfd->fid = -1;
    } else {
      int ret = fcntl(bfd->fid, F_SETFL, oldflags | O_NOATIME);
      /* EPERM means setting O_NOATIME was not allowed  */
      if (ret == -1 && errno != EPERM) {
        bfd->BErrNo = errno;
        close(bfd->fid);
        bfd->fid = -1;
      }
    }
  }
  bfd->BErrNo = errno;
  bfd->flags_ = flags;
  Dmsg1(400, "Open file %d\n", bfd->fid);
  errno = bfd->BErrNo;

  bfd->win32DecompContext.bIsInData = false;
  bfd->win32DecompContext.liNextHeader = 0;

#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
  if (bfd->fid != -1 && flags & O_RDONLY) {
    int status = posix_fadvise(bfd->fid, 0, 0, POSIX_FADV_WILLNEED);
    Dmsg2(400, "Did posix_fadvise on %s status=%d\n", fname, status);
  }
#endif

  return bfd->fid;
}

#ifdef HAVE_DARWIN_OS
/**
 * Open the resource fork of a file.
 */
int BopenRsrc(BareosWinFilePacket* bfd,
              const char* fname,
              int flags,
              mode_t mode)
{
  POOLMEM* rsrc_fname;

  rsrc_fname = GetPoolMemory(PM_FNAME);
  PmStrcpy(rsrc_fname, fname);
  PmStrcat(rsrc_fname, _PATH_RSRCFORKSPEC);
  bopen(bfd, rsrc_fname, flags, mode, 0);
  FreePoolMemory(rsrc_fname);

  return bfd->fid;
}
#else
int BopenRsrc(BareosWinFilePacket* bfd,
              const char* fname,
              int flags,
              mode_t mode)
{
  return -1;
}
#endif

int bclose(BareosWinFilePacket* bfd)
{
  int status;

  if (bfd->fid == -1) { return 0; }

  Dmsg1(400, "Close file %d\n", bfd->fid);

  if (bfd->cmd_plugin && plugin_bclose) {
    status = plugin_bclose(bfd);
    bfd->fid = -1;
    bfd->cmd_plugin = false;
  } else {
#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_DONTNEED)
    if (bfd->flags_ & O_RDONLY) {
      fdatasync(bfd->fid); /* sync the file */
      /* Tell OS we don't need it any more */
      posix_fadvise(bfd->fid, 0, 0, POSIX_FADV_DONTNEED);
    }
#endif

    /* Close normal file */
    status = close(bfd->fid);
    bfd->BErrNo = errno;
    bfd->fid = -1;
    bfd->cmd_plugin = false;
  }

  return status;
}

ssize_t bread(BareosWinFilePacket* bfd, void* buf, size_t count)
{
  ssize_t status;

  if (bfd->cmd_plugin && plugin_bread) { return plugin_bread(bfd, buf, count); }

  status = read(bfd->fid, buf, count);
  bfd->BErrNo = errno;
  return status;
}

ssize_t bwrite(BareosWinFilePacket* bfd, void* buf, size_t count)
{
  ssize_t status;

  if (bfd->cmd_plugin && plugin_bwrite) {
    return plugin_bwrite(bfd, buf, count);
  }
  status = write(bfd->fid, buf, count);
  bfd->BErrNo = errno;
  return status;
}

bool IsBopen(BareosWinFilePacket* bfd) { return bfd->fid >= 0; }

boffset_t blseek(BareosWinFilePacket* bfd, boffset_t offset, int whence)
{
  boffset_t pos;

  if (bfd->cmd_plugin && plugin_bwrite) {
    return plugin_blseek(bfd, offset, whence);
  }
  pos = (boffset_t)lseek(bfd->fid, offset, whence);
  bfd->BErrNo = errno;
  return pos;
}
#endif
