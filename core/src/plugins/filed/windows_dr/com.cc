/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#include <stdexcept>
#include <format>

#include <Windows.h>
#include <guiddef.h>
#include <Vss.h>
#include <VsWriter.h>
#include <VsBackup.h>

__declspec(dllexport) const char* hresult_as_str(HRESULT hr)
{
  switch (hr) {
    case E_INVALIDARG:
      return "One of the parameter values is not valid";
      break;
    case E_OUTOFMEMORY:
      return "The caller is out of memory or other system resources";
      break;
    case E_ACCESSDENIED:
      return "The caller does not have sufficient backup privileges or is not "
             "an administrator";
      break;
    case VSS_E_INVALID_XML_DOCUMENT:
      return "The XML document is not valid";
      break;
    case VSS_E_OBJECT_NOT_FOUND:
      return "The specified file does not exist";
      break;
    case VSS_E_BAD_STATE:
      return "Object is not initialized; called during restore or not called "
             "in "
             "correct sequence";
      break;
    case VSS_E_WRITER_INFRASTRUCTURE:
      return "The writer infrastructure is not operating properly. Check that "
             "the "
             "Event Service and VSS have been started, and check for errors "
             "associated with those services in the error log";
      break;
    case VSS_S_ASYNC_CANCELLED:
      return "The asynchronous operation was canceled by a previous call to "
             "IVssAsync::Cancel";
      break;
    case VSS_S_ASYNC_PENDING:
      return "The asynchronous operation is still running";
      break;
    case RPC_E_CHANGED_MODE:
      return "Previous call to CoInitializeEx specified the multithread "
             "apartment "
             "(MTA). This call indicates single-threaded apartment has "
             "occurred";
      break;
    case S_FALSE:
      return "No writer found for the current component";
      break;
    default:
      return "Unknown error";
      break;
  }
}

void throw_on_error(HRESULT hr, const char* callsite)
{
  if (!FAILED(hr)) { return; }

  throw std::runtime_error(
      std::format("{}: {} ({:X})", callsite, hresult_as_str(hr), hr));
}
