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
#include <dropletp.h>

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

struct ssl_method_authorized {
  const char            *name;
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
  const SSL_METHOD      *(*fn)(void);
#else
  SSL_METHOD            *(*fn)(void);
#endif
};

static struct ssl_method_authorized
list_ssl_method_authorized[] = {
#ifndef OPENSSL_NO_SSL2
#ifdef SSL_TXT_SSLV2
  { .name = SSL_TXT_SSLV2,   .fn = SSLv2_method   },
#endif
#endif
#ifdef SSL_TXT_SSLV3
  { .name = SSL_TXT_SSLV3,   .fn = SSLv3_method   },
#endif
  { .name = "SSLv23",        .fn = SSLv23_method  },
#ifdef SSL_TXT_TLSV1
  { .name = SSL_TXT_TLSV1,   .fn = TLSv1_method   },
#endif
#ifdef SSL_TXT_TLSV1_1
  { .name = SSL_TXT_TLSV1_1, .fn = TLSv1_1_method },
#endif
#ifdef SSL_TXT_TLSV1_2
  { .name = SSL_TXT_TLSV1_2, .fn = TLSv1_2_method },
#endif
  { .name = "",              .fn = NULL           }
};

static void
cbuf_reset(struct dpl_conf_buf *cbuf)
{
  cbuf->buf[0] = 0;
  cbuf->pos = 0;
}

static int
cbuf_add_char(struct dpl_conf_buf *cbuf,
              char c)
{
  if (cbuf->pos < DPL_CONF_MAX_BUF)
    {
      cbuf->buf[cbuf->pos++] = c;
      cbuf->buf[cbuf->pos] = 0;
      return 0;
    }
  else
    return -1;
}

struct dpl_conf_ctx *
dpl_conf_new(dpl_conf_cb_func_t cb_func,
             void *cb_arg)
{
  struct dpl_conf_ctx *ctx;

  ctx = malloc(sizeof (*ctx));
  if (NULL == ctx)
    return NULL;

  memset(ctx, 0, sizeof (*ctx));

  ctx->backslash = 0;
  ctx->comment = 0;
  ctx->quote = 0;
  ctx->cb_func = cb_func;
  ctx->cb_arg = cb_arg;

  cbuf_reset(&ctx->var_cbuf);
  cbuf_reset(&ctx->value_cbuf);
  ctx->cur_cbuf = &ctx->var_cbuf;

  return ctx;
}

void
dpl_conf_free(struct dpl_conf_ctx *ctx)
{
  free(ctx);
}

dpl_status_t
dpl_conf_parse(struct dpl_conf_ctx *ctx,
               const char *buf,
               int len)
{
  int i, ret;

  i = 0;
  while (i < len)
    {
      char c;

      c = buf[i];

      if (ctx->comment)
	{
	  if (c == '\n')
	    ctx->comment = 0;
	  else
	    i++;

	  continue ;
	}

      if (ctx->backslash)
	{
	  if (c == 'n')
	    c = '\n';
	  else
	    if (c == 'r')
	      c = '\r';
	    else
	      if (c == 't')
		c = '\t';

          ret = cbuf_add_char(ctx->cur_cbuf, c);
          if (-1 == ret)
            goto cbuf_error;

	  ctx->backslash = 0;
	  goto cont;
	}

      if (c == '\\')
	{
	  ctx->backslash = 1;
	  goto cont;
	}

      if (ctx->quote)
	{
	  if (c == '"')
	    ctx->quote = 0;
	  else
            {
              ret = cbuf_add_char(ctx->cur_cbuf, c);
              if (-1 == ret)
                goto cbuf_error;
            }

	  goto cont;
	}

      if (c == '"')
	{
	  ctx->quote = 1;
	  goto cont;
	}

      if (c == '#')
	{
	  ctx->comment = 1;
	  goto cont;
	}

      if (ctx->cur_cbuf != &ctx->value_cbuf)
	if (c == '=')
	  {
	    ctx->cur_cbuf = &ctx->value_cbuf;
	    goto cont;
	  }

      if (c == ' ' || c == '\t')
	goto cont;

      if (c == '\n' || c == ';')
	{
	  ret = ctx->cb_func(ctx->cb_arg,
                             ctx->var_cbuf.buf,
                             ctx->value_cbuf.buf);
          if (-1 == ret)
            return DPL_FAILURE;

	  cbuf_reset(&ctx->var_cbuf);
	  cbuf_reset(&ctx->value_cbuf);

	  ctx->cur_cbuf = &ctx->var_cbuf;
	  goto cont;
	}

      ret = cbuf_add_char(ctx->cur_cbuf, c);
      if (-1 == ret)
        goto cbuf_error;

    cont:
      i++;
    }

  return DPL_SUCCESS;

 cbuf_error:
  DPL_LOG(NULL, DPL_ERROR, "error appending to configuration");

  return DPL_FAILURE;
}

