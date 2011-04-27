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

dpl_status_t
dpl_close(dpl_vfile_t *vfile)
{
  int ret, ret2;
  dpl_dict_t *headers_returned = NULL;
  int connection_close = 0;
  dpl_var_t *var;

  DPL_TRACE(vfile->ctx, DPL_TRACE_VFILE, "close vfile=%p", vfile);

  ret = DPL_SUCCESS;

  if (NULL != vfile->conn)
    {
      ret2 = dpl_read_http_reply(vfile->conn, 1, NULL, NULL, &headers_returned);
      if (DPL_SUCCESS != ret2)
        {
          fprintf(stderr, "read http_reply failed %s (%d)\n", dpl_status_str(ret2), ret2);
          ret = ret2;
        }
      else
        {
          if (vfile->flags & DPL_VFILE_FLAG_MD5)
            {
              ret2 = dpl_dict_get_lowered(headers_returned, "Etag", &var);
              if (DPL_SUCCESS != ret2 || NULL == var)
                {
                  fprintf(stderr, "missing 'Etag' in answer\n");
                  ret = DPL_FAILURE;
                }
              else
                {
                  //compare MD5 with etag
                  u_char digest[MD5_DIGEST_LENGTH];
                  char bcd_digest[DPL_BCD_LENGTH(MD5_DIGEST_LENGTH) + 1];
                  u_int bcd_digest_len;

                  MD5_Final(digest, &vfile->md5_ctx);

                  bcd_digest_len = dpl_bcd_encode(digest, MD5_DIGEST_LENGTH, bcd_digest);
                  bcd_digest[bcd_digest_len] = 0;

                  //skip quotes
                  if (strncmp(var->value + 1, bcd_digest, DPL_BCD_LENGTH(MD5_DIGEST_LENGTH)))
                    {
                      fprintf(stderr, "MD5 checksum dont match\n");
                      ret = DPL_FAILURE;
                    }
                }
            }
        }

      connection_close = dpl_connection_close(headers_returned);

      if (1 == connection_close)
        dpl_conn_terminate(vfile->conn);
      else
        dpl_conn_release(vfile->conn);
    }

  if (vfile->flags & DPL_VFILE_FLAG_ENCRYPT)
    {
      if (NULL != vfile->cipher_ctx)
        EVP_CIPHER_CTX_free(vfile->cipher_ctx);
    }

  if (NULL != vfile->headers_reply)
    dpl_dict_free(vfile->headers_reply);

  free(vfile);

  if (NULL != headers_returned)
    dpl_dict_free(headers_returned);

  return ret;
}

