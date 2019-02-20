/*
 * Copyright (c) 1998,1999,2000
 *      Traakan, Inc., Los Altos, CA
 *      All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Project:  NDMJOB
 * Ident:    $Id: $
 *
 * Description:
 *      Handle representation of an NDMP agent.
 *
 *      This provides for a common text for specifying
 *      an agent host, connection authentication, and
 *      protocol version. The text specification may
 *      originate on the command line, in a config file,
 *      or elsewhere.
 *
 *      A text string representation has the form:
 *
 *              AGENT ::= HOST[:PORT][/FLAGS][,ACCOUNT[,PASSWORD]]
 *              AGENT ::= .
 *              FLAGS ::= [23][ntm]
 *
 *      The internal representation is a struct ndmagent.
 */


#include "ndmlib.h"

/* On some solaris distributions INADDR_NONE is not defined,
 * so define it here..
 */

#ifndef INADDR_NONE
#define INADDR_NONE ((in_addr_t)-1)
#endif

int ndmagent_from_str(struct ndmagent* agent, char* str)
{
  int have_vers = 0;
  int have_auth = 0;
  int rc;
  char* acct;
  char* port;
  char* flags;

  NDMOS_MACRO_ZEROFILL(agent);

  if ((acct = strchr(str, ',')) != 0) *acct++ = 0; /* stomp */

  if ((port = strchr(str, ':')) != 0) *port++ = 0; /* stomp */

  if (port) {
    flags = strchr(port, '/');
  } else {
    flags = strchr(str, '/');
  }
  if (flags) *flags++ = 0; /* stomp */

  /*
   *           HOST[:PORT][/FLAGS][,ACCOUNT[,PASSWORD]]
   *           ^     ^      ^       ^
   *  str------+     |      |       |
   *  port-----------+      |       |
   *  flags-----------------+       |
   *  acct--------------------------+
   *
   * The delimiters have been stomped (*p=0).
   * If a portion is missing, the respective pointer is NULL (p==0)
   * We restore the delimiters (p[-1]=x) as we proceed.
   */

  strncpy(agent->host, str, NDMAGENT_HOST_MAX - 1);

  if (port) {
    agent->port = atoi(port);
    port[-1] = ':'; /* restore */
  } else {
    agent->port = NDMPPORT;
  }

  if (flags) {
    char* p;

    for (p = flags; *p; p++) {
      switch (*p) {
#ifndef NDMOS_OPTION_NO_NDMP2
        case '2':
          agent->protocol_version = 2;
          have_vers++;
          break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
        case '3':
          agent->protocol_version = 3;
          have_vers++;
          break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
        case '4':
          agent->protocol_version = 4;
          have_vers++;
          break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */

        case 'n': /* NDMP_AUTH_NONE */
        case 't': /* NDMP_AUTH_TEXT */
        case 'm': /* NDMP_AUTH_MD5 */
        case 'v': /* void (don't auth) */
          agent->auth_type = *p;
          have_auth++;
          break;

        default:
          rc = -1;
          goto error_out;
      }
    }
    if (have_auth > 1 || have_vers > 1) {
      rc = -2;
      goto error_out;
    }
    flags[-1] = '/'; /* restore */
  }

  if (acct) {
    char* pass;

    if ((pass = strchr(acct, ',')) != 0) *pass++ = 0;

    strncpy(agent->account, acct, NDMAGENT_ACCOUNT_MAX - 1);
    if (pass) {
      strncpy(agent->password, pass, NDMAGENT_PASSWORD_MAX - 1);
      pass[-1] = ',';
    }
    acct[-1] = ','; /* restore */

    if (!have_auth) { agent->auth_type = 't'; /* NDMP_AUTH_TEXT */ }
  }

  if (strcmp(agent->host, ".") == 0) {
    agent->conn_type = NDMCONN_TYPE_RESIDENT;
    strcpy(agent->host, "(resident)");
  } else {
    agent->conn_type = NDMCONN_TYPE_REMOTE;
  }

  return 0;

error_out:
  if (acct) acct[-1] = ',';   /* restore */
  if (port) port[-1] = ':';   /* restore */
  if (flags) flags[-1] = '/'; /* restore */

  return rc;
}


#ifdef NDMOS_OPTION_USE_GETHOSTBYNAME
int ndmhost_lookup(char* hostname, struct sockaddr_in* sin)
{
  struct hostent* he;
  in_addr_t addr;

  NDMOS_MACRO_ZEROFILL(sin);
#ifdef NDMOS_OPTION_HAVE_SIN_LEN
  sin->sin_len = sizeof *sin;
#endif
  sin->sin_family = AF_INET;

  addr = inet_addr(hostname);
  if (addr != INADDR_NONE) {
    bcopy(&addr, &sin->sin_addr, 4);
  } else {
    he = gethostbyname(hostname);
    if (!he) return -1;
    bcopy(he->h_addr, &sin->sin_addr, 4);
  }

  return 0;
}
#endif

#ifdef NDMOS_OPTION_USE_GETADDRINFO
int ndmhost_lookup(char* hostname, struct sockaddr_in* sin)
{
  int res;
  struct addrinfo hints;
  struct addrinfo* ai;
  in_addr_t addr;

  NDMOS_MACRO_ZEROFILL(sin);
#ifdef NDMOS_OPTION_HAVE_SIN_LEN
  sin->sin_len = sizeof *sin;
#endif
  sin->sin_family = AF_INET;

  addr = inet_addr(hostname);
  if (addr != INADDR_NONE) {
    bcopy(&addr, &sin->sin_addr, 4);
  } else {
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    res = getaddrinfo(hostname, NULL, &hints, &ai);
    if (res != 0) { return 1; }

    bcopy(&(((struct sockaddr_in*)ai->ai_addr)->sin_addr), &sin->sin_addr, 4);
    freeaddrinfo(ai);
  }

  return 0;
}
#endif

int ndmagent_to_sockaddr_in(struct ndmagent* agent, struct sockaddr_in* sin)
{
  int rc;

  rc = ndmhost_lookup(agent->host, sin); /* inits sin */
  if (rc) return rc;

  sin->sin_port = htons(agent->port);

  return 0;
}
