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
#include "dropletp.h"

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

static dpl_status_t
addrlist_add_from_str_int(dpl_addrlist_t *addrlist,
                          char *addrlist_str);

/**
 * Lock a addrlist. This is useful to arbitrate concurrent access to the 
 * addrlist from different threads. If the context is already locked,
 * this will block until the locking succeeds. The context must then be
 * unlocked using dpl_addrlist_unlock().
 *
 * @param addrlist the addrlist to lock.
 */
void
dpl_addrlist_lock(dpl_addrlist_t *addrlist)
{
  //bizstd_backtrace();

  if (NULL == addrlist)
    return ;

  pthread_mutex_lock(&addrlist->lock);
}

/**
 * Unlock a addrlist. This must be done from the same thread that
 * locked the context.
 *
 * @sa dpl_addrlist_lock
 *
 * @param addrlist the addrlist to unlock.
 */
void
dpl_addrlist_unlock(dpl_addrlist_t *addrlist)
{
  if (NULL == addrlist)
    return ;

  pthread_mutex_unlock(&addrlist->lock);
}


/**
 *
 */
dpl_addrlist_t *dpl_addrlist_create(const char *default_port)
{
  dpl_addrlist_t *addrlist;

  addrlist = calloc(1, sizeof (dpl_addrlist_t));
  if (NULL == addrlist)
    return NULL;

  if (NULL == default_port)
    addrlist->default_port = strdup("80");
  else
    addrlist->default_port = strdup(default_port);
  if (NULL == addrlist->default_port)
    {
      free(addrlist);
      return NULL;
    }

  LIST_INIT(&addrlist->addr_list);

  pthread_mutex_init(&addrlist->lock, NULL);

  return addrlist;
}

dpl_addrlist_t *dpl_addrlist_create_from_str(const char *default_port,
                                             const char *addrlist_str)
{
  dpl_addrlist_t *addrlist;
  dpl_status_t ret;

  addrlist = dpl_addrlist_create(default_port);
  if (NULL == addrlist)
    return NULL;

  ret = dpl_addrlist_set_from_str(addrlist, addrlist_str);
  if (DPL_SUCCESS != ret)
    {
      dpl_addrlist_free(addrlist);
      return NULL;
    }

  return addrlist;
}

void dpl_addrlist_free(dpl_addrlist_t *addrlist)
{
  if (NULL == addrlist)
    return ;

  dpl_addrlist_clear_nolock(addrlist);

  pthread_mutex_destroy(&addrlist->lock);

  free(addrlist->default_port);
  free(addrlist);
}

/**
 * @brief Search a bootstrap list item associated with a given IP address
 * and port.
 *
 * @note This function does not take the addrlist lock, so use with care.
 *
 * @param addrlist a bootstrap list.
 * @param addr IP address of the item to look for.
 * @param port port used by the item to look for.
 *
 * @return a dpl_addr_t with address addr and port port, or NULL if no
 * matching item could be found in the bootstrap list. The returned object
 * is owned by addrlist and should not be freed.
 */
dpl_addr_t *
dpl_addrlist_get_byip_nolock(dpl_addrlist_t *addrlist,
                             struct in_addr ip_addr,
                             u_short port)
{
  dpl_addr_t *addr;

  if (NULL == addrlist)
    return NULL;

  LIST_FOREACH(addr, &addrlist->addr_list, list)
    {
      if (addr->addr.s_addr == ip_addr.s_addr && addr->port == port)
        return addr;
    }

  return NULL;
}

/**
 * @brief Lookup a bootstrap list item associated with a given IP address
 * and port.
 *
 * @note This function does not take the addrlist lock, so use with care.
 *
 * @param addrlist a bootstrap list.
 * @param host a string corresponding to the hostname or IP address to look for.
 * @param portstr a string containing the port number to look for.
 *
 * @return a dpl_addr_t matching the given host and port, or NULL if no
 * matching item could be found in the bootstrap list. The returned object
 * is owned by addrlist and should not be freed.
 */
