/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2006-2008 Free Software Foundation Europe e.V.

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
/*
 * mtops.cpp - Emulate the Linux st (scsi tape) driver on Microsoft Windows.
 *
 * Author: Robert Nelson, May, 2006 <robertn@the-nelsons.org>
 *
 * Version $Id$
 *
 * This file was contributed to the Bacula project by Robert Nelson.
 *
 * Robert Nelson has been granted a perpetual, worldwide,
 * non-exclusive, no-charge, royalty-free, irrevocable copyright
 * license to reproduce, prepare derivative works of, publicly
 * display, publicly perform, sublicense, and distribute the original
 * work contributed by Robert Nelson to the Bacula project in source 
 * or object form.
 *
 * If you wish to license contributions from Robert Nelson
 * under an alternate open source license please contact
 * Robert Nelson <robertn@the-nelsons.org>.
 */

#include <stdarg.h>
#include <stddef.h>

#include "bacula.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

#include "sys/mtio.h"
#if defined(_MSC_VER)
#include <winioctl.h>
#else
#include <ntddstor.h>
#endif
#include <ntddscsi.h>

//
// SCSI bus status codes.
//

#define SCSISTAT_GOOD                  0x00
#define SCSISTAT_CHECK_CONDITION       0x02
#define SCSISTAT_CONDITION_MET         0x04
#define SCSISTAT_BUSY                  0x08
#define SCSISTAT_INTERMEDIATE          0x10
#define SCSISTAT_INTERMEDIATE_COND_MET 0x14
#define SCSISTAT_RESERVATION_CONFLICT  0x18
#define SCSISTAT_COMMAND_TERMINATED    0x22
#define SCSISTAT_QUEUE_FULL            0x28

inline   SHORT  Read16BitSigned(const unsigned char *pValue)
{
   return (SHORT)(((USHORT)pValue[0] << 8) | (USHORT)pValue[1]);
}

inline   USHORT  Read16BitUnsigned(const unsigned char *pValue)
{
   return (((USHORT)pValue[0] << 8) | (USHORT)pValue[1]);
}

inline   LONG  Read24BitSigned(const unsigned char *pValue)
{
   return ((LONG)(((ULONG)pValue[0] << 16) | ((ULONG)pValue[1] << 8) |
                   (ULONG)pValue[2])) << 8 >> 8;
}

inline   ULONG  Read24BitUnsigned(const unsigned char *pValue)
{
   return ((ULONG)pValue[0] << 16) | ((ULONG)pValue[1] << 8) | (ULONG)pValue[2];
}

inline   LONG  Read32BitSigned(const unsigned char *pValue)
{
   return (LONG)(((ULONG)pValue[0] << 24) | ((ULONG)pValue[1] << 16) |
                 ((ULONG)pValue[2] << 8) |   (ULONG)pValue[3]);
}

inline   ULONG  Read32BitUnsigned(const unsigned char *pValue)
{
   return (((ULONG)pValue[0] << 24) | ((ULONG)pValue[1] << 16) |
           ((ULONG)pValue[2] << 8) | (ULONG)pValue[3]);
}

inline   LONGLONG  Read64BitSigned(const unsigned char *pValue)
{
   return (LONGLONG)(((ULONGLONG)pValue[0] << 56) | ((ULONGLONG)pValue[1] << 48) |
                     ((ULONGLONG)pValue[2] << 40) | ((ULONGLONG)pValue[3] << 32) |
                     ((ULONGLONG)pValue[4] << 24) | ((ULONGLONG)pValue[5] << 16) |
                     ((ULONGLONG)pValue[6] <<  8) |  (ULONGLONG)pValue[7]);
}

inline   ULONGLONG  Read64BitUnsigned(const unsigned char *pValue)
{
   return (LONGLONG)(((ULONGLONG)pValue[0] << 56) | ((ULONGLONG)pValue[1] << 48) |
                     ((ULONGLONG)pValue[2] << 40) | ((ULONGLONG)pValue[3] << 32) |
                     ((ULONGLONG)pValue[4] << 24) | ((ULONGLONG)pValue[5] << 16) |
                     ((ULONGLONG)pValue[6] <<  8) | (ULONGLONG)pValue[7]);
}

typedef  struct   _TAPE_POSITION_INFO
{
   UCHAR       AtPartitionStart:1;
   UCHAR       AtPartitionEnd:1;
   UCHAR       PartitionBlockValid:1;
   UCHAR       FileSetValid:1;
   UCHAR       :4;
   UCHAR       Reserved1[3];
   ULONG       Partition;
   ULONGLONG   BlockNumber;
   ULONGLONG   FileNumber;
   ULONGLONG   SetNumber;
}  TAPE_POSITION_INFO, *PTAPE_POSITION_INFO;

typedef  struct   _TAPE_HANDLE_INFO
{
   HANDLE      OSHandle;
   bool        bEOD;
   bool        bEOF;
   bool        bEOT;
   bool        bBlockValid;
   ULONG       FeaturesLow;
   ULONG       FeaturesHigh;
   ULONG       ulFile;
   ULONGLONG   ullFileStart;

}  TAPE_HANDLE_INFO, *PTAPE_HANDLE_INFO;

