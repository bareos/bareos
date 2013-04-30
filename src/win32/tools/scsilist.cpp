/*
 * scsilist.cpp - Outputs the contents of a ScsiDeviceList.
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
#include <conio.h>

#include "ScsiDeviceList.h"

#if defined(_MSC_VER) && defined(_DEBUG)
#define  new   DEBUG_NEW
#endif

int _tmain(int argc, _TCHAR* argv[])
{
#if defined(_MSC_VER) && defined(_DEBUG)
   CMemoryState InitialMemState, FinalMemState, DiffMemState;

   InitialMemState.Checkpoint();

   {
#endif

   CScsiDeviceList   DeviceList;

   if (!DeviceList.Populate())
   {
      DeviceList.PrintLastError();
      return 1;
   }

#define  HEADING \
   _T("Device                        Type     Physical     Name\n") \
   _T("======                        ====     ========     ====\n")

   _fputts(HEADING, stdout);

   for (DWORD index = 0; index < DeviceList.size(); index++) {

      CScsiDeviceListEntry &entry = DeviceList[index];

      if (entry.GetType() != CScsiDeviceListEntry::Disk) {

         _tprintf(_T("%-28s  %-7s  %-11s  %-27s\n"), 
                  entry.GetIdentifier(),
                  entry.GetTypeName(),
                  entry.GetDevicePath(),
                  entry.GetDeviceName());
      }
   }

#if defined(_MSC_VER) && defined(_DEBUG)
   }

   InitialMemState.DumpAllObjectsSince();

   FinalMemState.Checkpoint();
   DiffMemState.Difference(InitialMemState, FinalMemState);
   DiffMemState.DumpStatistics();
#endif

   if (argc > 1 && _tcsnicmp(argv[1], _T("/pause"), sizeof(_T("/pause")) - sizeof(TCHAR)) == 0) {
      _fputts(_T("\nPress any key to continue\n"), stderr);
      _getch();
   }

   return 0;
}
