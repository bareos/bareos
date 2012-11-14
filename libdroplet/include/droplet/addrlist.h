/*
 * Copyright (C) 2010 SCALITY SA. All rights reserved.
 * http://www.scality.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SCALITY SA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SCALITY SA OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of SCALITY SA.
 *
 * https://github.com/scality/Droplet
 */
#ifndef __DPL_ADDRLIST_H__
#define __DPL_ADDRLIST_H__ 1

typedef struct dpl_addr
{
  char           *host;
  char           *portstr;
  struct in_addr addr;
  u_short        port;
  time_t         blacklist_expire_timestamp;

  LIST_ENTRY(dpl_addr) list;
} dpl_addr_t;

typedef struct dpl_addrlist
{
  LIST_HEAD(dpl_addrs, dpl_addr) addr_list;
  char             *default_port;
  pthread_mutex_t  lock;
} dpl_addrlist_t;

void dpl_addrlist_lock(dpl_addrlist_t *addrlist);
void dpl_addrlist_unlock(dpl_addrlist_t *addrlist);
dpl_addrlist_t *dpl_addrlist_create(const char *default_port);
dpl_addrlist_t *dpl_addrlist_create_from_str(const char *default_port, const char *addrlist_str);
void dpl_addrlist_free(dpl_addrlist_t *addrlist);
dpl_addr_t *dpl_addrlist_get_byip_nolock(dpl_addrlist_t *addrlist, struct in_addr ip_addr, u_short port);
dpl_addr_t *dpl_addrlist_get_byname_nolock(dpl_addrlist_t *addrlist, const char *host, const char *portstr);
u_int dpl_addrlist_count_nolock(dpl_addrlist_t *addrlist);
u_int dpl_addrlist_count(dpl_addrlist_t *addrlist);
u_int dpl_addrlist_count_avail_nolock(dpl_addrlist_t *addrlist);
dpl_status_t dpl_addrlist_get_nth(dpl_addrlist_t *addrlist, int n, char **hostp, char **portstrp, struct in_addr *ip_addrp, u_short *portp);
dpl_status_t dpl_addrlist_get_rand(dpl_addrlist_t *addrlist, char **hostp, char **portstrp, struct in_addr *ip_addrp, u_short *portp);
dpl_status_t dpl_addrlist_blacklist(dpl_addrlist_t *addrlist, const char *host, const char *portstr, time_t expiretime);
dpl_status_t dpl_addrlist_unblacklist(dpl_addrlist_t *addrlist, const char *host, const char *portstr);
dpl_status_t dpl_addrlist_refresh_blacklist_nolock(dpl_addrlist_t *addrlist);
void dpl_addrlist_add_nolock(dpl_addrlist_t *addrlist, dpl_addr_t *addr);
void dpl_addrlist_remove_nolock(dpl_addrlist_t *addrlist, dpl_addr_t *addr);
dpl_status_t dpl_addrlist_add(dpl_addrlist_t *addrlist, const char *host, const char *portstr);
void dpl_addrlist_clear_nolock(dpl_addrlist_t *addrlist);
dpl_status_t dpl_addrlist_add_from_str(dpl_addrlist_t *addrlist, const char *addrlist_str);
dpl_status_t dpl_addrlist_set_from_str(dpl_addrlist_t *addrlist, const char *addrlist_str);
char *dpl_addrlist_get(dpl_addrlist_t *addrlist);
void utest_addrlist(void);

#endif
