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
 *  Kern Sibbald, August 2007
 *
 *  Version $Id$
 *
 * This is a generic tray monitor routine, which is used by all three
 *  of the daemons. Each one compiles it with slightly different
 *  #defines.
 *
 */

#include "bacula.h"
#include "jcr.h"
#include "win32.h"

trayMonitor::trayMonitor()
{

// m_tbcreated_msg = RegisterWindowMessage("TaskbarCreated");
   
   /* Create a window to handle tray icon messages */
   WNDCLASSEX trayclass;

   trayclass.cbSize         = sizeof(trayclass);
   trayclass.style          = 0;
   trayclass.lpfnWndProc    = trayMonitor::trayWinProc;
   trayclass.cbClsExtra     = 0;
   trayclass.cbWndExtra     = 0;
   trayclass.hInstance      = appInstance;
   trayclass.hIcon          = LoadIcon(NULL, IDI_APPLICATION);
   trayclass.hCursor        = LoadCursor(NULL, IDC_ARROW);
   trayclass.hbrBackground  = (HBRUSH)GetStockObject(WHITE_BRUSH);
   trayclass.lpszMenuName   = NULL;
   trayclass.lpszClassName  = APP_NAME;
   trayclass.hIconSm        = LoadIcon(NULL, IDI_APPLICATION);

   RegisterClassEx(&trayclass);

   m_hwnd = CreateWindow(APP_NAME, APP_NAME, WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, 200, 200,
                NULL, NULL, appInstance, NULL);
   if (!m_hwnd) {
      PostQuitMessage(0);
      return;
   }

   /* Save our class pointer */
   SetWindowLong(m_hwnd, GWL_USERDATA, (LPARAM)this);


   // Load the icons for the tray
   m_idle_icon    = LoadIcon(appInstance, MAKEINTRESOURCE(IDI_IDLE));
   m_running_icon = LoadIcon(appInstance, MAKEINTRESOURCE(IDI_RUNNING));
   m_error_icon   = LoadIcon(appInstance, MAKEINTRESOURCE(IDI_JOB_ERROR));
   m_warn_icon    = LoadIcon(appInstance, MAKEINTRESOURCE(IDI_JOB_WARNING));

   /* Load the menu */
   m_hmenu = LoadMenu(appInstance, MAKEINTRESOURCE(IDR_TRAYMENU));
   m_visible = false;
   m_installed = false;

   /* Install the icon in the tray */
   install();

   /* Timer to trigger icon updating */
   SetTimer(m_hwnd, 1, 5000, NULL);
}

trayMonitor::~trayMonitor()
{
   /* Remove the icon from the tray */
   sendMessage(NIM_DELETE, 0);
        
   if (m_hmenu) {
      DestroyMenu(m_hmenu);
      m_hmenu = NULL;
   }
}

void trayMonitor::install()
{
   m_installed = true;
   sendMessage(NIM_ADD, bacstat);
}

void trayMonitor::update(int bacstat)
{
   if (!m_installed) {
      install();
   }
   (void)bac_status(NULL, 0);
   sendMessage(NIM_MODIFY, bacstat);
}

void trayMonitor::sendMessage(DWORD msg, int bacstat)
{
   struct s_last_job *job;
   
   // Create the tray icon message
   m_nid.hWnd = m_hwnd;
   m_nid.cbSize = sizeof(m_nid);
   m_nid.uID = IDI_BACULA;                  // never changes after construction
   switch (bacstat) {
   case 0:
      m_nid.hIcon = m_idle_icon;
      break;
   case JS_Running:
      m_nid.hIcon = m_running_icon;
      break;
   case JS_ErrorTerminated:
      m_nid.hIcon = m_error_icon;
      break;
   default:
      if (last_jobs->size() > 0) {
         job = (struct s_last_job *)last_jobs->last();
         if (job->Errors) {
            m_nid.hIcon = m_warn_icon;
         } else {
            m_nid.hIcon = m_idle_icon;
         }
      } else {
         m_nid.hIcon = m_idle_icon;
      }
      break;
   }

   m_nid.uFlags = NIF_ICON | NIF_MESSAGE;
   m_nid.uCallbackMessage = WM_TRAYNOTIFY;


   /* Use the resource string as tip */
   if (LoadString(appInstance, IDI_BACULA, m_nid.szTip, sizeof(m_nid.szTip))) {
       m_nid.uFlags |= NIF_TIP;
   }

   /* Add the Bacula status to the tip string */
   if (m_nid.uFlags & NIF_TIP) {
       bac_status(m_nid.szTip, sizeof(m_nid.szTip));
   }

   if (Shell_NotifyIcon(msg, &m_nid)) {
      EnableMenuItem(m_hmenu, ID_CLOSE, MF_ENABLED);
   }
}

/*
 * This is the windows call back for our tray window
 */
LRESULT CALLBACK trayMonitor::trayWinProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
   HMENU menu;
   trayMonitor *mon = (trayMonitor *)GetWindowLong(hwnd, GWL_USERDATA);

   switch (iMsg) {

   /* Every five seconds, a timer message causes the icon to update */
   case WM_TIMER:
      if (isAService()) {
         mon->install();
      }
      mon->update(bacstat);
      break;

   case WM_CREATE:
      return 0;

   case WM_COMMAND:
      /* User has clicked an item on the tray monitor menu */
      switch (LOWORD(wParam)) {
      case ID_STATUS:
         /* show the dialog box */
         mon->m_status.show(true);
         mon->update(bacstat);
         break;

      case ID_ABOUT:
         /* Show the About box */
         mon->m_about.show(true);
         break;

      /* This is turned off now */
#ifdef xxx
      case ID_CLOSE:
         /* User selected Close from the tray menu */
         PostMessage(hwnd, WM_CLOSE, 0, 0);
         break;
#endif

      }
      return 0;

   /* Our special command to check for mouse events */
   case WM_TRAYNOTIFY:
      /* Right button click pops up the menu */
      if (lParam == WM_RBUTTONUP) {
         POINT mouse;
         /* Get the menu and pop it up */
         menu = GetSubMenu(mon->m_hmenu, 0);
         if (!menu) {
             return 0;
         }

         /* The first menu item (Status) is the default */
         SetMenuDefaultItem(menu, 0, TRUE);
         GetCursorPos(&mouse);
         SetForegroundWindow(mon->m_nid.hWnd);  /* display the menu */

         /* Open the menu at the mouse position */
         TrackPopupMenu(menu, 0, mouse.x, mouse.y, 0, mon->m_nid.hWnd, NULL);

      /* Left double click brings up status dialog directly */
      } else if (lParam == WM_LBUTTONDBLCLK) {
         mon->m_status.show(true);
         mon->update(bacstat);
      }
      return 0;

   case WM_CLOSE:
      if (isAService()) {
          mon->sendMessage(NIM_DELETE, 0);
      }
      terminate_app(0);
      break;

   case WM_DESTROY:
      /* zap everything */
      PostQuitMessage(0);
      return 0;

   case WM_QUERYENDSESSION:
      if (!isAService() || lParam == 0) {
         PostQuitMessage(0);
         return TRUE;
      }
      return TRUE;

    default:
       /* Need to redraw tray icon */
//     if (iMsg == mon->m_tbcreated_msg) {
//        mon->install();    
//     }
       break;
   }

   return DefWindowProc(hwnd, iMsg, wParam, lParam);
}
