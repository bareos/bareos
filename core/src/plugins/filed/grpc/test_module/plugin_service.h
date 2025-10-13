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

#ifndef BAREOS_PLUGINS_FILED_GRPC_TEST_MODULE_PLUGIN_SERVICE_H_
#define BAREOS_PLUGINS_FILED_GRPC_TEST_MODULE_PLUGIN_SERVICE_H_

#include <sys/stat.h>
#include <future>
#include <optional>
#include "plugin.grpc.pb.h"
#include "plugin.pb.h"

namespace bp = bareos::plugin;

class raii_fd {
 public:
  raii_fd() = default;
  explicit raii_fd(int fd_) : fd{fd_} {}

  raii_fd(const raii_fd&) = delete;
  raii_fd& operator=(const raii_fd&) = delete;
  raii_fd(raii_fd&& other) { *this = std::move(other); }
  raii_fd& operator=(raii_fd&& other)
  {
    std::swap(fd, other.fd);
    return *this;
  }

  int get() { return fd; }

  ~raii_fd()
  {
    if (fd >= 0) { close(fd); }
  }

 private:
  int fd{-1};
};

class PluginService : public bp::Plugin::Service {
 public:
  PluginService(int io_sock, std::promise<void> shutdown_signal)
      : io{io_sock}, shutdown{std::move(shutdown_signal)}
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
                  grpc::ServerWriter<bp::fileReadResponse>* writer);
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

  bool handle_arguments(std::string_view argstring);

 public:
  struct prepared_file {
    std::string name;
    bool added_children{false};
    struct stat s;
    bool ok{false};
    prepared_file() = default;
    prepared_file(std::string name_) : name{name_}
    {
      ok = lstat(name.c_str(), &s) >= 0;
    }

    bool isdir() const { return ok && S_ISDIR(s.st_mode); }
    bool islnk() const { return ok && S_ISLNK(s.st_mode); }
  };


 private:
  std::vector<prepared_file> files_to_backup{};
  std::optional<raii_fd> current_file{};

  int io;
  std::promise<void> shutdown;
};

#endif  // BAREOS_PLUGINS_FILED_GRPC_TEST_MODULE_PLUGIN_SERVICE_H_
