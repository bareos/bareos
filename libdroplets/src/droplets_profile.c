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

#include <dropletsp.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

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
               char *buf,
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
            return DPL_FAILURE;

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
                return DPL_FAILURE;
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
        return DPL_FAILURE;

    cont:
      i++;
    }

  return DPL_SUCCESS;
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
             char *var,
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
          fprintf(stderr, "invalid value '%s'\n", var);
          return -1;
        }
    }
  else if (!strcmp(var, "host"))
    {
      ctx->host = strdup(value);
      if (NULL == ctx->host)
        return -1;
    }
  else if (!strcmp(var, "access_key"))
    {
      ctx->access_key = strdup(value);
      if (NULL == ctx->access_key)
        return -1;
    }
  else if (!strcmp(var, "secret_key"))
    {
      ctx->secret_key = strdup(value);
      if (NULL == ctx->secret_key)
        return -1;
    }
  else if (!strcmp(var, "ssl_cert_file"))
    {
      if (value[0] != '/')
        {
          snprintf(path, sizeof (path), "%s/%s", ctx->droplets_dir, value);
          ctx->ssl_cert_file = strdup(path);
        }
      else
        ctx->ssl_cert_file = strdup(value);
      if (NULL == ctx->ssl_cert_file)
        return -1;
    }
  else if (!strcmp(var, "ssl_key_file"))
    {
      if (value[0] != '/')
        {
          snprintf(path, sizeof (path), "%s/%s", ctx->droplets_dir, value);
          ctx->ssl_key_file = strdup(path);
        }
      else
        ctx->ssl_key_file = strdup(value);
      if (NULL == ctx->ssl_key_file)
        return -1;
    }
  else if (!strcmp(var, "ssl_password"))
    {
      ctx->ssl_password = strdup(value);
      if (NULL == ctx->ssl_password)
        return -1;
    }
  else if (!strcmp(var, "ssl_ca_list"))
    {
      ctx->ssl_ca_list = strdup(value);
      if (NULL == ctx->ssl_ca_list)
        return -1;
    }
  else if (!strcmp(var, "pricing"))
    {
      ctx->pricing = strdup(value);
      if (NULL == ctx->pricing)
        return -1;
    }
  else if (!strcmp(var, "read_buf_size"))
    {
      ctx->read_buf_size = strtoul(value, NULL, 0);
    }
  else if (!strcmp(var, "encrypt_key"))
    {
      ctx->encrypt_key = strdup(value);
      if (NULL == ctx->encrypt_key)
        return -1;
    }
  else
    {
      fprintf(stderr, "no such variable '%s'\n", var);
      return -1;
    }

  return 0;
}

dpl_status_t
dpl_profile_parse(dpl_ctx_t *ctx,
                  char *path)
{
  struct dpl_conf_ctx *conf_ctx;
  char buf[4096];
  ssize_t cc;
  int fd = -1;
  int ret, ret2;

  conf_ctx = dpl_conf_new(conf_cb_func, ctx);
  if (NULL == conf_ctx)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  fd = open(path, O_RDONLY);
  if (-1 == fd)
    {
      fprintf(stderr, "droplets: error opening %s\n", path);
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

  dpl_conf_free(conf_ctx);

  ret = DPL_SUCCESS;

 end:

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
  ctx->n_conn_buckets = DPL_DEFAULT_N_CONN_BUCKETS;
  ctx->n_conn_max = DPL_DEFAULT_N_CONN_MAX;
  ctx->n_conn_max_hits = DPL_DEFAULT_N_CONN_MAX_HITS;
  ctx->conn_timeout = DPL_DEFAULT_CONN_TIMEOUT;
  ctx->conn_idle_time = DPL_DEFAULT_CONN_IDLE_TIME;
  ctx->read_timeout = DPL_DEFAULT_READ_TIMEOUT;
  ctx->write_timeout = DPL_DEFAULT_WRITE_TIMEOUT;
  ctx->use_https = 0;
  ctx->port = -1;
  ctx->pricing = NULL;
  ctx->read_buf_size = DPL_DEFAULT_READ_BUF_SIZE;

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

  snprintf(path, sizeof (path), "%s/%s.csv", ctx->droplets_dir, ctx->profile_name);
  
  ctx->event_log = fopen(path, "a+");
  if (NULL == ctx->event_log)
    return DPL_FAILURE;

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
  int ret, ret2;

  //sanity checks
  if (NULL == ctx->host)
    {
      fprintf(stderr, "missing 'host' in profile\n");
      ret = DPL_FAILURE;
      goto end;
    }

  if (-1 == ctx->port)
    {
      if (0 == ctx->use_https)
        ctx->port = 80;
      else
        ctx->port = 443;
    }

  //ssl stuff
  if (1 == ctx->use_https)
    {
      SSL_METHOD *method;

      if (NULL == ctx->ssl_cert_file ||
          NULL == ctx->ssl_key_file ||
          NULL == ctx->ssl_password)
        return DPL_EINVAL;

      ctx->ssl_bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);
      if (NULL == ctx->ssl_bio_err)
        {
          ret = DPL_FAILURE;
          goto end;
        }
  
      method = SSLv23_method();
      ctx->ssl_ctx = SSL_CTX_new(method);
  
      if (!(SSL_CTX_use_certificate_chain_file(ctx->ssl_ctx, ctx->ssl_cert_file)))
        {
          BIO_printf(ctx->ssl_bio_err, "use_certificate_chain_file: ");
          ERR_print_errors(ctx->ssl_bio_err);
          BIO_printf(ctx->ssl_bio_err, "\n");
          ret = DPL_FAILURE;
          goto end;
        }

      SSL_CTX_set_default_passwd_cb(ctx->ssl_ctx, passwd_cb);
      SSL_CTX_set_default_passwd_cb_userdata(ctx->ssl_ctx, ctx);
  
      if (!(SSL_CTX_use_PrivateKey_file(ctx->ssl_ctx, ctx->ssl_key_file, SSL_FILETYPE_PEM)))
        {
          BIO_printf(ctx->ssl_bio_err, "use_private_key_file: ");
          ERR_print_errors(ctx->ssl_bio_err);
          BIO_printf(ctx->ssl_bio_err, "\n");
          ret = DPL_FAILURE;
          goto end;
        }

      if (NULL != ctx->ssl_ca_list)
        {
          if (!(SSL_CTX_load_verify_locations(ctx->ssl_ctx, ctx->ssl_ca_list, 0)))
            {
              BIO_printf(ctx->ssl_bio_err, "load_verify_location: ");
              ERR_print_errors(ctx->ssl_bio_err);
              BIO_printf(ctx->ssl_bio_err, "\n");
              ret = DPL_FAILURE;
              goto end;
            }
        }
    }

  //pricing
  if (NULL != ctx->pricing)
    {
      ret2 = dpl_pricing_load(ctx);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_FAILURE;
          goto end;
        }
    }
  
  //encrypt
  if (NULL == ctx->encrypt_key)
    {
      fprintf(stderr, "misding 'encrypt_key'\n");
      ret = DPL_FAILURE;
      goto end;
    }

  OpenSSL_add_all_digests();
  OpenSSL_add_all_ciphers();

  //event log
  ret2 = dpl_open_event_log(ctx);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  //connection pool
  ret2 = dpl_conn_pool_init(ctx);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  return ret;
}

