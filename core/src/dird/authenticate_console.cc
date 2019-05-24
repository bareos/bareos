/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "dird/authenticate_console.h"
#include "dird/dird.h"
#if defined(HAVE_PAM)
#include "dird/auth_pam.h"
#endif
#include "dird/dird_globals.h"
#include "lib/bnet.h"
#include "lib/bsock.h"
#include "lib/qualified_resource_name_type_converter.h"
#include "lib/bstringlist.h"
#include "lib/jcr.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "include/version_numbers.h"

namespace directordaemon {

enum class OptionResult
{
  Completed,
  Skipped
};

class ConsoleAuthenticator {
 public:
  ConsoleAuthenticator(UaContext* ua, const std::string& console_name);
  virtual ~ConsoleAuthenticator() = default;

  ConsoleAuthenticator(const ConsoleAuthenticator& other) = delete;
  virtual OptionResult AuthenticateDefaultConsole() = 0;
  virtual void AuthenticateNamedConsole() = 0;
  virtual OptionResult AuthenticatePamUser() = 0;
  virtual bool SendInfoMessage() { return false; }

  bool auth_success_;
  std::string console_name_;

 protected:
  OptionResult DoDefaultAuthentication();
  void DoNamedAuthentication();

  UaContext* ua_;
  ConsoleResource* optional_console_resource_;
};

ConsoleAuthenticator::ConsoleAuthenticator(UaContext* ua,
                                           const std::string& console_name)
    : auth_success_(false), console_name_(console_name), ua_(ua)
{
  optional_console_resource_ = (ConsoleResource*)my_config->GetResWithName(
      R_CONSOLE, console_name_.c_str());
}

OptionResult ConsoleAuthenticator::DoDefaultAuthentication()
{
  const std::string default_console_name{"*UserAgent*"};

  if (console_name_ != default_console_name) { return OptionResult::Skipped; }

  auth_success_ = ua_->UA_sock->AuthenticateInboundConnection(
      NULL, "Console", default_console_name.c_str(), me->password_, me);
  ua_->cons = nullptr;
  return OptionResult::Completed;
}

void ConsoleAuthenticator::DoNamedAuthentication()
{
  if (!optional_console_resource_) {
    Dmsg1(200, "Could not find resource for console %s\n",
          console_name_.c_str());
    auth_success_ = false;
    return;
  }

  auth_success_ = ua_->UA_sock->AuthenticateInboundConnection(
      NULL, "Console", console_name_.c_str(),
      optional_console_resource_->password_, optional_console_resource_);
  if (auth_success_) {
    ua_->cons = optional_console_resource_;
  } else {
    ua_->cons = nullptr;
    Dmsg1(200, "Could not authenticate console %s\n", console_name_.c_str());
  }
}

class ConsoleAuthenticatorBefore_18_2 : public ConsoleAuthenticator {
 public:
  ConsoleAuthenticatorBefore_18_2(UaContext* ua,
                                  const std::string& console_name)
      : ConsoleAuthenticator(ua, console_name)
  {
  }
  OptionResult AuthenticateDefaultConsole() override;
  void AuthenticateNamedConsole() override;
  OptionResult AuthenticatePamUser() override;

 private:
  bool SendOkMessageWithSpaceSeparators()
  {
    if (ua_) {
      std::string cipher;
      ua_->UA_sock->GetCipherMessageString(cipher);
      if (ua_->UA_sock) {
        return ua_->UA_sock->fsend(_("1000 OK: %s Version: %s (%s) "
                                     "-- %s\n"),
                                   my_name, VERSION, BDATE, cipher.c_str());
      }
    }
    return false;
  }

