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

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

/* general */

dpl_status_t 
dpl_list_all_my_buckets(dpl_ctx_t *ctx,
                        dpl_vec_t **vecp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "list_all_my_buckets");

  if (NULL == ctx->backend->list_all_my_buckets)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->list_all_my_buckets(ctx, vecp);

 end:
  
  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_list_bucket(dpl_ctx_t *ctx, 
                char *bucket,
                char *prefix,
                char *delimiter,
                dpl_vec_t **objectsp, 
                dpl_vec_t **common_prefixesp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "list_bucket bucket=%s prefix=%s delimiter=%s", bucket, prefix, delimiter);

  if (NULL == ctx->backend->list_bucket)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->list_bucket(ctx, bucket, prefix, delimiter, objectsp, common_prefixesp);

 end:
  
  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_make_bucket(dpl_ctx_t *ctx,
                char *bucket, 
                dpl_location_constraint_t location_constraint,
                dpl_canned_acl_t canned_acl)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "makebucket bucket=%s", bucket);

  if (NULL == ctx->backend->make_bucket)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->make_bucket(ctx, bucket, location_constraint, canned_acl);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_deletebucket(dpl_ctx_t *ctx,
                 char *bucket)
{
  int ret;
  
  DPL_TRACE(ctx, DPL_TRACE_API, "deletebucket bucket=%s", bucket);

  if (NULL == ctx->backend->delete_bucket)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->delete_bucket(ctx, bucket);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_put(dpl_ctx_t *ctx,
        char *bucket,
        char *resource,
        char *subresource,
        dpl_dict_t *metadata,
        dpl_canned_acl_t canned_acl,
        char *data_buf,
        unsigned int data_len)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "put bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->put)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->put(ctx, bucket, resource, subresource, metadata, canned_acl, data_buf, data_len);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_get(dpl_ctx_t *ctx,
        char *bucket,
        char *resource,
        char *subresource,
        dpl_condition_t *condition,
        char **data_bufp,
        unsigned int *data_lenp,
        dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "get bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->get)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get(ctx, bucket, resource, subresource, condition, data_bufp, data_lenp, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_head(dpl_ctx_t *ctx,
         char *bucket,
         char *resource,
         char *subresource,
         dpl_condition_t *condition,
         dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "head bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->head)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->head(ctx, bucket, resource, subresource, condition, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_head_all(dpl_ctx_t *ctx,
             char *bucket,
             char *resource,
             char *subresource,
             dpl_condition_t *condition,
             dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "head_all bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->head_all)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->head_all(ctx, bucket, resource, subresource, condition, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_get_metadata_from_headers(dpl_ctx_t *ctx,
                              dpl_dict_t *headers,
                              dpl_dict_t *metadata)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "get_metadata_from_headers");

  if (NULL == ctx->backend->get_metadata_from_headers)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get_metadata_from_headers(headers, metadata);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_delete(dpl_ctx_t *ctx,
           char *bucket,
           char *resource,
           char *subresource)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "delete bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->delete)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->delete(ctx, bucket, resource, subresource);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

/* vdir */

dpl_status_t
dpl_namei(dpl_ctx_t *ctx,
          char *path,
          char *bucket,
          dpl_ino_t ino,
          dpl_ino_t *parent_inop,
          dpl_ino_t *obj_inop,
          dpl_ftype_t *obj_typep)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "namei path=%s bucket=%s", path, bucket);

  if (NULL == ctx->backend->namei)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->namei(ctx, path, bucket, ino, parent_inop, obj_inop, obj_typep);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_ino_t
dpl_cwd(dpl_ctx_t *ctx,
        char *bucket)
{
  dpl_ino_t ino;

  DPL_TRACE(ctx, DPL_TRACE_API, "cwd bucket=%s", bucket);

  if (NULL == ctx->backend->cwd)
    {
      ino = DPL_ROOT_INO;
      goto end;
    }
  
  ino = ctx->backend->cwd(ctx, bucket);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%s", ino.key);
  
  return ino;
}