TAPE_HANDLE_INFO TapeHandleTable[] =
{
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE },
   { INVALID_HANDLE_VALUE }
};

#define  NUMBER_HANDLE_ENTRIES      (sizeof(TapeHandleTable) / sizeof(TapeHandleTable[0]))

static DWORD GetTapePositionInfo(HANDLE hDevice, PTAPE_POSITION_INFO TapePositionInfo);
static DWORD GetDensityBlockSize(HANDLE hDevice, DWORD *pdwDensity, DWORD *pdwBlockSize);

static int tape_get(int fd, struct mtget *mt_get);
static int tape_op(int fd, struct mtop *mt_com);
static int tape_pos(int fd, struct mtpos *mt_pos);

int
win32_tape_open(const char *file, int flags, ...)
{
   HANDLE hDevice = INVALID_HANDLE_VALUE;
   char szDeviceName[256] = "\\\\.\\";
   int  idxFile;
   DWORD dwResult;

   for (idxFile = 0; idxFile < (int)NUMBER_HANDLE_ENTRIES; idxFile++) {
      if (TapeHandleTable[idxFile].OSHandle == INVALID_HANDLE_VALUE) {
         break;
      }
   }

   if (idxFile >= (int)NUMBER_HANDLE_ENTRIES) {
      return EMFILE;
   }

   memset(&TapeHandleTable[idxFile], 0, sizeof(TapeHandleTable[idxFile]));

   if (!IsPathSeparator(file[0])) {
       bstrncpy(&szDeviceName[4], file, sizeof(szDeviceName) - 4);
   } else {
       bstrncpy(&szDeviceName[0], file, sizeof(szDeviceName));
   }

   hDevice = CreateFile(szDeviceName, FILE_ALL_ACCESS, 0, NULL, OPEN_EXISTING, 0, NULL);

   if (hDevice != INVALID_HANDLE_VALUE) {
      PTAPE_HANDLE_INFO    pHandleInfo = &TapeHandleTable[idxFile];

      memset(pHandleInfo, 0, sizeof(*pHandleInfo));

      pHandleInfo->OSHandle = hDevice;

      TAPE_GET_DRIVE_PARAMETERS  TapeDriveParameters;
      DWORD    dwSize = sizeof(TapeDriveParameters);

      dwResult = GetTapeParameters(pHandleInfo->OSHandle, GET_TAPE_DRIVE_INFORMATION, &dwSize, &TapeDriveParameters);
      if (dwResult == NO_ERROR) {
         pHandleInfo->FeaturesLow = TapeDriveParameters.FeaturesLow;
         pHandleInfo->FeaturesHigh = TapeDriveParameters.FeaturesHigh;
      }

      TAPE_POSITION_INFO TapePositionInfo;

      dwResult =  GetTapePositionInfo(pHandleInfo->OSHandle, &TapePositionInfo);

      if (dwResult == NO_ERROR) {
         if (TapePositionInfo.AtPartitionStart || TapePositionInfo.AtPartitionEnd ||
             (TapePositionInfo.PartitionBlockValid && TapePositionInfo.BlockNumber == 0)) {
            pHandleInfo->ulFile = 0;
            pHandleInfo->bBlockValid = true;
            pHandleInfo->ullFileStart = 0;
         } else if (TapePositionInfo.FileSetValid) {
            pHandleInfo->ulFile = (ULONG)TapePositionInfo.FileNumber;
         }
      }
   } else {
      DWORD dwError = GetLastError();

      switch (dwError) {
      case ERROR_FILE_NOT_FOUND:
      case ERROR_PATH_NOT_FOUND:
         errno = ENOENT;
         break;

      case ERROR_TOO_MANY_OPEN_FILES:
         errno = EMFILE;
         break;

      default:
      case ERROR_ACCESS_DENIED:
      case ERROR_SHARING_VIOLATION:
      case ERROR_LOCK_VIOLATION:
      case ERROR_INVALID_NAME:
         errno = EACCES;
         break;

      case ERROR_FILE_EXISTS:
         errno = EEXIST;
         break;

      case ERROR_INVALID_PARAMETER:
         errno = EINVAL;
         break;
      }

      return(int) -1;
   }

   return (int)idxFile + 3;
}

ssize_t
win32_read(int fd, void *buffer, size_t count)
{
   return read(fd, buffer, count);
}

ssize_t
win32_write(int fd, const void *buffer, size_t count)
{
   return write(fd, buffer, count);
}

int
win32_ioctl(int d, unsigned long int req, ...)
{
   return -1;
}

