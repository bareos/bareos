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

void
dpl_vfile_free(dpl_vfile_t *vfile)
{
  if (1 == vfile->conn->ctx->encrypt)
    {
      if (NULL != vfile->cipher_ctx)
        EVP_CIPHER_CTX_free(vfile->cipher_ctx);
    }
  
  free(vfile);
}

dpl_vfile_t *
dpl_vfile_new(dpl_conn_t *conn)
{
  dpl_vfile_t *vfile = NULL;
  int ret;

  vfile = malloc(sizeof (*vfile));
  if (NULL == vfile)
    return NULL;

  memset(vfile, 0, sizeof (*vfile));

  vfile->conn = conn;

  /*
   * encrypt
   */
  if (1 == conn->ctx->encrypt)
    {
      const EVP_CIPHER *cipher;
      const EVP_MD *md;
      u_char key[EVP_MAX_KEY_LENGTH];
      u_char iv[EVP_MAX_IV_LENGTH];
      u_char salt[PKCS5_SALT_LEN];
      struct iovec iov[2];
      
      cipher = EVP_get_cipherbyname("aes-256-cfb");
      if (NULL == cipher)
        {
          fprintf(stderr, "unsupported cipher\n");
          goto bad;
        }

      md = EVP_md5();
      if (NULL == md)
        {
          fprintf(stderr, "unsupported md\n");
          goto bad;
        }

      vfile->cipher_ctx = EVP_CIPHER_CTX_new();
      if (NULL == vfile->cipher_ctx)
        goto bad;

      ret = RAND_pseudo_bytes(salt, sizeof salt);
      if (0 == ret)
        goto bad;

      EVP_BytesToKey(cipher, md, salt, (u_char *) conn->ctx->encrypt_key, strlen(conn->ctx->encrypt_key), 1, key, iv);

#define MAGIC "Salted__"
      iov[0].iov_base = MAGIC;
      iov[0].iov_len = strlen(MAGIC);

      iov[1].iov_base = salt;
      iov[1].iov_len = sizeof (salt);

      ret = dpl_conn_writev_all(vfile->conn, iov, 2, DPL_DEFAULT_WRITE_TIMEOUT);
      if (DPL_SUCCESS != ret)
        goto bad;

      EVP_CipherInit(vfile->cipher_ctx, cipher, key, iv, 1);
    }
  
  return vfile;

 bad:
  
  if (NULL != vfile)
    dpl_vfile_free(vfile);
  
  return NULL;
}