dpl_addr_t *
dpl_addrlist_get_byname_nolock(dpl_addrlist_t *addrlist,
                               const char *host,
                               const char *portstr)
{
  struct hostent hret, *hresult;
  char hbuf[1024];
  int  herr;
  struct in_addr ip_addr;
  u_short port;
  int ret;

  if (NULL == addrlist)
    return NULL;

  ret = dpl_gethostbyname_r(host, &hret, hbuf, sizeof (hbuf),
                            &hresult, &herr); 
  if (0 != ret)
    {
      return NULL;
    }
  
  memcpy(&ip_addr, hresult->h_addr_list[0], hresult->h_length);
  port = atoi(portstr);

  return dpl_addrlist_get_byip_nolock(addrlist, ip_addr, port);
}

/**
 * @brief Count the number of items in a bootstrap list.
 *
 * @note This function does not take the addrlist lock, so use with care.
 * @sa dpl_addrlist_count()
 *
 * @param addrlist a bootstrap list.
 *
 * @return the number of items in the bootstrap list.
 */
u_int
dpl_addrlist_count_nolock(dpl_addrlist_t *addrlist)
{
  dpl_addr_t *addr;
  u_int count;

  if (NULL == addrlist)
    return 0;

  count = 0;
  LIST_FOREACH(addr, &addrlist->addr_list, list)
    count++;

  return count;
}

/**
 * @brief Count the number of items in a bootstrap list.
 *
 * @param addrlist a bootstrap list.
 *
 * @return the number of items in the bootstrap list.
 */
u_int
dpl_addrlist_count(dpl_addrlist_t *addrlist)
{
  u_int count;

  if (NULL == addrlist)
    return 0;

  dpl_addrlist_lock(addrlist);

  count = dpl_addrlist_count_nolock(addrlist);

  dpl_addrlist_unlock(addrlist);

  return count;
}

/**
 * @brief Count the number of items not blacklisted in a bootstrap list.
 *
 * @note This function does not take the addrlist lock, so use with care.
 *
 * @param addrlist a bootstrap list.
 *
 * @return the number of items not blacklisted in the bootstrap list.
 */
u_int
dpl_addrlist_count_avail_nolock(dpl_addrlist_t *addrlist)
{
  dpl_addr_t *addr;
  u_int count;

  if (NULL == addrlist)
    return 0;

  count = 0;
  LIST_FOREACH(addr, &addrlist->addr_list, list)
    {
      if (0 == addr->blacklist_expire_timestamp)
        count++;
    }

  return count;
}

/**
 * @brief Get the nth item not blacklisted in a bootstrap list.
 *
 * @param addrlist a bootstrap list.
 * @param n index of the item to look for starting at 0. If n is bigger than
 * theh number of elements in the bootstrap list, n modulo length of bootstrap
 * list will be looked up instead.
 * @param[out] hostp returned host name of the nth bootstrap list item, can be NULL. hostp must NOT be freed by the caller.
 * @param[out] portstrp returned port string of the nth bootstrap list item, can be NULL. portstrp must NOT be freed by the caller.
 * @param[out] addrp returned address of the nth bootstrap list item, can be NULL.
 * @param[out] portp returned port of the nth bootstrap list item, can be NULL.
 *
 * @retval DPL_ENOENT if the bootstrap list is empty, addrp and portp
 * will be unchanged in this case.
 * @retval DPL_SUCCESS if the operation was successful. portp/addrp
 * will be filled if they are non-NULL.
 */