static dpl_status_t
encrypt_init(dpl_vfile_t *vfile,
             int enc)
{
  int ret;
  const EVP_CIPHER *cipher;
  const EVP_MD *md;
  u_char key[EVP_MAX_KEY_LENGTH];
  u_char iv[EVP_MAX_IV_LENGTH];

  if (NULL == vfile->ctx->encrypt_key)
    {
      fprintf(stderr, "missing encrypt_key in conf\n");
      ret = DPL_EINVAL;
      goto end;
    }

  cipher = EVP_get_cipherbyname("aes-256-cfb");
  if (NULL == cipher)
    {
      fprintf(stderr, "unsupported cipher\n");
      ret = DPL_EINVAL;
      goto end;
    }

  vfile->cipher_ctx = EVP_CIPHER_CTX_new();
  if (NULL == vfile->cipher_ctx)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  md = EVP_md5();
  if (NULL == md)
    {
      fprintf(stderr, "unsupported md\n");
      ret = DPL_FAILURE;
      goto end;
    }

  EVP_BytesToKey(cipher, md, vfile->salt, (u_char *) vfile->ctx->encrypt_key, strlen(vfile->ctx->encrypt_key), 1, key, iv);

  EVP_CipherInit(vfile->cipher_ctx, cipher, key, iv, enc);

  ret = DPL_SUCCESS;

 end:

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
  dpl_vfile_t *vfile = NULL;
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket, *path;
  int delim_len = strlen(ctx->delim);
  dpl_ino_t cur_ino;
  int own_metadata = 0;

  DPL_TRACE(ctx, DPL_TRACE_VFILE, "openwrite locator=%s flags=0x%x", locator, flags);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      bucket = nlocator;
      *path++ = 0;
    }
  else
    {
      bucket = ctx->cur_bucket;
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  vfile = malloc(sizeof (*vfile));
  if (NULL == vfile)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  memset(vfile, 0, sizeof (*vfile));

  vfile->ctx = ctx;
  vfile->flags = flags;

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          char *remote_name;

          if (!(vfile->flags & DPL_VFILE_FLAG_CREAT))
            {
              ret = DPL_FAILURE;
              goto end;
            }

          remote_name = dpl_strrstr(path, ctx->delim);
          if (NULL != remote_name)
            {
              remote_name += delim_len;
              *remote_name = 0;

              //fetch parent directory
              ret2 = dpl_namei(ctx, !strcmp(path, "") ? ctx->delim : path, bucket, cur_ino, NULL, &parent_ino, NULL);
              if (DPL_SUCCESS != ret2)
                {
                  DPLERR(0, "dst parent dir resolve failed %s: %s\n", path, dpl_status_str(ret2));
                  ret = ret2;
                  goto end;
                }
            }
          else
            {
              parent_ino = cur_ino;
              remote_name = path;
            }

          // If the remote_name string length is 0,
          // that means that the parent ino was searched with dpl_namei
          // and that we should concat the rest of the locator instead.
          obj_ino = parent_ino;
          if (remote_name[0] == '\0')
              strcat(obj_ino.key, &locator[path - nlocator + strlen(path)]);
          else
              strcat(obj_ino.key, remote_name); //XXX check length
          obj_type = DPL_FTYPE_REG;
        }
      else
        {
          ret = DPL_FAILURE;
          goto end;
        }
    }
  else
    {
      if (vfile->flags & DPL_VFILE_FLAG_EXCL)
        {
          ret = DPL_EEXIST;
          goto end;
        }
    }

  if (DPL_FTYPE_REG != obj_type)
    {
      ret = DPL_EISDIR;
      goto end;
    }

  if (vfile->flags & DPL_VFILE_FLAG_MD5)
    {
      MD5_Init(&vfile->md5_ctx);
    }

  if (vfile->flags & DPL_VFILE_FLAG_ENCRYPT)
    {
      ret2 = RAND_pseudo_bytes(vfile->salt, sizeof (vfile->salt));
      if (0 == ret2)
        {
          ret = DPL_FAILURE;
          goto end;
        }

      ret2 = encrypt_init(vfile, 1);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      if (NULL == metadata)
        {
          metadata = dpl_dict_new(13);
          if (NULL == metadata)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
          own_metadata = 1;
        }

      ret2 = dpl_dict_add(metadata, "cipher", "yes", 1);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      ret2 = dpl_dict_add(metadata, "cipher-type", "aes-256-cfb", 1);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_put_buffered(ctx,
                          bucket,
                          obj_ino.key,
                          NULL,
                          metadata,
                          canned_acl,
                          data_len,
                          &vfile->conn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != vfilep)
    {
      *vfilep = vfile;
      vfile = NULL;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != vfile)
    dpl_close(vfile);

  if (NULL != nlocator)
    free(nlocator);

  if (1 == own_metadata)
    dpl_dict_free(metadata);

  return ret;
}

/**
 * write buffer
 *
 * @param vfile
 * @param buf
 * @param len
 *
 * @note ensure that all the buffer is written
 *
 * @return
 */
