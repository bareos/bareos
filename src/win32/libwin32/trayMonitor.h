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
 * Kern Sibbald, August 2007
 *
 *   Version $Id$
 */

#ifndef __TRAY_MONITOR_H_
#define __TRAY_MONITOR_H_ 1

#define WM_TRAYNOTIFY WM_USER+1

/* Define the trayMonitor class */
class trayMonitor
{
public:
   trayMonitor();
  ~trayMonitor();

   void show(bool show);
   void install();
   void update(int bacstat);
   void sendMessage(DWORD msg, int bacstat);

   static LRESULT CALLBACK trayWinProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam);

   bool m_visible;
   bool m_installed;
   UINT m_tbcreated_msg;

   aboutDialog m_about;
   statusDialog m_status;

   HWND  m_hwnd;
   HMENU m_hmenu;
   NOTIFYICONDATA m_nid;
   HICON m_idle_icon;
   HICON m_running_icon;
   HICON m_error_icon;
   HICON m_warn_icon;
};

#endif /* __TRAY_MONITOR_H_ */
