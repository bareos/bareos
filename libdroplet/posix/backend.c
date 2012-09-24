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
#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <attr/xattr.h>
#include <utime.h>
#include <pwd.h>
#include <grp.h>

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
  char buf[MAXPATHLEN];

  if (strcmp(delimiter, "/"))
    {
      ret = DPL_EINVAL;
      goto end;
    }

  snprintf(path, sizeof (path), "/%s/%s", ctx->base_path ? ctx->base_path : "", prefix ? prefix : "");

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

      if (!strcmp(entryp->d_name, ".") ||
          !strcmp(entryp->d_name, ".."))
        continue ;

      if (entryp->d_type == DT_DIR)
        {
          //this is a directory
          snprintf(buf, sizeof (buf), "%s/", entryp->d_name);
          common_prefix = malloc(sizeof (*common_prefix));
          if (NULL == common_prefix)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
          memset(common_prefix, 0, sizeof (*common_prefix));
          common_prefix->prefix = strdup(buf);
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

static dpl_status_t
do_create(dpl_ftype_t object_type,
          char *path,
          mode_t mode)
{
  dpl_status_t ret;
  int iret;

  switch (object_type)
    {
    case DPL_FTYPE_UNDEF:
    case DPL_FTYPE_ANY:
    case DPL_FTYPE_CAP:
    case DPL_FTYPE_DOM:
      ret = DPL_EINVAL;
      goto end;
    case DPL_FTYPE_DIR:
      iret = mkdir(path, 0700);
      break ;
    case DPL_FTYPE_REG:
      iret = mknod(path, S_IFREG|0600, -1);
      break ;
    }

  if (-1 == iret)
    {
      perror("mknod");
      ret = DPL_FAILURE;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

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
  dpl_status_t ret, ret2;
  int iret;
  char path[MAXPATHLEN];
  char id[256];
  struct stat st;
  char *native_id = NULL;

  snprintf(path, sizeof (path), "/%s/%s", ctx->base_path ? ctx->base_path : "", resource);

  ret2 = do_create(object_type, path, 0700);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  iret = stat(path, &st);
  if (-1 == iret)
    {
      perror("stat");
      ret = DPL_FAILURE;
      goto end;
    }

  snprintf(id, sizeof (id), "%lu", st.st_ino);

  native_id = strdup(id);
  if (NULL == native_id)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (NULL != resource_idp)
    {
      *resource_idp = native_id;
      native_id = NULL;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != native_id)
    free(native_id);

  return ret;
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
  dpl_conn_t *conn = NULL;
  dpl_status_t ret;
  char path[MAXPATHLEN];

  snprintf(path, sizeof (path), "/%s/%s", ctx->base_path ? ctx->base_path : "", resource);

  conn = dpl_conn_open_file(ctx, path, 0700);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (NULL != connp)
    {
      *connp = conn;
      conn = NULL;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != conn)
    dpl_conn_release(conn);

  return ret;
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
  dpl_status_t ret, ret2;
  char path[MAXPATHLEN];

  snprintf(path, sizeof (path), "/%s/%s", ctx->base_path ? ctx->base_path : "", resource);

  ret2 = do_create(object_type, path, 0700);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  return ret;
}

dpl_status_t
dpl_posix_put_range(dpl_ctx_t *ctx,
                    const char *bucket,
                    const char *resource,
                    const char *subresource,
                    dpl_ftype_t object_type,
                    const dpl_range_t *range,
                    const dpl_dict_t *metadata,
                    const dpl_sysmd_t *sysmd,
                    const char *data_buf,
                    unsigned int data_len,
                    char **locationp)
{
  dpl_status_t ret, ret2;
  char path[MAXPATHLEN];

  snprintf(path, sizeof (path), "/%s/%s", ctx->base_path ? ctx->base_path : "", resource);

  ret2 = do_create(object_type, path, 0700);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  return ret;
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
dpl_posix_put_range_buffered(dpl_ctx_t *ctx,
                             const char *bucket,
                             const char *resource,
                             const char *subresource,
                             dpl_ftype_t object_type,
                             const dpl_range_t *range,
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
                    const dpl_range_t *range,
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
dpl_posix_get_range_buffered(dpl_ctx_t *ctx,
                             const char *bucket,
                             const char *resource,
                             const char *subresource,
                             dpl_ftype_t object_type,
                             const dpl_condition_t *condition,
                             const dpl_range_t *range, 
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
  dpl_status_t ret, ret2;
  char path[MAXPATHLEN];
  int iret;
  struct stat st;
  char buf[256];
  dpl_dict_t *metadata = NULL;

  snprintf(path, sizeof (path), "/%s/%s", ctx->base_path ? ctx->base_path : "", resource);

  iret = stat(path, &st);
  if (-1 == iret)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  metadata = dpl_dict_new(13);
  if (NULL == metadata)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  snprintf(buf, sizeof (buf), "%ld", st.st_dev);
  ret2 = dpl_dict_add(metadata, "dev", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(buf, sizeof (buf), "%ld", st.st_ino);
  ret2 = dpl_dict_add(metadata, "ino", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }
  
  snprintf(buf, sizeof (buf), "%u", st.st_mode);
  ret2 = dpl_dict_add(metadata, "mode", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(buf, sizeof (buf), "%ld", st.st_nlink);
  ret2 = dpl_dict_add(metadata, "nlink", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(buf, sizeof (buf), "%u", st.st_uid);
  ret2 = dpl_dict_add(metadata, "uid", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(buf, sizeof (buf), "%u", st.st_gid);
  ret2 = dpl_dict_add(metadata, "gid", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(buf, sizeof (buf), "%lu", st.st_rdev);
  ret2 = dpl_dict_add(metadata, "rdev", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(buf, sizeof (buf), "%lu", st.st_size);
  ret2 = dpl_dict_add(metadata, "size", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(buf, sizeof (buf), "%lu", st.st_blksize);
  ret2 = dpl_dict_add(metadata, "blksize", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(buf, sizeof (buf), "%lu", st.st_blocks);
  ret2 = dpl_dict_add(metadata, "blocks", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(buf, sizeof (buf), "%lu", st.st_atime);
  ret2 = dpl_dict_add(metadata, "atime", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(buf, sizeof (buf), "%lu", st.st_mtime);
  ret2 = dpl_dict_add(metadata, "mtime", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(buf, sizeof (buf), "%lu", st.st_ctime);
  ret2 = dpl_dict_add(metadata, "ctime", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != metadatap)
    {
      *metadatap = metadata;
      metadata = NULL;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != metadata)
    dpl_dict_free(metadata);

  return ret;
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
  dpl_status_t ret, ret2;
  dpl_dict_t *sys_metadata = NULL;
  dpl_dict_t *metadata = NULL;
  ssize_t ssize_ret;
  char path[MAXPATHLEN];
  char xattr[64*1024];
  dpl_dict_var_t *var;
  int iret;
  char buf[256];

  snprintf(path, sizeof (path), "/%s/%s", ctx->base_path ? ctx->base_path : "", resource);

  ret2 = dpl_posix_head_all(ctx, bucket, resource, subresource, 
                            object_type, condition, &sys_metadata, locationp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (sysmdp)
    {
      var = dpl_dict_get(sys_metadata, "atime");
      if (var)
        {
          assert(var->val->type == DPL_VALUE_STRING);
          sysmdp->atime = strtoul(var->val->string, NULL, 0);
          sysmdp->mask |= DPL_SYSMD_MASK_ATIME;
        }

      var = dpl_dict_get(sys_metadata, "mtime");
      if (var)
        {
          assert(var->val->type == DPL_VALUE_STRING);
          sysmdp->mtime = strtoul(var->val->string, NULL, 0);
          sysmdp->mask |= DPL_SYSMD_MASK_MTIME;
        }

      var = dpl_dict_get(sys_metadata, "ctime");
      if (var)
        {
          assert(var->val->type == DPL_VALUE_STRING);
          sysmdp->ctime = strtoul(var->val->string, NULL, 0);
          sysmdp->mask |= DPL_SYSMD_MASK_CTIME;
        }

      var = dpl_dict_get(sys_metadata, "size");
      if (var)
        {
          assert(var->val->type == DPL_VALUE_STRING);
          sysmdp->size = strtoul(var->val->string, NULL, 0);
          sysmdp->mask |= DPL_SYSMD_MASK_SIZE;
        }

      var = dpl_dict_get(sys_metadata, "uid");
      if (var)
        {
          uid_t uid;
          struct passwd pwd, *pwdp;

          assert(var->val->type == DPL_VALUE_STRING);
          uid = atoi(var->val->string);
          iret = getpwuid_r(uid, &pwd, buf, sizeof (buf), &pwdp);
          if (iret == -1)
            {
              perror("getpwuid");
              ret = DPL_FAILURE;
              goto end;
            }
          snprintf(sysmdp->owner, sizeof (sysmdp->owner), "%s", pwdp->pw_name);
          sysmdp->mask |= DPL_SYSMD_MASK_OWNER;
        }

      var = dpl_dict_get(sys_metadata, "gid");
      if (var)
        {
          gid_t gid;
          struct group grp, *grpp;

          assert(var->val->type == DPL_VALUE_STRING);
          gid = atoi(var->val->string);
          iret = getgrgid_r(gid, &grp, buf, sizeof (buf), &grpp);
          if (iret == -1)
            {
              perror("getgrgid");
              ret = DPL_FAILURE;
              goto end;
            }
          snprintf(sysmdp->group, sizeof (sysmdp->group), "%s", grpp->gr_name);
          sysmdp->mask |= DPL_SYSMD_MASK_GROUP;
        }
    }

  ssize_ret = llistxattr(path, xattr, sizeof (xattr));
  if (-1 == ssize_ret)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  metadata = dpl_dict_new(13);
  if (NULL == metadata)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (NULL != metadatap)
    {
      *metadatap = metadata;
      metadata = NULL;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != sys_metadata)
    dpl_dict_free(sys_metadata);

  if (NULL != metadata)
    dpl_dict_free(metadata);

  return ret;
}

dpl_status_t
dpl_posix_delete(dpl_ctx_t *ctx,
                const char *bucket,
                const char *resource,
                const char *subresource,
                dpl_ftype_t object_type,
                char **locationp)
{
  dpl_status_t ret;
  char path[MAXPATHLEN];
  int iret;

  snprintf(path, sizeof (path), "/%s/%s", ctx->base_path ? ctx->base_path : "", resource);

  switch (object_type)
    {
    case DPL_FTYPE_UNDEF:
      ret = DPL_ENOTSUPP;
      goto end;
    case DPL_FTYPE_ANY:
      ret = DPL_ENOTSUPP;
      goto end;
    case DPL_FTYPE_REG:
      iret = unlink(path);
      if (-1 == iret)
        {
          perror("unlink");
          ret = DPL_FAILURE;
          goto end;
        }
      goto end;
    case DPL_FTYPE_DIR:
      iret = rmdir(path);
      if (-1 == iret)
        {
          perror("rmdir");
          ret = DPL_FAILURE;
          goto end;
        }
      goto end;
    case DPL_FTYPE_CAP:
      ret = DPL_ENOTSUPP;
      goto end;
    case DPL_FTYPE_DOM:
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  return ret;
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
  dpl_status_t ret;//, ret2;
  int iret;
  char src_path[MAXPATHLEN];
  char dst_path[MAXPATHLEN];

  snprintf(src_path, sizeof (src_path), "/%s/%s", ctx->base_path ? ctx->base_path : "", src_resource);
  snprintf(dst_path, sizeof (src_path), "/%s/%s", ctx->base_path ? ctx->base_path : "", src_resource);

  switch (copy_directive)
    {
    case DPL_COPY_DIRECTIVE_UNDEF:
      break ;
    case DPL_COPY_DIRECTIVE_COPY:
      ret = DPL_ENOTSUPP;
      goto end;
    case DPL_COPY_DIRECTIVE_METADATA_REPLACE:
      if (strcmp(src_resource, dst_resource))
        {
          ret = DPL_EINVAL;
          goto end;
        }
      switch (sysmd->mask)
        {
        case DPL_SYSMD_MASK_SIZE:
          iret = truncate(src_path, sysmd->size);
          if (-1 == iret)
            {
              perror("truncate");
              ret = DPL_FAILURE;
              goto end;
            }
          break ;
        case DPL_SYSMD_MASK_CANNED_ACL:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_STORAGE_CLASS:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_ATIME:
        case DPL_SYSMD_MASK_MTIME:
          {
            struct utimbuf times;

            times.actime = sysmd->atime;
            times.modtime = sysmd->mtime;
            iret = utime(src_path, &times);
            if (-1 == iret)
              {
                perror("utime");
                ret = DPL_FAILURE;
                goto end;
              }
          }
          break ;
        case DPL_SYSMD_MASK_CTIME:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_ETAG:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_LOCATION_CONSTRAINT:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_OWNER:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_GROUP:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_ACL:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_ID:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_PARENT_ID:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_FTYPE:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_ENTERPRISE_NUMBER:
          ret = DPL_ENOTSUPP;
          goto end;
        }
      break ;
    case DPL_COPY_DIRECTIVE_LINK:
      iret = link(src_path, dst_path);
      if (-1 == iret)
        {
          perror("link");
          ret = DPL_FAILURE;
          goto end;
        }
      break ;
    case DPL_COPY_DIRECTIVE_SYMLINK:
      iret = symlink(src_path, dst_path);
      if (-1 == iret)
        {
          perror("symlink");
          ret = DPL_FAILURE;
          goto end;
        }
      break ;
    case DPL_COPY_DIRECTIVE_MOVE:
      iret = rename(src_path, dst_path);
      if (-1 == iret)
        {
          perror("rename");
          ret = DPL_FAILURE;
          goto end;
        }
      break ;
    case DPL_COPY_DIRECTIVE_MKDENT:
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  return ret;
}