  bool SendNotAuthorizedMessageWithSpaceSeparators()
  {
    if (ua_ && ua_->UA_sock) {
      return ua_->UA_sock->fsend("%s", "1999 You are not authorized.\n");
    }
    return false;
  }
};

OptionResult ConsoleAuthenticatorBefore_18_2::AuthenticateDefaultConsole()
{
  if (DoDefaultAuthentication() == OptionResult::Skipped) {
    return OptionResult::Skipped;
  }

  if (auth_success_) {
    auth_success_ = SendOkMessageWithSpaceSeparators();
  } else {
    SendNotAuthorizedMessageWithSpaceSeparators();
  }
  if (!auth_success_) {
    Dmsg1(200, "Could not authenticate outdated console %s\n",
          console_name_.c_str());
  }
  return OptionResult::Completed;
}

void ConsoleAuthenticatorBefore_18_2::AuthenticateNamedConsole()
{
  DoNamedAuthentication();

  if (auth_success_) {
    auth_success_ = SendOkMessageWithSpaceSeparators();
  } else {
    SendNotAuthorizedMessageWithSpaceSeparators();
  }
}

OptionResult ConsoleAuthenticatorBefore_18_2::AuthenticatePamUser()
{
  if (!optional_console_resource_) { return OptionResult::Skipped; }
  if (optional_console_resource_->use_pam_authentication_) {
    Dmsg1(200, "PAM authentication using outdated Bareos console denied: %s\n",
          console_name_.c_str());
    auth_success_ =
        false; /* console before 18_2 cannot do pam authentication */
    return OptionResult::Completed;
  }
  return OptionResult::Skipped;
}

class ConsoleAuthenticatorFrom_18_2 : public ConsoleAuthenticator {
 public:
  ConsoleAuthenticatorFrom_18_2(UaContext* ua, const std::string& console_name)
      : ConsoleAuthenticator(ua, console_name)
  {
  }
  OptionResult AuthenticateDefaultConsole() override;
  void AuthenticateNamedConsole() override;
  OptionResult AuthenticatePamUser() override;
  bool SendInfoMessage() override;