dpl_status_t
dpl_addrlist_get_nth(dpl_addrlist_t *addrlist,
                     int n,
                     char **hostp,
                     char **portstrp,
                     struct in_addr *ip_addrp,
                     u_short *portp)
{
  int i, count;
  dpl_addr_t *addr;

  if (NULL == addrlist)
    return DPL_ENOENT;

  dpl_addrlist_lock(addrlist);

  dpl_addrlist_refresh_blacklist_nolock(addrlist);

  count = dpl_addrlist_count_avail_nolock(addrlist);

  if (0 == count)
    {
      dpl_addrlist_unlock(addrlist);
      return DPL_ENOENT;
    }

  n %= count;

  i = 0;
  LIST_FOREACH(addr, &addrlist->addr_list, list)
    {
      if (0 == addr->blacklist_expire_timestamp)
        {
          i++;
          if (i > n)
            break;
        }
    }

  DPRINTF("found %s:%d\n", inet_ntoa(addr->addr), addr->port);

  if (NULL != hostp)
    *hostp = addr->host;

  if (NULL != portstrp)
    *portstrp = addr->portstr;

  if (NULL != ip_addrp)
    *ip_addrp = addr->addr;

  if (NULL != portp)
    *portp = addr->port;

  dpl_addrlist_unlock(addrlist);

  return DPL_SUCCESS;
}

/* FIXME: improve function description */
/**
 * @brief Get a random item not blacklisted in the bootstrap list.
 *
 * @param addrlist a bootstrap list.
 * @param[out] hostp returned host name of the nth bootstrap list item, can be NULL. hostp must NOT be freed by the caller.
 * @param[out] portstrp returned port string of the nth bootstrap list item, can be NULL. portstrp must NOT be freed by the caller.
 * @param[out] addrp returned address of a random bootstrap list item, can be NULL.
 * @param[out] portp returned port of a random bootstrap list item, can be NULL.
 *
 * @retval DPL_ENOENT if the bootstrap list is empty, addrp and portp
 * will be unchanged in this case.
 * @retval DPL_SUCCESS if the operation was successful. portp/addrp
 * will be filled if they are non-NULL.
 */
dpl_status_t
dpl_addrlist_get_rand(dpl_addrlist_t *addrlist,
                      char **hostp,
                      char **portstrp,
                      struct in_addr *ip_addrp,
                      u_short *portp)
{
  return dpl_addrlist_get_nth(addrlist, dpl_rand_u32(),
                              hostp, portstrp, ip_addrp, portp);
}


/**
 * @brief Blacklist a host/port couple for the specified time.
 *
 * @param addrlist a bootstrap list.
 * @param host a string corresponding to the hostname or IP address
 * @param portstr a string containing the port number
 * @param expiretime minimum duration in seconds to wait before retrying
 * to contact the host. If -1, never retry to contact host
 * (unblacklist the item via dpl_addrlist_unblacklist())
 *
 * @retval DPL_FAILURE if the addition failed.
 * @retval DPL_ENOENT if matching host was not found in addrlist
 * @retval DPL_SUCCESS otherwise.
 */
dpl_status_t
dpl_addrlist_blacklist(dpl_addrlist_t *addrlist,
                       const char *host,
                       const char *portstr,
                       time_t expiretime)
{
  dpl_addr_t *addr;

  if (NULL == addrlist)
    return DPL_FAILURE;

  dpl_addrlist_lock(addrlist);

  addr = dpl_addrlist_get_byname_nolock(addrlist, host, portstr);
  if (NULL == addr)
    {
      dpl_addrlist_unlock(addrlist);

      return DPL_ENOENT;
    }

  if ((time_t)-1 != expiretime)
    addr->blacklist_expire_timestamp = time(0) + expiretime;
  else
    addr->blacklist_expire_timestamp = (time_t)-1;

  dpl_addrlist_unlock(addrlist);

  return DPL_SUCCESS;
}

/**
 * @brief Un-blacklist a host/port couple previously blacklisted.
 *
 * @param addrlist a bootstrap list.
 * @param host a string corresponding to the hostname or IP address
 * @param portstr a string containing the port number
 *
 * @retval DPL_FAILURE if the addition failed.
 * @retval DPL_ENOENT if matching host was not found in addrlist
 * @retval DPL_SUCCESS otherwise (if host is not already
 * blacklisted, silently succeed)
 */
