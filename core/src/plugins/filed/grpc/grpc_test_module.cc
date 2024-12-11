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

#include "events.pb.h"
#include "plugin.grpc.pb.h"
#include "plugin.pb.h"
#include "bareos.grpc.pb.h"
#include "bareos.pb.h"
#include "filed/fd_plugins.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/create_channel_posix.h>
#include <grpcpp/server_posix.h>

#include <fmt/format.h>
#include <condition_variable>
#include <future>
#include <clocale>

#include "plugin_service.h"
#include "test_module.h"

struct grpc_connection {
  std::unique_ptr<bc::Core::Stub> stub;
  std::unique_ptr<grpc::Server> server;
  std::vector<std::unique_ptr<grpc::Service>> services;

  grpc_connection() = delete;
};

struct connection_builder {
  std::optional<std::unique_ptr<bc::Core::Stub>> opt_stub;
  std::unique_ptr<grpc::Server> opt_server;
  std::vector<std::unique_ptr<grpc::Service>> services;

  template <typename... Args> connection_builder(Args&&... args)
  {
    (services.emplace_back(std::forward<Args>(args)), ...);
  }

  connection_builder& connect_client(int sockfd)
  {
    opt_stub = bc::Core::NewStub(grpc::CreateInsecureChannelFromFd("", sockfd));

    return *this;
  }

  connection_builder& connect_server(int sockfd)
  {
    try {
      grpc::ServerBuilder builder;

      for (auto& service : services) { builder.RegisterService(service.get()); }

      opt_server = builder.BuildAndStart();

      if (!opt_server) { return *this; }

      grpc::AddInsecureChannelFromFd(opt_server.get(), sockfd);
    } catch (const std::exception& e) {
      // DebugLog(50, FMT_STRING("could not attach socket {} to server:
      // Err={}"),
      //          sockfd, e.what());
      opt_server.reset();
    } catch (...) {
      opt_server.reset();
    }
    return *this;
  }


  std::optional<grpc_connection> build()
  {
    if (!opt_stub) { return std::nullopt; }
    if (!opt_server) { return std::nullopt; }

    grpc_connection con{std::move(opt_stub.value()), std::move(opt_server),
                        std::move(services)};

    return con;
  }
};

#include <fcntl.h>

std::optional<grpc_connection> con;

static inline bc::Core::Stub* stub() { return con->stub.get(); }

bool Register(std::basic_string_view<bc::EventType> types)
{
  bc::RegisterRequest req;

  for (auto type : types) { req.add_event_types(type); }


  bc::RegisterResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Events_Register(&ctx, req, &resp);

  if (!status.ok()) { return false; }

  return true;
}
bool Unregister(std::basic_string_view<bc::EventType> types)
{
  bc::UnregisterRequest req;

  for (auto type : types) { req.add_event_types(type); }


  bc::UnregisterResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Events_Unregister(&ctx, req, &resp);

  if (!status.ok()) { return false; }

  return true;
}

// TODO: implement these

void AddExclude() { return; }
void AddInclude() { return; }
void AddOptions() { return; }
void AddRegex() { return; }
void AddWild() { return; }
void NewOptions() { return; }
void NewInclude() { return; }
void NewPreInclude() { return; }

//

std::optional<size_t> getInstanceCount()
{
  bc::getInstanceCountRequest req;
  bc::getInstanceCountResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_getInstanceCount(&ctx, req, &resp);

  if (!status.ok()) { return std::nullopt; }

  return resp.instance_count();
}

std::optional<bool> checkChanges(bco::FileType ft,
                                 std::string_view name,
                                 time_t timestamp,
                                 const struct stat& statp)
{
  bc::checkChangesRequest req;
  req.set_type(ft);
  req.set_file(name.data(), name.size());
  // if (link_name) { req.set_link_target(link_name->data(), link_name->size());
  // }
  req.mutable_since_time()->set_seconds(timestamp);
  req.set_stats(&statp, sizeof(statp));

  bc::checkChangesResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_checkChanges(&ctx, req, &resp);

  if (!status.ok()) { return std::nullopt; }

  return resp.old();
}
std::optional<bool> AcceptFile(std::string_view name, const struct stat& statp)
{
  bc::AcceptFileRequest req;
  req.set_file(name.data(), name.size());
  req.set_stats(&statp, sizeof(statp));

  bc::AcceptFileResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_AcceptFile(&ctx, req, &resp);

  if (!status.ok()) { return std::nullopt; }

  return resp.skip();
}

