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
 *
 */


#include "ndmagents.h"

/*
 * Env list mgmt.
 *
 * Return a chunk of memory with all entries from the envlist as
 * one big enumeration useable for rpc to use as return value.
 * We allacate the memory and keep the pointer in the table handle
 * which gets freed on destroy of the table.
 */
ndmp9_pval* ndma_enumerate_env_list(struct ndm_env_table* envtab)
{
  int i;
  struct ndm_env_entry* entry;

  /*
   * See if we need to allocate memory or can reuse the memory
   * already allocated in an earlier call.
   */
  if (!envtab->enumerate) {
    envtab->enumerate = NDMOS_API_MALLOC(sizeof(ndmp9_pval) * envtab->n_env);
    envtab->enumerate_length = envtab->n_env;
  } else if (envtab->enumerate_length != envtab->n_env) {
    NDMOS_API_FREE(envtab->enumerate);
    envtab->enumerate = NDMOS_API_MALLOC(sizeof(ndmp9_pval) * envtab->n_env);
    envtab->enumerate_length = envtab->n_env;
  }

  if (!envtab->enumerate) { return NULL; }
  NDMOS_API_BZERO(envtab->enumerate, sizeof(ndmp9_pval) * envtab->n_env);

  i = 0;
  for (entry = envtab->head; entry; entry = entry->next) {
    memcpy(&envtab->enumerate[i], &entry->pval, sizeof(ndmp9_pval));
    i++;
  }

  return envtab->enumerate;
}

/*
 * Add a new entry to an environment list table.
 * Return entry if caller wants to modify it.
 */
struct ndm_env_entry* ndma_store_env_list(struct ndm_env_table* envtab,
                                          ndmp9_pval* pv)
{
  struct ndm_env_entry* entry;

  if (envtab->n_env >= NDM_MAX_ENV) return NULL;

  entry = NDMOS_API_MALLOC(sizeof(struct ndm_env_entry));
  if (!entry) return NULL;

  entry->pval.name = NDMOS_API_STRDUP(pv->name);
  if (!entry->pval.name) {
    NDMOS_API_FREE(entry);
    return NULL;
  }

  entry->pval.value = NDMOS_API_STRDUP(pv->value);
  if (!entry->pval.value) {
    NDMOS_API_FREE(entry->pval.name);
    NDMOS_API_FREE(entry);
    return NULL;
  }

  entry->next = NULL;
  if (envtab->tail) {
    envtab->tail->next = entry;
    envtab->tail = entry;
  } else {
    envtab->head = entry;
    envtab->tail = entry;
  }

  envtab->n_env++;

  return entry;
}

/*
 * Update an entry in an environment list table.
 * If it doesn't exist add a new entry.
 * Return entry if caller want to modify it.
 */
struct ndm_env_entry* ndma_update_env_list(struct ndm_env_table* envtab,
                                           ndmp9_pval* pv)
{
  struct ndm_env_entry* entry;

  for (entry = envtab->head; entry; entry = entry->next) {
    if (strcmp(entry->pval.name, pv->name) == 0) {
      NDMOS_API_FREE(entry->pval.value);
      entry->pval.value = NDMOS_API_STRDUP(pv->value);
      return entry;
    }
  }

  return ndma_store_env_list(envtab, pv);
}

/*
 * Destroy an environment list table including any
 * enumerate buffers allocated for it.
 */
void ndma_destroy_env_list(struct ndm_env_table* envtab)
{
  struct ndm_env_entry* entry;
  struct ndm_env_entry* next;

  for (entry = envtab->head; entry; entry = next) {
    if (entry->pval.name) NDMOS_API_FREE(entry->pval.name);

    if (entry->pval.value) NDMOS_API_FREE(entry->pval.value);

    next = entry->next;
    NDMOS_API_FREE(entry);
  }

  if (envtab->enumerate) {
    NDMOS_API_FREE(envtab->enumerate);
    envtab->enumerate = NULL;
    envtab->enumerate_length = 0;
  }

  envtab->head = NULL;
  envtab->tail = NULL;
  envtab->n_env = 0;
}

/*
 * Nlist mgmt.
 *
 * Return a chunk of memory with all entries from the nlist as
 * one big enumeration useable for rpc to use as return value.
 * We allacate the memory and keep the pointer in the table handle
 * which gets freed on destroy of the table.
 */
ndmp9_name* ndma_enumerate_nlist(struct ndm_nlist_table* nlist)
{
  int i;
  struct ndm_nlist_entry* entry;

  /*
   * See if we need to allocate memory or can reuse the memory
   * already allocated in an earlier call.
   */
  if (!nlist->enumerate) {
    nlist->enumerate = NDMOS_API_MALLOC(sizeof(ndmp9_name) * nlist->n_nlist);
    nlist->enumerate_length = nlist->n_nlist;
  } else if (nlist->enumerate_length != nlist->n_nlist) {
    NDMOS_API_FREE(nlist->enumerate);
    nlist->enumerate = NDMOS_API_MALLOC(sizeof(ndmp9_name) * nlist->n_nlist);
    nlist->enumerate_length = nlist->n_nlist;
  }

  if (!nlist->enumerate) { return NULL; }
  NDMOS_API_BZERO(nlist->enumerate, sizeof(ndmp9_name) * nlist->n_nlist);

  i = 0;
  for (entry = nlist->head; entry; entry = entry->next) {
    memcpy(&nlist->enumerate[i], &entry->name, sizeof(ndmp9_name));
    i++;
  }

  return nlist->enumerate;
}