dpl_status_t
dpl_conf_finish(struct dpl_conf_ctx *ctx)
{
  int ret;

  ret = ctx->cb_func(ctx->cb_arg,
                     ctx->var_cbuf.buf,
                     ctx->value_cbuf.buf);
  if (-1 == ret)
    return DPL_FAILURE;

  return DPL_SUCCESS;
}

/*
 *
 */
static int
conf_cb_func(void *cb_arg,
             const char *var,
             char *value)
{
  dpl_ctx_t *ctx = (dpl_ctx_t *) cb_arg;
  char path[1024];

  DPRINTF("%s %s\n", var, value);

  if (!strcmp(var, ""))
    return 0;
  else if (!strcmp(var, "use_https"))
    {
      if (!strcasecmp(value, "true"))
        ctx->use_https = 1;
      else if (!strcasecmp(value, "false"))
        ctx->use_https = 0;
      else
        {
	  DPL_LOG(ctx, DPL_ERROR, "invalid value '%s'", var);
          return -1;
        }
    }
  else if (!strcmp(var, "host"))
    {
      if (NULL != ctx->addrlist)
        {
          DPL_LOG(ctx, DPL_ERROR, "address list already defined");
          return -1;
        }

      ctx->addrlist = dpl_addrlist_create_from_str(ctx->use_https ? "443" : "80", value);
      if (NULL == ctx->addrlist)
        {
          DPL_LOG(ctx, DPL_ERROR, "error parsing address list");
          return -1;
        }
    }
  else if (!strcmp(var, "port"))
    {
      DPL_LOG(ctx, DPL_WARNING, "The 'port' option in configuration file is ignored.");
      DPL_LOG(ctx, DPL_WARNING, "The port(s) must be specified in the hostname(s).");
      DPL_LOG(ctx, DPL_WARNING, "If nothing is specified, 80 is used (or 443 for https).");
    }
  else if (!strcmp(var, "blacklist_expiretime"))
    {
      ctx->blacklist_expiretime = atoi(value);
    }
  else if (! strcmp(var, "header_size"))
    {
      ctx->header_size = strtoul(value, NULL, 0);
    }
  else if (!strcmp(var, "base_path"))
    {
      int value_len = strlen(value);

      if (strcmp(value, "/"))
        {
          if (value_len >= 1 && value[value_len-1] == '/')
            {
              DPL_LOG(ctx, DPL_ERROR, "base_path must not end with a delimiter");
              return -1;
            }
        }

      free(ctx->base_path);
      ctx->base_path = strdup(value);
      if (NULL == ctx->base_path)
        return -1;
    }
  else if (!strcmp(var, "access_key"))
    {
      free(ctx->access_key);
      ctx->access_key = strdup(value);
      if (NULL == ctx->access_key)
        return -1;
    }
  else if (!strcmp(var, "secret_key"))
    {
      free(ctx->secret_key);
      ctx->secret_key = strdup(value);
      if (NULL == ctx->secret_key)
        return -1;
    }
  else if (!strcmp(var, "aws_auth_sign_version"))
    {
      ctx->aws_auth_sign_version = atoi(value);
      if (ctx->aws_auth_sign_version != 2 && ctx->aws_auth_sign_version != 4) {
        DPL_LOG(ctx, DPL_ERROR, "aws_auth_sign_version must be set 2 or 4");
        return -1;
      }        
    }
  else if (!strcmp(var, "aws_region"))
    {
      strncpy(ctx->aws_region, value, sizeof(ctx->aws_region));
    }
  else if (!strcmp(var, "ssl_cert_file"))
    {
      free(ctx->ssl_cert_file);
      if (value[0] != '/')
        {
          snprintf(path, sizeof (path), "%s/%s", ctx->droplet_dir, value);
          ctx->ssl_cert_file = strdup(path);
        }
      else
        ctx->ssl_cert_file = strdup(value);
      if (NULL == ctx->ssl_cert_file)
        return -1;
    }
  else if (!strcmp(var, "ssl_method"))
    {
      char                              allow_ssl_methods[128] = "";
      unsigned char                     i;
      struct ssl_method_authorized      *ssl_method = NULL;

      i = 0;
      while (1) {
        ssl_method = &list_ssl_method_authorized[i];
        if (ssl_method->fn == NULL) {
          ssl_method = NULL;
          break;
        }

        if (*allow_ssl_methods != '\0')
          strcat(allow_ssl_methods, ", ");
        strcat(allow_ssl_methods, ssl_method->name);

        if (!strcmp(ssl_method->name, value))
          break;

        i++;
      }

      if (ssl_method != NULL)
        ctx->ssl_method = (*ssl_method->fn)();
      else {
        DPL_LOG(ctx, DPL_ERROR, "ssl_method must be defined by a value among %s", allow_ssl_methods);
        return -1;
      }
    }
  else if (!strcmp(var, "ssl_cipher_list"))
    {
      free(ctx->ssl_cipher_list);
      ctx->ssl_cipher_list = strdup(value);
      if (NULL == ctx->ssl_cipher_list) {
        DPL_LOG(ctx, DPL_ERROR, "memory error");
        return DPL_ENOMEM;
      }
      if (ctx->ssl_cipher_list == NULL)
        return -1;
  }
  else if (!strcmp(var, "ssl_key_file"))
    {
      free(ctx->ssl_key_file);
      if (value[0] != '/')
        {
          snprintf(path, sizeof (path), "%s/%s", ctx->droplet_dir, value);
          ctx->ssl_key_file = strdup(path);
        }
      else
        ctx->ssl_key_file = strdup(value);
      if (NULL == ctx->ssl_key_file)
        return -1;
    }
  else if (!strcmp(var, "ssl_password"))
    {
      free(ctx->ssl_password);
      ctx->ssl_password = strdup(value);
      if (NULL == ctx->ssl_password)
        return -1;
    }
  else if (!strcmp(var, "ssl_ca_list"))
    {
      free(ctx->ssl_ca_list);
      ctx->ssl_ca_list = strdup(value);
      if (NULL == ctx->ssl_ca_list)
        return -1;
    }
  else if (!strcmp(var, "ssl_crl_list"))
    {
      free(ctx->ssl_crl_list);
      ctx->ssl_crl_list = strdup(value);
      if (NULL == ctx->ssl_crl_list)
        return -1;
    }
  else if (!strcmp(var, "cert_verif"))
    {
      if (!strcasecmp(value, "true"))
        ctx->cert_verif = DPL_DEFAULT_SSL_CERT_VERIF;
      else if (!strcasecmp(value, "false"))
        ctx->cert_verif = 0;
      else
        {
          DPL_LOG(ctx, DPL_ERROR, "invalid boolean value for '%s'", var);
          return -1;
        }
    }
  else if (!strcmp(var, "ssl_comp"))
    {
      if (!strcasecmp(value, "true"))
        ctx->ssl_comp = 1;
      else if (!strcasecmp(value, "false"))
        ctx->ssl_comp = DPL_DEFAULT_SSL_COMP_NONE;
      else
        {
          DPL_LOG(ctx, DPL_ERROR, "invalid boolean value for '%s'", var);
          return -1;
        }
    }
  else if (!strcmp(var, "pricing"))
    {
      free(ctx->pricing);
      ctx->pricing = strdup(value);
      if (NULL == ctx->pricing)
        return -1;
    }
  else if (!strcmp(var, "pricing_dir"))
    {
      free(ctx->pricing_dir);
      ctx->pricing_dir = strdup(value);
      if (NULL == ctx->pricing_dir)
        return -1;
    }
  else if (!strcmp(var, "read_buf_size"))
    {
      ctx->read_buf_size = strtoul(value, NULL, 0);
    }
  else if (!strcmp(var, "encrypt_key"))
    {
      free(ctx->encrypt_key);
      ctx->encrypt_key = strdup(value);
      if (NULL == ctx->encrypt_key)
        return -1;
    }
  else if (!strcmp(var, "backend"))
    {
      ctx->backend = dpl_backend_find(value);
      if (NULL == ctx->backend)
        {
          DPL_LOG(ctx, DPL_ERROR, "no such backend '%s'", value);
          return -1;
        }
    }
  else if (!strcmp(var, "encode_slashes"))
    {
      if (!strcasecmp(value, "true"))
        ctx->encode_slashes = 1;
      else if (!strcasecmp(value, "false"))
        ctx->encode_slashes = 0;
      else
        {
          DPL_LOG(ctx, DPL_ERROR, "invalid boolean value for '%s'", var);
          return -1;
        }
    }
  else if (!strcmp(var, "empty_folder_emulation"))
    {
      if (!strcasecmp(value, "true"))
        ctx->empty_folder_emulation = 1;
      else if (!strcasecmp(value, "false"))
        ctx->empty_folder_emulation = 0;
      else
        {
          DPL_LOG(ctx, DPL_ERROR, "invalid boolean value for '%s'", var);
          return -1;
        }
    }
  else if (!strcmp(var, "keep_alive"))
    {
      if (!strcasecmp(value, "true"))
        ctx->keep_alive = 1;
      else if (!strcasecmp(value, "false"))
        ctx->keep_alive = 0;
      else
        {
          DPL_LOG(ctx, DPL_ERROR, "invalid boolean value for '%s'", var);
          return -1;
        }
    }
  else if (!strcmp(var, "url_encoding"))
    {
      if (!strcasecmp(value, "true"))
        ctx->url_encoding = 1;
      else if (!strcasecmp(value, "false"))
        ctx->url_encoding = 0;
      else
        {
          DPL_LOG(ctx, DPL_ERROR, "invalid boolean value for '%s'", var);
          return -1;
        }
    }
  else if (!strcmp(var, "enterprise_number"))
    {
      ctx->enterprise_number = strtoul(value, NULL, 0);
    }
  else if (! strcmp(var, "connection_timeout"))
    {
      ctx->conn_timeout = strtoul(value, NULL, 0);
    }
  else if (! strcmp(var, "read_timeout"))
    {
      ctx->read_timeout = strtoul(value, NULL, 0);
    }
  else if (! strcmp(var, "write_timeout"))
    {
      ctx->write_timeout = strtoul(value, NULL, 0);
    }
  else if (! strcmp(var, "droplet_dir") ||
	   ! strcmp(var, "profile_name"))
    {
      /* silently ignore these if we see them in the profile
       * file; they're handled elsewhere if we load a dict. */
      return 0;
    }
  else
    {
      DPL_LOG(ctx, DPL_ERROR, "no such variable '%s'", var);
      return -1;
    }

  return 0;
}

