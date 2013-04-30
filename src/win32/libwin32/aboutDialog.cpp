/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2007 Free Software Foundation Europe e.V.

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
 *
 * Kern Sibbald, August 2007
 *
 * Version $Id$
 *
*/

#include "bacula.h"
#include "win32.h"

static BOOL CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   /* Get the dialog class pointer from USERDATA */
   aboutDialog *about;

   switch (uMsg) {
   case WM_INITDIALOG:
      /* save the dialog class pointer */
      SetWindowLong(hwnd, GWL_USERDATA, lParam);
      about = (aboutDialog *)lParam;

      /* Show the dialog */
      SetForegroundWindow(hwnd);
      about->m_visible = true;
      return true;

   case WM_COMMAND:
      switch (LOWORD(wParam)) {
      case IDCANCEL:
      case IDOK:
         EndDialog(hwnd, true);
         about = (aboutDialog *)GetWindowLong(hwnd, GWL_USERDATA);
         about->m_visible = false;
         return true;
      }
      break;

   case WM_DESTROY:
      EndDialog(hwnd, false);
      about = (aboutDialog *)GetWindowLong(hwnd, GWL_USERDATA);
      about->m_visible = false;
      return true;
   }
   return false;
}

void aboutDialog::show(bool show)
{
   if (show && !m_visible) {
      DialogBoxParam(appInstance, MAKEINTRESOURCE(IDD_ABOUT), NULL,
         (DLGPROC)DialogProc, (LPARAM)this);
   }
}
