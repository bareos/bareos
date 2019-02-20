#ifndef AUTHENTICATE_H_INCLUDED
#define AUTHENTICATE_H_INCLUDED

class MonitorResource;
class MonitorItem;
class JobControlRecord;

enum class AuthenticationResult
{
  kNoError,
  kAlreadyAuthenticated,
  kQualifiedResourceNameFailed,
  kTlsHandshakeFailed,
  kSendHelloMessageFailed,
  kCramMd5HandshakeFailed,
  kDaemonResponseFailed,
  kRejectedByDaemon,
  kUnknownDaemon
};

AuthenticationResult AuthenticateWithDaemon(MonitorItem* item,
                                            JobControlRecord* jcr);
bool GetAuthenticationResultString(AuthenticationResult err,
                                   std::string& buffer);

#endif  // AUTHENTICATE_H_INCLUDED