dpl_status_t
dpl_write(dpl_vfile_t *vfile,
          char *buf,
          unsigned int len)
{
  int ret, ret2;
  struct iovec iov[10];
  int n_iov = 0;
  char *obuf = NULL;
  int olen;

  DPL_TRACE(vfile->ctx, DPL_TRACE_VFILE, "write_all vfile=%p", vfile);

  if (vfile->flags & DPL_VFILE_FLAG_MD5)
    {
      MD5_Update(&vfile->md5_ctx, buf, len);
    }

  if (vfile->flags & DPL_VFILE_FLAG_ENCRYPT)
    {
      if (0 == vfile->header_done)
        {
          iov[n_iov].iov_base = DPL_ENCRYPT_MAGIC;
          iov[n_iov].iov_len = strlen(DPL_ENCRYPT_MAGIC);

          n_iov++;

          iov[n_iov].iov_base = vfile->salt;
          iov[n_iov].iov_len = sizeof (vfile->salt);

          n_iov++;

          vfile->header_done = 1;
        }

      obuf = malloc(len);
      if (NULL == obuf)
        {
          ret = DPL_ENOMEM;
          goto end;
        }

      ret = EVP_CipherUpdate(vfile->cipher_ctx, (u_char *) obuf, &olen, (u_char *) buf, len);
      if (0 == ret)
        {
          DPLERR(0, "CipherUpdate failed\n");
          ret = DPL_FAILURE;
          goto end;
        }

      iov[n_iov].iov_base = obuf;
      iov[n_iov].iov_len = olen;

      n_iov++;
    }
  else
    {
      iov[n_iov].iov_base = buf;
      iov[n_iov].iov_len = len;

      n_iov++;
    }

  ret2 = dpl_conn_writev_all(vfile->conn, iov, n_iov, DPL_DEFAULT_WRITE_TIMEOUT);

  ret = ret2;

 end:

  if (NULL != obuf)
    free(obuf);

  return ret;
}

/**/

static dpl_status_t
cb_vfile_header(void *cb_arg,
                char *header,
                char *value)
{
  dpl_vfile_t *vfile = (dpl_vfile_t *) cb_arg;
  int ret, ret2;

  ret2 = dpl_dict_add(vfile->headers_reply, header, value, 1);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (!strcmp(header, "x-amz-meta-cipher"))
    {
      if (!strcmp(value, "yes"))
        {
          vfile->flags |= DPL_VFILE_FLAG_ENCRYPT;
        }
    }

  //XXX check cipher-type

  ret = DPL_SUCCESS;

 end:

  return ret;
}

