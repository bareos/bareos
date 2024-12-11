/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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

#include "bareos_api.h"

struct globals {
  const filedaemon::CoreFunctions* core{nullptr};
};

static globals fd{};

void SetupBareosApi(const filedaemon::CoreFunctions* core) { fd.core = core; }

namespace internal {
void DebugMessage(/* optional */ PluginContext* ctx,
                  const char* file,
                  int line,
                  int level,
                  const char* string)
{
  if (fd.core) {
    fd.core->DebugMessage(ctx, file, line, level, "%s\n", string);
  }
}

void JobMessage(PluginContext* ctx,
                const char* file,
                int line,
                int type,
                const char* string)
{
  if (fd.core) {
    fd.core->JobMessage(ctx, file, line, type, 0, "%s\n", string);
  }
}
};  // namespace internal

void RegisterBareosEvent(PluginContext* ctx, filedaemon::bEventType event)
{
  if (fd.core) { fd.core->registerBareosEvents(ctx, 1, event); }
}

void UnregisterBareosEvent(PluginContext* ctx, filedaemon::bEventType event)
{
  if (fd.core) { fd.core->unregisterBareosEvents(ctx, 1, event); }
}

bool SetBareosValue(PluginContext* ctx, filedaemon::bVariable var, void* value)
{
  if (fd.core) { return fd.core->setBareosValue(ctx, var, value) != bRC_Error; }
  return false;
}

bool GetBareosValue(PluginContext* ctx, filedaemon::bVariable var, void* value)
{
  if (fd.core) { return fd.core->getBareosValue(ctx, var, value) != bRC_Error; }
  return false;
}

bool checkChanges(PluginContext* ctx,
                  const std::string& file,
                  int type,
                  const struct stat& statp,
                  time_t since)
{
  if (fd.core) {
    filedaemon::save_pkt pkt{};

    // depending on whether the paket is a file or a directory,
    // checkChanges uses either the link or fname field to get the file name.
    // We just set both to the correct name here and hope for the best!

    // im not happy about this
    pkt.fname = const_cast<char*>(file.c_str());
    pkt.statp = statp;
    pkt.link = const_cast<char*>(file.c_str());
    pkt.type = type;
    pkt.save_time = since;

    auto result = fd.core->checkChanges(ctx, &pkt);

    // TODO: also return the delta sequence number

    return result != bRC_Seen;
  }

  return false;
}

bool AcceptFile(PluginContext* ctx,
                const std::string& file,
                const struct stat& statp)
{
  if (fd.core) {
    filedaemon::save_pkt pkt{};

    // depending on whether the paket is a file or a directory,
    // checkChanges uses either the link or fname field to get the file name.
    // We just set both to the correct name here and hope for the best!

    // im not happy about this
    pkt.fname = const_cast<char*>(file.c_str());
    pkt.statp = statp;
    pkt.link = const_cast<char*>(file.c_str());

    auto result = fd.core->AcceptFile(ctx, &pkt);

    // TODO: also return the delta sequence number

    return result != bRC_Seen;
  }

  return false;
}

bRC SetSeenBitmap(PluginContext* ctx, bool all, const char* fname)
{
  if (fd.core) {
    return fd.core->SetSeenBitmap(ctx, all, const_cast<char*>(fname));
  }
  return bRC_Error;
}
bRC ClearSeenBitmap(PluginContext* ctx, bool all, const char* fname)
{
  if (fd.core) { fd.core->ClearSeenBitmap(ctx, all, const_cast<char*>(fname)); }
  return bRC_Error;
}
