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
#include "plugin.grpc.pb.h"
#include "plugin.pb.h"

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

class PluginService : public bp::Plugin::Service {
 public:
  PluginService(PluginContext* context,
                int io_sock,
                PluginFunctions functions,
                std::promise<void>&& shutdown_signal)
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

 private:
  using ServerContext = ::grpc::ServerContext;
  using Status = ::grpc::Status;

  Status Setup(ServerContext*,
               const bp::SetupRequest*,
               bp::SetupResponse*) override;

  Status StartSession(
      ServerContext* context,
      ::grpc::ServerReaderWriter<bp::PluginResponse, bp::PluginRequest>* stream)
      override;

  Status handlePluginEvent(ServerContext*,
                           const bp::handlePluginEventRequest* request,
                           bp::handlePluginEventResponse* response);
  Status startBackupFile(ServerContext*,
                         const bp::startBackupFileRequest* request,
                         bp::startBackupFileResponse* response);
  Status endBackupFile(ServerContext*,
                       const bp::endBackupFileRequest* request,
                       bp::endBackupFileResponse* response);
  Status startRestoreFile(ServerContext*,
                          const bp::startRestoreFileRequest* request,
                          bp::startRestoreFileResponse* response);
  Status endRestoreFile(ServerContext*,
                        const bp::endRestoreFileRequest* request,
                        bp::endRestoreFileResponse* response);
  Status FileOpen(ServerContext*,
                  const bp::fileOpenRequest* request,
                  bp::fileOpenResponse* response);
  Status FileSeek(ServerContext*,
                  const bp::fileSeekRequest* request,
                  bp::fileSeekResponse* response);
  Status FileRead(ServerContext*,
                  const bp::fileReadRequest* request,
                  grpc::ServerWriter<bp::fileReadResponse>* writer) override;
  Status FileWrite(ServerContext*,
                   const bp::fileWriteRequest* request,
                   bp::fileWriteResponse* response);
  Status FileClose(ServerContext*,
                   const bp::fileCloseRequest* request,
                   bp::fileCloseResponse* response);
  Status createFile(ServerContext*,
                    const bp::createFileRequest* request,
                    bp::createFileResponse* response);
  Status setFileAttributes(ServerContext*,
                           const bp::setFileAttributesRequest* request,
                           bp::setFileAttributesResponse* response);
  Status checkFile(ServerContext*,
                   const bp::checkFileRequest* request,
                   bp::checkFileResponse* response);
  Status getAcl(ServerContext*,
                const bp::getAclRequest* request,
                bp::getAclResponse* response);
  Status setAcl(ServerContext*,
                const bp::setAclRequest* request,
                bp::setAclResponse* response);
  Status getXattr(ServerContext*,
                  const bp::getXattrRequest* request,
                  bp::getXattrResponse* response);
  Status setXattr(ServerContext*,
                  const bp::setXattrRequest* request,
                  bp::setXattrResponse* response);

  PluginFunctions funcs{};
  PluginContext* ctx{nullptr};
  int io{};

  std::promise<void> shutdown;

  std::vector<char> vec;

  char* buffer(size_t size)
  {
    if (vec.size() < size) { vec.resize(size); }

    return vec.data();
  }
};

#endif  // BAREOS_PLUGINS_FILED_GRPC_BRIDGE_MODULE_PLUGIN_SERVICE_H_
