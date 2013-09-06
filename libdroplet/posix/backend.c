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
#include <linux/xattr.h>
#include <attr/xattr.h>
#include <utime.h>
#include <pwd.h>
#include <grp.h>

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_status_t
dpl_posix_get_capabilities(dpl_ctx_t *ctx,
                           dpl_capability_t *maskp)
{
  if (NULL != maskp)
    *maskp = DPL_CAP_FNAMES;
  
  return DPL_SUCCESS;
}

dpl_status_t
dpl_posix_list_bucket(dpl_ctx_t *ctx,
                      const char *bucket,
                      const char *prefix,
                      const char *delimiter,
                      const int max_keys,
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

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

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

      DPL_TRACE(ctx, DPL_TRACE_BACKEND, "%s", entryp->d_name);

      if (entryp->d_type == DT_DIR)
        {
          //this is a directory
          snprintf(buf, sizeof (buf), "%s%s/", prefix ? prefix : "", entryp->d_name);
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
          snprintf(buf, sizeof (buf), "%s%s", prefix ? prefix : "", entryp->d_name);
          object = malloc(sizeof (*object));
          if (NULL == object)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
          memset(object, 0, sizeof (*object));
          object->path = strdup(buf);
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

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_posix_put(dpl_ctx_t *ctx,
              const char *bucket,
              const char *resource,
              const char *subresource,
              const dpl_option_t *option, 
              dpl_ftype_t object_type,
              const dpl_condition_t *condition,
              const dpl_range_t *range,
              const dpl_dict_t *metadata,
              const dpl_sysmd_t *sysmd,
              const char *data_buf,
              unsigned int data_len,
              const dpl_dict_t *query_params, 
              dpl_sysmd_t *returned_sysmdp,
              char **locationp)
{
  dpl_status_t ret, ret2;
  int iret;
  char path[MAXPATHLEN];
  ssize_t cc;
  int fd = -1;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  snprintf(path, sizeof (path), "/%s/%s", ctx->base_path ? ctx->base_path : "", resource);

  switch (object_type)
    {
    case DPL_FTYPE_UNDEF:
    case DPL_FTYPE_ANY:
    case DPL_FTYPE_CAP:
    case DPL_FTYPE_DOM:
    case DPL_FTYPE_CHRDEV:
    case DPL_FTYPE_BLKDEV:
    case DPL_FTYPE_FIFO:
    case DPL_FTYPE_SOCKET:
    case DPL_FTYPE_SYMLINK:
      ret = DPL_EINVAL;
      goto end;
    case DPL_FTYPE_DIR:
      iret = mkdir(path, 0700);
      if (-1 == iret)
        {
          if (ENOENT == errno)
            {
              ret = DPL_ENOENT;
            }
          else
            {
              perror("mkdir");
              ret = DPL_FAILURE;
            }
          goto end;
        }
      break ;
    case DPL_FTYPE_REG:
      fd = creat(path, 0600);
      if (-1 == fd)
        {
          if (ENOENT == errno)
            {
              ret = DPL_ENOENT;
            }
          else
            {
              perror("creat");
              ret = DPL_FAILURE;
            }
          goto end;
        }
      break ;
    }

  if (DPL_FTYPE_REG == object_type)
    {
      uint64_t offset, length;

      if (range)
        {
          int range_len;

          offset = range->start;
          range_len = range->start - range->end;
          if (data_len > range_len)
            {
              ret = DPL_EINVAL;
              goto end;
            }

          length = data_len;
        }
      else
        {
          offset = 0;
          length = data_len;
        }

      iret = ftruncate(fd, offset + length);
      if (-1 == iret)
        {
          ret = DPL_FAILURE;
          goto end;
        }

      cc = pwrite(fd, data_buf, length, offset);
      if (-1 == cc)
        {
          ret = DPL_FAILURE;
          goto end;
        }
      
      if (data_len != cc)
        {
          ret = DPL_FAILURE;
          goto end;
        }
    }

  ret2 = dpl_posix_setattr(path, metadata, sysmd);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (-1 != fd)
    close(fd);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_posix_get(dpl_ctx_t *ctx,
              const char *bucket,
              const char *resource,
              const char *subresource,
              const dpl_option_t *option, 
              dpl_ftype_t object_type,
              const dpl_condition_t *condition,
              const dpl_range_t *range,
              char **data_bufp,
              unsigned int *data_lenp,
              dpl_dict_t **metadatap,
              dpl_sysmd_t *sysmdp,
              char **locationp)
{
  dpl_status_t ret;
  int iret;
  char path[MAXPATHLEN];
  ssize_t cc;
  int fd = -1;
  uint64_t offset, length;
  struct stat st;
  u_int data_len;
  char *data_buf = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  snprintf(path, sizeof (path), "/%s/%s", ctx->base_path ? ctx->base_path : "", resource);

  switch (object_type)
    {
    case DPL_FTYPE_UNDEF:
    case DPL_FTYPE_CAP:
    case DPL_FTYPE_DIR:
    case DPL_FTYPE_DOM:
    case DPL_FTYPE_CHRDEV:
    case DPL_FTYPE_BLKDEV:
    case DPL_FTYPE_FIFO:
    case DPL_FTYPE_SOCKET:
    case DPL_FTYPE_SYMLINK:
      ret = DPL_EINVAL;
      goto end;
    case DPL_FTYPE_ANY:
    case DPL_FTYPE_REG:
      fd = open(path, O_RDONLY);
      if (-1 == fd)
        {
          perror("open");
          ret = DPL_FAILURE;
          goto end;
        }
      break ;
    }

  iret = fstat(fd, &st);
  if (-1 == iret)
    {
      perror("fstat");
      ret = DPL_FAILURE;
      goto end;
    }

  data_len = st.st_size;

  if (range)
    {
      int range_len;
      
      offset = range->start;
      range_len = range->start - range->end;
      if (data_len > range_len)
        {
          ret = DPL_EINVAL;
          goto end;
        }
      
      length = data_len;
    }
  else
    {
      offset = 0;
      length = data_len;
    }

  if (option && option->mask & DPL_OPTION_NOALLOC)
    {
      data_buf = *data_bufp;
      length = *data_lenp;
    }
  else
    {
      data_buf = malloc(length);
      if (NULL == data_buf)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
    }

  cc = pread(fd, data_buf, length, offset);
  if (-1 == cc)
    {
      ret = DPL_FAILURE;
      goto end;
    }
  
  if (data_len != cc)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (NULL != data_lenp)
    *data_lenp = length;

  if (NULL != data_bufp)
    {
      *data_bufp = data_buf;
      data_buf = NULL;
    }

  ret = DPL_SUCCESS;

 end:

  if ((option && !(option->mask & DPL_OPTION_NOALLOC)) && NULL != data_buf)
    free(data_buf);

  if (-1 != fd)
    close(fd);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

struct get_conven
{
  dpl_metadatum_func_t metadatum_func;
  dpl_dict_t *metadata;
  dpl_sysmd_t *sysmdp;
  dpl_buffer_func_t buffer_func;
  void *cb_arg;
};

static dpl_status_t
cb_get_value(dpl_dict_var_t *var,
             void *cb_arg)
{
  struct get_conven *gc = (struct get_conven *) cb_arg;
  dpl_status_t ret, ret2;
  
  ret2 = dpl_posix_get_metadatum_from_value(var->key,
                                            var->val,
                                            gc->metadatum_func,
                                            gc->cb_arg,
                                            gc->metadata,
                                            gc->sysmdp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  return ret;
}

static dpl_status_t
cb_get_buffer(void *cb_arg,
              char *buf,
              unsigned int len)
{
  struct get_conven *gc = (struct get_conven *) cb_arg;
  int ret;

  if (NULL != gc->buffer_func)
    {
      ret = gc->buffer_func(gc->cb_arg, buf, len);
      if (DPL_SUCCESS != ret)
        return ret;
    }

  return DPL_SUCCESS;
}

dpl_status_t
dpl_posix_head_raw(dpl_ctx_t *ctx,
                   const char *bucket,
                   const char *resource,
                   const char *subresource,
                   const dpl_option_t *option, 
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
  char xattr[64*1024];
  ssize_t ssize_ret, off;
  dpl_dict_t *subdict = NULL;
  dpl_value_t value;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

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

  snprintf(buf, sizeof (buf), "%lX", st.st_ino);
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

  subdict = dpl_dict_new(13);
  if (NULL == subdict)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ssize_ret = llistxattr(path, xattr, sizeof (xattr));
  if (-1 == ssize_ret)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  off = 0;
  while (off < ssize_ret)
    {
      char *key;
      int key_len;
      ssize_t val_len;
      
      key = (xattr + off);
      key_len = strlen(key);

      val_len = lgetxattr(path, key, buf, sizeof (buf) - 1);
      if (val_len == -1)
        {
          ret = DPL_FAILURE;
          goto end;
        }

      if (strncmp(key, DPL_POSIX_XATTR_PREFIX, strlen(DPL_POSIX_XATTR_PREFIX)))
        {
          ret = DPL_EINVAL;
          goto end;
        }

      buf[val_len] = 0;
      ret2 = dpl_dict_add(subdict, key + strlen(DPL_POSIX_XATTR_PREFIX), buf, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      off += key_len + 1;
    }

  value.type = DPL_VALUE_SUBDICT;
  value.subdict = subdict;
  subdict = NULL;
  ret2 = dpl_dict_add_value(metadata, "xattr", &value, 0);
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

  if (NULL != subdict)
    dpl_dict_free(subdict);

  if (NULL != metadata)
    dpl_dict_free(metadata);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_posix_head(dpl_ctx_t *ctx,
               const char *bucket,
               const char *resource,
               const char *subresource,
               const dpl_option_t *option, 
               dpl_ftype_t object_type,
               const dpl_condition_t *condition,
               dpl_dict_t **metadatap,
               dpl_sysmd_t *sysmdp,
               char **locationp)
{
  dpl_status_t ret, ret2;
  dpl_dict_t *all_mds = NULL;
  char path[MAXPATHLEN];

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  snprintf(path, sizeof (path), "/%s/%s", ctx->base_path ? ctx->base_path : "", resource);

  ret2 = dpl_posix_head_raw(ctx, bucket, resource, subresource, option,
                            object_type, condition, &all_mds, locationp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_posix_get_metadata_from_values(all_mds, metadatap, sysmdp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != all_mds)
    dpl_dict_free(all_mds);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_posix_delete(dpl_ctx_t *ctx,
                 const char *bucket,
                 const char *resource,
                 const char *subresource,
                 const dpl_option_t *option, 
                 dpl_ftype_t object_type,
                 const dpl_condition_t *condition, 
                 char **locationp)
{
  dpl_status_t ret;
  char path[MAXPATHLEN];
  int iret;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

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
      ret = DPL_SUCCESS;
      goto end;
    case DPL_FTYPE_DIR:
      iret = rmdir(path);
      if (-1 == iret)
        {
          if (ENOTEMPTY == errno)
            {
              ret = DPL_ENOTEMPTY;
            }
          else
            {
              perror("rmdir");
              ret = DPL_FAILURE;
            }
          goto end;
        }
      ret = DPL_SUCCESS;
      goto end;
    case DPL_FTYPE_CAP:
    case DPL_FTYPE_DOM:
    case DPL_FTYPE_CHRDEV:
    case DPL_FTYPE_BLKDEV:
    case DPL_FTYPE_FIFO:
    case DPL_FTYPE_SOCKET:
    case DPL_FTYPE_SYMLINK:
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

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
               const dpl_option_t *option, 
               dpl_ftype_t object_type,
               dpl_copy_directive_t copy_directive,
               const dpl_dict_t *metadata,
               const dpl_sysmd_t *sysmd,
               const dpl_condition_t *condition,
               char **locationp)
{
  dpl_status_t ret, ret2;
  int iret;
  char src_path[MAXPATHLEN];
  char dst_path[MAXPATHLEN];

  snprintf(src_path, sizeof (src_path), "/%s/%s", ctx->base_path ? ctx->base_path : "", src_resource);
  snprintf(dst_path, sizeof (src_path), "/%s/%s", ctx->base_path ? ctx->base_path : "", dst_resource);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "directive: %s: %s -> %s",
            dpl_copy_directive_to_str(copy_directive), src_path, dst_path);

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

      ret2 = dpl_posix_setattr(src_path, metadata, sysmd);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
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
    case DPL_COPY_DIRECTIVE_RMDENT:
    case DPL_COPY_DIRECTIVE_MVDENT:
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_backend_t
dpl_backend_posix = 
  {
    "posix",
    .get_capabilities   = dpl_posix_get_capabilities,
    .list_bucket 	= dpl_posix_list_bucket,
    .put 		= dpl_posix_put,
    .get 		= dpl_posix_get,
    .head 		= dpl_posix_head,
    .head_raw 		= dpl_posix_head_raw,
    .deletef 		= dpl_posix_delete,
    .copy               = dpl_posix_copy,
  };
