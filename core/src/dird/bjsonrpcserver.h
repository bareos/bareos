#ifndef BJSONRPCSERVER_H
#define BJSONRPCSERVER_H

#include "dird/stubserver.h"
#include "dird/ua.h"
#include "dird/bsr.h"
#include "dird/commandresourcemanager.h"
#include "dird/ua_db.h"
#include <unordered_map>

enum class RestoreState
{
  initiate,
  start_select_files,
  end_select_files
};

struct RestoreResources {
  directordaemon::RestoreContext rx{};
  directordaemon::CommandResourceManager manager{};
  directordaemon::TreeContext tree{};
  RestoreState state{RestoreState::initiate};
};

class RestoreManager {
 public:
  Json::Value NewRestore(int restoreid)
  {
    Json::Value result;
    std::pair inserted_pair = active_restores_.try_emplace(restoreid);
    bool pair_already_exists = !inserted_pair.second;
    if (pair_already_exists) {
      result["error"] = "A restore with the same ID="
                        + std::to_string(restoreid) + " is already running.";
      return result;
    }
    return result;
  }

  void RemoveRestore(int restoreid) { active_restores_.erase(restoreid); }

  Json::Value CheckRestoreAvailablity(int restoreid)
  {
    Json::Value result;
    if (active_restores_.find(restoreid) == active_restores_.end()) {
      result["error"] = "Restore ID=" + std::to_string(restoreid)
                        + " does not refer to an active restore";
    }
    return result;
  }

  std::vector<int> GetActiveRestores()
  {
    std::vector<int> restorekeys;
    if (!active_restores_.empty()) {
      for (const auto& restore : active_restores_) {
        restorekeys.push_back(restore.first);
      }
    }
    return restorekeys;
  }

  RestoreResources& GetRestoreResources(int restoreid)
  {
    return active_restores_[restoreid];
  }

 private:
  std::unordered_map<int, RestoreResources> active_restores_;
};

class BJsonRpcServer : public StubServer {
 public:
  BJsonRpcServer(jsonrpc::AbstractServerConnector& connector,
                 jsonrpc::serverVersion_t type = jsonrpc::JSONRPC_SERVER_V2)
      : StubServer(connector, type)
  {
  }

  Json::Value list_client(const std::string& name) override;
  Json::Value list_clients() override;
  Json::Value list_filesets() override;
  Json::Value list_fileset(int jobid, const std::string& jobname) override;
  Json::Value list_restores() override;
  Json::Value restore(const std::string& clientname,
                      int jobid,
                      int restoreid) override;
  Json::Value cd(const std::string& path, int restoreid) override;
  Json::Value ls(int limit,
                 int offset,
                 const std::string& path,
                 int restoreid) override;
  Json::Value lscount(const std::string& path, int restoreid) override;
  Json::Value mark(const Json::Value& files, int restoreid) override;
  Json::Value unmark(const Json::Value& files, int restoreid) override;
  Json::Value done(int restoreid) override;
  Json::Value commitrestore(int restoreid) override;
  Json::Value cancelrestore(int restoreid) override;

 private:
  Json::Value ToJson(const std::string&);

  RestoreManager restore_manager_;
};


#endif  // BJSONRPCSERVER_H
