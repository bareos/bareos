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

dbi_node_t::dbi_node_t(char *name, node_t *parent_node) : node_t(name, NODE_TYPE_DATABASE_INFO, parent_node)
{
   restore_display_name = NULL;
   restore_input_streams = NULL;
   buffer = NULL;
}

dbi_node_t::~dbi_node_t()
{
   if (buffer != NULL)
      delete buffer;
   if (restore_input_streams != NULL)
      delete restore_input_streams;
   if (restore_display_name != NULL)
      delete restore_display_name;
}

bRC
dbi_node_t::startBackupFile(exchange_fd_context_t *context, struct save_pkt *sp)
{
   time_t now = time(NULL);

   _DebugMessage(100, "startBackupNode_DBI state = %d\n", state);

   if (context->job_level == 'F') {
      sp->fname = full_path;
      sp->link = full_path;
      sp->statp.st_mode = 0700 | S_IFREG;
      sp->statp.st_ctime = now;
      sp->statp.st_mtime = now;
      sp->statp.st_atime = now;
      sp->statp.st_size = (uint64_t)-1;
      sp->type = FT_REG;
      return bRC_OK;
   }
   else
   {
      bfuncs->setBaculaValue(context->bpContext, bVarFileSeen, (void *)full_path);
      return bRC_Seen;
   }
}

bRC
dbi_node_t::endBackupFile(exchange_fd_context_t *context)
{
   _DebugMessage(100, "endBackupNode_DBI state = %d\n", state);

   context->current_node = parent;

   return bRC_OK;
}

bRC
dbi_node_t::createFile(exchange_fd_context_t *context, struct restore_pkt *rp)
{
   _DebugMessage(100, "createFile_DBI state = %d\n", state);

   rp->create_status = CF_EXTRACT;

   return bRC_OK;
}

bRC
dbi_node_t::endRestoreFile(exchange_fd_context_t *context)
{
   _DebugMessage(100, "endRestoreFile_DBI state = %d\n", state);

   context->current_node = parent;

   return bRC_OK;
}

bRC
dbi_node_t::pluginIoOpen(exchange_fd_context_t *context, struct io_pkt *io)
{
   uint32_t len;
   WCHAR *ptr;
   WCHAR *stream;
   //char tmp[512];

   buffer_pos = 0;
   buffer_size = 65536;
   buffer = new char[buffer_size];

   if (context->job_type == JOB_TYPE_BACKUP)
   {
      ptr = (WCHAR *)buffer;
      len = snwprintf(ptr, (buffer_size - buffer_pos) / 2, L"DatabaseBackupInfo\n");
      if (len < 0)
         goto fail;
      buffer_pos += len * 2;
      ptr += len;

      len = snwprintf(ptr, (buffer_size - buffer_pos) / 2, L"%d\n", EXCHANGE_PLUGIN_VERSION);
      if (len < 0)
         goto fail;
      buffer_pos += len * 2;
      ptr += len;

      len = snwprintf(ptr, (buffer_size - buffer_pos) / 2, L"%s\n", dbi->wszDatabaseDisplayName);
      if (len < 0)
         goto fail;
      buffer_pos += len * 2;
      ptr += len;

      len = snwprintf(ptr, (buffer_size - buffer_pos) / 2, L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
         dbi->rguidDatabase.Data1, dbi->rguidDatabase.Data2, dbi->rguidDatabase.Data3,
         dbi->rguidDatabase.Data4[0], dbi->rguidDatabase.Data4[1],
         dbi->rguidDatabase.Data4[2], dbi->rguidDatabase.Data4[3],
         dbi->rguidDatabase.Data4[4], dbi->rguidDatabase.Data4[5],
         dbi->rguidDatabase.Data4[6], dbi->rguidDatabase.Data4[7]);
      if (len < 0)
         goto fail;
      buffer_pos += len * 2;
      ptr += len;
      
      stream = dbi->wszDatabaseStreams;
      while (*stream)
      {
         len = snwprintf(ptr, (buffer_size - buffer_pos) / 2, L"%s\n", stream);
         if (len < 0)
            goto fail;
         buffer_pos += len * 2;
         ptr += len;
         stream += wcslen(stream) + 1;
      }

      buffer_size = buffer_pos;
      buffer_pos = 0;
   }

   io->status = 0;
   io->io_errno = 0;
   return bRC_OK;

fail:
   io->status = 0;
   io->io_errno = 1;
   return bRC_Error;
}