ssize_t
win32_tape_read(int fd, void *buffer, size_t count)
{
   if (buffer == NULL) {
      errno = EINVAL;
      return -1;
   }

   if (fd < 3 || fd >= (int)(NUMBER_HANDLE_ENTRIES + 3) || 
       TapeHandleTable[fd - 3].OSHandle == INVALID_HANDLE_VALUE)
   {
      errno = EBADF;
      return -1;
   }

   PTAPE_HANDLE_INFO    pHandleInfo = &TapeHandleTable[fd - 3];

   DWORD bytes_read;
   BOOL bResult;

   bResult = ReadFile(pHandleInfo->OSHandle, buffer, count, &bytes_read, NULL);

   if (bResult) {
      pHandleInfo->bEOF = false;
      pHandleInfo->bEOT = false;
      pHandleInfo->bEOD = false;
      return bytes_read;
   } else {
      int iReturnValue = 0;
      DWORD last_error = GetLastError();

      switch (last_error) {

      case ERROR_FILEMARK_DETECTED:
         pHandleInfo->bEOF = true;
         break;

      case ERROR_END_OF_MEDIA:
         pHandleInfo->bEOT = true;
         break;

      case ERROR_NO_MEDIA_IN_DRIVE:
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
         pHandleInfo->bEOD = false;
         errno = ENOMEDIUM;
         iReturnValue = -1;
         break;

      case ERROR_NO_DATA_DETECTED:
         pHandleInfo->bEOD = true;
         break;

      case ERROR_INVALID_HANDLE:
      case ERROR_ACCESS_DENIED:
      case ERROR_LOCK_VIOLATION:
         errno = EBADF;
         iReturnValue = -1;
         break;

      default:
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
         pHandleInfo->bEOD = false;
         errno = EIO;
         iReturnValue = -1;
      }

      return iReturnValue;
   }
}

ssize_t
win32_tape_write(int fd, const void *buffer, size_t count)
{
   if (buffer == NULL) {
      errno = EINVAL;
      return -1;
   }

   if (fd < 3 || fd >= (int)(NUMBER_HANDLE_ENTRIES + 3) || TapeHandleTable[fd - 3].OSHandle == INVALID_HANDLE_VALUE)
   {
      errno = EBADF;
      return -1;
   }

   PTAPE_HANDLE_INFO    pHandleInfo = &TapeHandleTable[fd - 3];

   DWORD bytes_written;
   BOOL bResult;

   bResult = WriteFile(pHandleInfo->OSHandle, buffer, count, &bytes_written, NULL);

   if (bResult) {
      pHandleInfo->bEOF = false;
      pHandleInfo->bEOT = false;
      return bytes_written;
   } else {
      DWORD last_error = GetLastError();

      switch (last_error) {
      case ERROR_END_OF_MEDIA:
      case ERROR_DISK_FULL:
         pHandleInfo->bEOT = true;
         errno = ENOSPC;
         break;

      case ERROR_NO_MEDIA_IN_DRIVE:
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
         pHandleInfo->bEOD = false;
         errno = ENOMEDIUM;
         break;

      case ERROR_INVALID_HANDLE:
      case ERROR_ACCESS_DENIED:
         errno = EBADF;
         break;

      default:
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
         pHandleInfo->bEOD = false;
         errno = EIO;
         break;
      }
      return -1;
   }
}

int
win32_tape_close(int fd)
{
   if (fd < 3 || fd >= (int)(NUMBER_HANDLE_ENTRIES + 3) || 
      TapeHandleTable[fd - 3].OSHandle == INVALID_HANDLE_VALUE) {
      errno = EBADF;
      return -1;
   }

   PTAPE_HANDLE_INFO    pHandleInfo = &TapeHandleTable[fd - 3];

   if (!CloseHandle(pHandleInfo->OSHandle)) {
      pHandleInfo->OSHandle = INVALID_HANDLE_VALUE;
      errno = EBADF;
      return -1;
   }

   pHandleInfo->OSHandle = INVALID_HANDLE_VALUE;

   return 0;
}

int
win32_tape_ioctl(int fd, unsigned long int request, ...)
{
   va_list argp;
   int result;

   va_start(argp, request);

   switch (request) {
   case MTIOCTOP:
      result = tape_op(fd, va_arg(argp, mtop *));
      break;

   case MTIOCGET:
      result = tape_get(fd, va_arg(argp, mtget *));
      break;

   case MTIOCPOS:
      result = tape_pos(fd, va_arg(argp, mtpos *));
      break;

   default:
      errno = ENOTTY;
      result = -1;
      break;
   }

   va_end(argp);

   return result;
}

