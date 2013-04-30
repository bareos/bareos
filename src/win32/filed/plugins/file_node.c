/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2008-2010 Free Software Foundation Europe e.V.

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
 *  Written by James Harper, October 2008
 */

#include "exchange-fd.h"

file_node_t::file_node_t(char *name, node_t *parent_node) : node_t(name, NODE_TYPE_FILE, parent_node)
{
   backup_file_handle = INVALID_HANDLE_VALUE;
   restore_file_handle = INVALID_HANDLE_VALUE;
   restore_at_file_level = FALSE;
}

file_node_t::~file_node_t()
{
   if (backup_file_handle != INVALID_HANDLE_VALUE)
   {
      //_DebugMessage(100, "closing file handle in destructor\n");
      CloseHandle(backup_file_handle);
   }
   if (restore_file_handle != INVALID_HANDLE_VALUE)
   {
      //_DebugMessage(100, "closing file handle in destructor\n");
      if (restore_at_file_level)
      {
         CloseHandle(restore_file_handle);
      }
      else
      {
         // maybe one day
      }
   }
}

bRC
file_node_t::startBackupFile(exchange_fd_context_t *context, struct save_pkt *sp)
{
   time_t now = time(NULL);
   _DebugMessage(100, "startBackupNode_FILE state = %d\n", state);

   if (context->job_level == 'F' || parent->type == NODE_TYPE_STORAGE_GROUP) {
      sp->fname = full_path;
      sp->link = full_path;
      _DebugMessage(100, "fname = %s\n", sp->fname);
      sp->statp.st_mode = 0700 | S_IFREG;
      sp->statp.st_ctime = now;
      sp->statp.st_mtime = now;
      sp->statp.st_atime = now;
      sp->statp.st_size = (uint64_t)-1;
      sp->type = FT_REG;
      return bRC_OK;
   } else {
      bfuncs->setBaculaValue(context->bpContext, bVarFileSeen, (void *)full_path);
      return bRC_Seen;
   }
}

bRC
file_node_t::endBackupFile(exchange_fd_context_t *context)
{
   _DebugMessage(100, "endBackupNode_FILE state = %d\n", state);

   context->current_node = parent;

   return bRC_OK;
}

bRC
file_node_t::createFile(exchange_fd_context_t *context, struct restore_pkt *rp)
{
   //HrESERestoreOpenFile with name of log file

   _DebugMessage(100, "createFile_FILE state = %d\n", state);
   rp->create_status = CF_EXTRACT;
   return bRC_OK;
}

bRC
file_node_t::endRestoreFile(exchange_fd_context_t *context)
{
   _DebugMessage(100, "endRestoreFile_FILE state = %d\n", state);
   context->current_node = parent;
   return bRC_OK;
}

bRC
file_node_t::pluginIoOpen(exchange_fd_context_t *context, struct io_pkt *io)
{
   HRESULT result;
   HANDLE handle;
   char *tmp = new char[wcslen(filename) + 1];
   wcstombs(tmp, filename, wcslen(filename) + 1);

   _DebugMessage(100, "pluginIoOpen_FILE - filename = %s\n", tmp);
   io->status = 0;
   io->io_errno = 0;
   if (context->job_type == JOB_TYPE_BACKUP)
   {
      _DebugMessage(10, "Calling HrESEBackupOpenFile\n");
      result = HrESEBackupOpenFile(hccx, filename, 65535, 1, &backup_file_handle, &section_size);
      if (result)
      {
         _JobMessage(M_FATAL, "HrESEBackupOpenFile failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
         backup_file_handle = INVALID_HANDLE_VALUE;
         io->io_errno = 1;
         return bRC_Error;
      }
   }
   else
   {
      _DebugMessage(10, "Calling HrESERestoreOpenFile for '%s'\n", tmp);
      result = HrESERestoreOpenFile(hccx, filename, 1, &restore_file_handle);
      if (result == hrRestoreAtFileLevel)
      {
         restore_at_file_level = true;
         _DebugMessage(100, "Calling CreateFileW for '%s'\n", tmp);
         handle = CreateFileW(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
         if (handle == INVALID_HANDLE_VALUE)
         {
            _JobMessage(M_FATAL, "CreateFile failed");
            return bRC_Error;
         }
         restore_file_handle = (void *)handle;
         return bRC_OK;
      }
      else if (result == 0)
      {
         _JobMessage(M_FATAL, "Exchange File IO API not yet supported for restore\n");
         restore_at_file_level = false;
         return bRC_Error;
      }
      else
      {
         _JobMessage(M_FATAL, "HrESERestoreOpenFile failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
         return bRC_Error;
      }
   }
   return bRC_OK;
}

bRC
file_node_t::pluginIoRead(exchange_fd_context_t *context, struct io_pkt *io)
{
   HRESULT result;
   uint32_t readLength;

   io->status = 0;
   io->io_errno = 0;
   _DebugMessage(200, "Calling HrESEBackupReadFile\n");
   result = HrESEBackupReadFile(hccx, backup_file_handle, io->buf, io->count, &readLength);
   if (result)
   {
      io->io_errno = 1;
      return bRC_Error;
   }
   io->status = readLength;
   size += readLength;
   return bRC_OK;
}

bRC
file_node_t::pluginIoWrite(exchange_fd_context_t *context, struct io_pkt *io)
{
   DWORD bytes_written;

   io->io_errno = 0;
   if (!restore_at_file_level)
      return bRC_Error;

   if (!WriteFile(restore_file_handle, io->buf, io->count, &bytes_written, NULL))
   {
      _JobMessage(M_FATAL, "Write Error");
      return bRC_Error;
   }

   if (bytes_written != (DWORD)io->count)
   {
      _JobMessage(M_FATAL, "Short write");
      return bRC_Error;
   }
   io->status = bytes_written;
   
   return bRC_OK;
}

bRC
file_node_t::pluginIoClose(exchange_fd_context_t *context, struct io_pkt *io)
{
   if (context->job_type == JOB_TYPE_BACKUP)
   {
      _DebugMessage(100, "Calling HrESEBackupCloseFile\n");
      HrESEBackupCloseFile(hccx, backup_file_handle);
      backup_file_handle = INVALID_HANDLE_VALUE;
      return bRC_OK;
   }
   else
   {
      if (restore_at_file_level)
      {
         CloseHandle(restore_file_handle);
         restore_file_handle = INVALID_HANDLE_VALUE;
         return bRC_OK;
      }
      else
      {
         return bRC_OK;
      }
   }
}
