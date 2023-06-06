#include "jsonrpccommands.h"

#include "lib/bsock_tcp.h"
#include "include/job_types.h"
#include "include/jcr.h"
#include "lib/output_formatter.h"
#include "dird/ua_server.h"
#include "dird/ua_output.h"
#include "cats/cats.h"
#include "dird/ua_db.h"

namespace directordaemon {

JsonRpcCommands::JsonRpcCommands() {}

json ListClient(std::string name)
{
  json result{};
  CommandResourceManager manager;
  if (!OpenDb(manager.ua_)) { return result; }


  manager.ua_->db->ListClientRecords(manager.ua_->jcr, name.data(),
                                     manager.ua_->send, VERT_LIST);
  result = json::parse(manager.GetResponse());


  return result;
}

json ListClients()
{
  json result{};
  CommandResourceManager manager;
  if (!OpenDb(manager.ua_)) { return result; }

  manager.ua_->db->ListClientRecords(manager.ua_->jcr, nullptr,
                                     manager.ua_->send, VERT_LIST);
  result = json::parse(manager.GetResponse());

  return result;
}


class EmptySocket : public BareosSocketTCP {
 public:
  EmptySocket();
  ~EmptySocket();

  bool send() override;
};

EmptySocket::EmptySocket() : BareosSocketTCP() {}

EmptySocket::~EmptySocket() { destroy(); }

bool EmptySocket::send() { return true; }

CommandResourceManager::CommandResourceManager()

{
  jcr_ = new_control_jcr("-RunScript-", JT_CONSOLE);
  ua_ = new_ws_ua_context(jcr_);
}

CommandResourceManager::~CommandResourceManager()
{
  FreeUaContext(ua_);
  FreeJcr(jcr_);
}

std::string CommandResourceManager::GetResponse() const
{
  ua_->send->FinalizeResult();
  return response_text_;
}

bool CommandResourceManager::result_append(void* pm_string,
                                           const char* fmt,
                                           ...)
{
  PoolMem additionalstring;
  std::string* pm = static_cast<std::string*>(pm_string);

  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  additionalstring.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);

  pm->append(additionalstring.c_str());

  return true;
}

UaContext* CommandResourceManager::new_ws_ua_context(JobControlRecord* jcr)
{
  UaContext* ua = (UaContext*)malloc(sizeof(UaContext));
  ua = new (ua) UaContext();
  ua->jcr = jcr;
  ua->db = jcr->db;
  ua->cmd = GetPoolMemory(PM_FNAME);
  ua->args = GetPoolMemory(PM_FNAME);
  ua->verbose = true;
  ua->automount = true;

  ua->send = new OutputFormatter(result_append, (void*)&response_text_,
                                 filterit, ua_);

  EmptySocket* emptysocket = new EmptySocket();
  ua->UA_sock = emptysocket;

  ua->api = API_MODE_JSON;
  ua->send->SetMode(ua->api);
  ua->send->SetCompact(true);
  ua->batch = true;

  return ua;
}

}  // namespace directordaemon
