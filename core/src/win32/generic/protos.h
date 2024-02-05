/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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
// Kern Sibbald, August 2007

#ifndef BAREOS_WIN32_GENERIC_PROTOS_H_
#define BAREOS_WIN32_GENERIC_PROTOS_H_

#define LogErrorMessage(msg) LogLastErrorMsg((msg), __FILE__, __LINE__)

extern int BareosAppMain();
extern void LogLastErrorMsg(const char* msg, const char* fname, int lineno);
extern int BareosMain(int argc, char* argv[]);
extern BOOL ReportStatus(DWORD state, DWORD exitcode, DWORD waithint);

/* service.cpp */
bool postToBareos(UINT message, WPARAM wParam, LPARAM lParam);
bool isAService();
int installService(const char* svc);
int removeService();
int stopRunningBareos();
int bareosServiceMain();

/* Globals */
extern DWORD service_thread_id;
extern DWORD service_error;
extern bool opt_debug;
extern bool have_service_api;
extern HINSTANCE appInstance;
extern int bareosstat;

#endif  // BAREOS_WIN32_GENERIC_PROTOS_H_