 private:
  bool SendResponseMessage(uint32_t response_id, bool send_version_info);
};

bool ConsoleAuthenticatorFrom_18_2::SendResponseMessage(uint32_t response_id,
                                                        bool send_version_info)
{
  std::string message;
  if (send_version_info) {
    char version_info[128];
    ::snprintf(version_info, 100, "OK: %s Version: %s (%s)", my_name, VERSION,
               BDATE);
    message = version_info;
  }
  return ua_->UA_sock->FormatAndSendResponseMessage(response_id, message);
}

bool ConsoleAuthenticatorFrom_18_2::SendInfoMessage()
{
  std::string message;
  message += BAREOS_BINARY_INFO;
  message += " binary\n";
  message += BAREOS_SERVICES_MESSAGE;
  message += "\n";
  message += "You are ";
  if (ua_->cons) {
    message += "logged in as: ";
    message += ua_->cons->resource_name_;
  } else {
    message += "connected using the default console";
  }
  auth_success_ = ua_->UA_sock->FormatAndSendResponseMessage(
      kMessageIdInfoMessage, message);
  return true;
}

OptionResult ConsoleAuthenticatorFrom_18_2::AuthenticateDefaultConsole()
{
  if (DoDefaultAuthentication() == OptionResult::Skipped) {
    return OptionResult::Skipped;
  }

  if (auth_success_) {
    if (!SendResponseMessage(kMessageIdOk, true)) { auth_success_ = false; }
  }

  if (!auth_success_) {
    Dmsg1(200, "Could not authenticate console %s\n", console_name_.c_str());
  }
  return OptionResult::Completed;
}

void ConsoleAuthenticatorFrom_18_2::AuthenticateNamedConsole()
{
  DoNamedAuthentication();

  if (!auth_success_) { return; }

  uint32_t response_id = kMessageIdOk;
  bool send_version = true;

  if (optional_console_resource_ &&
      optional_console_resource_->use_pam_authentication_) {
    response_id = kMessageIdPamRequired;
    send_version = false;
  }

  if (!SendResponseMessage(response_id, send_version)) {
    Dmsg1(200, "Send of response message failed %s\n", console_name_.c_str());
    auth_success_ = false;
  }
}

OptionResult ConsoleAuthenticatorFrom_18_2::AuthenticatePamUser()
{
#if !defined(HAVE_PAM)
  {
    if (optional_console_resource_ &&
        optional_console_resource_->use_pam_authentication_) {
      Emsg0(M_ERROR, 0, _("PAM is not available on this director\n"));
      auth_success_ = false;
      return OptionResult::Completed;
    } else {
      return OptionResult::Skipped;
    }
  }
#else  /* HAVE_PAM */
  {
    if (!optional_console_resource_) {
      auth_success_ = false;
      return OptionResult::Completed;
    }

    if (!optional_console_resource_->use_pam_authentication_) {
      return OptionResult::Skipped;
    }

    uint32_t response_id;
    BStringList message_arguments;

    if (!ua_->UA_sock->ReceiveAndEvaluateResponseMessage(response_id,
                                                         message_arguments)) {
      Dmsg2(100, "Could not evaluate response_id: %d - %d", response_id,
            message_arguments.JoinReadable().c_str());
      auth_success_ = false;
      return OptionResult::Completed;
    }

    std::string pam_username;
    std::string pam_password;

    if (response_id == kMessageIdPamUserCredentials) {
      Dmsg0(200, "Console chooses PAM direct credentials\n");
      if (message_arguments.size() < 3) {
        Dmsg0(200, "Console sent wrong number of credentials\n");
        auth_success_ = false;
        return OptionResult::Completed;
      } else {
        pam_username = message_arguments.at(1);
        pam_password = message_arguments.at(2);
      }
    } else if (response_id == kMessageIdPamInteractive) {
      Dmsg0(200, "Console chooses PAM interactive\n");
    }

    Dmsg1(200, "Try to authenticate user using PAM:%s\n", pam_username.c_str());

    std::string authenticated_username;
    if (!PamAuthenticateUser(ua_->UA_sock, pam_username, pam_password,
                             authenticated_username)) {
      ua_->cons = nullptr;
      auth_success_ = false;
    } else {
      ConsoleResource* user = (ConsoleResource*)my_config->GetResWithName(
          R_CONSOLE, authenticated_username.c_str());
      if (!user) {
        Dmsg1(200, "No user config found for user %s\n",
              authenticated_username.c_str());
        ua_->cons = nullptr;
        auth_success_ = false;
      } else {
        ua_->cons = user;
        auth_success_ = true;
      }
    }
    if (auth_success_) {
      if (!SendResponseMessage(kMessageIdOk, true)) { auth_success_ = false; }
    }
    return OptionResult::Completed;
  } /* HAVE PAM */
#endif /* !HAVE_PAM */
}

static void LogErrorMessage(std::string console_name, UaContext* ua)
{
  Emsg4(M_ERROR, 0, _("Unable to authenticate console \"%s\" at %s:%s:%d.\n"),
        console_name.c_str(), ua->UA_sock->who(), ua->UA_sock->host(),
        ua->UA_sock->port());
}

static bool NumberOfConsoleConnectionsExceeded()
{
  JobControlRecord* jcr;
  unsigned int cnt = 0;

  foreach_jcr (jcr) {
    if (jcr->is_JobType(JT_CONSOLE)) { cnt++; }
  }
  endeach_jcr(jcr);

  return (cnt >= me->MaxConsoleConnections) ? true : false;
}

static bool GetConsoleNameAndVersion(BareosSocket* ua_sock,
                                     std::string& name_out,
                                     BareosVersionNumber& version_out)
{
  std::string name;
  std::string r_code_str_unused;
  BareosVersionNumber version = BareosVersionNumber::kUndefined;

  if (GetNameAndResourceTypeAndVersionFromHello(ua_sock->msg, name,
                                                r_code_str_unused, version)) {
    name_out = name;
    version_out = version;
    return true;
  } else {
    Emsg4(M_ERROR, 0, _("UA Hello from %s:%s:%d is invalid. Got: %s\n"),
          ua_sock->who(), ua_sock->host(), ua_sock->port(), ua_sock->msg);
    return false;
  }
}

static ConsoleAuthenticator* CreateConsoleAuthenticator(UaContext* ua)
{
  std::string console_name;
  BareosVersionNumber version = BareosVersionNumber::kUndefined;
  if (!GetConsoleNameAndVersion(ua->UA_sock, console_name, version)) {
    return nullptr;
  }

  ua->UA_sock->connected_daemon_version_ =
      version; /* this is redundant if using cleartext handshake */

  if (version < BareosVersionNumber::kRelease_18_2) {
    return new ConsoleAuthenticatorBefore_18_2(ua, console_name);
  } else /* == kRelease_18_2 */ {
    return new ConsoleAuthenticatorFrom_18_2(ua, console_name);
  }
}

bool AuthenticateConsole(UaContext* ua)
{
  if (NumberOfConsoleConnectionsExceeded()) {
    Emsg0(M_ERROR, 0,
          _("Number of console connections exceeded "
            "MaximumConsoleConnections\n"));
    return false;
  }

  std::unique_ptr<ConsoleAuthenticator> console_authenticator(
      CreateConsoleAuthenticator(ua));
  if (!console_authenticator) { return false; }

  if (console_authenticator->AuthenticateDefaultConsole() ==
      OptionResult::Completed) {
    if (!console_authenticator->auth_success_) {
      LogErrorMessage(console_authenticator->console_name_, ua);
      return false;
    }
  } else {
    console_authenticator->AuthenticateNamedConsole();
    if (!console_authenticator->auth_success_) {
      LogErrorMessage(console_authenticator->console_name_, ua);
      return false;
    }
    if (console_authenticator->AuthenticatePamUser() ==
        OptionResult::Completed) {
      if (!console_authenticator->auth_success_) {
        LogErrorMessage(console_authenticator->console_name_, ua);
        return false;
      }
    }
  }
  if (console_authenticator->SendInfoMessage()) {
    if (!console_authenticator->auth_success_) { return false; }
  }
  return true;
}
} /* namespace directordaemon */
