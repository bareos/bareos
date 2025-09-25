/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
/*
   Derived from a SMTPclient:

  ======== Original copyrights ==========

       SMTPclient -- simple SMTP client

       Copyright (c) 1997 Ralf S. Engelschall, All rights reserved.

       This program is free software; it may be redistributed and/or modified
       only under the terms of either the Artistic License or the GNU General
       Public License, which may be found in the SMTP source distribution.
       Look at the file COPYING.

       This program is distributed in the hope that it will be useful, but
       WITHOUT ANY WARRANTY; without even the implied warranty of
       MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
       GNU Affero General Public License for more details.

       ======================================================================

       smtpclient_main.c -- program source

       Based on smtp.c as of August 11, 1995 from
           W.Z. Venema,
           Eindhoven University of Technology,
           Department of Mathematics and Computer Science,
           Den Dolech 2, P.O. Box 513, 5600 MB Eindhoven, The Netherlands.

   =========


   Kern Sibbald, July 2001

     Note, the original W.Z. Venema smtp.c had no license and no
     copyright.  See:
        http://archives.neohapsis.com/archives/postfix/2000-05/1520.html
 */
#include <netdb.h>
#include <pwd.h>
#if !defined(HAVE_MSVC)
#  include <unistd.h>
#endif
#include "include/bareos.h"
#include "include/exit_codes.h"
#include "include/jcr.h"
#include "lib/cli.h"
#include "lib/bstringlist.h"
#define MY_NAME "bsmtp"

#if defined(HAVE_WIN32)
#  include <lmcons.h>
#endif

#ifndef MAXSTRING
#  define MAXSTRING 254
#endif

enum resolv_type
{
  RESOLV_PROTO_ANY,
  RESOLV_PROTO_IPV4,
  RESOLV_PROTO_IPV6
};

static FILE* sfp;
static FILE* rfp;

const int default_port = 25;
const std::string default_mailhost = "localhost";

/*
 * Take input that may have names and other stuff and strip
 *  it down to the mail box address ... i.e. what is enclosed
 *  in < >.  Otherwise add < >.
 */
static char* cleanup_addr(std::string addr, char* buf, int buf_len)
{
  char *p, *q;

  if ((p = strchr(addr.data(), '<')) == nullptr) {
    snprintf(buf, buf_len, "<%s>", addr.c_str());
  } else {
    /* Copy <addr> */
    for (q = buf; *p && *p != '>';) { *q++ = *p++; }
    if (*p) { *q++ = *p; }
    *q = 0;
  }
  Dmsg2(100, "cleanup in=%s out=%s\n", addr.c_str(), buf);
  return buf;
}

//  examine message from server
static void GetResponse(const std::string& mailhost)
{
  char buf[1000];

  Dmsg0(50, "Calling fgets on read socket rfp.\n");
  buf[3] = 0;
  while (fgets(buf, sizeof(buf), rfp)) {
    int len = strlen(buf);
    if (len > 0) { buf[len - 1] = 0; }
    if (debug_level >= 10) {
      fprintf(stderr, "%s <-- %s\n", mailhost.c_str(), buf);
    }
    Dmsg2(10, "%s --> %s\n", mailhost.c_str(), buf);
    if (!isdigit((int)buf[0]) || buf[0] > '3') {
      Pmsg2(0, T_("Fatal malformed reply from %s: %s\n"), mailhost.c_str(),
            buf);
      exit(BEXIT_FAILURE);
    }
    if (buf[3] != '-') { break; }
  }
  if (ferror(rfp)) {
    fprintf(stderr, T_("Fatal fgets error: ERR=%s\n"), strerror(errno));
  }
  return;
}

//  say something to server and check the response
static void chat(char* my_hostname,
                 const std::string& mailhost,
                 const char* fmt,
                 ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(sfp, fmt, ap);
  va_end(ap);
  if (debug_level >= 10) {
    fprintf(stdout, "%s --> ", my_hostname);
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
  }

  fflush(sfp);
  if (debug_level >= 10) { fflush(stdout); }
  GetResponse(mailhost);
}

