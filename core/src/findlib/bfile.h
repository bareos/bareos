/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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
  int32_t dwStreamId{0};
  int32_t dwStreamAttributes{0};
  int64_t Size{0};
  int32_t dwStreamNameSize{0};
} BWIN32_STREAM_ID, *LPBWIN32_STREAM_ID;


typedef struct _PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT {
  int64_t liNextHeader{0};
  bool bIsInData{false};
  BWIN32_STREAM_ID header_stream;
} PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT;

/*  =======================================================
 *
 *   W I N D O W S
 *
 *  =======================================================
 */
#if defined(HAVE_WIN32)

enum
{
  BF_CLOSED,
  BF_READ, /* BackupRead */
  BF_WRITE /* BackupWrite */
};

/* In bfile.c */

/* Basic Win32 low level I/O file packet */
struct BareosWinFilePacket {
  bool use_backup_api = false; /**< set if using BackupRead/Write */
  bool encrypted =
      false;    /**< set if using ReadEncryptedFileRaw/WriteEncryptedFileRaw */
  int mode = 0; /**< set if file is open */
  HANDLE fh = INVALID_HANDLE_VALUE; /**< Win32 file handle */
  int fid = 0;                      /**< fd if doing Unix style */
  LPVOID lpContext = nullptr;       /**< BackupRead/Write context */
  PVOID pvContext = nullptr;        /**< Encryption context */
  POOLMEM* errmsg = nullptr;        /**< error message buffer */
  DWORD rw_bytes = 0;               /**< Bytes read or written */
  DWORD lerror = 0;                 /**< Last error code */
  int BErrNo = 0;                   /**< errno */
  boffset_t offset = 0;             /**< Delta offset */
  JobControlRecord* jcr = nullptr;  /**< jcr for editing job codes */
  PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT
  win32DecompContext; /**< context for decomposition of win32 backup streams */
  int use_backup_decomp =
      0; /**< set if using BackupRead Stream Decomposition */
  bool reparse_point = false; /**< set if reparse point */
  bool cmd_plugin = false;    /**< set if we have a command plugin */
};

HANDLE BgetHandle(BareosWinFilePacket* bfd);

#else /* Linux/Unix systems */

/*  =======================================================
 *
 *   U N I X
 *
 *  =======================================================
 */

/* Basic Unix low level I/O file packet */
struct BareosWinFilePacket {
  int fid{0};                     /**< file id on Unix */
  int flags_{0};                  /**< open flags */
  int BErrNo{0};                  /**< errno */
  int32_t lerror{0};              /**< not used - simplies Win32 builds */
  boffset_t offset{0};            /**< Delta offset */
  JobControlRecord* jcr{nullptr}; /**< jcr for editing job codes */
  PROCESS_WIN32_BACKUPAPIBLOCK_CONTEXT
  win32DecompContext; /**< context for decomposition of win32 backup streams
                       */
  int use_backup_decomp{0}; /**< set if using BackupRead Stream Decomposition */
  bool reparse_point{false}; /**< not used in Unix */
  bool cmd_plugin{false};    /**< set if we have a command plugin */
};

#endif

void binit(BareosWinFilePacket* bfd);
bool IsBopen(BareosWinFilePacket* bfd);
bool set_win32_backup(BareosWinFilePacket* bfd);
bool SetPortableBackup(BareosWinFilePacket* bfd);
bool SetCmdPlugin(BareosWinFilePacket* bfd, JobControlRecord* jcr);
bool have_win32_api();
bool IsPortableBackup(BareosWinFilePacket* bfd);
bool IsRestoreStreamSupported(int stream);
bool is_win32_stream(int stream);
int bopen(BareosWinFilePacket* bfd,
          const char* fname,
          int flags,
          mode_t mode,
          dev_t rdev);
int BopenRsrc(BareosWinFilePacket* bfd,
              const char* fname,
              int flags,
              mode_t mode);
int bclose(BareosWinFilePacket* bfd);
ssize_t bread(BareosWinFilePacket* bfd, void* buf, size_t count);
ssize_t bwrite(BareosWinFilePacket* bfd, void* buf, size_t count);
boffset_t blseek(BareosWinFilePacket* bfd, boffset_t offset, int whence);
const char* stream_to_ascii(int stream);

bool processWin32BackupAPIBlock(BareosWinFilePacket* bfd,
                                void* pBuffer,
                                ssize_t dwSize);

#endif /* BAREOS_FINDLIB_BFILE_H_ */