bRC
dbi_node_t::pluginIoRead(exchange_fd_context_t *context, struct io_pkt *io)
{
   io->status = 0;
   io->io_errno = 0;

   io->status = MIN(io->count, (int)(buffer_size - buffer_pos));
   if (io->status == 0)
      return bRC_OK;
   memcpy(io->buf, buffer + buffer_pos, io->status);
   buffer_pos += io->status;

   return bRC_OK;
}

bRC 
dbi_node_t::pluginIoWrite(exchange_fd_context_t *context, struct io_pkt *io)
{
   memcpy(&buffer[buffer_pos], io->buf, io->count);
   buffer_pos += io->count;
   io->status = io->count;
   io->io_errno = 0;
   return bRC_OK;
}

bRC
dbi_node_t::pluginIoClose(exchange_fd_context_t *context, struct io_pkt *io)
{
   WCHAR tmp[128];
   WCHAR *ptr;
   WCHAR eol;
   int wchars_read;
   int version;
   int stream_buf_count;
   WCHAR *streams_start;

   if (context->job_type == JOB_TYPE_RESTORE)
   {
      // need to think about making this buffer overflow proof...
      _DebugMessage(100, "analyzing DatabaseBackupInfo\n");
      ptr = (WCHAR *)buffer;

      if (swscanf(ptr, L"%127[^\n]%c%n", tmp, &eol, &wchars_read) != 2)
         goto restore_fail;
      ptr += wchars_read;
      _DebugMessage(150, "Header = %S\n", tmp);
      // verify that header == "DatabaseBackupInfo"

      if (swscanf(ptr, L"%127[^\n]%c%n", tmp, &eol, &wchars_read) != 2)
         goto restore_fail;
      if (swscanf(tmp, L"%d%c", &version, &eol) != 1)
      {
         version = 0;
         _DebugMessage(150, "Version = 0 (inferred)\n");
      }
      else
      {
         ptr += wchars_read;
         _DebugMessage(150, "Version = %d\n", version);
         if (swscanf(ptr, L"%127[^\n]%c%n", tmp, &eol, &wchars_read) != 2)
            goto restore_fail;
      }
      restore_display_name = new WCHAR[wchars_read];
      swscanf(ptr, L"%127[^\n]", restore_display_name);
      _DebugMessage(150, "Database Display Name = %S\n", restore_display_name);
      ptr += wchars_read;

      if (swscanf(ptr, L"%127[^\n]%c%n", tmp, &eol, &wchars_read) != 2)
         goto restore_fail;
      
      if (swscanf(ptr, L"%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x",
         &restore_guid.Data1, &restore_guid.Data2, &restore_guid.Data3,
         &restore_guid.Data4[0], &restore_guid.Data4[1],
         &restore_guid.Data4[2], &restore_guid.Data4[3],
         &restore_guid.Data4[4], &restore_guid.Data4[5],
         &restore_guid.Data4[6], &restore_guid.Data4[7]) != 11)
      {
         goto restore_fail;
      }
         _DebugMessage(150, "GUID = %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
         restore_guid.Data1, restore_guid.Data2, restore_guid.Data3,
         restore_guid.Data4[0], restore_guid.Data4[1],
         restore_guid.Data4[2], restore_guid.Data4[3],
         restore_guid.Data4[4], restore_guid.Data4[5],
         restore_guid.Data4[6], restore_guid.Data4[7]);

      ptr += wchars_read;

      stream_buf_count = 1;
      streams_start = ptr;
      while (ptr < (WCHAR *)(buffer + buffer_pos) && swscanf(ptr, L"%127[^\n]%c%n", tmp, &eol, &wchars_read) == 2)
      {
         _DebugMessage(150, "File = %S\n", tmp);
         ptr += wchars_read;
         stream_buf_count += wchars_read;
      }
      restore_input_streams = new WCHAR[stream_buf_count];
      ptr = streams_start;
      stream_buf_count = 0;
      while (ptr < (WCHAR *)(buffer + buffer_pos) && swscanf(ptr, L"%127[^\n]%c%n", tmp, &eol, &wchars_read) == 2)
      {
         snwprintf(&restore_input_streams[stream_buf_count], 65535, L"%s", tmp);
         ptr += wchars_read;
         stream_buf_count += wchars_read;
      }
      restore_input_streams[stream_buf_count] = 0;

      _DebugMessage(100, "done analyzing DatabasePluginInfo\n");
   }
   delete buffer;
   buffer = NULL;
   return bRC_OK;
restore_fail:
   _JobMessage(M_FATAL, "Format of %s is incorrect", full_path);
   delete buffer;
   buffer = NULL;
   return bRC_Error;
}