dpl_status_t
dpl_profile_parse(dpl_ctx_t *ctx,
                  const char *path)
{
  struct dpl_conf_ctx *conf_ctx = NULL;
  char buf[4096];
  ssize_t cc;
  int fd = -1;
  int ret, ret2;

  conf_ctx = dpl_conf_new(conf_cb_func, ctx);
  if (NULL == conf_ctx)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  fd = open(path, O_RDONLY);
  if (-1 == fd)
    {
      DPL_LOG(ctx, DPL_ERROR, "error opening '%s': %s",
		path, strerror(errno));
      ret = DPL_FAILURE;
      goto end;
    }

  while (1)
    {
      cc = read(fd, buf, sizeof (buf));
      if (0 == cc)
        break ;

      if (-1 == cc)
        {
          DPL_LOG(ctx, DPL_ERROR, "error reading from '%s': %s",
                  path, strerror(errno));
          ret = DPL_FAILURE;
          goto end;
        }

      ret2 = dpl_conf_parse(conf_ctx, buf, cc);
      if (DPL_FAILURE == ret2)
        {
          ret = DPL_FAILURE;
          goto end;
        }
    }

  ret2 = dpl_conf_finish(conf_ctx);
  if (DPL_FAILURE == ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (conf_ctx)
    dpl_conf_free(conf_ctx);

  if (-1 != fd)
    (void) close(fd);

  return ret;
}

/**
 * provide default values for config
 *
 * @param ctx
 */
dpl_status_t
dpl_profile_default(dpl_ctx_t *ctx)
{
  ctx->header_size = DPL_DEFAULT_HEADER_SIZE;
  ctx->n_conn_buckets = DPL_DEFAULT_N_CONN_BUCKETS;
  ctx->n_conn_max = DPL_DEFAULT_N_CONN_MAX;
  ctx->n_conn_max_hits = DPL_DEFAULT_N_CONN_MAX_HITS;
  ctx->conn_idle_time = DPL_DEFAULT_CONN_IDLE_TIME;
  ctx->conn_timeout = DPL_DEFAULT_CONN_TIMEOUT;
  ctx->read_timeout = DPL_DEFAULT_READ_TIMEOUT;
  ctx->write_timeout = DPL_DEFAULT_WRITE_TIMEOUT;
  ctx->use_https = 0;
  ctx->addrlist = NULL;
  ctx->blacklist_expiretime = 10;
  ctx->pricing = NULL;
  ctx->pricing_dir = NULL;
  ctx->read_buf_size = DPL_DEFAULT_READ_BUF_SIZE;
  ctx->backend = dpl_backend_find("s3");
  assert(NULL != ctx->backend);
  ctx->encode_slashes = 0;
  ctx->keep_alive = 1;
  ctx->url_encoding = 1;
  ctx->max_redirects = DPL_DEFAULT_MAX_REDIRECTS;
  ctx->enterprise_number = DPL_DEFAULT_ENTERPRISE_NUMBER;
  ctx->base_path = strdup(DPL_DEFAULT_BASE_PATH);
  if (NULL == ctx->base_path)
    return DPL_ENOMEM;
  ctx->aws_auth_sign_version = DPL_DEFAULT_AWS_AUTH_SIGN_VERSION;
  strncpy(ctx->aws_region, DPL_DEFAULT_AWS_REGION, sizeof(ctx->aws_region));
  ctx->ssl_method = DPL_DEFAULT_SSL_METHOD;
  ctx->ssl_cipher_list = strdup(DPL_DEFAULT_SSL_CIPHER_LIST);
  if (NULL == ctx->ssl_cipher_list)
    return DPL_ENOMEM;
  ctx->ssl_comp = DPL_DEFAULT_SSL_COMP_NONE;
  ctx->cert_verif = DPL_DEFAULT_SSL_CERT_VERIF;

  return DPL_SUCCESS;
}

static int
passwd_cb(char *buf,
          int num,
          int rwflag,
          void *userdata)
{
  dpl_ctx_t *ctx = (dpl_ctx_t *) userdata;

  if (num < strlen(ctx->ssl_password) + 1)
    return 0;

  strcpy(buf, ctx->ssl_password);

  return strlen(buf);
}

dpl_status_t
dpl_open_event_log(dpl_ctx_t *ctx)
{
  char path[1024];
  char *pricing_dir;

  pricing_dir = ctx->pricing_dir;
  if (pricing_dir == NULL)
    pricing_dir = ctx->droplet_dir;

  if (*pricing_dir == '\0') {
    ctx->event_log = NULL;
    return DPL_SUCCESS;
  }

  snprintf(path, sizeof (path), "%s/%s.csv", pricing_dir, ctx->profile_name);

  ctx->event_log = fopen(path, "a+");
  if (ctx->event_log == NULL) {
    if (errno != ENOENT) {
      DPL_LOG(ctx, DPL_ERROR, "error opening '%s': %s", path, strerror(errno));
      return DPL_FAILURE;
    } else
      DPL_LOG(ctx, DPL_WARNING, "error opening '%s': %s", path, strerror(errno));
  }

  return DPL_SUCCESS;
}

void
dpl_close_event_log(dpl_ctx_t *ctx)
{
  if (NULL != ctx->event_log)
    {
      (void) fclose(ctx->event_log);
    }
}

static dpl_status_t
dpl_profile_init(dpl_ctx_t *ctx,
		 const char *droplet_dir,
		 const char *profile_name)
{
  char default_dir[1024];
  struct passwd *pwd;
  int ret;

  ret = dpl_profile_default(ctx);
  if (DPL_SUCCESS != ret)
    return ret;

  if (NULL == droplet_dir)
    {
      droplet_dir = getenv("DPLDIR");

      if (NULL == droplet_dir)
        {
          pwd = getpwuid(getuid());
          if (NULL == pwd)
            {
              DPL_LOG(ctx, DPL_ERROR, "unable to get home directory");
              return DPL_SUCCESS;
            }

          snprintf(default_dir, sizeof (default_dir), "%s/.droplet", pwd->pw_dir);
          droplet_dir = default_dir;
        }
    }

  if (NULL == profile_name)
    {
      profile_name = getenv("DPLPROFILE");

      if (NULL == profile_name)
        profile_name = "default";
    }

  ctx->droplet_dir = strdup(droplet_dir);
  if (NULL == ctx->droplet_dir)
      return DPL_ENOMEM;

  ctx->profile_name = strdup(profile_name);
  if (NULL == ctx->profile_name)
      return DPL_ENOMEM;

  return DPL_SUCCESS;
}

static int
ssl_verify_cert(X509_STORE_CTX *cert, void *arg)
{
  dpl_ctx_t     *ctx = (dpl_ctx_t *) arg;
  char          buffer[256];

  DPL_TRACE(ctx, DPL_TRACE_SSL, "Server certificates:");

  X509_NAME_oneline(X509_get_subject_name(cert->cert), buffer, sizeof(buffer));
  DPL_TRACE(ctx, DPL_TRACE_SSL, "Subject: %s", buffer);

  X509_NAME_oneline(X509_get_issuer_name(cert->cert), buffer, sizeof(buffer));
  DPL_TRACE(ctx, DPL_TRACE_SSL, "Issuer: %s", buffer);

  return 1;
}

static dpl_status_t
dpl_ssl_profile_post(dpl_ctx_t *ctx)
{
  OpenSSL_add_all_digests();
  OpenSSL_add_all_ciphers();

  ctx->ssl_ctx = SSL_CTX_new(ctx->ssl_method);
  if (ctx->ssl_ctx == NULL) {
    DPL_SSL_PERROR(ctx, "SSL_CTX_new");
    DPL_TRACE(ctx, DPL_TRACE_ERR, "error while creating new SSL context");
    return DPL_FAILURE;
  }

  if (SSL_CTX_set_cipher_list(ctx->ssl_ctx, ctx->ssl_cipher_list) == 0) {
    DPL_SSL_PERROR(ctx, "SSL_CTX_set_cipher_list");
    return DPL_FAILURE;
  }

  if (ctx->ssl_password != NULL) {
    SSL_CTX_set_default_passwd_cb(ctx->ssl_ctx, passwd_cb);
    SSL_CTX_set_default_passwd_cb_userdata(ctx->ssl_ctx, ctx);
  }

  if (DPL_DEFAULT_SSL_CERT_VERIF == ctx->cert_verif)
    SSL_CTX_set_verify(ctx->ssl_ctx, SSL_VERIFY_PEER, NULL);
  else if (0 == ctx->cert_verif)
    SSL_CTX_set_verify(ctx->ssl_ctx, SSL_VERIFY_NONE, NULL);

  if (ctx->ssl_crl_list != NULL) {
    /* load CRL in the X509_STORE */
    X509_STORE *cert_store;
    X509_CRL *cert_crl; 
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
    X509_VERIFY_PARAM *cert_verif_param;
#endif
    BIO *in = NULL;
    int ret = 0;
    char *crl_issuer = NULL;

    cert_store = SSL_CTX_get_cert_store(ctx->ssl_ctx);

    in = BIO_new(BIO_s_file());
    BIO_read_filename(in, ctx->ssl_crl_list);
    cert_crl = PEM_read_bio_X509_CRL(in, NULL, NULL, NULL);
    if (NULL == cert_crl) {
       DPL_TRACE(ctx, DPL_TRACE_ERR, "unable to load CRL from file: %s", ctx->ssl_crl_list);
       return DPL_FAILURE;
    } else {
       crl_issuer = X509_NAME_oneline(X509_CRL_get_issuer(cert_crl), 0, 0);
       DPL_TRACE(ctx, DPL_TRACE_SSL, "sucessufuly loaded CRL from file: %s, and issued by: %s", ctx->ssl_crl_list, crl_issuer);
       OPENSSL_free(crl_issuer);
    }
    ret = X509_STORE_add_crl(cert_store, cert_crl);
    if (!ret) {
      DPL_TRACE(ctx, DPL_TRACE_ERR, "unable to add CRL for SSL certificate verification: %d", ret);
      return DPL_FAILURE;
    }
    if (NULL != cert_crl)
      X509_CRL_free(cert_crl);
    if (NULL != in)
      BIO_free(in);

#if OPENSSL_VERSION_NUMBER >= 0x10000000L
    /* set CRL verification */
    cert_verif_param = X509_VERIFY_PARAM_new();
    X509_VERIFY_PARAM_set_flags(cert_verif_param, X509_V_FLAG_CRL_CHECK);
    SSL_CTX_set1_param(ctx->ssl_ctx, cert_verif_param);
    X509_VERIFY_PARAM_free(cert_verif_param);
#endif
  }

  if (ctx->ssl_cert_file != NULL) {
    if (!SSL_CTX_use_certificate_chain_file(ctx->ssl_ctx, ctx->ssl_cert_file)) {
      DPL_SSL_PERROR(ctx, "SSL_CTX_use_certificate_chain_file");
      return DPL_FAILURE;
    }
  }

  if (ctx->ssl_key_file != NULL) {
    if (!SSL_CTX_use_PrivateKey_file(ctx->ssl_ctx, ctx->ssl_key_file, SSL_FILETYPE_PEM)) {
      DPL_SSL_PERROR(ctx, "SSL_CTX_use_PrivateKey_file");
      return DPL_FAILURE;
    }
  }

  if (ctx->ssl_key_file != NULL && ctx->ssl_cert_file != NULL) {
    if (!SSL_CTX_check_private_key(ctx->ssl_ctx)) {
      unsigned long ssl_err = ERR_get_error();
      char buf[256];
      ERR_error_string_n(ssl_err, buf, sizeof buf);
      DPL_TRACE(ctx, DPL_TRACE_ERR, "Private key does not match the certificate public key: %s", buf);
      DPL_SSL_PERROR(ctx, "SSL_CTX_check_private_key");
      return DPL_FAILURE;
    }
  }

  if (ctx->ssl_ca_list != NULL) {
    if (!SSL_CTX_load_verify_locations(ctx->ssl_ctx, ctx->ssl_ca_list, NULL) || !SSL_CTX_set_default_verify_paths(ctx->ssl_ctx)) {
      unsigned long ssl_err = ERR_get_error();
      char buf[256];
      ERR_error_string_n(ssl_err, buf, sizeof buf);
      DPL_TRACE(ctx, DPL_TRACE_ERR, "Failed to load CA locations: %s", buf);
      DPL_SSL_PERROR(ctx, "SSL_CTX_load_verify_locations");
      return DPL_FAILURE;
    }
  }

  if (DPL_DEFAULT_SSL_COMP_NONE == ctx->ssl_comp) {
#ifdef SSL_OP_NO_COMPRESSION
    DPL_TRACE(ctx, DPL_TRACE_SSL, "disabling SSL compression for OpenSSL >= 1.0.0");
    SSL_CTX_set_options(ctx->ssl_ctx, SSL_OP_NO_COMPRESSION);
#elif OPENSSL_VERSION_NUMBER <= 0x00909000L     /* SSL_OP_NO_COMPRESSION has been added in OpenSSL 0.9.9 */
    STACK_OF(SSL_COMP)* comp_methods = SSL_COMP_get_compression_methods();
    DPL_TRACE(ctx, DPL_TRACE_SSL, "disabling SSL compression for OpenSSL <= 0.9.9");
    /* NB: Additional code for retrieving the name of the default SSL compression method
    if (comp_methods && sk_SSL_COMP_num(comp_methods) > 0) {
      SSL_COMP* default_compression_method = sk_SSL_COMP_pop(comp_methods);
      DPL_TRACE(ctx, DPL_TRACE_SSL, "default SSL compression method: %s", SSL_COMP_get_name(default_compression_method->method));
    } */
    sk_SSL_COMP_zero(comp_methods);
    assert(sk_SSL_COMP_num(comp_methods) == 0);
    if (NULL != comp_methods)
      sk_SSL_COMP_free(comp_methods);
#endif
  }

  return DPL_SUCCESS;
}

/**
 * post processing of profile, e.g. init SSL
 *
 * @param ctx
 *
 * @return
 */

dpl_status_t
dpl_profile_post(dpl_ctx_t *ctx)
{
  dpl_status_t  ret;

  //sanity checks
  if (strcmp(ctx->backend->name, "posix")) {
    if (NULL == ctx->addrlist) {
      DPL_LOG(ctx, DPL_ERROR, "missing 'host' in profile");
      return DPL_FAILURE;
    }
  }

  //ssl stuff
  if (ctx->use_https) {
    ret = dpl_ssl_profile_post(ctx);
    if (ret != DPL_SUCCESS)
      return ret;
  }

  //pricing
  if (NULL != ctx->pricing) {
    ret = dpl_pricing_load(ctx);
    if (DPL_SUCCESS != ret)
      return ret;
  }

  //event log
  ret = dpl_open_event_log(ctx);
  if (DPL_SUCCESS != ret)
    return ret;

  //connection pool
  ret = dpl_conn_pool_init(ctx);
  if (DPL_SUCCESS != ret)
    return ret;

  ctx->cwds = dpl_dict_new(13);
  if (NULL == ctx->cwds)
    return DPL_FAILURE;

  ctx->cur_bucket = strdup("");
  if (NULL == ctx->cur_bucket)
    return DPL_FAILURE;

  return DPL_SUCCESS;
}

/**
 * load profile
 *
 * @param ctx
 * @param droplet_dir if NULL loads ~/.droplet
 * @param profile_name if NULL then default
 *
 * @return
 */
dpl_status_t
dpl_profile_load(dpl_ctx_t *ctx,
                 const char *droplet_dir,
                 const char *profile_name)
{
  char          path[1024];
  dpl_status_t  ret;

  ret = dpl_profile_init(ctx, droplet_dir, profile_name);
  if (DPL_SUCCESS != ret) {
    if (DPL_ENOMEM == ret)
      DPL_LOG(ctx, DPL_ERROR, "No memory for droplet context initialization.");
    else
      DPL_LOG(ctx, DPL_ERROR, "Error during droplet context initialization.");
    return ret;
  }
  snprintf(path, sizeof (path), "%s/%s.profile", ctx->droplet_dir, ctx->profile_name);

  ret = dpl_profile_parse(ctx, path);
  if (DPL_SUCCESS != ret)
    return ret;

  return dpl_profile_post(ctx);
}

static dpl_status_t
profile_dict_cb_func(dpl_dict_var_t *var, void *cb_arg)
{
  int ret;

  assert(var->val->type == DPL_VALUE_STRING);
  ret = conf_cb_func(cb_arg, var->key, dpl_sbuf_get_str(var->val->string));
  return (ret < 0 ? DPL_FAILURE : DPL_SUCCESS);
}

dpl_status_t
dpl_profile_set_from_dict(dpl_ctx_t *ctx,
			  const dpl_dict_t *profile)
{
  const char *droplet_dir;
  const char *profile_name;
  int ret;

  droplet_dir = dpl_dict_get_value(profile, "droplet_dir");
  profile_name = dpl_dict_get_value(profile, "profile_name");

  ret = dpl_profile_init(ctx, droplet_dir, profile_name);
  if (DPL_SUCCESS != ret)
      goto end;

  ret = dpl_dict_iterate(profile, profile_dict_cb_func, ctx);
  if (DPL_SUCCESS != ret)
      goto end;

  ret = dpl_profile_post(ctx);
  if (DPL_SUCCESS != ret)
      goto end;

 end:

  return ret;
}

void
dpl_profile_free(dpl_ctx_t *ctx)
{
  dpl_conn_pool_destroy(ctx);

  dpl_close_event_log(ctx);

  if (NULL != ctx->pricing)
    dpl_pricing_free(ctx);

  if (ctx->ssl_ctx != NULL) {
    SSL_CTX_free(ctx->ssl_ctx);
  }

  /*
   * profile
   */

  if (NULL != ctx->addrlist)
    dpl_addrlist_free(ctx->addrlist);
  if (NULL != ctx->base_path)
    free(ctx->base_path);
  if (NULL != ctx->access_key)
    free(ctx->access_key);
  if (NULL != ctx->secret_key)
    free(ctx->secret_key);
  if (NULL != ctx->ssl_cipher_list)
    free(ctx->ssl_cipher_list);
  if (NULL != ctx->ssl_cert_file)
    free(ctx->ssl_cert_file);
  if (NULL != ctx->ssl_key_file)
    free(ctx->ssl_key_file);
  if (NULL != ctx->ssl_password)
    free(ctx->ssl_password);
  if (NULL != ctx->ssl_ca_list)
    free(ctx->ssl_ca_list);
  if (NULL != ctx->ssl_crl_list)
    free(ctx->ssl_crl_list);
  if (NULL != ctx->pricing)
    free(ctx->pricing);
  if (NULL != ctx->encrypt_key)
    free(ctx->encrypt_key);
  if (NULL != ctx->pricing_dir)
    free(ctx->pricing_dir);

  /**/

  if (NULL != ctx->droplet_dir)
    free(ctx->droplet_dir);
  if (NULL != ctx->profile_name)
    free(ctx->profile_name);

  if (NULL != ctx->cwds)
    dpl_dict_free(ctx->cwds);
  if (NULL != ctx->cur_bucket)
    free(ctx->cur_bucket);

}