/*
 * Return the offset west from localtime to UTC in minutes
 * Same as timezone.tz_minuteswest
 *   Unix TzOffset coded by:  Attila Fülöp
 */
static long TzOffset([[maybe_unused]] time_t lnow, struct tm& tm)
{
#if defined(HAVE_WIN32)
#  if defined(HAVE_MINGW)
  __MINGW_IMPORT long _dstbias;
#  endif
#  if defined(MINGW64)
#    define _tzset tzset
#  endif
  /* Win32 code */
  long offset;
  _tzset();
  offset = _timezone;
  if (tm.tm_isdst) { offset += _dstbias; }
  return offset /= 60;
#else

  /* Unix/Linux code */
  struct tm tm_utc;
  time_t now = lnow;

  (void)gmtime_r(&now, &tm_utc);
  tm_utc.tm_isdst = tm.tm_isdst;
  return (long)difftime(mktime(&tm_utc), now) / 60;
#endif
}

static void GetDateString(char* buf, int buf_len)
{
  time_t now = time(NULL);
  struct tm tm;
  char tzbuf[MAXSTRING];
  long my_timezone;

  /* Add RFC822 date */
  Blocaltime(&now, &tm);

  my_timezone = TzOffset(now, tm);
  strftime(buf, buf_len, "%a, %d %b %Y %H:%M:%S", &tm);
  snprintf(tzbuf, sizeof(tzbuf), " %+2.2ld%2.2ld", -my_timezone / 60,
           labs(my_timezone) % 60);
  strcat(buf, tzbuf); /* add +0100 */
  strftime(tzbuf, sizeof(tzbuf), " (%Z)", &tm);
  strcat(buf, tzbuf); /* add (CEST) */
}


std::pair<std::string, int> ParseHostAndPort(std::string val)
{
  BStringList host_and_port;
  if (val.at(0) == '[') {
    host_and_port = BStringList(val, "]:");
    host_and_port[0].erase(0, 1);
  } else {
    host_and_port = BStringList(val, ':');
  }

  if (host_and_port[0].back() == ']') { host_and_port[0].pop_back(); }
  int mailport = default_port;
  if (host_and_port.size() == 2) { mailport = atoi(host_and_port[1].c_str()); }
  std::string mailhost{host_and_port[0]};
  return std::make_pair(mailhost, mailport);
}


/*********************************************************************
 *
 *  Program to send email
 */
