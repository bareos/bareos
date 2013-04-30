/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2009 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is 
   listed in the file LICENSE.

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

#define log_error_message(msg) LogLastErrorMsg((msg), __FILE__, __LINE__)

extern int BaculaAppMain();
extern void LogLastErrorMsg(const char *msg, const char *fname, int lineno);

extern int BaculaMain(int argc, char *argv[]);
extern BOOL ReportStatus(DWORD state, DWORD exitcode, DWORD waithint);
extern void d_msg(const char *, int, int, const char *, ...);
extern char *bac_status(char *buf, int buf_len);


/* service.cpp */
bool postToBacula(UINT message, WPARAM wParam, LPARAM lParam);
bool isAService();
int installService(const char *svc);
int removeService();
int stopRunningBacula();
int baculaServiceMain();


/* Globals */
extern DWORD service_thread_id;
extern DWORD service_error;
extern bool opt_debug;
extern bool have_service_api;
extern HINSTANCE appInstance;
extern int bacstat;