static int tape_op(int fd, struct mtop *mt_com)
{
   DWORD result = NO_ERROR;
   int   index;

   if (fd < 3 || fd >= (int)(NUMBER_HANDLE_ENTRIES + 3) || 
       TapeHandleTable[fd - 3].OSHandle == INVALID_HANDLE_VALUE)
   {
      errno = EBADF;
      return -1;
   }

   PTAPE_HANDLE_INFO    pHandleInfo = &TapeHandleTable[fd - 3];

   switch (mt_com->mt_op)
   {
   case MTRESET:
   case MTNOP:
   case MTSETDRVBUFFER:
      break;

   default:
   case MTRAS1:
   case MTRAS2:
   case MTRAS3:
   case MTSETDENSITY:
      errno = ENOTTY;
      result = (DWORD)-1;
      break;

   case MTFSF:
      for (index = 0; index < mt_com->mt_count; index++) {
         result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, 1, 0, FALSE);
         if (result == NO_ERROR) {
            pHandleInfo->ulFile++;
            pHandleInfo->bEOF = true;
            pHandleInfo->bEOT = false;
         }
      }
      break;

   case MTBSF:
      for (index = 0; index < mt_com->mt_count; index++) {
         result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, (DWORD)-1, ~0, FALSE);
         if (result == NO_ERROR) {
            pHandleInfo->ulFile--;
            pHandleInfo->bBlockValid = false;
            pHandleInfo->bEOD = false;
            pHandleInfo->bEOF = false;
            pHandleInfo->bEOT = false;
         }
      }
      break;

   case MTFSR:
      result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_RELATIVE_BLOCKS, 0, mt_com->mt_count, 0, FALSE);
      if (result == NO_ERROR) {
         pHandleInfo->bEOD = false;
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
      } else if (result == ERROR_FILEMARK_DETECTED) {
         pHandleInfo->bEOF = true;
      }
      break;

   case MTBSR:
      result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_RELATIVE_BLOCKS, 0, -mt_com->mt_count, ~0, FALSE);
      if (result == NO_ERROR) {
         pHandleInfo->bEOD = false;
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
      } else if (result == ERROR_FILEMARK_DETECTED) {
         pHandleInfo->ulFile--;
         pHandleInfo->bBlockValid = false;
         pHandleInfo->bEOD = false;
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
      }
      break;

   case MTWEOF:
      result = WriteTapemark(pHandleInfo->OSHandle, TAPE_FILEMARKS, mt_com->mt_count, FALSE);
      if (result == NO_ERROR) {
         pHandleInfo->bEOF = true;
         pHandleInfo->bEOT = false;
         pHandleInfo->ulFile += mt_com->mt_count;
         pHandleInfo->bBlockValid = true;
         pHandleInfo->ullFileStart = 0;
      }
      break;

   case MTREW:
      result = SetTapePosition(pHandleInfo->OSHandle, TAPE_REWIND, 0, 0, 0, FALSE);
      if (result == NO_ERROR) {
         pHandleInfo->bEOD = false;
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
         pHandleInfo->ulFile = 0;
         pHandleInfo->bBlockValid = true;
         pHandleInfo->ullFileStart = 0;
      }
      break;

   case MTOFFL:
      result = PrepareTape(pHandleInfo->OSHandle, TAPE_UNLOAD, FALSE);
      if (result == NO_ERROR) {
         pHandleInfo->bEOD = false;
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
         pHandleInfo->ulFile = 0;
         pHandleInfo->ullFileStart = 0;
      }
      break;

   case MTRETEN:
      result = PrepareTape(pHandleInfo->OSHandle, TAPE_TENSION, FALSE);
      if (result == NO_ERROR) {
         pHandleInfo->bEOD = false;
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
         pHandleInfo->ulFile = 0;
         pHandleInfo->bBlockValid = true;
         pHandleInfo->ullFileStart = 0;
      }
      break;

   case MTBSFM:
      for (index = 0; index < mt_com->mt_count; index++) {
         result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, (DWORD)-1, ~0, FALSE);
         if (result == NO_ERROR) {
            result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, 1, 0, FALSE);
            pHandleInfo->bEOD = false;
            pHandleInfo->bEOF = false;
            pHandleInfo->bEOT = false;
         }
      }
      break;

   case MTFSFM:
      for (index = 0; index < mt_com->mt_count; index++) {
         result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, mt_com->mt_count, 0, FALSE);
         if (result == NO_ERROR) {
            result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, (DWORD)-1, ~0, FALSE);
            pHandleInfo->bEOD = false;
            pHandleInfo->bEOF = false;
            pHandleInfo->bEOT = false;
         }
      }
      break;

   case MTEOM:
      for ( ; ; ) {
         result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, 1, 0, FALSE);
         if (result != NO_ERROR) {
            pHandleInfo->bEOF = false;

            if (result == ERROR_END_OF_MEDIA) {
               pHandleInfo->bEOD = true;
               pHandleInfo->bEOT = true;
               return 0;
            }
            if (result == ERROR_NO_DATA_DETECTED) {
               pHandleInfo->bEOD = true;
               pHandleInfo->bEOT = false;
               return 0;
            }
            break;
         } else {
            pHandleInfo->bEOF = true;
            pHandleInfo->ulFile++;
         }
      }
      break;

   case MTERASE:
      result = EraseTape(pHandleInfo->OSHandle, TAPE_ERASE_LONG, FALSE);
      if (result == NO_ERROR) {
         pHandleInfo->bEOD = true;
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
         pHandleInfo->ulFile = 0;
         pHandleInfo->bBlockValid = true;
         pHandleInfo->ullFileStart = 0;
      }
      break;

   case MTSETBLK:
      {
         TAPE_SET_MEDIA_PARAMETERS  SetMediaParameters;

         SetMediaParameters.BlockSize = mt_com->mt_count;
         result = SetTapeParameters(pHandleInfo->OSHandle, SET_TAPE_MEDIA_INFORMATION, &SetMediaParameters);
      }
      break;

   case MTSEEK:
      {
         TAPE_POSITION_INFO   TapePositionInfo;

         result = SetTapePosition(pHandleInfo->OSHandle, TAPE_ABSOLUTE_BLOCK, 0, mt_com->mt_count, 0, FALSE);

         memset(&TapePositionInfo, 0, sizeof(TapePositionInfo));
         DWORD dwPosResult = GetTapePositionInfo(pHandleInfo->OSHandle, &TapePositionInfo);
         if (dwPosResult == NO_ERROR && TapePositionInfo.FileSetValid) {
            pHandleInfo->ulFile = (ULONG)TapePositionInfo.FileNumber;
         } else {
            pHandleInfo->ulFile = ~0U;
         }
      }
      break;

   case MTTELL:
      {
         DWORD partition;
         DWORD offset;
         DWORD offsetHi;

         result = GetTapePosition(pHandleInfo->OSHandle, TAPE_ABSOLUTE_BLOCK, &partition, &offset, &offsetHi);
         if (result == NO_ERROR) {
            return offset;
         }
      }
      break;

   case MTFSS:
      result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_SETMARKS, 0, mt_com->mt_count, 0, FALSE);
      break;

   case MTBSS:
      result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_SETMARKS, 0, -mt_com->mt_count, ~0, FALSE);
      break;

   case MTWSM:
      result = WriteTapemark(pHandleInfo->OSHandle, TAPE_SETMARKS, mt_com->mt_count, FALSE);
      break;

   case MTLOCK:
      result = PrepareTape(pHandleInfo->OSHandle, TAPE_LOCK, FALSE);
      break;

   case MTUNLOCK:
      result = PrepareTape(pHandleInfo->OSHandle, TAPE_UNLOCK, FALSE);
      break;

   case MTLOAD:
      result = PrepareTape(pHandleInfo->OSHandle, TAPE_LOAD, FALSE);
      break;

   case MTUNLOAD:
      result = PrepareTape(pHandleInfo->OSHandle, TAPE_UNLOAD, FALSE);
      break;

   case MTCOMPRESSION:
      {
         TAPE_GET_DRIVE_PARAMETERS  GetDriveParameters;
         TAPE_SET_DRIVE_PARAMETERS  SetDriveParameters;
         DWORD                      size;

         size = sizeof(GetDriveParameters);

         result = GetTapeParameters(pHandleInfo->OSHandle, GET_TAPE_DRIVE_INFORMATION, &size, &GetDriveParameters);

         if (result == NO_ERROR)
         {
            SetDriveParameters.ECC = GetDriveParameters.ECC;
            SetDriveParameters.Compression = (BOOLEAN)mt_com->mt_count;
            SetDriveParameters.DataPadding = GetDriveParameters.DataPadding;
            SetDriveParameters.ReportSetmarks = GetDriveParameters.ReportSetmarks;
            SetDriveParameters.EOTWarningZoneSize = GetDriveParameters.EOTWarningZoneSize;

            result = SetTapeParameters(pHandleInfo->OSHandle, SET_TAPE_DRIVE_INFORMATION, &SetDriveParameters);
         }
      }
      break;

   case MTSETPART:
      result = SetTapePosition(pHandleInfo->OSHandle, TAPE_LOGICAL_BLOCK, mt_com->mt_count, 0, 0, FALSE);
      break;

   case MTMKPART:
      if (mt_com->mt_count == 0)
      {
         result = CreateTapePartition(pHandleInfo->OSHandle, TAPE_INITIATOR_PARTITIONS, 1, 0);
      }
      else
      {
         result = CreateTapePartition(pHandleInfo->OSHandle, TAPE_INITIATOR_PARTITIONS, 2, mt_com->mt_count);
      }
      break;
   }

   if ((result == NO_ERROR && pHandleInfo->bEOF) || 
       (result == ERROR_FILEMARK_DETECTED && mt_com->mt_op == MTFSR)) {

      TAPE_POSITION_INFO TapePositionInfo;

      if (GetTapePositionInfo(pHandleInfo->OSHandle, &TapePositionInfo) == NO_ERROR) {
         pHandleInfo->bBlockValid = true;
         pHandleInfo->ullFileStart = TapePositionInfo.BlockNumber;
      }
   }

   switch (result) {
   case NO_ERROR:
   case (DWORD)-1:   /* Error has already been translated into errno */
      break;

   default:
   case ERROR_FILEMARK_DETECTED:
      errno = EIO;
      break;

   case ERROR_END_OF_MEDIA:
      pHandleInfo->bEOT = true;
      errno = EIO;
      break;

   case ERROR_NO_DATA_DETECTED:
      pHandleInfo->bEOD = true;
      errno = EIO;
      break;

   case ERROR_NO_MEDIA_IN_DRIVE:
      pHandleInfo->bEOF = false;
      pHandleInfo->bEOT = false;
      pHandleInfo->bEOD = false;
      errno = ENOMEDIUM;
      break;

   case ERROR_INVALID_HANDLE:
   case ERROR_ACCESS_DENIED:
   case ERROR_LOCK_VIOLATION:
      errno = EBADF;
      break;
   }

   return result == NO_ERROR ? 0 : -1;
}