/** 
 * load profile
 * 
 * @param ctx 
 * @param droplets_dir if NULL loads ~/.droplets
 * @param profile_name if NULL then default
 * 
 * @return 
 */
dpl_status_t
dpl_profile_load(dpl_ctx_t *ctx,
                 char *droplets_dir,
                 char *profile_name)
{
  char path[1024];
  char default_dir[1024];
  struct passwd *pwd;
  int ret, ret2;

  ret2 = dpl_profile_default(ctx);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }
  
  if (NULL == droplets_dir)
    {
      droplets_dir = getenv("DPLDIR");
      
      if (NULL == droplets_dir)
        {
          pwd = getpwuid(getuid());
          if (NULL == pwd)
            {
              fprintf(stderr, "unable to get home directory\n");
              return DPL_SUCCESS;
            }
          
          snprintf(default_dir, sizeof (default_dir), "%s/.droplets", pwd->pw_dir);
          droplets_dir = default_dir;
        }
    }

  if (NULL == profile_name)
    {
      profile_name = getenv("DPLPROFILE");
      
      if (NULL == profile_name)
        profile_name = "default";
    }

  ctx->droplets_dir = strdup(droplets_dir);
  if (NULL == ctx->droplets_dir)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ctx->profile_name = strdup(profile_name);
  if (NULL == ctx->profile_name)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  snprintf(path, sizeof (path), "%s/%s.profile", droplets_dir, profile_name);
      
  ret2 = dpl_profile_parse(ctx, path);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret = dpl_profile_post(ctx);
  if (DPL_SUCCESS != ret)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret = DPL_SUCCESS;

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

  if (1 == ctx->use_https)
    {
      SSL_CTX_free(ctx->ssl_ctx);
      BIO_vfree(ctx->ssl_bio_err);
    }

  /*
   * profile
   */
  if (NULL != ctx->host)
    free(ctx->host);
  if (NULL != ctx->access_key)
    free(ctx->access_key);
  if (NULL != ctx->secret_key)
    free(ctx->secret_key);
  if (NULL != ctx->ssl_cert_file)
    free(ctx->ssl_cert_file);
  if (NULL != ctx->ssl_key_file)
    free(ctx->ssl_key_file);
  if (NULL != ctx->ssl_password)
    free(ctx->ssl_password);
  if (NULL != ctx->ssl_ca_list)
    free(ctx->ssl_ca_list);
  if (NULL != ctx->pricing)
    free(ctx->pricing);
  if (NULL != ctx->encrypt_key)
    free(ctx->encrypt_key);

  /**/

  if (NULL != ctx->droplets_dir)
    free(ctx->droplets_dir);
  if (NULL != ctx->profile_name)
    free(ctx->profile_name);

}
