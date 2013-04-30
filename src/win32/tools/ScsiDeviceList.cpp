/*
 * ScsiDeviceList.cpp - Class which provides information on installed devices.
 *
 * Author: Robert Nelson, August, 2006 <robertn@the-nelsons.org>
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
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2006-2006 Free Software Foundation Europe e.V.

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

#if defined(_MSC_VER) && defined(_DEBUG)
#include <afx.h>
#else
#include <windows.h>
#endif

#include <stdio.h>
#include <tchar.h>

#include "ScsiDeviceList.h"

#if defined(_MSC_VER) && defined(_DEBUG)
#define  new   DEBUG_NEW
#endif

TCHAR    CScsiDeviceList::c_ScsiPath[] = _T("HARDWARE\\DEVICEMAP\\Scsi");

LPCTSTR  CScsiDeviceList::c_lpszFormatList[] =
{
   _T("Logical Unit Id %d"),
   _T("Target Id %d"),
   _T("Scsi Bus %d"),
   _T("Scsi Port %d")
};

LPCTSTR  CScsiDeviceListEntry::c_DeviceTypes[] =
{
   _T("Unknown"),
   _T("CDRom"),
   _T("Changer"),
   _T("Disk"),
   _T("Tape")
};

CScsiDeviceListEntry::CScsiDeviceListEntry(const CScsiDeviceListEntry &other)
{
   m_eDeviceType = other.m_eDeviceType;

   m_lpszIdentifier = other.m_lpszIdentifier != NULL ? _tcsdup(other.m_lpszIdentifier) : NULL;

   m_lpszDeviceName = other.m_lpszDeviceName != NULL ? _tcsdup(other.m_lpszDeviceName) : NULL;

   m_dwDeviceId = other.m_dwDeviceId;
   _tcscpy(m_szDevicePath, other.m_szDevicePath);
}

CScsiDeviceListEntry::CScsiDeviceListEntry(void)
{
   m_eDeviceType = Unknown;
   m_lpszIdentifier = NULL;
   m_lpszDeviceName = NULL;
   m_dwDeviceId = 0;
   m_szDevicePath[0] = _T('\0');
}

CScsiDeviceListEntry::~CScsiDeviceListEntry(void)
{
   if (m_lpszIdentifier != NULL)
   {
      free(m_lpszIdentifier);
   }

   if (m_lpszDeviceName != NULL)
   {
      free(m_lpszDeviceName);
   }
}

bool
CScsiDeviceList::Populate()
{
   this->clear();

   HKEY  hScsiKey;

   _tcscpy(m_szLastKey, _T("\\Scsi"));
   m_dwLastKeyLength = 5;

   m_lLastError = RegOpenKeyEx(  HKEY_LOCAL_MACHINE, 
                                 c_ScsiPath, 
                                 0, 
                                 KEY_READ, 
                                 &hScsiKey);

   if (m_lLastError != ERROR_SUCCESS) {
      _tcscpy(m_szLastOperation, _T("Opening key "));
      _tcscpy(m_szLastKey, c_ScsiPath);
      return false;
   }

   if (!ProcessKey(hScsiKey, c_MaxKeyDepth - 1, 0)) {
      return false;
   }

#if defined(_DEBUG)
   _fputtc(_T('\n'), stderr);
#endif

   return true;
}

bool
CScsiDeviceList::ProcessKey(HKEY hKey, int iLevel, DWORD dwDeviceId)
{
#if defined(_DEBUG)
   switch (iLevel)
   {
   case 3:
      _ftprintf(  stderr, 
                  _T("%-64s\n"), 
                  &m_szLastKey[1]);
      break;

   case 2:
      _ftprintf(  stderr, 
                  _T("%-64s%d\n"), 
                  &m_szLastKey[1], 
                  dwDeviceId & 0xFF);
      break;

   case 1:
      _ftprintf(  stderr, 
                  _T("%-64s%d:%d\n"), 
                  &m_szLastKey[1], 
                  (dwDeviceId >>  8) & 0xFF,
                   dwDeviceId        & 0xFF);
      break;

   case 0:
      _ftprintf(  stderr, 
                  _T("%-64s%d:%d:%d\n"), 
                  &m_szLastKey[1], 
                  (dwDeviceId >>  16) & 0xFF,
                  (dwDeviceId >>  8) & 0xFF,
                   dwDeviceId        & 0xFF);
      break;
   }
#endif

   for (int idxSubkey = 0; ; idxSubkey++) {

      TCHAR szSubkeyName[c_MaxSubkeyLength + 1];
      DWORD dwLength;

      dwLength = sizeof(szSubkeyName);

      m_lLastError = RegEnumKeyEx(  hKey, 
                                    idxSubkey, 
                                    szSubkeyName, 
                                    &dwLength, 
                                    NULL, 
                                    NULL, 
                                    NULL, 
                                    NULL);

      if (m_lLastError == ERROR_NO_MORE_ITEMS) {
         break;
      } else  if (m_lLastError == ERROR_MORE_DATA) {
#if defined(_DEBUG)
         _tcscpy(m_szLastOperation, _T("Enumerating subkeys of "));
         PrintLastError();
#endif
         // Subkey name is too long
         continue;
      } else if (m_lLastError != ERROR_SUCCESS) {
         // Unexpected Error
         _tcscpy(m_szLastOperation, _T("Enumerating subkeys of "));
         return false;
      }

      int   iValue;

      if (_stscanf(szSubkeyName, c_lpszFormatList[iLevel], &iValue) != 1) {
         // Ignore this subkey, it is probably Initiator Id n
         continue;
      }

      m_szLastKey[m_dwLastKeyLength++] = _T('\\');

      DWORD dwSubkeyLength = (DWORD)_tcslen(szSubkeyName);
      memcpy(&m_szLastKey[m_dwLastKeyLength], szSubkeyName, (dwSubkeyLength + 1) * sizeof(TCHAR));
      m_dwLastKeyLength += dwSubkeyLength;

      HKEY  hSubkey;

      m_lLastError = RegOpenKeyEx(hKey, szSubkeyName, 0, KEY_READ, &hSubkey);

      if (m_lLastError != ERROR_SUCCESS) {
         _tcscpy(m_szLastOperation, _T("Opening key "));
         return false;
      }

      if (iLevel == 0) {
#if defined(_DEBUG)
         _ftprintf(  stderr, 
                     _T("%-64s%d:%d:%d:%d\n"), 
                     &m_szLastKey[1], 
                     (dwDeviceId >> 16) & 0xFF,
                     (dwDeviceId >>  8) & 0xFF,
                      dwDeviceId        & 0xFF,
                      iValue);
#endif

         ProcessValues(hSubkey, (dwDeviceId << 8) | iValue);
      } else {
         if (!ProcessKey(hSubkey, iLevel - 1, (dwDeviceId << 8) | iValue)) {
            return false;
         }
      }

      m_dwLastKeyLength -= dwSubkeyLength;
      m_dwLastKeyLength--;
      m_szLastKey[m_dwLastKeyLength] = _T('\0');
   }

   return true;
}

bool
CScsiDeviceList::ProcessValues(HKEY hKey, DWORD dwDeviceId)
{
   CScsiDeviceListEntry    EntryTemplate;
   DWORD                   dwType;
   DWORD                   dwSize;
   TCHAR                   szValue[c_MaxValueLength + 1];

   this->push_back(EntryTemplate);
   CScsiDeviceListEntry &  entry = this->back();

   dwSize = sizeof(szValue);

   m_lLastError = RegQueryValueEx(  hKey, 
                                    _T("Identifier"), 
                                    NULL, 
                                    &dwType, 
                                    (LPBYTE)&szValue[0], 
                                    &dwSize);

   if (m_lLastError == ERROR_SUCCESS) {
      entry.m_lpszIdentifier = _tcsdup(szValue);
   } else {
#if defined(_DEBUG)
      _tcscpy(m_szLastOperation, _T("Reading value "));
      PrintLastError(_T("Identifier"));
#endif
   }

   dwSize = sizeof(szValue);

   m_lLastError = RegQueryValueEx(  hKey, 
                                    _T("DeviceName"), 
                                    NULL, 
                                    &dwType, 
                                    (LPBYTE)&szValue[0], 
                                    &dwSize);

   if (m_lLastError == ERROR_SUCCESS) {
      entry.m_lpszDeviceName = _tcsdup(szValue);
   } else {
#if defined(_DEBUG)
      _tcscpy(m_szLastOperation, _T("Reading value "));
      PrintLastError(_T("DeviceName"));
#endif
   }

   dwSize = sizeof(szValue);

   m_lLastError = RegQueryValueEx(  hKey, 
                                    _T("Type"), 
                                    NULL, 
                                    &dwType, 
                                    (LPBYTE)&szValue[0], 
                                    &dwSize);

   if (m_lLastError == ERROR_SUCCESS) {
      if (_tcscmp(_T("CdRomPeripheral"), szValue) == 0) {
         entry.m_eDeviceType = CScsiDeviceListEntry::CDRom;
      } else if (_tcscmp(_T("DiskPeripheral"), szValue) == 0) {
         entry.m_eDeviceType = CScsiDeviceListEntry::Disk;
      } else if (_tcscmp(_T("MediumChangerPeripheral"), szValue) == 0) {
         entry.m_eDeviceType = CScsiDeviceListEntry::Changer;
      } else if (_tcscmp(_T("TapePeripheral"), szValue) == 0) {
         entry.m_eDeviceType = CScsiDeviceListEntry::Tape;
      }
   } else {
#if defined(_DEBUG)
      _tcscpy(m_szLastOperation, _T("Reading value "));
      PrintLastError(_T("Type"));
#endif
   }

   entry.m_dwDeviceId = dwDeviceId;

   return true;
}

void
CScsiDeviceList::PrintLastError(LPTSTR lpszName)
{
   LPTSTR   lpszMessage = NULL;

   _fputts(_T("Error: "), stderr);
   _fputts(m_szLastOperation, stderr);
   _fputtc(_T('"'), stderr);
   _fputts(lpszName != NULL ? lpszName : m_szLastKey, stderr);
   _fputts(_T("\" - "), stderr);

   FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, 
                  NULL, m_lLastError, 0, (LPTSTR)&lpszMessage, 0, NULL);

   if (lpszMessage != NULL) {
      _fputts(lpszMessage, stderr);
      LocalFree(lpszMessage);
   }
}
