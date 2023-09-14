#include "jsonrpccommands.h"

#include "dird/commandresourcemanager.h"

namespace directordaemon {

std::string ListClient(std::string name)
{
  CommandResourceManager manager{};

  manager.ua_->db->ListClientRecords(manager.ua_->jcr, name.data(),
                                     manager.ua_->send, VERT_LIST);

  return manager.GetResponse();
}

std::string ListClients()
{
  CommandResourceManager manager{};

  manager.ua_->db->ListClientRecords(manager.ua_->jcr, nullptr,
                                     manager.ua_->send, VERT_LIST);

  return manager.GetResponse();
}

std::string ListFileset(int jobid, const std::string& jobname)
{
  JobDbRecord jr;
  CommandResourceManager manager{};

  std::string query_range{};

  if (jobid > 0) {
    jr.JobId = jobid;
  } else if (!jobname.empty()) {
    jr.JobId = 0;
    bstrncpy(jr.Name, jobname.c_str(), MAX_NAME_LENGTH);
  }

  manager.ua_->db->ListFilesets(manager.jcr_, &jr, query_range.c_str(),
                                manager.ua_->send, VERT_LIST);

  return manager.GetResponse();
}

std::string ListFilesets()
{
  JobDbRecord jr;
  CommandResourceManager manager{};

  std::string query_range{};

  manager.ua_->db->ListFilesets(manager.jcr_, &jr, query_range.c_str(),
                                manager.ua_->send, VERT_LIST);

  return manager.GetResponse();
}

}  // namespace directordaemon