dpl_status_t
dpl_addrlist_unblacklist(dpl_addrlist_t *addrlist,
                         const char *host,
                         const char *portstr)
{
  dpl_addr_t *addr;

  if (NULL == addrlist)
    return DPL_FAILURE;

  dpl_addrlist_lock(addrlist);

  addr = dpl_addrlist_get_byname_nolock(addrlist, host, portstr);
  if (NULL == addr)
    {
      dpl_addrlist_unlock(addrlist);

      return DPL_ENOENT;
    }

  addr->blacklist_expire_timestamp = 0;

  dpl_addrlist_unlock(addrlist);

  return DPL_SUCCESS;
}

/**
 * @brief Refresh the bootstrap list blacklist by un-blacklisting
 * expired entries.
 *
 * @param addrlist a bootstrap list.
 *
 * @retval DPL_FAILURE if the refresh failed.
 * @retval DPL_SUCCESS after a successful refresh
 */
dpl_status_t
dpl_addrlist_refresh_blacklist_nolock(dpl_addrlist_t *addrlist)
{
  dpl_addr_t *addr;
  time_t curtime;

  if (NULL == addrlist)
    return DPL_FAILURE;

  curtime = 0;

  LIST_FOREACH(addr, &addrlist->addr_list, list)
    {
      if (0 != addr->blacklist_expire_timestamp &&
          (time_t)-1 != addr->blacklist_expire_timestamp)
        {
          if (0 == curtime)
            curtime = time(0);

          if (addr->blacklist_expire_timestamp <= curtime)
            {
              addr->blacklist_expire_timestamp = 0;
            }
        }
    }

  return DPL_SUCCESS;
}

/**
 * @brief Add an item to a bootstrap list.
 *
 * The addrlist takes ownership of the boostrap item and will free it
 * when appropriate.
 *
 * @note This function does not take the addrlist lock, so use with care.
 *
 * @param addrlist a bootstrap list.
 * @param addr a bootstrap item to add to the addrlist.
 */
void
dpl_addrlist_add_nolock(dpl_addrlist_t *addrlist,
                        dpl_addr_t *addr)
{
  int count, n, i;
  dpl_addr_t *after;

  if (NULL == addrlist)
    return ;

  count = dpl_addrlist_count_nolock(addrlist);
  if (0 == count)
    {
      LIST_INSERT_HEAD(&addrlist->addr_list, addr, list);
      return ;
    }

  n = dpl_rand_u32() % (count + 1);
  if (0 == n)
    {
      LIST_INSERT_HEAD(&addrlist->addr_list, addr, list);
    }
  else
    {
      i = 0;
      LIST_FOREACH(after, &addrlist->addr_list, list)
        {
          i++;
          if (i >= n)
            break;
        }

      LIST_INSERT_AFTER(after, addr, list);
    }
}

/**
 * @brief Remove an item from a bootstrap list.
 *
 * @note This function does not take the addrlist lock, so use with care.
 * @note This function will remove the item from the bootstrap list it's part
 * of without checking if this is the bootstrap list of the passed in ring
 * context.
 *
 * @param addrlist a bootstrap list.
 * @param addr the bootstrap item to remove from the addrlist
 */
void
dpl_addrlist_remove_nolock(dpl_addrlist_t *addrlist,
                           dpl_addr_t *addr)
{
  if (NULL == addrlist)
    return ;

  LIST_REMOVE(addr, list);
}

/**
 * @brief Add a new host/port couple to a bootstrap list.
 *
 * @param addrlist a bootstrap list.
 * @param host a string corresponding to the hostname or IP address to add.
 * @param portstr a string containing the port number to add.
 *
 * @retval DPL_FAILURE if the addition failed.
 * @retval DPL_SUCCESS otherwise.
 * @return If the given host/port is already present in the bootstrap list,
 * nothing will be done and DPL_SUCCESS is returned.
 */
