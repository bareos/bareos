/*
 * Droplets, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplets
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dropletsp.h"

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_status_t
dpl_close(dpl_vfile_t *vfile)
{
  int ret, ret2;
  dpl_dict_t *headers_returned = NULL;
  int connection_close = 0;

  DPL_TRACE(vfile->ctx, DPL_TRACE_VFILE, "close vfile=%p", vfile);

  if (NULL != vfile->conn)
    {
      ret2 = dpl_read_http_reply(vfile->conn, NULL, NULL, &headers_returned);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
      else
        {
          connection_close = dpl_connection_close(headers_returned);
        }
      
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

  free(vfile);

  ret = DPL_SUCCESS;

 end:

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
              char *path,
              u_int flags,
              dpl_dict_t *metadata,
              dpl_canned_acl_t canned_acl,
              u_int data_len,
              dpl_vfile_t **vfilep)
{
  dpl_vfile_t *vfile = NULL;
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;

  DPL_TRACE(ctx, DPL_TRACE_VFILE, "openwrite path=%s flags=0x%x", path, flags);

  vfile = malloc(sizeof (*vfile));
  if (NULL == vfile)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  memset(vfile, 0, sizeof (*vfile));

  vfile->ctx = ctx;
  vfile->flags = flags;

  ret2 = dpl_vdir_namei(ctx, path, ctx->cur_bucket, ctx->cur_ino, &parent_ino, &obj_ino, &obj_type);
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
          remote_name = index(path, '/');
          if (NULL != remote_name)
            remote_name++;
          else
            remote_name = path;
          obj_ino = ctx->cur_ino;
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
          ret = DPL_FAILURE;
          goto end;
        }
    }

  if (DPL_FTYPE_REG != obj_type)
    {
      ret = DPL_EISDIR;
      goto end;
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
    }

  ret2 = dpl_put_buffered(ctx,
                          ctx->cur_bucket,
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
          u_int len)
{
  int ret, ret2;
  struct iovec iov[10];
  int n_iov = 0;
  char *obuf = NULL;
  int olen;

  DPL_TRACE(vfile->ctx, DPL_TRACE_VFILE, "write_all vfile=%p", vfile);

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
             char *path,
             u_int flags,
             dpl_condition_t *condition,
             dpl_buffer_func_t buffer_func,
             void *cb_arg,
             dpl_dict_t **metadatap)
{
  dpl_vfile_t *vfile = NULL;
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;

  DPL_TRACE(ctx, DPL_TRACE_VFILE, "openread path=%s flags=0x%x", path, flags);

  vfile = malloc(sizeof (*vfile));
  if (NULL == vfile)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  memset(vfile, 0, sizeof (*vfile));

  vfile->ctx = ctx;
  vfile->flags = flags;
  vfile->buffer_func = buffer_func;
  vfile->cb_arg = cb_arg;

  ret2 = dpl_vdir_namei(ctx, path, ctx->cur_bucket, ctx->cur_ino, &parent_ino, &obj_ino, &obj_type);
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
                          ctx->cur_bucket,
                          obj_ino.key,
                          NULL, 
                          condition,
                          cb_vfile_buffer,
                          vfile,
                          metadatap);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != vfile)
    dpl_close(vfile);

  return ret;
}

dpl_status_t
dpl_unlink(dpl_ctx_t *ctx,
           char *path)
{
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;

  DPL_TRACE(ctx, DPL_TRACE_VFILE, "unlink path=%s", path);

  ret2 = dpl_vdir_namei(ctx, path, ctx->cur_bucket, ctx->cur_ino, &parent_ino, &obj_ino, &obj_type);
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

  ret2 = dpl_delete(ctx, ctx->cur_bucket, obj_ino.key, NULL);
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
dpl_getattr(dpl_ctx_t *ctx,
            char *path,
            dpl_condition_t *condition,
            dpl_dict_t **metadatap)
{
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;

  DPL_TRACE(ctx, DPL_TRACE_VFILE, "unlink path=%s", path);

  ret2 = dpl_vdir_namei(ctx, path, ctx->cur_bucket, ctx->cur_ino, &parent_ino, &obj_ino, &obj_type);
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

  ret2 = dpl_head(ctx, ctx->cur_bucket, obj_ino.key, NULL, condition, metadatap);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }
  
  ret = DPL_SUCCESS;
  
 end:

  return ret;
}
