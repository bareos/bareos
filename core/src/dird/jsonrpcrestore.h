#ifndef BAREOS_DIRD_JSONRPCRESTORE_H_
#define BAREOS_DIRD_JSONRPCRESTORE_H_

#include "dird/bjsonrpcserver.h"

namespace directordaemon {

Json::Value InititiateRestore(const std::string& clientname,
                              RestoreResources& restore_resources,
                              int jobid);
Json::Value CdCmd(const std::string& path, TreeContext* tree);
Json::Value LsCmd(int limit,
                  int offset,
                  const std::string& path,
                  TreeContext* tree);
Json::Value LsCount(const std::string& path, TreeContext* tree);
Json::Value DoneCmd(RestoreResources& restore_resources);
Json::Value CommitRestore(RestoreResources& restore_resources);
Json::Value UnmarkCmd(UaContext *ua, TreeContext &tree,
                      const Json::Value& files);
Json::Value MarkCmd(UaContext* ua, TreeContext& tree, const Json::Value& files);

}  // namespace directordaemon

#endif  // BAREOS_DIRD_JSONRPCRESTORE_H_