dpl_status_t 
dpl_opendir(dpl_ctx_t *ctx, 
            char *locator, 
            void **dir_hdlp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "opendir locator=%s", locator);

  if (NULL == ctx->backend->opendir)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->opendir(ctx, locator, dir_hdlp);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_readdir(void *dir_hdl, 
            dpl_dirent_t *dirent)
{
  int ret;
  dpl_dir_t *dir = (dpl_dir_t *) dir_hdl;

  DPL_TRACE(dir->ctx, DPL_TRACE_API, "readdir");

  if (NULL == dir->ctx->backend->readdir)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = dir->ctx->backend->readdir(dir_hdl, dirent);
  
 end:

  DPL_TRACE(dir->ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

int 
dpl_eof(void *dir_hdl)
{
  int ret;
  dpl_dir_t *dir = (dpl_dir_t *) dir_hdl;

  DPL_TRACE(dir->ctx, DPL_TRACE_API, "eof");

  if (NULL == dir->ctx->backend->eof)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = dir->ctx->backend->eof(dir_hdl);
  
 end:

  DPL_TRACE(dir->ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

void 
dpl_closedir(void *dir_hdl)
{
  int ret;
  dpl_dir_t *dir = (dpl_dir_t *) dir_hdl;

  DPL_TRACE(dir->ctx, DPL_TRACE_API, "closedir");

  if (NULL == dir->ctx->backend->closedir)
    {
      goto end;
    }
  
  dir->ctx->backend->closedir(dir_hdl);
  
 end:

  DPL_TRACE(dir->ctx, DPL_TRACE_API, "");
}

dpl_status_t 
dpl_chdir(dpl_ctx_t *ctx, 
          char *locator)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "chdir locator=%s", locator);

  if (NULL == ctx->backend->chdir)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->chdir(ctx, locator);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_mkdir(dpl_ctx_t *ctx, 
          char *locator)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "mkdir locator=%s", locator);

  if (NULL == ctx->backend->mkdir)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->mkdir(ctx, locator);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_mknod(dpl_ctx_t *ctx,
          char *locator)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "mknod locator=%s", locator);

  if (NULL == ctx->backend->mknod)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->mknod(ctx, locator);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_rmdir(dpl_ctx_t *ctx, 
          char *locator)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "rmdir locator=%s", locator);

  if (NULL == ctx->backend->rmdir)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->rmdir(ctx, locator);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

/* vfile */

dpl_status_t 
dpl_close(dpl_vfile_t *vfile)
{
  int ret;

  DPL_TRACE(vfile->ctx, DPL_TRACE_API, "close");

  if (NULL == vfile->ctx->backend->close)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = vfile->ctx->backend->close(vfile);
  
 end:

  DPL_TRACE(vfile->ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_openwrite(dpl_ctx_t *ctx,
              char *locator,
              unsigned int flags, 
              dpl_dict_t *metadata,
              dpl_canned_acl_t canned_acl,
              unsigned int data_len,
              dpl_vfile_t **vfilep)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "openwrite locator=%s", locator);

  if (NULL == ctx->backend->openwrite)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->openwrite(ctx, locator, flags, metadata, canned_acl, data_len, vfilep);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_write(dpl_vfile_t *vfile, 
          char *buf,
          unsigned int len)
{
  int ret;

  DPL_TRACE(vfile->ctx, DPL_TRACE_API, "write");

  if (NULL == vfile->ctx->backend->write)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = vfile->ctx->backend->write(vfile, buf, len);
  
 end:

  DPL_TRACE(vfile->ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_openread(dpl_ctx_t *ctx, 
             char *locator, 
             unsigned int flags, 
             dpl_condition_t *condition,
             dpl_buffer_func_t buffer_func, 
             void *cb_arg, 
             dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "openread locator=%s", locator);

  if (NULL == ctx->backend->openread)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->openread(ctx, locator, flags, condition, buffer_func, cb_arg, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_openread_range(dpl_ctx_t *ctx, 
                   char *locator,
                   unsigned int flags,
                   dpl_condition_t *condition, 
                   int start, 
                   int end,
                   char **data_bufp,
                   unsigned int *data_lenp,
                   dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "openread_range locator=%s", locator);

  if (NULL == ctx->backend->openread_range)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->openread_range(ctx, locator, flags, condition, start, end, data_bufp, data_lenp, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_unlink(dpl_ctx_t *ctx,
           char *locator)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "unlink locator=%s", locator);

  if (NULL == ctx->backend->unlink)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->unlink(ctx, locator);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_getattr(dpl_ctx_t *ctx,
            char *locator,
            dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "getattr locator=%s", locator);

  if (NULL == ctx->backend->getattr)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->getattr(ctx, locator, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_setattr(dpl_ctx_t *ctx,
            char *locator,
            dpl_dict_t *metadata)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "setattr locator=%s", locator);

  if (NULL == ctx->backend->setattr)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->setattr(ctx, locator, metadata);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_fgenurl(dpl_ctx_t *ctx,
            char *locator,
            time_t expires,
            char *buf,
            unsigned int len,
            unsigned int *lenp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "fgenurl locator=%s", locator);

  if (NULL == ctx->backend->fgenurl)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->fgenurl(ctx, locator, expires, buf, len, lenp);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_fcopy(dpl_ctx_t *ctx, 
          char *src_locator,
          char *dst_locator)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "fcopy src_locator=%s dst_locator=%s", src_locator, dst_locator);

  if (NULL == ctx->backend->fcopy)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->fcopy(ctx, src_locator, dst_locator);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}
