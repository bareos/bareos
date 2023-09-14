#include "commandresourcemanager.h"
#include "include/job_types.h"
#include "include/jcr.h"
#include "lib/output_formatter.h"
#include "dird/ua_server.h"
#include "dird/ua_output.h"
#include "cats/cats.h"
#include "dird/ua_db.h"
#include "lib/parse_conf.h"
#include "dird/dird_globals.h"


namespace directordaemon {

CommandResourceManager::CommandResourceManager()
{
  jcr_ = new_control_jcr("-RpcWebsocket-", JT_CONSOLE);
  ua_ = new_ws_ua_context(jcr_, API_MODE_JSON);
  OpenClientDb(ua_, true);
}

CommandResourceManager::~CommandResourceManager()
{
  if (ua_) {
    CloseDb(ua_);
    FreeUaContext(ua_);
  }
  if (jcr_) { FreeJcr(jcr_); }
}

std::string CommandResourceManager::GetResponse()
{
  ua_->send->FinalizeResult(true);
  std::string result = response_text_;
  response_text_.clear();
  return result;
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

UaContext* CommandResourceManager::new_ws_ua_context(JobControlRecord* jcr,
                                                     int api)
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

  ua->api = api;
  ua->send->SetMode(api);
  ua->send->SetCompact(false);
  ua->batch = true;

  return ua;
}

}  // namespace directordaemon