dpl_status_t
dpl_vfile_put(dpl_ctx_t *ctx,
              char *bucket,
              char *resource,
              char *subresource,
              dpl_dict_t *metadata,
              dpl_canned_acl_t canned_acl,
              u_int data_len,
              dpl_vfile_t **vfilep)
{
  char          host[1024];
  char          data_len_str[64];
  int           ret, ret2;
  struct hostent hret, *hresult;
  char          hbuf[1024];
  int           herr;
  struct in_addr addr;
  dpl_conn_t    *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  char          *acl_str;
  dpl_dict_t    *headers = NULL;
  dpl_dict_t    *headers_returned = NULL;
  dpl_vfile_t *vfile = NULL;

  snprintf(host, sizeof (host), "%s.%s", bucket, ctx->host);

  DPRINTF("dpl_make_bucket: host=%s:%d\n", host, ctx->port);

  headers = dpl_dict_new(13);
  if (NULL == headers)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_dict_add(headers, "Host", host, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_dict_add(headers, "Connection", "keep-alive", 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_dict_add(headers, "Expect", "100-continue", 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  snprintf(data_len_str, sizeof (data_len_str), "%u", data_len);
  ret2 = dpl_dict_add(headers, "Content-Length", data_len_str, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  acl_str = dpl_canned_acl_str(canned_acl);
  if (NULL == acl_str)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret2 = dpl_dict_add(headers, "x-amz-acl", acl_str, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (NULL != metadata)
    {
      ret2 = dpl_add_metadata_to_headers(metadata, headers);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
    }

  ret2 = linux_gethostbyname_r(host, &hret, hbuf, sizeof (hbuf), &hresult, &herr); 
  if (0 != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (AF_INET != hresult->h_addrtype)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  memcpy(&addr, hresult->h_addr_list[0], hresult->h_length);

  conn = dpl_conn_open(ctx, addr, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  //build request
  ret2 = dpl_build_s3_request(ctx, 
                              "PUT",
                              bucket,
                              resource,
                              subresource,
                              NULL,
                              headers, 
                              header,
                              sizeof (header),
                              &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  //final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(1, "writev failed");
      connection_close = 1;
      ret = DPL_ENOENT; //mapped to 404
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, NULL, NULL, &headers_returned);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          ret = DPL_ENOENT;
          goto end;
        }
      else
        {
          DPLERR(0, "read http answer failed");
          connection_close = 1;
          ret = DPL_ENOENT; //mapped to 404
          goto end;
        }
    }
  else
    {
      if (NULL != headers_returned) //possible if continue succeeded
        connection_close = dpl_connection_close(headers_returned);
    }

  (void) dpl_log_charged_event(ctx, "DATA", "IN", data_len);

  vfile = dpl_vfile_new(conn);
  if (NULL == vfile)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (NULL != vfilep)
    {
      *vfilep = vfile;
      vfile = NULL; //consume it
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != vfile)
    dpl_vfile_free(vfile);

  if (NULL != conn)
    {
      if (1 == connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != headers)
    dpl_dict_free(headers);

  if (NULL != headers_returned)
    dpl_dict_free(headers_returned);

  DPRINTF("ret=%d\n", ret);

  return ret;
}

dpl_status_t
dpl_vfile_write_all(dpl_vfile_t *vfile,
                    char *buf,
                    u_int len)
{
  struct iovec iov;
  int ret;
  
  if (1 == vfile->conn->ctx->encrypt)
    {
      char *obuf;
      int olen;

      obuf = malloc(len);
      if (NULL == obuf)
        return DPL_FAILURE;
      
      ret = EVP_CipherUpdate(vfile->cipher_ctx, (u_char *) obuf, &olen, (u_char *) buf, len);
      if (0 == ret)
        {
          DPLERR(0, "CipherUpdate failed\n");
          free(obuf);
          return DPL_FAILURE;
        }

      iov.iov_base = obuf;
      iov.iov_len = olen;

      ret = dpl_conn_writev_all(vfile->conn, &iov, 1, DPL_DEFAULT_WRITE_TIMEOUT);

      if (DPL_SUCCESS != ret)
        {
          DPLERR(0, "writev failed\n");
          free(obuf);
          return DPL_FAILURE;
        }

      //printf("len=%d olen=%d\n", len, olen);

      free(obuf);
      
      return DPL_SUCCESS;
    }
  else
    {
      iov.iov_base = buf;
      iov.iov_len = len;

      return dpl_conn_writev_all(vfile->conn, &iov, 1, DPL_DEFAULT_WRITE_TIMEOUT);
    }

  return DPL_FAILURE;
}

dpl_status_t
dpl_vfile_open(dpl_ctx_t *ctx,
               char *bucket,
               dpl_ino_t ino,
               char *path,
               u_int mode)
{
  //dpl_vfile_t *vfile;
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;

  ret2 = dpl_vdir_namei(ctx, path, bucket, ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  return ret;
}

dpl_status_t
dpl_vfile_close()
{
  return DPL_FAILURE;
}

dpl_status_t
dpl_vfile_read()
{
  return DPL_FAILURE;
}

dpl_status_t
dpl_vfile_write()
{
  return DPL_FAILURE;
}

dpl_status_t
dpl_vfile_pread()
{
  return DPL_FAILURE;
}
