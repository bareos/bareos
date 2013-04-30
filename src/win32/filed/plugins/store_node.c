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

store_node_t::store_node_t(char *name, node_t *parent_node) : node_t(name, NODE_TYPE_STORE, parent_node)
{
   dbi = NULL;
   hccx = NULL;
   dbi_node = NULL;
   file_node = NULL;
}

store_node_t::~store_node_t()
{
   if (dbi_node != NULL)
      delete dbi_node;

   if (file_node != NULL)
      delete file_node;
}

bRC
store_node_t::startBackupFile(exchange_fd_context_t *context, struct save_pkt *sp)
{
   char *tmp;

   _DebugMessage(100, "startBackupNode_STORE state = %d\n", state);

   switch(state)
   {
   case 0:
      stream_ptr = dbi->wszDatabaseStreams;
      state = 1;
      // fall through
   case 1:
      dbi_node = new dbi_node_t("DatabaseBackupInfo", this);
      dbi_node->dbi = dbi;
      context->current_node = dbi_node;
      break;
   case 2:
      tmp = new char[wcslen(stream_ptr) + 1];
      wcstombs(tmp, stream_ptr, wcslen(stream_ptr) + 1);
      file_node = new file_node_t(tmp, this);
      file_node->hccx = hccx;
      file_node->filename = stream_ptr;
      context->current_node = file_node;
      break;
   case 3:
      if (context->job_level == 'F')
      {
         time_t now = time(NULL);
         sp->fname = full_path;
         sp->link = full_path;
         sp->statp.st_mode = 0700 | S_IFDIR;
         sp->statp.st_ctime = now;
         sp->statp.st_mtime = now;
         sp->statp.st_atime = now;
         sp->statp.st_size = 0;
         sp->type = FT_DIREND;
      }
      else
      {
         bfuncs->setBaculaValue(context->bpContext, bVarFileSeen, (void *)full_path);
         return bRC_Seen;
      }
      break;
   }

   return bRC_OK;
}

bRC
store_node_t::endBackupFile(exchange_fd_context_t *context)
{
   _DebugMessage(100, "endBackupNode_STORE state = %d\n", state);
   bRC retval = bRC_OK;

   switch(state)
   {
   case 0:
      // should never happen
      break;
   case 1:
      state = 2;
      retval = bRC_More;
      break;
   case 2:
      delete file_node;
      stream_ptr += wcslen(stream_ptr) + 1;
      if (*stream_ptr == 0)
         state = 3;
      retval = bRC_More;
      break;
   case 3:
      //delete dbi_node;
      context->current_node = parent;
      break;
   }
   return retval;
}

bRC
store_node_t::createFile(exchange_fd_context_t *context, struct restore_pkt *rp)
{
   _DebugMessage(100, "createFile_STORE state = %d\n", state);

   if (strcmp(context->path_bits[level - 1], parent->name) != 0)
   {
      _DebugMessage(100, "Different storage group - switching back to parent\n", state);
      context->current_node = parent;
      return bRC_OK;
   }
   for (;;)
   {
      switch (state)
      {
      case 0:
         if (strcmp("DatabaseBackupInfo", context->path_bits[level + 1]) != 0)
         {
            _JobMessage(M_FATAL, "DatabaseBackupInfo file must exist and must be first in directory\n");
            state = 999;
            break;
         }
         dbi_node = new dbi_node_t(bstrdup(context->path_bits[level + 1]), this);
         context->current_node = dbi_node;
         return bRC_OK;
      case 1:
         if (strcmp(context->path_bits[level - 1], parent->name) != 0)
         {
            _JobMessage(M_ERROR, "Unexpected Storage Group Change\n");
            state = 999;
            break;
         }

         if (*stream_ptr != 0)
         {
            // verify that stream_ptr == context->path_bits[level + 1];
            _DebugMessage(150, "stream_ptr = %S\n", stream_ptr);
            _DebugMessage(150, "out_stream_ptr = %S\n", out_stream_ptr);
            file_node = new file_node_t(bstrdup(context->path_bits[level + 1]), this);
            file_node->hccx = hccx;
            file_node->filename = out_stream_ptr;
            context->current_node = file_node;
            return bRC_OK;
         }
         else
         {
            _JobMessage(M_ERROR, "Extra file found '%s'\n", full_path);
            state = 999;
            break;
         }
      case 2:
         if (rp->type != FT_DIREND)
         {
            _JobMessage(M_ERROR, "Unexpected file '%s'\n", full_path);
            state = 999;
            break;
         }
         rp->create_status = CF_CREATED;
         return bRC_OK;
      case 999:
         if (strcmp(context->path_bits[level], name) != 0)
         {
            _DebugMessage(100, "End of Store when in error state - switching back to parent\n", state);
            context->current_node = parent;
            return bRC_OK;
         }
         rp->create_status = CF_CREATED;
         return bRC_OK;
      }
   }
}

bRC
store_node_t::endRestoreFile(exchange_fd_context_t *context)
{
   HRESULT result;

   _DebugMessage(100, "endRestoreFile_STORE state = %d\n", state);
   for (;;)
   {
      switch (state)
      {
      case 0:
         state = 1;
         _DebugMessage(100, "Calling HrESERestoreAddDatabase\n");
         result = HrESERestoreAddDatabase(hccx, dbi_node->restore_display_name, dbi_node->restore_guid, dbi_node->restore_input_streams, &dbi_node->restore_output_streams);
         if (result != 0)
         {
            _JobMessage(M_FATAL, "HrESERestoreAddDatabase failed with error 0x%08x - %s\n", result, ESEErrorMessage(result));
            state = 999;
            break;
         }
         stream_ptr = dbi_node->restore_input_streams;
         out_stream_ptr = dbi_node->restore_output_streams;
         return bRC_OK;
      case 1:
         if (*stream_ptr != 0)
         {
            delete file_node;
            file_node = NULL;
            stream_ptr += wcslen(stream_ptr) + 1;
            out_stream_ptr += wcslen(out_stream_ptr) + 1;
            if (*stream_ptr == 0)
               state = 2;
            return bRC_OK;
         }
         else
         {
            state = 999;
            break;
         }
      case 2:
         context->current_node = parent;
         return bRC_OK;
      case 999:
         return bRC_OK;
      }
   }
}