static dpl_status_t
cb_vfile_buffer(void *cb_arg,
                char *buf,
                u_int len)
{
  dpl_vfile_t *vfile = (dpl_vfile_t *) cb_arg;
  char *obuf = NULL;
  int olen;
  int ret, ret2;

  if (vfile->flags & DPL_VFILE_FLAG_ENCRYPT)
    {
      if (0 == vfile->header_done)
        {
          u_int magic_len;
          u_int header_len;

          magic_len = strlen(DPL_ENCRYPT_MAGIC);

          header_len = magic_len + sizeof (vfile->salt);
          if (len < header_len)
            {
              DPLERR(0, "not enough bytes for decrypting");
              ret = DPL_EINVAL;
              goto end;
            }

          memcpy(vfile->salt, buf + magic_len, sizeof (vfile->salt));
          buf += header_len;
          len -= header_len;

          ret2 = encrypt_init(vfile, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }

          vfile->header_done = 1;
        }

      obuf = malloc(len);
      if (NULL == obuf)
        {
          ret = DPL_ENOMEM;
          goto end;
        }

      ret2 = EVP_CipherUpdate(vfile->cipher_ctx, (u_char *) obuf, &olen, (u_char *) buf, len);
      if (0 == ret2)
        {
          DPLERR(0, "CipherUpdate failed\n");
          ret = DPL_FAILURE;
          goto end;
        }

      buf = obuf;
      len = olen;
    }

  if (NULL != vfile->buffer_func)
    {
      ret2 = vfile->buffer_func(vfile->cb_arg, buf, len);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != obuf)
    free(obuf);

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
  dpl_vfile_t *vfile = NULL;
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket, *path;
  dpl_ino_t cur_ino;
  dpl_dict_t *metadata = NULL;

  DPL_TRACE(ctx, DPL_TRACE_VFILE, "openread locator=%s flags=0x%x", locator, flags);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      bucket = nlocator;
      *path++ = 0;
    }
  else
    {
      bucket = ctx->cur_bucket;
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  vfile = malloc(sizeof (*vfile));
  if (NULL == vfile)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  memset(vfile, 0, sizeof (*vfile));

  vfile->headers_reply = dpl_dict_new(13);
  if (NULL == vfile->headers_reply)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  vfile->ctx = ctx;
  vfile->flags = flags;
  vfile->buffer_func = buffer_func;
  vfile->cb_arg = cb_arg;

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (DPL_FTYPE_REG != obj_type)
    {
      ret = DPL_EISDIR;
      goto end;
    }

  ret2 = dpl_get_buffered(ctx,
                          bucket,
                          obj_ino.key,
                          NULL,
                          condition,
                          cb_vfile_header,
                          cb_vfile_buffer,
                          vfile);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  metadata = dpl_dict_new(13);
  if (NULL == metadata)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_get_metadata_from_headers(vfile->headers_reply, metadata);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (NULL != metadatap)
    {
      *metadatap = metadata;
      metadata = NULL;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != vfile)
    dpl_close(vfile);

  if (NULL != nlocator)
    free(nlocator);

  if (NULL != metadata)
    dpl_dict_free(metadata);

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
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket, *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFILE, "openread locator=%s flags=0x%x", locator, flags);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      bucket = nlocator;
      *path++ = 0;
    }
  else
    {
      bucket = ctx->cur_bucket;
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (DPL_FTYPE_REG != obj_type)
    {
      ret = DPL_EISDIR;
      goto end;
    }

  ret2 = dpl_get_range(ctx,
                       bucket,
                       obj_ino.key,
                       NULL,
                       condition,
                       start,
                       end,
                       data_bufp,
                       data_lenp,
                       metadatap);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != nlocator)
    free(nlocator);

  return ret;
}

dpl_status_t
dpl_unlink(dpl_ctx_t *ctx,
           char *locator)
{
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket, *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFILE, "unlink locator=%s", locator);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      bucket = nlocator;
      *path++ = 0;
    }
  else
    {
      bucket = ctx->cur_bucket;
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (DPL_FTYPE_REG != obj_type)
    {
      ret = DPL_EISDIR;
      goto end;
    }

  ret2 = dpl_delete(ctx, bucket, obj_ino.key, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != nlocator)
    free(nlocator);

  return ret;
}

dpl_status_t
dpl_getattr(dpl_ctx_t *ctx,
            char *locator,
            dpl_dict_t **metadatap)
{
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket, *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFILE, "getattr locator=%s", locator);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      bucket = nlocator;
      *path++ = 0;
    }
  else
    {
      bucket = ctx->cur_bucket;
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (DPL_FTYPE_REG != obj_type)
    {
      ret = DPL_EISDIR;
      goto end;
    }

  ret2 = dpl_head(ctx, bucket, obj_ino.key, NULL, NULL, metadatap);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != nlocator)
    free(nlocator);

  return ret;
}

