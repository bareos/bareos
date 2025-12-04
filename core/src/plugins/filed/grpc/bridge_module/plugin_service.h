/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2025 Bareos GmbH & Co. KG

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

#ifndef BAREOS_PLUGINS_FILED_GRPC_BRIDGE_MODULE_PLUGIN_SERVICE_H_
#define BAREOS_PLUGINS_FILED_GRPC_BRIDGE_MODULE_PLUGIN_SERVICE_H_

#include <sys/stat.h>
#include <future>
#include <optional>
#include <variant>
#include "plugin.pb.h"

#include "../prototools.h"
#include "filed/fd_plugins.h"

namespace bp = bareos::plugin;
namespace bpe = bareos::plugin::events;

struct PluginFunctions {
  bRC (*newPlugin)(PluginContext* ctx);
  bRC (*freePlugin)(PluginContext* ctx);
  bRC (*getPluginValue)(PluginContext* ctx,
                        filedaemon::pVariable var,
                        void* value);
  bRC (*setPluginValue)(PluginContext* ctx,
                        filedaemon::pVariable var,
                        void* value);
  bRC (*handlePluginEvent)(PluginContext* ctx,
                           filedaemon::bEvent* event,
                           void* value,
                           std::size_t size);
  bRC (*startBackupFile)(PluginContext* ctx, filedaemon::save_pkt* sp);
  bRC (*endBackupFile)(PluginContext* ctx);
  bRC (*startRestoreFile)(PluginContext* ctx, const char* cmd);
  bRC (*endRestoreFile)(PluginContext* ctx);
  bRC (*pluginIO)(PluginContext* ctx, filedaemon::io_pkt* io);
  bRC (*createFile)(PluginContext* ctx, filedaemon::restore_pkt* rp);
  bRC (*setFileAttributes)(PluginContext* ctx, filedaemon::restore_pkt* rp);
  bRC (*checkFile)(PluginContext* ctx, char* fname);
  bRC (*getAcl)(PluginContext* ctx, filedaemon::acl_pkt* ap);
  bRC (*setAcl)(PluginContext* ctx, filedaemon::acl_pkt* ap);
  bRC (*getXattr)(PluginContext* ctx, filedaemon::xattr_pkt* xp);
  bRC (*setXattr)(PluginContext* ctx, filedaemon::xattr_pkt* xp);
};

// class PluginService {
//  public:
//   PluginService(PluginContext* context,
//                 PluginFunctions functions,
//                 bool* shutdown_signal)
//       : funcs{functions}, ctx{context}, shutdown{std::move(shutdown_signal)}
//   {
//   }

//   ~PluginService()
//   {
//     if (io >= 0) { close(io); }
//   }


//   bool HandleRequest(const bp::PluginRequest& req,
//                      bp::PluginResponse& resp,
//                      prototools::ProtoOutputStream& writer);

//  private:
//   using file_descriptor = int;
//   enum class StatusCode
//   {
//     INTERNAL,
//     INVALID_ARGUMENT,
//     UNIMPLEMENTED,
//     FAILED_PRECONDITION,
//   };
//   struct Status {
//     Status(StatusCode, std::string) {}

//     static Status OK;
//     static Status CANCELLED;

//     bool ok() const { return true; }
//   };

//  private:
//   // const char* non_blocking_write(int fd, int32_t byte_count, char*
//   buffer);
//   // Status Setup(const bp::SetupRequest*, bp::SetupResponse*);
//   // Status handlePluginEvent(const bp::HandlePluginEventRequest* request,
//   //                          bp::HandlePluginEventResponse* response);
//   // Status startBackupFile(const bp::StartBackupFileRequest* request,
//   //                        prototools::ProtoOutputStream& writer);
//   // Status startRestoreFile(const bp::startRestoreFileRequest* request,
//   //                         bp::startRestoreFileResponse* response);
//   // Status endRestoreFile(const bp::endRestoreFileRequest* request,
//   //                       bp::endRestoreFileResponse* response);
//   // std::optional<file_descriptor> FileOpen2(const bp::fileOpenRequest*
//   // request); Status FileOpen(const bp::fileOpenRequest* request,
//   //                 bp::fileOpenResponse* response);
//   // Status FileSeek(const bp::fileSeekRequest* request,
//   //                 bp::fileSeekResponse* response);
//   // Status FileWrite(const bp::fileWriteRequest* request,
//   //                  bp::fileWriteResponse* response);
//   // Status FileClose(const bp::fileCloseRequest* request,
//   //                  bp::fileCloseResponse* response);
//   // Status createFile(const bp::createFileRequest* request,
//   //                   bp::createFileResponse* response);
//   // Status setFileAttributes(const bp::setFileAttributesRequest* request,
//   //                          bp::setFileAttributesResponse* response);
//   // Status checkFile(const bp::checkFileRequest* request,
//   //                  bp::checkFileResponse* response);
//   // Status getAcl(const bp::getAclRequest* request, bp::getAclResponse*
//   // response); Status setAcl(const bp::setAclRequest* request,
//   // bp::setAclResponse* response); Status getXattr(const
//   bp::getXattrRequest*
//   // request,
//   //                 bp::getXattrResponse* response);
//   // Status setXattr(const bp::setXattrRequest* request,
//   //                 bp::setXattrResponse* response);

//   char* buffer(size_t size)
//   {
//     if (vec.size() < size) { vec.resize(size); }

//     return vec.data();
//   }

//  private:
//   std::optional<file_descriptor> last_io_in_core_fd;

//   PluginFunctions funcs{};
//   PluginContext* ctx{nullptr};

//   bool* shutdown;
//   bool last_file_done{false};

//   std::vector<char> vec;
// };

struct PluginThread;

void allow_events(PluginThread*, std::uint64_t event_mask);
void disallow_events(PluginThread*, std::uint64_t event_mask);

struct Inferior {
 public:
  void allow_events(std::uint64_t event_mask) { allowed_events |= event_mask; }
  void disallow_events(std::uint64_t event_mask)
  {
    allowed_events &= ~event_mask;
  }
  bool is_event_allowed(const bpe::Event&) { return false; }

  bool handle_core_event(const bp::PluginRequest* req,
                         bp::PluginResponse* resp);


  Inferior(const filedaemon::CoreFunctions* funs,
           prototools::ProtoBidiStream* data_transfer,
           std::string_view plugin_path);
  ~Inferior();

 private:
  std::uint64_t allowed_events = 0;

  std::vector<bpe::Event> event_cache;

  std::unique_ptr<PluginThread> plugin;
};

#endif  // BAREOS_PLUGINS_FILED_GRPC_BRIDGE_MODULE_PLUGIN_SERVICE_H_