static int tape_get(int fd, struct mtget *mt_get)
{
   TAPE_POSITION_INFO pos_info;
   BOOL result;

   if (fd < 3 || fd >= (int)(NUMBER_HANDLE_ENTRIES + 3) || 
       TapeHandleTable[fd - 3].OSHandle == INVALID_HANDLE_VALUE) {
      errno = EBADF;
      return -1;
   }

   PTAPE_HANDLE_INFO    pHandleInfo = &TapeHandleTable[fd - 3];

   if (GetTapePositionInfo(pHandleInfo->OSHandle, &pos_info) != NO_ERROR) {
      return -1;
   }

   DWORD density = 0;
   DWORD blocksize = 0;

   result = GetDensityBlockSize(pHandleInfo->OSHandle, &density, &blocksize);

   if (result != NO_ERROR) {
      TAPE_GET_DRIVE_PARAMETERS drive_params;
      DWORD size;

      size = sizeof(drive_params);

      result = GetTapeParameters(pHandleInfo->OSHandle, GET_TAPE_DRIVE_INFORMATION, &size, &drive_params);

      if (result == NO_ERROR) {
         blocksize = drive_params.DefaultBlockSize;
      }
   }

   mt_get->mt_type = MT_ISSCSI2;

   // Partition #
   mt_get->mt_resid = pos_info.PartitionBlockValid ? pos_info.Partition : (ULONG)-1;

   // Density / Block Size
   mt_get->mt_dsreg = ((density << MT_ST_DENSITY_SHIFT) & MT_ST_DENSITY_MASK) |
                      ((blocksize << MT_ST_BLKSIZE_SHIFT) & MT_ST_BLKSIZE_MASK);

   mt_get->mt_gstat = 0x00010000;  /* Immediate report mode.*/

   if (pHandleInfo->bEOF) {
      mt_get->mt_gstat |= 0x80000000;     // GMT_EOF
   }

   if (pos_info.PartitionBlockValid && pos_info.BlockNumber == 0) {
      mt_get->mt_gstat |= 0x40000000;     // GMT_BOT
   }

   if (pHandleInfo->bEOT) {
      mt_get->mt_gstat |= 0x20000000;     // GMT_EOT
   }

   if (pHandleInfo->bEOD) {
      mt_get->mt_gstat |= 0x08000000;     // GMT_EOD
   }

   TAPE_GET_MEDIA_PARAMETERS  media_params;
   DWORD size = sizeof(media_params);
   
   result = GetTapeParameters(pHandleInfo->OSHandle, GET_TAPE_MEDIA_INFORMATION, &size, &media_params);

   if (result == NO_ERROR && media_params.WriteProtected) {
      mt_get->mt_gstat |= 0x04000000;     // GMT_WR_PROT
   }

   result = GetTapeStatus(pHandleInfo->OSHandle);

   if (result != NO_ERROR) {
      if (result == ERROR_NO_MEDIA_IN_DRIVE) {
         mt_get->mt_gstat |= 0x00040000;  // GMT_DR_OPEN
      }
   } else {
      mt_get->mt_gstat |= 0x01000000;     // GMT_ONLINE
   }

   // Recovered Error Count
   mt_get->mt_erreg = 0;

   // File Number
   mt_get->mt_fileno = (__daddr_t)pHandleInfo->ulFile;

   // Block Number
   mt_get->mt_blkno = (__daddr_t)(pHandleInfo->bBlockValid ? pos_info.BlockNumber - pHandleInfo->ullFileStart : (ULONGLONG)-1);

   return 0;
}