dpl_status_t
dpl_setattr(dpl_ctx_t *ctx,
            char *locator,
            dpl_dict_t *metadata)
{
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket, *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFILE, "setattr locator=%s", locator);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      bucket = nlocator;
      *path++ = 0;
    }
  else
    {
      bucket = ctx->cur_bucket;
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (DPL_FTYPE_REG != obj_type)
    {
      ret = DPL_EISDIR;
      goto end;
    }

  ret2 = dpl_copy(ctx, bucket, obj_ino.key, NULL, bucket, obj_ino.key, NULL, DPL_METADATA_DIRECTIVE_REPLACE, metadata, DPL_CANNED_ACL_UNDEF, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != nlocator)
    free(nlocator);

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
  int ret, ret2;
  dpl_ino_t obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket, *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFILE, "fgenurl locator=%s", locator);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      bucket = nlocator;
      *path++ = 0;
    }
  else
    {
      bucket = ctx->cur_bucket;
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, NULL, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "namei failed %s (%d)\n", dpl_status_str(ret2), ret2);
      ret = ret2;
      goto end;
    }

  ret2 = dpl_genurl(ctx, bucket, obj_ino.key, NULL, expires, buf, len, lenp);
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
dpl_fcopy(dpl_ctx_t *ctx,
          char *src_locator,
          char *dst_locator)
{
  int ret, ret2;
  char *src_nlocator = NULL;
  char *src_bucket, *src_path;
  dpl_ino_t src_cur_ino;
  dpl_ino_t src_obj_ino;
  dpl_ftype_t src_obj_type;
  char *dst_nlocator = NULL;
  char *dst_bucket, *dst_path;
  dpl_ino_t dst_cur_ino;
  dpl_ino_t dst_obj_ino;
  dpl_ftype_t dst_obj_type;
  int delim_len = strlen(ctx->delim);
  dpl_ino_t parent_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFILE, "fcopy src_locator=%s dst_locator=%s", src_locator, dst_locator);

  src_nlocator = strdup(src_locator);
  if (NULL == src_nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  src_path = index(src_nlocator, ':');
  if (NULL != src_path)
    {
      src_bucket = src_nlocator;
      *src_path++ = 0;
    }
  else
    {
      src_bucket = ctx->cur_bucket;
      src_path = src_nlocator;
    }

  src_cur_ino = dpl_cwd(ctx, src_bucket);

  dst_nlocator = strdup(dst_locator);
  if (NULL == dst_nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dst_path = index(dst_nlocator, ':');
  if (NULL != dst_path)
    {
      dst_bucket = dst_nlocator;
      *dst_path++ = 0;
    }
  else
    {
      dst_bucket = ctx->cur_bucket;
      dst_path = dst_nlocator;
    }

  dst_cur_ino = dpl_cwd(ctx, dst_bucket);

  ret2 = dpl_namei(ctx, src_path, src_bucket, src_cur_ino, NULL, &src_obj_ino, &src_obj_type);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "namei failed %s (%d)\n", dpl_status_str(ret2), ret2);
      ret = ret2;
      goto end;
    }

  ret2 = dpl_namei(ctx, dst_path, dst_bucket, dst_cur_ino, NULL, &dst_obj_ino, &dst_obj_type);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          char *remote_name;

          remote_name = dpl_strrstr(dst_path, ctx->delim);
          if (NULL != remote_name)
            {
              *remote_name = 0;
              remote_name += delim_len;

              //fetch parent directory
              ret2 = dpl_namei(ctx, !strcmp(dst_path, "") ? ctx->delim : dst_path, dst_bucket, dst_cur_ino, NULL, &parent_ino, NULL);
              if (DPL_SUCCESS != ret2)
                {
                  DPLERR(0, "dst parent dir resolve failed %s: %s\n", dst_path, dpl_status_str(ret2));
                  ret = ret2;
                  goto end;
                }
            }
          else
            {
              parent_ino = dst_cur_ino;
              remote_name = dst_path;
            }

          dst_obj_ino = parent_ino;
          strcat(dst_obj_ino.key, remote_name); //XXX check length
          dst_obj_type = DPL_FTYPE_REG;
        }
    }

  ret2 = dpl_copy(ctx, src_bucket, src_obj_ino.key, NULL, dst_bucket, dst_obj_ino.key, NULL, DPL_METADATA_DIRECTIVE_COPY, NULL, DPL_CANNED_ACL_UNDEF, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != dst_nlocator)
    free(dst_nlocator);

  if (NULL != src_nlocator)
    free(src_nlocator);

  return ret;
}
