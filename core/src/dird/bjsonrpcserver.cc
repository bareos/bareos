#include "bjsonrpcserver.h"
#include "dird/jsonrpccommands.h"
#include "dird/jsonrpcrestore.h"

static std::string CreateJsonMessage(const std::string& message)
{
  return "{ \"message\":\"" + message + "\"";
}

Json::Value BJsonRpcServer::ToJson(const std::string& result_string)
{
  Json::Value jsonresult;
  std::stringstream stream;
  stream << result_string;
  Json::CharReaderBuilder rbuilder;
  std::string errs;

  if (!Json::parseFromStream(rbuilder, stream, &jsonresult, &errs)) {
    stream.flush();
    stream << CreateJsonMessage(result_string);
    Json::parseFromStream(rbuilder, stream, &jsonresult, &errs);
  }
  return jsonresult;
}

Json::Value BJsonRpcServer::list_client(const std::string& name)
{
  return ToJson(directordaemon::ListClient(name));
}

Json::Value BJsonRpcServer::list_clients()
{
  return ToJson(directordaemon::ListClients());
}

Json::Value BJsonRpcServer::list_filesets()
{
  return ToJson(directordaemon::ListFilesets());
}

Json::Value BJsonRpcServer::list_fileset(int jobid, const std::string& jobname)
{
  return ToJson(directordaemon::ListFileset(jobid, jobname));
}

Json::Value BJsonRpcServer::list_restores()
{
  Json::Value result;
  std::vector<int> restore_list = restore_manager_.GetActiveRestores();
  if (!restore_list.empty()) {
    for (auto restorekey : restore_list) {
      result["restoreids"].append(restorekey);
    }
  } else {
    result["error"] = "No running restores available.";
  }

  return result;
}
Json::Value BJsonRpcServer::restore(const std::string& clientname,
                                    int jobid,
                                    int restoreid)
{
  Json::Value result = restore_manager_.NewRestore(restoreid);
  if (result.isMember("error")) { return result; }
  RestoreResources& restore_resources
      = restore_manager_.GetRestoreResources(restoreid);

  switch (restore_resources.state) {
    case RestoreState::initiate:
      result = directordaemon::InititiateRestore(clientname, restore_resources,
                                                 jobid);
      if (result.isMember("error")) {
        restore_manager_.RemoveRestore(restoreid);
      }
      return result;

    case RestoreState::start_select_files:
      result["error"]
          = "Restore is in selection mode. Make sure you finish that first "
            "using the file selection methods";
      return result;

    case RestoreState::end_select_files: {
      result["error"]
          = "This restore has finished file selection and is waiting for a "
            "commit";
      return result;
    }
    default:
      result["error"] = "unknown restore state";
      return result;
  }

  return result;
}

Json::Value BJsonRpcServer::cd(const std::string& path, int restoreid)
{
  Json::Value result = restore_manager_.CheckRestoreAvailablity(restoreid);
  return result.isMember("error")
             ? result
             : directordaemon::CdCmd(
                 path, &restore_manager_.GetRestoreResources(restoreid).tree);
}

Json::Value BJsonRpcServer::ls(int limit,
                               int offset,
                               const std::string& path,
                               int restoreid)
{
  Json::Value result = restore_manager_.CheckRestoreAvailablity(restoreid);
  return result.isMember("error")
             ? result
             : directordaemon::LsCmd(
                 limit, offset, path,
                 &restore_manager_.GetRestoreResources(restoreid).tree);
}

Json::Value BJsonRpcServer::lscount(const std::string& path, int restoreid)
{
  Json::Value result = restore_manager_.CheckRestoreAvailablity(restoreid);
  return result.isMember("error")
             ? result
             : directordaemon::LsCount(
                 path, &restore_manager_.GetRestoreResources(restoreid).tree);
}

Json::Value BJsonRpcServer::mark(const Json::Value& files, int restoreid)
{
  Json::Value result = restore_manager_.CheckRestoreAvailablity(restoreid);
  if (result.isMember("error")) {
    return result;
  } else {
    RestoreResources& restoreresource
        = restore_manager_.GetRestoreResources(restoreid);
    result = directordaemon::MarkCmd(restoreresource.manager.ua_,
                                     restoreresource.tree, files);
    return result;
  }
}

Json::Value BJsonRpcServer::unmark(const Json::Value& files, int restoreid)
{
  Json::Value result = restore_manager_.CheckRestoreAvailablity(restoreid);
  if (result.isMember("error")) {
    return result;
  } else {
    RestoreResources& restoreresource
        = restore_manager_.GetRestoreResources(restoreid);
    result = directordaemon::UnmarkCmd(restoreresource.manager.ua_,
                                       restoreresource.tree, files);
    return result;
  }
}

Json::Value BJsonRpcServer::done(int restoreid)
{
  Json::Value result = restore_manager_.CheckRestoreAvailablity(restoreid);
  if (!result.isMember("error")) {
    result = directordaemon::DoneCmd(
        restore_manager_.GetRestoreResources(restoreid));
    if (result["selectedfiles"] == 0) {
      result["warning"]
          = "Selection done but no files selected. Aborting restore";
      restore_manager_.RemoveRestore(restoreid);
    }
  }

  return result;
}

Json::Value BJsonRpcServer::commitrestore(int restoreid)
{
  Json::Value result = restore_manager_.CheckRestoreAvailablity(restoreid);
  if (result.isMember("error")) { return result; }
  result = directordaemon::CommitRestore(
      restore_manager_.GetRestoreResources(restoreid));
  if (!result.isMember("error")) { restore_manager_.RemoveRestore(restoreid); }
  return result;
}


Json::Value BJsonRpcServer::cancelrestore(int restoreid)
{
  Json::Value result = restore_manager_.CheckRestoreAvailablity(restoreid);
  if (result.isMember("error")) { return result; }

  restore_manager_.RemoveRestore(restoreid);
  result["status"]
      = "Restore with ID=" + std::to_string(restoreid) + " removed";
  return result;
}
