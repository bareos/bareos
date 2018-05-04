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
 * Kern Sibbald May MMIII
 */
/**
 * @file
 * Bareos low level File I/O routines.  This routine simulates
 * open(), read(), write(), and close(), but using native routines.
 * I.e. on Windows, we use Windows APIs.
 */

#ifndef BAREOS_FINDLIB_BFILE_H_
#define BAREOS_FINDLIB_BFILE_H_

/* this should physically correspond to WIN32_STREAM_ID
 * from winbase.h on Win32. We didn't inlcude cStreamName
 * as we don't use it and don't need it for a correct struct size.
 */

#define WIN32_BACKUP_DATA 1

typedef struct _BWIN32_STREAM_ID {
   int32_t dwStreamId;
   int32_t dwStreamAttributes;
   int64_t Size;
   int32_t dwStreamNameSize;
} BWIN32_STREAM_ID, *LPBWIN32_STREAM_ID ;


typedef struct _PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT {
   int64_t liNextHeader;
   bool bIsInData;
   BWIN32_STREAM_ID header_stream;
} PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT;

/*  =======================================================
 *
 *   W I N D O W S
 *
 *  =======================================================
 */
#if defined(HAVE_WIN32)

enum {
   BF_CLOSED,
   BF_READ,                           /* BackupRead */
   BF_WRITE                           /* BackupWrite */
};

/* In bfile.c */

/* Basic Win32 low level I/O file packet */
struct BareosWinFilePacket {
   bool use_backup_api;               /**< set if using BackupRead/Write */
   bool encrypted;                    /**< set if using ReadEncryptedFileRaw/WriteEncryptedFileRaw */
   int mode;                          /**< set if file is open */
   HANDLE fh;                         /**< Win32 file handle */
   int fid;                           /**< fd if doing Unix style */
   LPVOID lpContext;                  /**< BackupRead/Write context */
   PVOID pvContext;                   /**< Encryption context */
   POOLMEM *errmsg;                   /**< error message buffer */
   DWORD rw_bytes;                    /**< Bytes read or written */
   DWORD lerror;                      /**< Last error code */
   int berrno;                        /**< errno */
   boffset_t offset;                  /**< Delta offset */
   JobControlRecord *jcr;                          /**< jcr for editing job codes */
   PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT win32DecompContext; /**< context for decomposition of win32 backup streams */
   int use_backup_decomp;             /**< set if using BackupRead Stream Decomposition */
   bool reparse_point;                /**< set if reparse point */
   bool cmd_plugin;                   /**< set if we have a command plugin */
};

HANDLE BgetHandle(BareosWinFilePacket *bfd);

#else   /* Linux/Unix systems */

/*  =======================================================
 *
 *   U N I X
 *
 *  =======================================================
 */

/* Basic Unix low level I/O file packet */
struct BareosWinFilePacket {
   int fid;                           /**< file id on Unix */
   int flags_;                       /**< open flags */
   int berrno;                        /**< errno */
   int32_t lerror;                    /**< not used - simplies Win32 builds */
   boffset_t offset;                  /**< Delta offset */
   JobControlRecord *jcr;                          /**< jcr for editing job codes */
   PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT win32DecompContext; /**< context for decomposition of win32 backup streams */
   int use_backup_decomp;             /**< set if using BackupRead Stream Decomposition */
   bool reparse_point;                /**< not used in Unix */
   bool cmd_plugin;                   /**< set if we have a command plugin */
};

#endif

DLL_IMP_EXP void binit(BareosWinFilePacket *bfd);
DLL_IMP_EXP bool IsBopen(BareosWinFilePacket *bfd);
DLL_IMP_EXP bool set_win32_backup(BareosWinFilePacket *bfd);
DLL_IMP_EXP bool SetPortableBackup(BareosWinFilePacket *bfd);
DLL_IMP_EXP bool SetCmdPlugin(BareosWinFilePacket *bfd, JobControlRecord *jcr);
DLL_IMP_EXP bool have_win32_api();
DLL_IMP_EXP bool IsPortableBackup(BareosWinFilePacket *bfd);
DLL_IMP_EXP bool IsRestoreStreamSupported(int stream);
DLL_IMP_EXP bool is_win32_stream(int stream);
DLL_IMP_EXP int bopen(BareosWinFilePacket *bfd, const char *fname, int flags, mode_t mode, dev_t rdev);
DLL_IMP_EXP int BopenRsrc(BareosWinFilePacket *bfd, const char *fname, int flags, mode_t mode);
DLL_IMP_EXP int bclose(BareosWinFilePacket *bfd);
DLL_IMP_EXP ssize_t bread(BareosWinFilePacket *bfd, void *buf, size_t count);
DLL_IMP_EXP ssize_t bwrite(BareosWinFilePacket *bfd, void *buf, size_t count);
DLL_IMP_EXP boffset_t blseek(BareosWinFilePacket *bfd, boffset_t offset, int whence);
DLL_IMP_EXP const char *stream_to_ascii(int stream);

DLL_IMP_EXP bool processWin32BackupAPIBlock (BareosWinFilePacket *bfd, void *pBuffer, ssize_t dwSize);

#endif /* BAREOS_FINDLIB_BFILE_H_ */