/*
 * Add a new entry to a nlist list table.
 * Return entry if caller want to modify it.
 */
struct ndm_nlist_entry* ndma_store_nlist(struct ndm_nlist_table* nlist,
                                         ndmp9_name* nl)
{
  struct ndm_nlist_entry* entry;

  if (nlist->n_nlist >= NDM_MAX_NLIST) return NULL;

  entry = NDMOS_API_MALLOC(sizeof(struct ndm_nlist_entry));
  if (!entry) return NULL;

  NDMOS_MACRO_ZEROFILL(entry);

  entry->name.original_path = NDMOS_API_STRDUP(nl->original_path);
  if (!entry->name.original_path) goto bail_out;

  entry->name.destination_path = NDMOS_API_STRDUP(nl->destination_path);
  if (!entry->name.destination_path) goto bail_out;

  entry->name.name = NDMOS_API_STRDUP(nl->name);
  if (!entry->name.name) goto bail_out;

  entry->name.other_name = NDMOS_API_STRDUP(nl->other_name);
  if (!entry->name.other_name) goto bail_out;

  entry->name.node = nl->node;
  entry->name.fh_info = nl->fh_info;
  entry->result_err = NDMP9_UNDEFINED_ERR;
  entry->result_count = 0;

  entry->next = NULL;
  if (nlist->tail) {
    nlist->tail->next = entry;
    nlist->tail = entry;
  } else {
    nlist->head = entry;
    nlist->tail = entry;
  }

  nlist->n_nlist++;

  return entry;

bail_out:
  if (entry->name.other_name) NDMOS_API_FREE(entry->name.other_name);

  if (entry->name.name) NDMOS_API_FREE(entry->name.name);

  if (entry->name.destination_path)
    NDMOS_API_FREE(entry->name.destination_path);

  if (entry->name.original_path) NDMOS_API_FREE(entry->name.original_path);

  NDMOS_API_FREE(entry);

  return NULL;
}

/*
 * Destroy a nlist list table including any
 * enumerate buffers allocated for it.
 */
void ndma_destroy_nlist(struct ndm_nlist_table* nlist)
{
  struct ndm_nlist_entry* entry;
  struct ndm_nlist_entry* next;

  for (entry = nlist->head; entry; entry = next) {
    if (entry->name.original_path) NDMOS_API_FREE(entry->name.original_path);

    if (entry->name.destination_path)
      NDMOS_API_FREE(entry->name.destination_path);

    next = entry->next;
    NDMOS_API_FREE(entry);
  }

  if (nlist->enumerate) {
    NDMOS_API_FREE(nlist->enumerate);
    nlist->enumerate = NULL;
    nlist->enumerate_length = 0;
  }

  nlist->head = NULL;
  nlist->tail = NULL;
  nlist->n_nlist = 0;
}

/*
 * Media list mgmt.
 *
 * Create a new media entry and add it to the Media Table.
 * Return entry if caller want to modify it.
 */
struct ndmmedia* ndma_store_media(struct ndm_media_table* mtab,
                                  uint16_t element_address)
{
  struct ndmmedia* me;

  if (mtab->n_media >= NDM_MAX_MEDIA) return NULL;

  me = NDMOS_API_MALLOC(sizeof(struct ndmmedia));
  if (!me) { return NULL; }

  NDMOS_MACRO_ZEROFILL(me);

  me->valid_slot = 1;
  me->slot_addr = element_address;
  me->index = mtab->n_media + 1;

  me->next = NULL;
  if (mtab->tail) {
    mtab->tail->next = me;
    mtab->tail = me;
  } else {
    mtab->head = me;
    mtab->tail = me;
  }

  mtab->n_media++;

  return me;
}

/*
 * Clone an existing media entry and add it to the Media Table.
 * Return entry if caller want to modify it.
 */
struct ndmmedia* ndma_clone_media_entry(struct ndm_media_table* mtab,
                                        struct ndmmedia* to_clone)
{
  struct ndmmedia* me;

  if (mtab->n_media >= NDM_MAX_MEDIA) return NULL;

  me = NDMOS_API_MALLOC(sizeof(struct ndmmedia));
  if (!me) { return NULL; }

  memcpy(me, to_clone, sizeof(struct ndmmedia));
  me->index = mtab->n_media + 1;

  me->next = NULL;
  if (mtab->tail) {
    mtab->tail->next = me;
    mtab->tail = me;
  } else {
    mtab->head = me;
    mtab->tail = me;
  }

  mtab->n_media++;

  return me;
}

/*
 * Destroy a Media Table.
 */
void ndmca_destroy_media_table(struct ndm_media_table* mtab)
{
  struct ndmmedia *me, *next;

  for (me = mtab->head; me; me = next) {
    next = me->next;

    NDMOS_API_FREE(me);
  }

  mtab->head = NULL;
  mtab->tail = NULL;
  mtab->n_media = 0;
}
