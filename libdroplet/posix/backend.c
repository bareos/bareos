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
#include <droplet/posix/posix.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/param.h>

#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
//#define DPRINTF(fmt,...)

dpl_status_t
dpl_posix_make_bucket(dpl_ctx_t *ctx,
                     const char *bucket,
                     const dpl_sysmd_t *sysmd,
                     char **locationp)
{
  return DPL_ENOTSUPP;
}

dpl_status_t
dpl_posix_list_bucket(dpl_ctx_t *ctx,
                     const char *bucket,
                     const char *prefix,
                     const char *delimiter,
                     dpl_vec_t **objectsp,
                     dpl_vec_t **common_prefixesp,
                     char **locationp)
{
  DIR *dir = NULL;
  dpl_status_t ret, ret2;
  int iret;
  char path[MAXPATHLEN];
  struct dirent entry, *entryp;
  dpl_vec_t     *common_prefixes = NULL;
  dpl_vec_t     *objects = NULL;
  dpl_common_prefix_t *common_prefix = NULL;
  dpl_object_t *object = NULL;

  DPRINTF("%s:%s:%s:%s\n", ctx->base_path ? ctx->base_path : "", bucket ? bucket : "", prefix ? prefix : "", delimiter);
  
  if (strcmp(delimiter, "/"))
    {
      ret = DPL_EINVAL;
      goto end;
    }

  snprintf(path, sizeof (path), "/%s/%s/%s", ctx->base_path ? ctx->base_path : "", bucket ? bucket : "", prefix ? prefix : "");

  dir = opendir(path);
  if (NULL == dir)
    {
      perror("opendir");
      ret = DPL_FAILURE;
      goto end;
    }

  objects = dpl_vec_new(2, 2);
  if (NULL == objects)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  common_prefixes = dpl_vec_new(2, 2);
  if (NULL == common_prefixes)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  while (1)
    {
      iret = readdir_r(dir, &entry, &entryp);
      if (0 != iret)
        {
          perror("readdir");
          ret = DPL_FAILURE;
          goto end;
        }
      
      if (!entryp)
        break ;

      if (entryp->d_type == DT_DIR)
        {
          //this is a directory
          common_prefix = malloc(sizeof (*common_prefix));
          if (NULL == common_prefix)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
          memset(common_prefix, 0, sizeof (*common_prefix));
          common_prefix->prefix = strdup(entryp->d_name);
          if (NULL == common_prefix->prefix)
            {
              ret = DPL_ENOMEM;
              goto end;
            }

          ret2 = dpl_vec_add(common_prefixes, common_prefix);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }

          common_prefix = NULL;
        }
      else
        {
          object = malloc(sizeof (*object));
          if (NULL == object)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
          memset(object, 0, sizeof (*object));
          object->path = strdup(entryp->d_name);
          if (NULL == object->path)
            {
              ret = DPL_ENOMEM;
              goto end;
            }

          ret2 = dpl_vec_add(objects, object);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }

          object = NULL;
        }

    }

  if (NULL != objectsp)
    {
      *objectsp = objects;
      objects = NULL; //consume it
    }

  if (NULL != common_prefixesp)
    {
      *common_prefixesp = common_prefixes;
      common_prefixes = NULL; //consume it
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != object)
    free(object);

  if (NULL != common_prefix)
    free(common_prefix);

  if (NULL != objects)
    dpl_vec_objects_free(objects);

  if (NULL != common_prefixes)
    dpl_vec_common_prefixes_free(common_prefixes);

  if (NULL != dir)
    closedir(dir);

  return ret;
}

dpl_status_t
dpl_posix_post(dpl_ctx_t *ctx,
              const char *bucket,
              const char *resource,
              const char *subresource,
              dpl_ftype_t object_type,
              const dpl_dict_t *metadata,
              const dpl_sysmd_t *sysmd,
              const char *data_buf,
              unsigned int data_len,
              const dpl_dict_t *query_params,
              char **resource_idp,
              char **locationp)
{
  return DPL_ENOTSUPP;
}