#define  SERVICEACTION_SHORT_FORM_BLOCKID             0
#define  SERVICEACTION_SHORT_FORM_VENDOR_SPECIFIC     1
#define  SERVICEACTION_LONG_FORM                      6
#define  SERVICEACTION_EXTENDED_FORM                  8


typedef  struct   _SCSI_READ_POSITION_SHORT_BUFFER
{
   UCHAR    :1;
   UCHAR    PERR:1;
   UCHAR    BPU:1;
   UCHAR    :1;
   UCHAR    BYCU:1;
   UCHAR    BCU:1;
   UCHAR    EOP:1;
   UCHAR    BOP:1;
   UCHAR    Partition;
   UCHAR    Reserved1[2];
   UCHAR    FirstBlock[4];
   UCHAR    LastBlock[4];
   UCHAR    Reserved2;
   UCHAR    NumberBufferBlocks[3];
   UCHAR    NumberBufferBytes[4];
}  SCSI_READ_POSITION_SHORT_BUFFER, *PSCSI_READ_POSITION_SHORT_BUFFER;

typedef  struct   _SCSI_READ_POSITION_LONG_BUFFER
{
   UCHAR    :2;
   UCHAR    BPU:1;
   UCHAR    MPU:1;
   UCHAR    :2;
   UCHAR    EOP:1;
   UCHAR    BOP:1;
   UCHAR    Reserved3[3];
   UCHAR    Partition[4];
   UCHAR    BlockNumber[8];
   UCHAR    FileNumber[8];
   UCHAR    SetNumber[8];
}  SCSI_READ_POSITION_LONG_BUFFER, *PSCSI_READ_POSITION_LONG_BUFFER;

