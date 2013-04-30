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

#pragma once
#include "vector"

class CScsiDeviceListEntry
{
   friend class CScsiDeviceList;

   static LPCTSTR    c_DeviceTypes[];
   static const int  c_MaxDevicePathLength = 16;      // :255:255:255:255

public:
   enum DeviceType { Unknown, CDRom, Changer, Disk, Tape };

   CScsiDeviceListEntry(void);
   CScsiDeviceListEntry(const CScsiDeviceListEntry &other);
   ~CScsiDeviceListEntry(void);

   inline CScsiDeviceListEntry &operator =(const CScsiDeviceListEntry &other);

   inline DeviceType GetType() { return m_eDeviceType; }
   inline LPCTSTR    GetTypeName() { return c_DeviceTypes[m_eDeviceType]; }
   inline LPCTSTR    GetIdentifier() { return m_lpszIdentifier != NULL ? m_lpszIdentifier : _T(""); }
   inline LPCTSTR    GetDeviceName() { return m_lpszDeviceName != NULL ? m_lpszDeviceName : _T(""); }
   inline LPCTSTR    GetDevicePath();

private:
   DeviceType  m_eDeviceType;
   LPTSTR      m_lpszIdentifier;
   LPTSTR      m_lpszDeviceName;
   DWORD       m_dwDeviceId;
   TCHAR       m_szDevicePath[c_MaxDevicePathLength + 1];
};

CScsiDeviceListEntry &
CScsiDeviceListEntry::operator =(const CScsiDeviceListEntry &other)
{
   m_eDeviceType = other.m_eDeviceType;

   if (m_lpszIdentifier != NULL)
   {
      free(m_lpszIdentifier);
   }
   m_lpszIdentifier = other.m_lpszIdentifier != NULL ? _tcsdup(other.m_lpszIdentifier) : NULL;

   if (m_lpszDeviceName != NULL)
   {
      free(m_lpszDeviceName);
   }
   m_lpszDeviceName = other.m_lpszDeviceName != NULL ? _tcsdup(other.m_lpszDeviceName) : NULL;

   m_dwDeviceId = other.m_dwDeviceId;
   _tcscpy(m_szDevicePath, other.m_szDevicePath);

   return *this;
}

LPCTSTR
CScsiDeviceListEntry::GetDevicePath()
{
   if (m_szDevicePath[0] == _T('\0'))
   {
      _sntprintf( m_szDevicePath, c_MaxDevicePathLength, 
                  _T("%d:%d:%d:%d"), 
                  (m_dwDeviceId >> 24) & 0xFF,
                  (m_dwDeviceId >> 16) & 0xFF,
                  (m_dwDeviceId >>  8) & 0xFF,
                  m_dwDeviceId        & 0xFF);
      m_szDevicePath[c_MaxDevicePathLength] = _T('\0');
   }

   return m_szDevicePath;
}

class CScsiDeviceList :
   public std::vector<CScsiDeviceListEntry>
{
   static TCHAR         c_ScsiPath[];
   static LPCTSTR       c_lpszFormatList[];

// \\Scsi\\Scsi Port 255\\Scsi Bus 255\\Target Id 255\\Logical Unit Id 255
// 1 4   1 13           1 12          1 13           1 19 = 66
   static const int     c_MaxKeyPathLength = 66;

// Logical Unit Id 255
   static const int     c_MaxSubkeyLength = 19;

// Identifier = 28, DeviceName = 10+, Type = 23
   static const int     c_MaxValueLength = 30;

// Adapter \\ Bus \\ Target \\ LUN
   static const int     c_MaxKeyDepth = 4;

public:
   inline   CScsiDeviceList(void);
   inline   ~CScsiDeviceList(void);

   bool     Populate();
   void     PrintLastError(LPTSTR lpszName = NULL);

protected:
   bool     ProcessKey(HKEY hKey, int iLevel, DWORD dwDeviceId);
   bool     ProcessValues(HKEY hKey, DWORD dwDeviceId);

private:
   TCHAR    m_szLastOperation[80 + 1]; // Max length "Enumerating subkeys of "
   TCHAR    m_szLastKey[c_MaxKeyPathLength + 1];
   DWORD    m_dwLastKeyLength;
   LONG     m_lLastError;
};

CScsiDeviceList::CScsiDeviceList(void)
{
   m_szLastOperation[0] = _T('\0');
   m_szLastKey[0] = _T('\0');
   m_dwLastKeyLength = 0;
   m_lLastError = 0;
}

CScsiDeviceList::~CScsiDeviceList(void)
{
}
