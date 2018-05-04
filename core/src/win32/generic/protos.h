/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2009 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
 * Kern Sibbald, August 2007
 */

#define LogErrorMessage(msg) LogLastErrorMsg((msg), __FILE__, __LINE__)

DLL_IMP_EXP extern int BareosAppMain();
DLL_IMP_EXP extern void LogLastErrorMsg(const char *msg, const char *fname, int lineno);
DLL_IMP_EXP extern int BareosMain(int argc, char *argv[]);
DLL_IMP_EXP extern BOOL ReportStatus(DWORD state, DWORD exitcode, DWORD waithint);
DLL_IMP_EXP extern void d_msg(const char *, int, int, const char *, ...);
DLL_IMP_EXP extern char *bareos_status(char *buf, int buf_len);

/* service.cpp */
DLL_IMP_EXP bool postToBareos(UINT message, WPARAM wParam, LPARAM lParam);
DLL_IMP_EXP bool isAService();
DLL_IMP_EXP int installService(const char *svc);
DLL_IMP_EXP int removeService();
DLL_IMP_EXP int stopRunningBareos();
DLL_IMP_EXP int bareosServiceMain();

/* Globals */
DLL_IMP_EXP extern DWORD service_thread_id;
DLL_IMP_EXP extern DWORD service_error;
DLL_IMP_EXP extern bool opt_debug;
DLL_IMP_EXP extern bool have_service_api;
DLL_IMP_EXP extern HINSTANCE appInstance;
DLL_IMP_EXP extern int bareosstat;