int main(int argc, char* argv[])
{
  setlocale(LC_ALL, "en_US");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  MyNameIs(argc, argv, "bsmtp");

  CLI::App bsmtp_app;
  InitCLIApp(bsmtp_app, "The Bareos simple mail transport program.");
  bsmtp_app.set_help_flag("--help,-?", "Print this help message and exit.");

  resolv_type default_resolv_type = RESOLV_PROTO_IPV4;

  bsmtp_app.add_flag(
      "-4,--ipv4-protocol",
      [&default_resolv_type](bool) { default_resolv_type = RESOLV_PROTO_IPV4; },
      "Forces bsmtp to use IPv4 addresses only.");

  bsmtp_app.add_flag(
      "-6,--ipv6-protocol",
      [&default_resolv_type](bool) { default_resolv_type = RESOLV_PROTO_IPV6; },
      "Forces bsmtp to use IPv6 addresses only.");


  bool content_utf8 = false;
  bsmtp_app.add_flag("-8,--utf8", content_utf8, "set charset to UTF-8.");

  bsmtp_app.add_flag(
      "-a,--any-protocol",
      [&default_resolv_type](bool) { default_resolv_type = RESOLV_PROTO_ANY; },
      "Use any ip protocol for address resolution.");


  std::string cc_addr{};
  bsmtp_app.add_option("-c,--copy-to", cc_addr, "Set the Cc: field.");

  AddDebugOptions(bsmtp_app);

  std::string from_addr{};
  bsmtp_app.add_option("-f,--from", from_addr, "Set the From: field.")
      ->required();

  int mailport = default_port;
  std::string mailhost = default_mailhost;
  std::string host_and_port_source = "Mailhost and mailport set to default";

  char* env_variable_smtpserver;
  if ((env_variable_smtpserver = getenv("SMTPSERVER")) != nullptr) {
    std::pair<std::string, int> host_and_port
        = ParseHostAndPort(std::string(env_variable_smtpserver));
    mailhost = host_and_port.first;
    mailport = host_and_port.second;
    host_and_port_source
        = "Mailhost and port extracted from SMTPSERVER environment variable";
  }

  bsmtp_app
      .add_option(
          "-h,--mailhost",
          [&mailport, &mailhost,
           &host_and_port_source](std::vector<std::string> val) {
            std::pair<std::string, int> host_and_port
                = ParseHostAndPort(val[0]);
            mailhost = host_and_port.first;
            mailport = host_and_port.second;
            host_and_port_source
                = "Mailhost and port extracted from CLI arguments";
            return true;
          },
          "Use mailhost:port as the SMTP server.")
      ->type_name(
          "<mailhost/IPv4_address:port>,<[mailhost/IPv6_address]:port>");

  std::string subject{};
  bsmtp_app.add_option("-s,--subject", subject, "Set the Subject: field.")
      ->required();

  std::string reply_addr{};
  bsmtp_app.add_option("-r,--reply-to", reply_addr, "Set the Reply-To: field.");

  unsigned long maxlines = 0;
  bsmtp_app.add_option("-l,--max-lines", maxlines,
                       "Set the maximum number of lines to send.");

  std::vector<std::string> recipients;
  bsmtp_app.add_option("recipients", recipients, "List of recipients.")
      ->required();

  ParseBareosApp(bsmtp_app, argc, argv);

  Dmsg3(20, "%s: mailhost=%s ; mailport=%d\n", host_and_port_source.c_str(),
        mailhost.c_str(), mailport);


#if defined(HAVE_WIN32)
  WSADATA wsaData;

  _setmode(0, _O_BINARY);
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

  /*  Find out my own host name for HELO;
   *  if possible, get the fully qualified domain name */
  char my_hostname[MAXSTRING];
  if (gethostname(my_hostname, sizeof(my_hostname) - 1) < 0) {
    Pmsg1(0, T_("Fatal gethostname error: ERR=%s\n"), strerror(errno));
    exit(BEXIT_FAILURE);
  }

  int res;
  struct addrinfo hints;
  struct addrinfo *ai, *rp;
  char service_port[10];

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = 0;
  hints.ai_protocol = 0;
  hints.ai_flags = AI_CANONNAME;

  if ((res = getaddrinfo(my_hostname, NULL, &hints, &ai)) != 0) {
    Pmsg2(0, T_("Fatal getaddrinfo for myself failed \"%s\": ERR=%s\n"),
          my_hostname, gai_strerror(res));
    exit(BEXIT_FAILURE);
  }
  strncpy(my_hostname, ai->ai_canonname, sizeof(my_hostname) - 1);
  my_hostname[sizeof(my_hostname) - 1] = '\0';
  freeaddrinfo(ai);
  Dmsg1(20, "My hostname is: %s\n", my_hostname);

  //  Determine from address.
#if !defined(HAVE_WIN32)
  struct passwd* pwd;
#endif
  char buf[1000];
  if (from_addr.empty()) {
#if defined(HAVE_WIN32)
    DWORD dwSize = UNLEN + 1;
    LPSTR lpszBuffer = (LPSTR)alloca(dwSize);

    if (GetUserName(lpszBuffer, &dwSize)) {
      snprintf(buf, sizeof(buf), "%s@%s", lpszBuffer, my_hostname);
    } else {
      snprintf(buf, sizeof(buf), "unknown-user@%s", my_hostname);
    }
#else
    if ((pwd = getpwuid(getuid())) == 0) {
      snprintf(buf, sizeof(buf), "userid-%d@%s", (int)getuid(), my_hostname);
    } else {
      snprintf(buf, sizeof(buf), "%s@%s", pwd->pw_name, my_hostname);
    }
#endif
    from_addr = buf;
  }
  Dmsg1(20, "From addr=%s\n", from_addr.c_str());

#if defined(HAVE_WIN32)
  SOCKET s = INVALID_SOCKET;
#else
  int s{}, r{};
#endif
  //  Connect to smtp daemon on mailhost.
lookup_host:
  memset(&hints, 0, sizeof(struct addrinfo));
  switch (default_resolv_type) {
    case RESOLV_PROTO_ANY:
      hints.ai_family = AF_UNSPEC;
      break;
    case RESOLV_PROTO_IPV4:
      hints.ai_family = AF_INET;
      break;
    case RESOLV_PROTO_IPV6:
      hints.ai_family = AF_INET6;
      break;
    default:
      hints.ai_family = AF_UNSPEC;
      break;
  }
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  hints.ai_flags = 0;
  snprintf(service_port, sizeof(service_port), "%d", mailport);

  if ((res = getaddrinfo(mailhost.c_str(), service_port, &hints, &ai)) != 0) {
    Pmsg2(0, T_("Error unknown mail host \"%s\": ERR=%s\n"), mailhost.c_str(),
          gai_strerror(res));
    if (!Bstrcasecmp(mailhost.c_str(), "localhost")) {
      Pmsg0(0, T_("Retrying connection using \"localhost\".\n"));
      mailhost = "localhost";
      goto lookup_host;
    }
    exit(BEXIT_FAILURE);
  }
  for (rp = ai; rp != NULL; rp = rp->ai_next) {
#if defined(HAVE_WIN32)
    s = WSASocket(rp->ai_family, rp->ai_socktype, rp->ai_protocol, NULL, 0, 0);
    if (s == INVALID_SOCKET) { continue; }
#else
    s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (s < 0) { continue; }
#endif

    if (connect(s, rp->ai_addr, rp->ai_addrlen) != -1) { break; }

    close(s);
  }

  if (!rp) {
    Pmsg1(0, T_("Failed to connect to mailhost %s\n"), mailhost.c_str());
    exit(BEXIT_FAILURE);
  }

  freeaddrinfo(ai);

#if defined(HAVE_WIN32)
  int fdSocket = _open_osfhandle(s, _O_RDWR | _O_BINARY);
  if (fdSocket == -1) {
    Pmsg1(0, T_("Fatal _open_osfhandle error: ERR=%s\n"), strerror(errno));
    exit(BEXIT_FAILURE);
  }

  int fdSocket2 = dup(fdSocket);

  if ((sfp = fdopen(fdSocket, "wb")) == NULL) {
    Pmsg1(0, T_("Fatal fdopen error: ERR=%s\n"), strerror(errno));
    exit(BEXIT_FAILURE);
  }
  if ((rfp = fdopen(fdSocket2, "rb")) == NULL) {
    Pmsg1(0, T_("Fatal fdopen error: ERR=%s\n"), strerror(errno));
    exit(BEXIT_FAILURE);
  }
#else
  if ((r = dup(s)) < 0) {
    Pmsg1(0, T_("Fatal dup error: ERR=%s\n"), strerror(errno));
    exit(BEXIT_FAILURE);
  }
  if ((sfp = fdopen(s, "w")) == 0) {
    Pmsg1(0, T_("Fatal fdopen error: ERR=%s\n"), strerror(errno));
    exit(BEXIT_FAILURE);
  }
  if ((rfp = fdopen(r, "r")) == 0) {
    Pmsg1(0, T_("Fatal fdopen error: ERR=%s\n"), strerror(errno));
    exit(BEXIT_FAILURE);
  }
#endif

  /*  Send SMTP headers.  Note, if any of the strings have a <
   *   in them already, we do not enclose the string in < >, otherwise
   *   we do. */
  GetResponse(mailhost); /* banner */
  chat(my_hostname, mailhost, "HELO %s\r\n", my_hostname);
  chat(my_hostname, mailhost, "MAIL FROM:%s\r\n",
       cleanup_addr(from_addr, buf, sizeof(buf)));

  for (const auto& recipient : recipients) {
    Dmsg1(20, "rcpt to: %s\n", recipient.c_str());
    chat(my_hostname, mailhost, "RCPT TO:%s\r\n",
         cleanup_addr(recipient, buf, sizeof(buf)));
  }

  if (!cc_addr.empty()) {
    chat(my_hostname, mailhost, "RCPT TO:%s\r\n",
         cleanup_addr(cc_addr, buf, sizeof(buf)));
  }
  Dmsg0(20, "Data\n");
  chat(my_hostname, mailhost, "DATA\r\n");

  //  Send message header
  fprintf(sfp, "From: %s\r\n", from_addr.c_str());
  Dmsg1(10, "From: %s\r\n", from_addr.c_str());
  // Subject has to be encoded if utf-8 see
  // https://datatracker.ietf.org/doc/html/rfc2047
  if (!subject.empty()) {
    // rtrim subject
    auto last_nonspace = subject.find_last_not_of(" \t\n\r\f\v") + 1;
    int last_nonspace_int
        = last_nonspace == subject.npos ? subject.size() : last_nonspace;
    fprintf(sfp, "Subject: %.*s\r\n", last_nonspace_int, subject.c_str());
    Dmsg1(10, "Subject: %s\r\n", subject.c_str());
  }
  if (!reply_addr.empty()) {
    fprintf(sfp, "Reply-To: %s\r\n", reply_addr.c_str());
    Dmsg1(10, "Reply-To: %s\r\n", reply_addr.c_str());
  }


  BStringList list_of_recipients;
  list_of_recipients << recipients;
  fprintf(sfp, "To: %s", list_of_recipients.Join(',').c_str());
  Dmsg1(10, "To: %s", list_of_recipients.Join(',').c_str());


  fprintf(sfp, "\r\n");
  Dmsg0(10, "\r\n");
  if (!cc_addr.empty()) {
    fprintf(sfp, "Cc: %s\r\n", cc_addr.c_str());
    Dmsg1(10, "Cc: %s\r\n", cc_addr.c_str());
  }

  fprintf(sfp, "MIME-Version: 1.0\r\n");
  Dmsg0(10, "MIME-Version: 1.0\r\n");

  std::string charset = "us-ascii";
  std::string encoding = "7bit";

  if (content_utf8) {
    charset = "utf-8";
    encoding = "8bit";
  }
  fprintf(sfp, "Content-Type: text/plain; charset=%s\r\n", charset.c_str());
  Dmsg0(10, "Content-Type: text/plain; charset=%s\r\n", charset.c_str());

  fprintf(sfp, "Content-Transfer-Encoding: %s\r\n", encoding.c_str());
  Dmsg0(10, "Content-Transfer-Encoding: %s\r\n", encoding.c_str());

  GetDateString(buf, sizeof(buf));
  fprintf(sfp, "Date: %s\r\n", buf);
  Dmsg1(10, "Date: %s\r\n", buf);

  fprintf(sfp, "\r\n");

  //  Send message body
  unsigned long lines = 0;
  while (fgets(buf, sizeof(buf), stdin)) {
    if (maxlines > 0 && ++lines > maxlines) {
      Dmsg1(20, "skip line because of maxlines limit: %lu\n", maxlines);
      while (fgets(buf, sizeof(buf), stdin)) { ++lines; }
      break;
    }
    buf[sizeof(buf) - 1] = '\0';
    buf[strlen(buf) - 1] = '\0';
    if (buf[0] == '.') { /* add extra . see RFC 2821 4.5.2 */
      fputs(".", sfp);
    }
    fputs(buf, sfp);
    fputs("\r\n", sfp);
  }

  if (lines > maxlines) {
    Dmsg1(10, "hit maxlines limit: %lu\n", maxlines);
    fprintf(sfp,
            "\r\n\r\n[maximum of %lu lines exceeded, skipped %lu lines of "
            "output]\r\n",
            maxlines, lines - maxlines);
  }

  //  Send SMTP quit command
  chat(my_hostname, mailhost, ".\r\n");
  chat(my_hostname, mailhost, "QUIT\r\n");

  //  Go away gracefully ...
  return BEXIT_SUCCESS;
}