dpl_status_t
dpl_addrlist_add(dpl_addrlist_t *addrlist,
                 const char *host,
                 const char *portstr)
{
  struct hostent hret, *hresult;
  char hbuf[1024];
  int  herr;
  struct in_addr ip_addr;
  u_short port;
  int ret;
  dpl_addr_t *addr;

  if (NULL == addrlist)
    return DPL_FAILURE;

  ret = dpl_gethostbyname_r(host, &hret, hbuf, sizeof (hbuf),
                            &hresult, &herr); 
  if (0 != ret || hresult == NULL) /* workaround for CentOS: see #5748 */
    {
      DPL_LOG(NULL, DPL_ERROR, "cannot lookup host %s: %s\n",
		host, hstrerror(herr));
      return DPL_FAILURE;
    }
  
  memcpy(&ip_addr, hresult->h_addr_list[0], hresult->h_length);

  port = atoi(portstr);

  dpl_addrlist_lock(addrlist);

  addr = dpl_addrlist_get_byip_nolock(addrlist, ip_addr, port);
  if (NULL != addr)
    {
      //addr already exists, reset the timestamp and silently succeeds
      addr->blacklist_expire_timestamp = 0;
      dpl_addrlist_unlock(addrlist);
      return DPL_SUCCESS;
    }

  addr = malloc(sizeof (*addr));
  if (NULL == addr)
    {
      dpl_addrlist_unlock(addrlist);
      return DPL_FAILURE;
    }

  memset(addr, 0, sizeof (*addr));

  addr->host = strdup(host);
  addr->portstr = strdup(portstr);
  if (NULL == addr->host || NULL == addr->portstr)
    {
      dpl_addrlist_unlock(addrlist);
      free(addr->host);
      free(addr->portstr);
      free(addr);
      return DPL_FAILURE;
    }

  addr->addr.s_addr = ip_addr.s_addr;
  addr->port = port;
  addr->blacklist_expire_timestamp = 0;

  dpl_addrlist_add_nolock(addrlist, addr);

  dpl_addrlist_unlock(addrlist);

  return DPL_SUCCESS;
}

/**
 * @brief Free a bootstrap list.
 *
 * This will empty the bootstrap list associated with the given ring
 * context and free all the memory associated with this list.
 *
 * @param addrlist a bootstrap list.
 */
void
dpl_addrlist_clear_nolock(dpl_addrlist_t *addrlist)
{
  dpl_addr_t *addr, *tmp;

  if (NULL == addrlist)
    return ;

  LIST_FOREACH_SAFE(addr, &addrlist->addr_list, list, tmp)
    {
      free(addr->host);
      free(addr->portstr);
      free(addr);
    }

  LIST_INIT(&addrlist->addr_list);
}

/**
 * @brief Add new elements to a bootstrap list from a list of
 * hosts/ports.
 *
 * @param addrlist a bootstrap list.
 * @param addrlist a string containing a list of hosts and ports to use to
 * populate the bootstrap list. The list is a semicolon/comma/space
 * separated list of the form host1[:port1][; ,]...[; ,]hostn[:portn].
 *
 * @note addrlist is modified
 *
 * @retval DPL_SUCCESS on success.
 * @retval DPL_FAILURE otherwise.
 */
dpl_status_t
dpl_addrlist_add_from_str(dpl_addrlist_t *addrlist,
                          const char *addrlist_str)
{
  char *nstr;
  int ret;
  
  if (NULL == addrlist || NULL == addrlist_str)
    return DPL_FAILURE;

  nstr = strdup(addrlist_str);
  if (NULL == nstr)
    return DPL_FAILURE;

  ret = addrlist_add_from_str_int(addrlist, nstr);
  
  free(nstr);

  return ret;
}


static dpl_status_t
addrlist_add_from_str_int(dpl_addrlist_t *addrlist,
                          char *addrlist_str)
{
  char *saveptr, *str, *tok, *p;
  int ret;

  for (str = addrlist_str;;str = NULL)
    {
      tok = strtok_r(str, ";, ", &saveptr);
      if (NULL == tok)
        break ;

      DPRINTF("tok=%s\n", tok);

      p = index(tok, ':');
      if (NULL == p)
        p = addrlist->default_port;
      else
        *p++ = 0;
      
      ret = dpl_addrlist_add(addrlist, tok, p);
      if (0 != ret)
        return ret;
    }

  return DPL_SUCCESS;
}

