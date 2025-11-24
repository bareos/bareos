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
#include "plugin.pb.h"

#include "../prototools.h"
#include "filed/fd_plugins.h"

namespace bp = bareos::plugin;

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

class PluginService {
 public:
  PluginService(PluginContext* context,
                int io_sock,
                PluginFunctions functions,
                bool* shutdown_signal)
      : funcs{functions}
      , ctx{context}
      , io{io_sock}
      , shutdown{std::move(shutdown_signal)}
  {
  }

  ~PluginService()
  {
    if (io >= 0) { close(io); }
  }

  bool HandleRequest(const bp::PluginRequest& req,
                     prototools::ProtoOutputStream& writer);

 private:
  const char* non_blocking_write(int fd, int32_t byte_count, char* buffer);

  enum class StatusCode
  {
    INTERNAL,
    INVALID_ARGUMENT,
    UNIMPLEMENTED,
    FAILED_PRECONDITION,
  };
  struct Status {
    Status(StatusCode, std::string) {}

    static Status OK;
    static Status CANCELLED;

    bool ok() const { return true; }
  };

  Status Setup(const bp::SetupRequest*, bp::SetupResponse*);

  Status handlePluginEvent(const bp::handlePluginEventRequest* request,
                           bp::handlePluginEventResponse* response);
  Status startBackupFile(const bp::StartBackupFileRequest* request,
                         prototools::ProtoOutputStream& writer);
  // Status endBackupFile(const bp::endBackupFileRequest* request,
  //                      bp::endBackupFileResponse* response);
  Status startRestoreFile(const bp::startRestoreFileRequest* request,
                          bp::startRestoreFileResponse* response);
  Status endRestoreFile(const bp::endRestoreFileRequest* request,
                        bp::endRestoreFileResponse* response);
  Status FileOpen(const bp::fileOpenRequest* request,
                  bp::fileOpenResponse* response);
  Status FileSeek(const bp::fileSeekRequest* request,
                  bp::fileSeekResponse* response);
  // Status FileRead(const bp::fileReadRequest* request,
  //                 bp::fileReadResponse* response);
  Status FileWrite(const bp::fileWriteRequest* request,
                   bp::fileWriteResponse* response);
  Status FileClose(const bp::fileCloseRequest* request,
                   bp::fileCloseResponse* response);
  Status createFile(const bp::createFileRequest* request,
                    bp::createFileResponse* response);
  Status setFileAttributes(const bp::setFileAttributesRequest* request,
                           bp::setFileAttributesResponse* response);
  Status checkFile(const bp::checkFileRequest* request,
                   bp::checkFileResponse* response);
  Status getAcl(const bp::getAclRequest* request, bp::getAclResponse* response);
  Status setAcl(const bp::setAclRequest* request, bp::setAclResponse* response);
  Status getXattr(const bp::getXattrRequest* request,
                  bp::getXattrResponse* response);
  Status setXattr(const bp::setXattrRequest* request,
                  bp::setXattrResponse* response);

  PluginFunctions funcs{};
  PluginContext* ctx{nullptr};
  int io{};

  bool* shutdown;
  bool last_file_done{false};

  std::vector<char> vec;

  char* buffer(size_t size)
  {
    if (vec.size() < size) { vec.resize(size); }

    return vec.data();
  }
};

#endif  // BAREOS_PLUGINS_FILED_GRPC_BRIDGE_MODULE_PLUGIN_SERVICE_H_