typedef  struct   _SCSI_READ_POSITION_EXTENDED_BUFFER
{
   UCHAR    :1;
   UCHAR    PERR:1;
   UCHAR    LOPU:1;
   UCHAR    :1;
   UCHAR    BYCU:1;
   UCHAR    LOCU:1;
   UCHAR    EOP:1;
   UCHAR    BOP:1;
   UCHAR    Partition;
   UCHAR    AdditionalLength[2];
   UCHAR    Reserved1;
   UCHAR    NumberBufferObjects[3];
   UCHAR    FirstLogicalObject[8];
   UCHAR    LastLogicalObject[8];
   UCHAR    NumberBufferObjectBytes[8];
}  SCSI_READ_POSITION_EXTENDED_BUFFER, *PSCSI_READ_POSITION_EXTENDED_BUFFER;

typedef union _READ_POSITION_RESULT {
   SCSI_READ_POSITION_SHORT_BUFFER     ShortBuffer;
   SCSI_READ_POSITION_LONG_BUFFER      LongBuffer;
   SCSI_READ_POSITION_EXTENDED_BUFFER  ExtendedBuffer;
}  READ_POSITION_RESULT, *PREAD_POSITION_RESULT;

static DWORD GetTapePositionInfo(HANDLE hDevice, PTAPE_POSITION_INFO TapePositionInfo)
{
   PSCSI_PASS_THROUGH   ScsiPassThrough;
   BOOL                 bResult;
   DWORD                dwBytesReturned;

   const DWORD dwBufferSize = sizeof(SCSI_PASS_THROUGH) + sizeof(READ_POSITION_RESULT) + 28;

   memset(TapePositionInfo, 0, sizeof(*TapePositionInfo));

   ScsiPassThrough = (PSCSI_PASS_THROUGH)malloc(dwBufferSize);

   for (int pass = 0; pass < 2; pass++)
   {
      memset(ScsiPassThrough, 0, dwBufferSize);

      ScsiPassThrough->Length = sizeof(SCSI_PASS_THROUGH);

      ScsiPassThrough->CdbLength = 10;
      ScsiPassThrough->SenseInfoLength = 28;
      ScsiPassThrough->DataIn = 1;
      ScsiPassThrough->DataTransferLength = sizeof(SCSI_READ_POSITION_LONG_BUFFER);
      ScsiPassThrough->TimeOutValue = 1000;
      ScsiPassThrough->DataBufferOffset = sizeof(SCSI_PASS_THROUGH) + 28;
      ScsiPassThrough->SenseInfoOffset = sizeof(SCSI_PASS_THROUGH);

      ScsiPassThrough->Cdb[0] = 0x34;  // READ POSITION

      switch (pass)
      {
      case 0:
         ScsiPassThrough->Cdb[1] = SERVICEACTION_LONG_FORM;
         break;

      case 1:
         ScsiPassThrough->Cdb[1] = SERVICEACTION_SHORT_FORM_BLOCKID;
         break;
      }

      bResult = DeviceIoControl( hDevice, 
                                 IOCTL_SCSI_PASS_THROUGH, 
                                 ScsiPassThrough, sizeof(SCSI_PASS_THROUGH), 
                                 ScsiPassThrough, dwBufferSize, 
                                 &dwBytesReturned, 
                                 NULL);

      if (bResult && dwBytesReturned >= (offsetof(SCSI_PASS_THROUGH, ScsiStatus) + sizeof(ScsiPassThrough->ScsiStatus))) {
         if (ScsiPassThrough->ScsiStatus == SCSISTAT_GOOD) {
            PREAD_POSITION_RESULT   pPosResult = (PREAD_POSITION_RESULT)((PUCHAR)ScsiPassThrough + ScsiPassThrough->DataBufferOffset);

            switch (pass)
            {
            case 0:     // SERVICEACTION_LONG_FORM
               {
                  TapePositionInfo->AtPartitionStart = pPosResult->LongBuffer.BOP;
                  TapePositionInfo->AtPartitionEnd = pPosResult->LongBuffer.EOP;

                  if (!TapePositionInfo->PartitionBlockValid) {
                     TapePositionInfo->PartitionBlockValid = !pPosResult->LongBuffer.BPU;

                     if (TapePositionInfo->PartitionBlockValid) {
                        TapePositionInfo->Partition =   Read32BitUnsigned(pPosResult->LongBuffer.Partition);
                        TapePositionInfo->BlockNumber = Read64BitUnsigned(pPosResult->LongBuffer.BlockNumber);
                     }
                  }

                  TapePositionInfo->FileSetValid = !pPosResult->LongBuffer.MPU;
                  if (TapePositionInfo->FileSetValid) {
                     TapePositionInfo->FileNumber =  Read64BitUnsigned(pPosResult->LongBuffer.FileNumber);
                     TapePositionInfo->SetNumber =   Read64BitUnsigned(pPosResult->LongBuffer.SetNumber);
                  }
               }
               break;

            case 1:     // SERVICEACTION_SHORT_FORM_BLOCKID
               {
                  // pPosResult->ShortBuffer.PERR;
                  // pPosResult->ShortBuffer.BYCU;
                  // pPosResult->ShortBuffer.BCU;
                  TapePositionInfo->AtPartitionStart = pPosResult->ShortBuffer.BOP;
                  TapePositionInfo->AtPartitionEnd = pPosResult->ShortBuffer.EOP;

                  if (!TapePositionInfo->PartitionBlockValid) {
                     TapePositionInfo->PartitionBlockValid = !pPosResult->ShortBuffer.BPU;

                     if (TapePositionInfo->PartitionBlockValid) {
                        TapePositionInfo->Partition =   pPosResult->ShortBuffer.Partition;
                        TapePositionInfo->BlockNumber = Read32BitUnsigned(pPosResult->ShortBuffer.FirstBlock);
                     }
                  }
                  // Read32BitsUnsigned(pPosResult->ShortBuffer.LastBlock);
                  // Read24BitsUnsigned(pPosResult->ShortBuffer.NumberBufferBlocks);
                  // Read32BitsUnsigned(pPosResult->ShortBuffer.NumberBufferBytes);
               }
               break;
            }
         }
      }
   }
   free(ScsiPassThrough);

   return NO_ERROR;
}