bool SetSeen(std::optional<std::string_view> name)
{
  bc::SetSeenRequest req;
  if (name) { req.set_file(name->data(), name->size()); }

  bc::SetSeenResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_SetSeen(&ctx, req, &resp);

  if (!status.ok()) { return false; }

  return true;
}
bool ClearSeen(std::optional<std::string_view> name)
{
  bc::ClearSeenRequest req;
  if (name) { req.set_file(name->data(), name->size()); }

  bc::ClearSeenResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_ClearSeen(&ctx, req, &resp);

  if (!status.ok()) { return false; }

  return true;
}
void JobMessage(bc::JMsgType type,
                int line,
                const char* file,
                const char* fun,
                std::string_view msg)
{
  bc::JobMessageRequest req;
  req.set_type(type);
  req.set_msg(msg.data(), msg.size());
  req.set_line(line);
  req.set_file(file);
  req.set_function(fun);

  bc::JobMessageResponse resp;
  grpc::ClientContext ctx;
  (void)stub()->Bareos_JobMessage(&ctx, req, &resp);
}

void DebugMessage(int level,
                  std::string_view msg,
                  int line,
                  const char* file,
                  const char* fun)
{
  bc::DebugMessageRequest req;
  req.set_level(level);
  req.set_msg(msg.data(), msg.size());
  req.set_line(line);
  req.set_file(file);
  req.set_function(fun);

  bc::DebugMessageResponse resp;
  grpc::ClientContext ctx;
  (void)stub()->Bareos_DebugMessage(&ctx, req, &resp);
}
bool Bareos_SetString(bc::BareosStringVariable var, std::string_view val)
{
  bc::SetStringRequest req;
  req.set_var(var);
  req.set_value(val.data(), val.size());

  bc::SetStringResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_SetString(&ctx, req, &resp);

  if (!status.ok()) { return false; }
  return true;
}
std::optional<std::string> Bareos_GetString(bc::BareosStringVariable var)
{
  bc::GetStringRequest req;
  req.set_var(var);

  bc::GetStringResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_GetString(&ctx, req, &resp);

  if (!status.ok()) { return std::nullopt; }
  return std::move(*resp.release_value());
}

bool Bareos_SetInt(bc::BareosIntVariable var, int val)
{
  bc::SetIntRequest req;
  req.set_var(var);
  req.set_value(val);

  bc::SetIntResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_SetInt(&ctx, req, &resp);

  if (!status.ok()) { return false; }
  return true;
}
std::optional<int> Bareos_GetInt(bc::BareosIntVariable var)
{
  bc::GetIntRequest req;
  req.set_var(var);

  bc::GetIntResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_GetInt(&ctx, req, &resp);

  if (!status.ok()) { return std::nullopt; }
  return resp.value();
}

bool Bareos_SetFlag(bc::BareosFlagVariable var, bool val)
{
  bc::SetFlagRequest req;
  req.set_var(var);
  req.set_value(val);

  bc::SetFlagResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_SetFlag(&ctx, req, &resp);

  if (!status.ok()) { return false; }
  return true;
}
std::optional<bool> Bareos_GetFlag(bc::BareosFlagVariable var)
{
  bc::GetFlagRequest req;
  req.set_var(var);

  bc::GetFlagResponse resp;
  grpc::ClientContext ctx;
  grpc::Status status = stub()->Bareos_GetFlag(&ctx, req, &resp);

  if (!status.ok()) { return std::nullopt; }
  return resp.value();
}

void HandleConnection(int server_sock, int client_sock, int io_sock)
{
  std::promise<void> shutdown_signal;

  auto barrier = shutdown_signal.get_future();

  con = connection_builder{std::make_unique<PluginService>(
                               io_sock, std::move(shutdown_signal))}
            .connect_client(client_sock)
            .connect_server(server_sock)
            .build();

  if (!con) { exit(1); }

  DebugLog(100, FMT_STRING("waiting for server to finish ..."));

  barrier.wait();

  // con->server->Wait();

  DebugLog(100, FMT_STRING("grpc server finished: closing connections"));
  con.reset();
}


int main(int argc, char* argv[])
{
  // every (C) program is started in the C locale,
  // so the following call ensures that we respect the users locale setting.
  // This is necessary to ensure UTF-8 support as well.
  setlocale(LC_ALL, "");

  (void)argv;
  if (argc != 1) {
    fprintf(
        stderr,
        "Usage: call this program with no arguments but the sockets set up\n");
    return 5;
  }

  // todo: add these in a header somewhere ?
  int server = 3;
  int client = 4;
  int io = 5;

  if (fcntl(server, F_GETFD) < 0) {
    printf("bad server file descriptor given: %d\n", server);
    return 3;
  }

  if (fcntl(client, F_GETFD) < 0) {
    printf("bad client file descriptor given: %d\n", client);
    return 3;
  }

  if (fcntl(io, F_GETFD) < 0) {
    printf("bad io file descriptor given: %d\n", io);
    return 3;
  }

  HandleConnection(server, client, io);
}
