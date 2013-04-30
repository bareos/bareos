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

storage_group_node_t::storage_group_node_t(char *name, node_t *parent_node) : node_t(name, NODE_TYPE_STORAGE_GROUP, parent_node)
{
   ibi = NULL;
   store_node = NULL;
   current_dbi = 0;
   restore_environment = NULL;
   saved_log_path = NULL;
   next = NULL;
}

storage_group_node_t::~storage_group_node_t()
{
/*
   if (dbi_node != NULL)
      delete dbi_node;

   if (file_node != NULL)
      delete file_node;
*/
}

bRC
storage_group_node_t::startBackupFile(exchange_fd_context_t *context, struct save_pkt *sp)
{
   HRESULT result;
   int len;
   WCHAR *tmp_logfiles, *tmp_logfile_ptr;
   char *tmp;

   for(;;)
   {
      _DebugMessage(100, "startBackupNode_STORAGE_GROUP state = %d, name = %s\n", state, name);
      switch(state)
      {
      case 0:
         current_dbi = 0;
         store_node = NULL;
         logfile_ptr = NULL;
         if (context->job_level == 'F')
         {
            _DebugMessage(100, "Calling HrESEBackupSetup (BACKUP_TYPE_FULL)\n");
            result = HrESEBackupSetup(hccx, ibi->hInstanceId, BACKUP_TYPE_FULL);
            state = 1;
         }
         else
         {
            _DebugMessage(100, "Calling HrESEBackupSetup (BACKUP_TYPE_LOGS_ONLY)\n");
            result = HrESEBackupSetup(hccx, ibi->hInstanceId, BACKUP_TYPE_LOGS_ONLY);
            if (context->accurate)
               state = 1;
            else
               state = 2;
         }
         if (result != 0)
         {
            _JobMessage(M_FATAL, "HrESEBackupSetup failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
            state = 999;
            return bRC_Error;
         }
         break;
      case 1:
         if (context->path_bits[level + 1] == NULL)
         {
            _DebugMessage(100, "No specific database specified - backing them all up\n");
            DATABASE_BACKUP_INFO *dbi = &ibi->rgDatabase[current_dbi];
            char *tmp = new char[wcslen(dbi->wszDatabaseDisplayName) + 1];
            wcstombs(tmp, dbi->wszDatabaseDisplayName, wcslen(dbi->wszDatabaseDisplayName) + 1);
            store_node = new store_node_t(tmp, this);
            store_node->dbi = dbi;
            store_node->hccx = hccx;
            context->current_node = store_node;
         }
         else
         {
            DATABASE_BACKUP_INFO *dbi = NULL;
            char *tmp = NULL;
            for (current_dbi = 0; current_dbi < ibi->cDatabase; current_dbi++)
            {
               dbi = &ibi->rgDatabase[current_dbi];
               char *tmp = new char[wcslen(dbi->wszDatabaseDisplayName) + 1];
               wcstombs(tmp, dbi->wszDatabaseDisplayName, wcslen(dbi->wszDatabaseDisplayName) + 1);
               if (stricmp(tmp, context->path_bits[level + 1]) == 0)
                  break;
               delete tmp;
            }
            if (current_dbi == ibi->cDatabase)
            {
               _JobMessage(M_FATAL, "Invalid Database '%s'\n", context->path_bits[level + 1]);
               return bRC_Error;
            }
            store_node = new store_node_t(tmp, this);
            _DebugMessage(100, "Database name = %s\n", store_node->name);
            delete tmp;
            store_node->hccx = hccx;
            store_node->dbi = dbi;
            context->current_node = store_node;
         }
         return bRC_OK;
      case 2:
         _DebugMessage(100, "Calling HrESEBackupGetLogAndPatchFiles\n");
         result = HrESEBackupGetLogAndPatchFiles(hccx, &tmp_logfiles);
         if (result != 0)
         {
            _JobMessage(M_FATAL, "HrESEBackupGetLogAndPatchFiles failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
            return bRC_Error;
         }
         for (len = 0, tmp_logfile_ptr = tmp_logfiles; *tmp_logfile_ptr != 0; tmp_logfile_ptr += wcslen(tmp_logfile_ptr) + 1)
         {
            len += wcslen(tmp_logfile_ptr) + 1;
         }
         logfiles = new WCHAR[len + 1];
         logfile_ptr = logfiles;
         for (tmp_logfile_ptr = tmp_logfiles; *tmp_logfile_ptr != 0; tmp_logfile_ptr += wcslen(tmp_logfile_ptr) + 1)
         {
            // check file modification date
            HANDLE handle;
            FILETIME modified_time;
            //int64_t tmp_time;
            __int64 tmp_time;
            bool include_file;
            include_file = false;
            handle = INVALID_HANDLE_VALUE;
            if (context->job_since == 0)
               include_file = true;
            if (!include_file)
            {
               handle = CreateFileW(tmp_logfile_ptr, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
               if (handle == INVALID_HANDLE_VALUE)
               {
                  //_JobMessage(M_WARNING, "Could not open '%S' to check last modified date (0x%08x), including anyway\n", tmp_logfile_ptr, GetLastError());
                  include_file = true;
               }
            }
            if (!include_file)
            {
               if (GetFileTime(handle, NULL, NULL, &modified_time) == 0)
               {
                  //_JobMessage(M_WARNING, "Could not check last modified date for '%S' (0x%08x), including anyway\n", tmp_logfile_ptr, GetLastError());
                  include_file = true;
               }
            }
            if (!include_file)
            {
               tmp_time = (((int64_t)modified_time.dwHighDateTime) << 32) | modified_time.dwLowDateTime;
               tmp_time -= 116444736000000000LL;
               tmp_time /= 10000000;
               if (tmp_time > context->job_since)
               {
                  include_file = true;
               }
            }
            if (include_file)
            {
               memcpy(logfile_ptr, tmp_logfile_ptr, (wcslen(tmp_logfile_ptr) + 1) * 2);
               logfile_ptr += wcslen(logfile_ptr) + 1;
               //_DebugMessage(100, "Including file %S\n", logfile_ptr);
            }
#if 0
/* this is handled via checkFile now */
            else
            {
               if (context->accurate) {
                  tmp = new char[strlen(full_path) + wcslen(tmp_logfile_ptr) + 1];
                  strcpy(tmp, full_path);
                  wcstombs(tmp + strlen(full_path), tmp_logfile_ptr, wcslen(tmp_logfile_ptr) + 1);
                  bfuncs->setBaculaValue(context->bpContext, bVarFileSeen, (void *)tmp);
                  delete tmp;
               }
            }
#endif

            if (handle != INVALID_HANDLE_VALUE)
               CloseHandle(handle);

         }
         *logfile_ptr = 0;
         logfile_ptr = logfiles;
         state = 3;
         break;
      case 3:
         tmp = new char[wcslen(logfile_ptr) + 1];
         wcstombs(tmp, logfile_ptr, wcslen(logfile_ptr) + 1);
         file_node = new file_node_t(tmp, this);
         delete tmp;
         file_node->hccx = hccx;
         file_node->filename = logfile_ptr;
         context->current_node = file_node;
         return bRC_OK;
      case 4:
         time_t now = time(NULL);
         sp->fname = full_path;
         sp->link = full_path;
         _DebugMessage(100, "fname = %s\n", sp->fname);
         sp->statp.st_mode = 0700 | S_IFDIR;
         sp->statp.st_ctime = now;
         sp->statp.st_mtime = now;
         sp->statp.st_atime = now;
         sp->statp.st_size = 0;
         //sp->statp.st_blocks = 0;
         sp->type = FT_DIREND;
         return bRC_OK;
      }
   }
}

bRC
storage_group_node_t::endBackupFile(exchange_fd_context_t *context)
{
   HRESULT result;
   bRC retval = bRC_Error;

   _DebugMessage(100, "endBackupNode_STORAGE_GROUP state = %d\n", state);

   switch(state)
   {
   case 0:
      // should never happen
      break;
   case 1:
      // free node->storage_group_node
      if (context->path_bits[level + 1] == NULL)
      {
         current_dbi++;
         if (current_dbi == ibi->cDatabase)
            state = 2;
      }
      else
         state = 2;
      retval = bRC_More;
      break;
   case 2:
      // should never happen
      break;
   case 3:
      delete file_node;
      logfile_ptr += wcslen(logfile_ptr) + 1;
      if (*logfile_ptr == 0)
         state = 4;
      retval = bRC_More;
      break;
   case 4:
      if (context->truncate_logs)
      {
         _DebugMessage(100, "Calling HrESEBackupTruncateLogs\n");
         result = HrESEBackupTruncateLogs(hccx);
         if (result != 0)
         {
            _JobMessage(M_FATAL, "HrESEBackupTruncateLogs failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
         }
         else
         {
            _JobMessage(M_INFO, "Truncated database logs for Storage Group %s\n", name);
         }
      }
      else
      {
         _JobMessage(M_INFO, "Did NOT truncate database logs for Storage Group %s\n", name);
      }
      _DebugMessage(100, "Calling HrESEBackupInstanceEnd\n");
      result = HrESEBackupInstanceEnd(hccx, ESE_BACKUP_INSTANCE_END_SUCCESS);
      if (result != 0)
      {
         _JobMessage(M_FATAL, "HrESEBackupInstanceEnd failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
         return bRC_Error;
      }
      retval = bRC_OK;
      context->current_node = parent;
      break;
   }
   return retval;
}

bRC
storage_group_node_t::createFile(exchange_fd_context_t *context, struct restore_pkt *rp)
{
   HRESULT result;
   int len;

   _DebugMessage(100, "createFile_STORAGE_GROUP state = %d\n", state);

   if (strcmp(context->path_bits[level], name) != 0)
   {
      _DebugMessage(100, "Different storage group - switching back to parent\n", state);
      saved_log_path = new WCHAR[wcslen(restore_environment->m_wszRestoreLogPath) + 1];
      wcscpy(saved_log_path, restore_environment->m_wszRestoreLogPath);
      _DebugMessage(100, "Calling HrESERestoreSaveEnvironment\n");
      result = HrESERestoreSaveEnvironment(hccx);
      if (result != 0)
      {
         _JobMessage(M_FATAL, "HrESERestoreSaveEnvironment failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
         state = 999;
         rp->create_status = CF_CREATED;
         return bRC_OK;
      }
      _DebugMessage(100, "Calling HrESERestoreClose\n");
      result = HrESERestoreClose(hccx, RESTORE_CLOSE_NORMAL);
      if (result != 0)
      {
         _JobMessage(M_FATAL, "HrESERestoreClose failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
         state = 999;
         rp->create_status = CF_CREATED;
         return bRC_OK;
      }
      context->current_node = parent;
      return bRC_OK;
   }
   if (saved_log_path != NULL)
   {
      _DebugMessage(100, "Calling HrESERestoreReopen\n");
      result = HrESERestoreReopen(context->computer_name, service_name, saved_log_path, &hccx);
      if (result != 0)
      {
         _JobMessage(M_FATAL, "HrESERestoreReopen failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
         state = 999;
         saved_log_path = NULL;
         rp->create_status = CF_CREATED;
         return bRC_OK;
      }
      _DebugMessage(100, "Calling HrESERestoreGetEnvironment\n");
      result = HrESERestoreGetEnvironment(hccx, &restore_environment);
      if (result != 0)
      {
         _JobMessage(M_FATAL, "HrESERestoreGetEnvironment failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
         state = 999;
         saved_log_path = NULL;
         rp->create_status = CF_CREATED;
         return bRC_OK;
      }
      saved_log_path = NULL;
   }

   for (;;)
   {
      switch (state)
      {
      case 0:
         if (context->path_bits[level + 2] == NULL)
         {
            _JobMessage(M_ERROR, "Unexpected log file '%s%s' - expecting database\n", full_path, context->path_bits[level + 1]);
            state = 999;
            break;
         }
         service_name = new WCHAR[strlen(parent->name) + 1];
         storage_group_name = new WCHAR[strlen(name) + 1];
         mbstowcs(service_name, parent->name, strlen(parent->name) + 1);
         mbstowcs(storage_group_name, name, strlen(name) + 1);
         _DebugMessage(100, "Calling HrESERestoreOpen\n");
         result = HrESERestoreOpen(context->computer_name, service_name, storage_group_name, NULL, &hccx);
         if (result != 0)
         {
            _JobMessage(M_FATAL, "HrESERestoreOpen failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
            state = 999;
            break;
         }
         _DebugMessage(100, "Calling HrESERestoreGetEnvironment\n");
         result = HrESERestoreGetEnvironment(hccx, &restore_environment);
         if (result != 0)
         {
            _JobMessage(M_FATAL, "HrESERestoreGetEnvironment failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
            state = 999;
            break;
         }
         state = 1;
         break;
      case 1:
         if (context->path_bits[level + 2] == NULL)
         {
            state = 2;
            break;
         }
         store_node = new store_node_t(bstrdup(context->path_bits[level + 1]), this);
         store_node->hccx = hccx;
         context->current_node = store_node;
         return bRC_OK;
      case 2:
         if (context->path_bits[level + 2] != NULL)
         {
            _JobMessage(M_ERROR, "Unexpected file '%s'\n", full_path);
            state = 999;
            break;
         }
         if (context->path_bits[level + 1] == NULL)
         {
            state = 3;
            break;
         }
         state = 2;
         file_node = new file_node_t(bstrdup(context->path_bits[level + 1]), this);
         file_node->hccx = hccx;
         int i;
         for (i = strlen(file_node->name) - 1; i >= 0; i--)
         {
            if (file_node->name[i] == '\\')
            {
               i++;
               break;
            }
         }
         len = wcslen(restore_environment->m_wszRestoreLogPath) + strlen(file_node->name + i) + 1 + 1;
         file_node->filename = new WCHAR[len];
         wcscpy(file_node->filename, restore_environment->m_wszRestoreLogPath);
         wcscat(file_node->filename, L"\\");
         mbstowcs(&file_node->filename[wcslen(file_node->filename)], file_node->name + i, strlen(file_node->name + i) + 1);
         context->current_node = file_node;
         return bRC_OK;
      case 3:
         if (rp->type != FT_DIREND)
         {
            _JobMessage(M_ERROR, "Unexpected file '%s'\n", full_path);
            state = 999;
            break;
         }
         // must be the storage group node
         _DebugMessage(100, "Calling HrESERestoreSaveEnvironment\n");
         result = HrESERestoreSaveEnvironment(hccx);
         if (result != 0)
         {
            _JobMessage(M_FATAL, "HrESERestoreSaveEnvironment failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
            state = 999;
            break;
         }

         _DebugMessage(100, "Calling HrESERestoreComplete\n");
         result = HrESERestoreComplete(hccx, restore_environment->m_wszRestoreLogPath,
            restore_environment->m_wszRestoreLogPath, storage_group_name, ESE_RESTORE_COMPLETE_ATTACH_DBS);
         if (result != 0)
         {
            _JobMessage(M_FATAL, "HrESERestoreComplete failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
            _DebugMessage(100, "Calling HrESERestoreClose\n");
            result = HrESERestoreClose(hccx, RESTORE_CLOSE_NORMAL);
            state = 999;
            break;
         }
         else
         {
            _JobMessage(M_INFO, "Storage Group '%s' restored successfully\n", name);
         }

         _DebugMessage(100, "Calling HrESERestoreClose\n");
         result = HrESERestoreClose(hccx, RESTORE_CLOSE_NORMAL);
         if (result != 0)
         {
            _JobMessage(M_FATAL, "HrESERestoreClose failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
            state = 999;
            break;
         }

         rp->create_status = CF_CREATED;
         return bRC_OK;
      case 999:
         rp->create_status = CF_CREATED;
         return bRC_OK;
      }
   }
}

bRC
storage_group_node_t::endRestoreFile(exchange_fd_context_t *context)
{
   _DebugMessage(100, "endRestoreFile_STORAGE_GROUP state = %d\n", state);
   switch (state)
   {
   case 0:
      return bRC_Error;
   case 1:
      return bRC_OK;
   case 2:
      return bRC_OK;
   case 3:
      context->current_node = parent;
      return bRC_OK;
   case 999:
      context->current_node = parent;
      return bRC_OK;
   }

   return bRC_Error;
}