static DWORD GetDensityBlockSize(HANDLE hDevice, DWORD *pdwDensity, DWORD *pdwBlockSize)
{
   DWORD             dwBufferSize = sizeof(GET_MEDIA_TYPES) + 5 * sizeof(DEVICE_MEDIA_INFO);
   GET_MEDIA_TYPES * pGetMediaTypes = (GET_MEDIA_TYPES *)malloc(dwBufferSize);
   BOOL              bResult;
   DWORD             dwResult;

   if (pGetMediaTypes == NULL) {
      return ERROR_OUTOFMEMORY;
   }

   do {
      DWORD          dwBytesReturned;
      
      bResult = DeviceIoControl( hDevice, 
                                 IOCTL_STORAGE_GET_MEDIA_TYPES_EX, 
                                 NULL, 0, 
                                 (LPVOID)pGetMediaTypes, dwBufferSize, 
                                 &dwBytesReturned, 
                                 NULL);

      if (!bResult) {
         dwResult = GetLastError();

         if (dwResult != ERROR_INSUFFICIENT_BUFFER) {
            free(pGetMediaTypes);
            return dwResult;
         }

         dwBufferSize += 6 * sizeof(DEVICE_MEDIA_INFO);

         GET_MEDIA_TYPES * pNewBuffer = (GET_MEDIA_TYPES *)realloc(pGetMediaTypes, dwBufferSize);

         if (pNewBuffer != pGetMediaTypes) {
            free(pGetMediaTypes);

            if (pNewBuffer == NULL) {
               return ERROR_OUTOFMEMORY;
            }

            pGetMediaTypes = pNewBuffer;
         }
      }
   } while (!bResult);

   if (pGetMediaTypes->DeviceType != FILE_DEVICE_TAPE) {
      free(pGetMediaTypes);
      return ERROR_BAD_DEVICE;
   }

   for (DWORD idxMedia = 0; idxMedia < pGetMediaTypes->MediaInfoCount; idxMedia++) {

      if (pGetMediaTypes->MediaInfo[idxMedia].DeviceSpecific.TapeInfo.MediaCharacteristics & MEDIA_CURRENTLY_MOUNTED) {

         if (pGetMediaTypes->MediaInfo[idxMedia].DeviceSpecific.TapeInfo.BusType == BusTypeScsi) {
            *pdwDensity = pGetMediaTypes->MediaInfo[idxMedia].DeviceSpecific.TapeInfo.BusSpecificData.ScsiInformation.DensityCode;
         } else {
            *pdwDensity = 0;
         }

         *pdwBlockSize = pGetMediaTypes->MediaInfo[idxMedia].DeviceSpecific.TapeInfo.CurrentBlockSize;

         free(pGetMediaTypes);

         return NO_ERROR;
      }
   }

   free(pGetMediaTypes);

   return ERROR_NO_MEDIA_IN_DRIVE;
}

static int tape_pos(int fd, struct mtpos *mt_pos)
{
   DWORD partition;
   DWORD offset;
   DWORD offsetHi;
   BOOL result;

   if (fd < 3 || fd >= (int)(NUMBER_HANDLE_ENTRIES + 3) || 
       TapeHandleTable[fd - 3].OSHandle == INVALID_HANDLE_VALUE) {
      errno = EBADF;
      return -1;
   }

   PTAPE_HANDLE_INFO    pHandleInfo = &TapeHandleTable[fd - 3];

   result = GetTapePosition(pHandleInfo->OSHandle, TAPE_ABSOLUTE_BLOCK, &partition, &offset, &offsetHi);
   if (result == NO_ERROR) {
      mt_pos->mt_blkno = offset;
      return 0;
   }

   return -1;
}
