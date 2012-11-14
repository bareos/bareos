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

#define XATTR_PREFIX "user."

dpl_status_t
dpl_posix_get_metadatum_from_value(const char *key,
                                   dpl_value_t *val,
                                   dpl_metadatum_func_t metadatum_func,
                                   void *cb_arg,
                                   dpl_dict_t *metadata,
                                   dpl_sysmd_t *sysmdp)
{
  dpl_status_t ret, ret2;
  int iret;
  char buf[256];

  if (sysmdp)
    {
      if (!strcmp(key, "atime"))
        {
          assert(val->type == DPL_VALUE_STRING);
          sysmdp->atime = strtoul(val->string, NULL, 0);
          sysmdp->mask |= DPL_SYSMD_MASK_ATIME;
        }
      else if (!strcmp(key, "mtime"))
        {
          assert(val->type == DPL_VALUE_STRING);
          sysmdp->mtime = strtoul(val->string, NULL, 0);
          sysmdp->mask |= DPL_SYSMD_MASK_MTIME;
        }
      else if (!strcmp(key, "ctime"))
        {
          assert(val->type == DPL_VALUE_STRING);
          sysmdp->ctime = strtoul(val->string, NULL, 0);
          sysmdp->mask |= DPL_SYSMD_MASK_CTIME;
        }
      else if (!strcmp(key, "size"))
        {
          assert(val->type == DPL_VALUE_STRING);
          sysmdp->size = strtoul(val->string, NULL, 0);
          sysmdp->mask |= DPL_SYSMD_MASK_SIZE;
        }
      else if (!strcmp(key, "uid"))
        {
          uid_t uid;
          struct passwd pwd, *pwdp;

          assert(val->type == DPL_VALUE_STRING);
          uid = atoi(val->string);
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
      else if (!strcmp(key, "gid"))
        {
          gid_t gid;
          struct group grp, *grpp;

          assert(val->type == DPL_VALUE_STRING);
          gid = atoi(val->string);
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
      else if (!strcmp(key, "ino"))
        {
          assert(val->type == DPL_VALUE_STRING);
          snprintf(sysmdp->id, sizeof (sysmdp->id), "%s", val->string);
          sysmdp->mask |= DPL_SYSMD_MASK_ID;
        }
    }

  if (!strcmp(key, "xattr"))
    {
      //this is the metadata object
      if (DPL_VALUE_SUBDICT != val->type)
        {
          ret = DPL_EINVAL;
          goto end;
        }

      if (metadata)
        {
          if (metadatum_func)
            {
              ret2 = metadatum_func(cb_arg, key, val);
              if (DPL_SUCCESS != ret2)
                {
                  ret = ret2;
                  goto end;
                }
            }

          //add md into metadata
          ret2 = dpl_dict_add_value(metadata, key, val, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }
    }
  
  ret = DPL_SUCCESS;
  
 end:

  return ret;
}

struct metadata_conven
{
  dpl_dict_t *metadata;
  dpl_sysmd_t *sysmdp;
};

static dpl_status_t
cb_values_iterate(dpl_dict_var_t *var,
                  void *cb_arg)
{
  struct metadata_conven *mc = (struct metadata_conven *) cb_arg;
  
  return dpl_posix_get_metadatum_from_value(var->key,
                                            var->val,
                                            NULL,
                                            NULL,
                                            mc->metadata,
                                            mc->sysmdp);
}

/** 
 * get metadata from values
 * 
 * @param values 
 * @param metadatap 
 * @param sysmdp 
 * 
 * @return 
 */
dpl_status_t
dpl_posix_get_metadata_from_values(const dpl_dict_t *values,
                                   dpl_dict_t **metadatap,
                                   dpl_sysmd_t *sysmdp)
{
  dpl_dict_t *metadata = NULL;
  dpl_status_t ret, ret2;
  struct metadata_conven mc;

  if (metadatap)
    {
      metadata = dpl_dict_new(13);
      if (NULL == metadata)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
    }

  memset(&mc, 0, sizeof (mc));
  mc.metadata = metadata;
  mc.sysmdp = sysmdp;

  if (sysmdp)
    sysmdp->mask = 0;

  ret2 = dpl_dict_iterate(values, cb_values_iterate, &mc);
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

static dpl_status_t
cb_posix_setattr(dpl_dict_var_t *var,
                 void *cb_arg)
{
  char *path = (char *) cb_arg;
  int iret;
  char buf[256];

  assert(var->val->type == DPL_VALUE_STRING);

  snprintf(buf, sizeof (buf), "%s%s", XATTR_PREFIX, var->key);
 
  iret = lsetxattr(path, buf, var->val->string, strlen(var->val->string), 0);
  if (-1 == iret)
    {
      perror("lsetxattr");
      return DPL_FAILURE;
    }

  return DPL_SUCCESS;
}

static dpl_status_t
posix_setattr(const char *path,
              const dpl_dict_t *metadata,
              const dpl_sysmd_t *sysmd)
{
  dpl_status_t ret, ret2;
  int iret;

  if (sysmd)
    {
      switch (sysmd->mask)
        {
        case DPL_SYSMD_MASK_SIZE:
          iret = truncate(path, sysmd->size);
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
            iret = utime(path, &times);
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
        case DPL_SYSMD_MASK_PATH:
          ret = DPL_ENOTSUPP;
          goto end;
        }
    }

  if (metadata)
    {
      ret2 = dpl_dict_iterate(metadata, cb_posix_setattr, (char *) path);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }
     
  ret = DPL_SUCCESS;
  
 end:

  return ret;
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
          perror("mknod");
          ret = DPL_FAILURE;
          goto end;
        }
      break ;
    case DPL_FTYPE_REG:
      fd = creat(path, -1);
      if (-1 == fd)
        {
          perror("creat");
          ret = DPL_FAILURE;
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

  ret2 = posix_setattr(path, metadata, sysmd);
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
dpl_posix_put_buffered(dpl_ctx_t *ctx,
                       const char *bucket,
                       const char *resource,
                       const char *subresource,
                       const dpl_option_t *option, 
                       dpl_ftype_t object_type,
                       const dpl_condition_t *condition,
                       const dpl_range_t *range,
                       const dpl_dict_t *metadata,
                       const dpl_sysmd_t *sysmd,
                       unsigned int data_len,
                       const dpl_dict_t *query_params, 
                       dpl_conn_t **connp,
                       char **locationp)
{
  dpl_conn_t *conn = NULL;
  dpl_status_t ret, ret2;
  char path[MAXPATHLEN];
  int fd = -1;
  int iret;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  snprintf(path, sizeof (path), "/%s/%s", ctx->base_path ? ctx->base_path : "", resource);

  switch (object_type)
    {
    case DPL_FTYPE_UNDEF:
    case DPL_FTYPE_ANY:
    case DPL_FTYPE_CAP:
    case DPL_FTYPE_DOM:
    case DPL_FTYPE_DIR:
    case DPL_FTYPE_CHRDEV:
    case DPL_FTYPE_BLKDEV:
    case DPL_FTYPE_FIFO:
    case DPL_FTYPE_SOCKET:
    case DPL_FTYPE_SYMLINK:
      ret = DPL_EINVAL;
      goto end;
    case DPL_FTYPE_REG:
      fd = creat(path, 0700);
      if (-1 == fd)
        {
          perror("mknod");
          ret = DPL_FAILURE;
          goto end;
        }
      break ;
    }

  ret2 = posix_setattr(path, metadata, sysmd);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (range)
    {
      int range_len;
      
      range_len = range->start - range->end;
      if (data_len > range_len)
        {
          ret = DPL_EINVAL;
          goto end;
        }

      iret = lseek(fd, range->start, SEEK_SET);
      if (-1 == iret)
        {
          perror("lseek");
          ret = DPL_FAILURE;
          goto end;
        }
    }

  conn = dpl_conn_open_file(ctx, fd);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }
  fd = -1;

  if (NULL != connp)
    {
      *connp = conn;
      conn = NULL;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != conn)
    dpl_conn_release(conn);

  if (-1 == fd)
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

  data_buf = malloc(length);
  if (NULL == data_buf)
    {
      ret = DPL_ENOMEM;
      goto end;
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

  if (NULL != data_buf)
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
dpl_posix_get_buffered(dpl_ctx_t *ctx,
                       const char *bucket,
                       const char *resource,
                       const char *subresource,
                       const dpl_option_t *option, 
                       dpl_ftype_t object_type,
                       const dpl_condition_t *condition,
                       const dpl_range_t *range, 
                       dpl_metadatum_func_t metadatum_func,
                       dpl_dict_t **metadatap,
                       dpl_sysmd_t *sysmdp,
                       dpl_buffer_func_t buffer_func,
                       void *cb_arg,
                       char **locationp)
{
  dpl_status_t ret, ret2;
  char path[MAXPATHLEN];
  int fd = -1;
  int iret;
  char buf[8192];
  ssize_t cc;
  dpl_dict_t *all_mds = NULL;
  struct get_conven gc;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  memset(&gc, 0, sizeof (gc));
  gc.metadata = dpl_dict_new(13);
  if (NULL == gc.metadata)
    {
      ret = DPL_ENOMEM;
      goto end;
    }
  gc.sysmdp = sysmdp;
  gc.metadatum_func = metadatum_func;
  gc.buffer_func = buffer_func;
  gc.cb_arg = cb_arg;

  snprintf(path, sizeof (path), "/%s/%s", ctx->base_path ? ctx->base_path : "", resource);

  switch (object_type)
    {
    case DPL_FTYPE_UNDEF:
    case DPL_FTYPE_CAP:
    case DPL_FTYPE_DOM:
    case DPL_FTYPE_DIR:
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

  if (range)
    {
      iret = lseek(fd, range->start, SEEK_SET);
      if (-1 == iret)
        {
          perror("lseek");
          ret = DPL_FAILURE;
          goto end;
        }
    }

  ret2 = dpl_posix_head_raw(ctx, bucket, resource, subresource, option,
                            object_type, condition, &all_mds, locationp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_dict_iterate(all_mds, cb_get_value, &gc);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  while (1)
    {
      cc = read(fd, buf, sizeof (buf));
      if (-1 == cc)
        {
          perror("read");
          ret = DPL_FAILURE;
          goto end;
        }

      ret2 = cb_get_buffer(&gc, buf, cc);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
      
      if (0 == cc)
        break ;
    }

  if (NULL != metadatap)
    {
      *metadatap = gc.metadata;
      gc.metadata = NULL;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != all_mds)
    dpl_dict_free(all_mds);

  if (NULL != gc.metadata)
    dpl_dict_free(gc.metadata);

  if (-1 == fd)
    close(fd);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
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

      if (strncmp(key, XATTR_PREFIX, strlen(XATTR_PREFIX)))
        {
          ret = DPL_EINVAL;
          goto end;
        }

      buf[val_len] = 0;
      ret2 = dpl_dict_add(subdict, key + strlen(XATTR_PREFIX), buf, 0);
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
          perror("rmdir");
          ret = DPL_FAILURE;
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

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

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

      ret2 = posix_setattr(src_path, metadata, sysmd);
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