dpl_status_t
dpl_posix_post_buffered(dpl_ctx_t *ctx,
                       const char *bucket,
                       const char *resource,
                       const char *subresource,
                       dpl_ftype_t object_type,
                       const dpl_dict_t *metadata,
                       const dpl_sysmd_t *sysmd,
                       unsigned int data_len,
                       const dpl_dict_t *query_params,
                       dpl_conn_t **connp,
                       char **locationp)
{
  return DPL_ENOTSUPP;
}

dpl_status_t
dpl_posix_put(dpl_ctx_t *ctx,
             const char *bucket,
             const char *resource,
             const char *subresource,
             dpl_ftype_t object_type,
             const dpl_dict_t *metadata,
             const dpl_sysmd_t *sysmd,
             const char *data_buf,
             unsigned int data_len,
             char **locationp)
{
  return DPL_ENOTSUPP;
}

dpl_status_t
dpl_posix_put_buffered(dpl_ctx_t *ctx,
                      const char *bucket,
                      const char *resource,
                      const char *subresource,
                      dpl_ftype_t object_type,
                      const dpl_dict_t *metadata,
                      const dpl_sysmd_t *sysmd,
                      unsigned int data_len,
                      dpl_conn_t **connp,
                      char **locationp)
{
  return DPL_ENOTSUPP;
}

dpl_status_t
dpl_posix_get(dpl_ctx_t *ctx,
             const char *bucket,
             const char *resource,
             const char *subresource,
             dpl_ftype_t object_type,
             const dpl_condition_t *condition,
             char **data_bufp,
             unsigned int *data_lenp,
             dpl_dict_t **metadatap,
             dpl_sysmd_t *sysmdp,
             char **locationp)
{
  return DPL_ENOTSUPP;
}

dpl_status_t
dpl_posix_get_range(dpl_ctx_t *ctx,
                   const char *bucket,
                   const char *resource,
                   const char *subresource,
                   dpl_ftype_t object_type,
                   const dpl_condition_t *condition,
                   int start,
                   int end,
                   char **data_bufp,
                   unsigned int *data_lenp,
                   dpl_dict_t **metadatap,
                   dpl_sysmd_t *sysmdp,
                   char **locationp)
{
  return DPL_ENOTSUPP;
}

dpl_status_t
dpl_posix_get_buffered(dpl_ctx_t *ctx,
                      const char *bucket,
                      const char *resource,
                      const char *subresource,
                      dpl_ftype_t object_type,
                      const dpl_condition_t *condition,
                      dpl_header_func_t header_func,
                      dpl_buffer_func_t buffer_func,
                      void *cb_arg,
                      char **locationp)
{
  return DPL_ENOTSUPP;
}

dpl_status_t
dpl_posix_head_all(dpl_ctx_t *ctx,
                  const char *bucket,
                  const char *resource,
                  const char *subresource,
                  dpl_ftype_t object_type,
                  const dpl_condition_t *condition,
                  dpl_dict_t **metadatap,
                  char **locationp)
{
  return DPL_ENOTSUPP;
}

dpl_status_t
dpl_posix_head(dpl_ctx_t *ctx,
              const char *bucket,
              const char *resource,
              const char *subresource,
              dpl_ftype_t object_type,
              const dpl_condition_t *condition,
              dpl_dict_t **metadatap,
              dpl_sysmd_t *sysmdp,
              char **locationp)
{
  return DPL_ENOTSUPP;
}

dpl_status_t
dpl_posix_delete(dpl_ctx_t *ctx,
                const char *bucket,
                const char *resource,
                const char *subresource,
                dpl_ftype_t object_type,
                char **locationp)
{
  return DPL_ENOTSUPP;
}

dpl_status_t
dpl_posix_copy(dpl_ctx_t *ctx,
              const char *src_bucket,
              const char *src_resource,
              const char *src_subresource,
              const char *dst_bucket,
              const char *dst_resource,
              const char *dst_subresource,
              dpl_ftype_t object_type,
              dpl_copy_directive_t copy_directive,
              const dpl_dict_t *metadata,
              const dpl_sysmd_t *sysmd,
              const dpl_condition_t *condition,
              char **locationp)
{
  return DPL_ENOTSUPP;
}