static dpl_status_t
addrlist_set_from_str_int(dpl_addrlist_t *addrlist,
                          char *addrlist_str)
{
  dpl_addrlist_lock(addrlist);
  dpl_addrlist_clear_nolock(addrlist);
  dpl_addrlist_unlock(addrlist);

  return addrlist_add_from_str_int(addrlist, addrlist_str);
}

/**
 * @brief Initialise the bootstrap list from a list of
 * hosts/ports.
 *
 * @sa dpl_addrlist_add_from_str()
 *
 * @param addrlist a bootstrap list.
 * @param addrlist a string containing a list of hosts and ports to use to
 * populate the bootstrap list. The list is a semicolon/comma/space
 * separated list of the form host1[:port1][; ,]...[; ,]hostn[:portn].
 *
 * @note addrlist is not modified
 *
 * @retval DPL_SUCCESS on failure.
 * @retval DPL_FAILURE otherwise.
 */
dpl_status_t
dpl_addrlist_set_from_str(dpl_addrlist_t *addrlist,
                          const char *addrlist_str)
{
  char *nstr;
  int ret;
  
  if (NULL == addrlist || NULL == addrlist_str)
    return DPL_FAILURE;

  nstr = strdup(addrlist_str);
  if (NULL == nstr)
    return DPL_FAILURE;

  ret = addrlist_set_from_str_int(addrlist, nstr);
  
  free(nstr);

  return ret;
}

/**
 * @brief Get a string describing the bootstrap list.
 *
 * @param addrlist a bootstrap list.
 *
 * @return NULL on failure, an empty ("\0") string if the bootstrap list was
 * empty or a comma-separated list of host:port couples. The returned string
 * should be freed with free() when no longer used.
 */
char *
dpl_addrlist_get(dpl_addrlist_t *addrlist)
{
  char *addrlist_str = NULL;
  char *naddrlist_str;
  dpl_addr_t *addr;
  size_t size;
  int first = 1;

  size = 1;

  addrlist_str = malloc(size);
  if (NULL == addrlist_str)
    return NULL;
  addrlist_str[0] = '\0';

  if (NULL == addrlist)
    return addrlist_str;

  dpl_addrlist_lock(addrlist);

  LIST_FOREACH(addr, &addrlist->addr_list, list)
    {
      char buf[256];

      snprintf(buf, sizeof (buf), "%s:%d",
               inet_ntoa(addr->addr), addr->port);
      size += strlen(buf);
      if (!first)
        size += 1; //comma
      size++; //\0
      naddrlist_str = realloc(addrlist_str, size);
      if (NULL == naddrlist_str)
        {
          dpl_addrlist_unlock(addrlist);
          free(addrlist_str);
          return NULL;
        }
      addrlist_str = naddrlist_str;
        
      if (!first)
        strcat(addrlist_str, ",");
      strcat(addrlist_str, buf);

      first = 0;
    }

  dpl_addrlist_unlock(addrlist);

  return addrlist_str;
}

/*
** unit tests
*/

void
utest_dpl_ring_addr()
{
  dpl_addrlist_t *addrlist;
  struct in_addr ip_addr;
  u_short port;
  int i, ret;

  addrlist = dpl_addrlist_create("4244");
  if (NULL == addrlist)
    {
      fprintf(stderr, "dpl_ring addrlist creation failed\n");
      exit(1);
    }

  ret = dpl_addrlist_set_from_str(addrlist,
                                  "192.168.1.1:4242,192.168.1.2:4242,192.168.1.3");
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "addrlist_set_from_str failed\n");
      exit(1);
    }

  for (i = 0;i < 1000;i++)
    {
      ret = dpl_addrlist_get_rand(addrlist, NULL, NULL, &ip_addr, &port);
      if (DPL_SUCCESS != ret)
        {
          fprintf(stderr, "addr_get failed %d\n", ret);
          exit(1);
        }
      
      printf("%s:%d\n", inet_ntoa(ip_addr), port);
    }
  
  dpl_addrlist_free(addrlist);
}
