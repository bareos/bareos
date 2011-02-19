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

const char *
dpl_status_str(dpl_status_t status)
{
  switch (status)
    {
    case DPL_SUCCESS:
      return "DPL_SUCCESS";
    case DPL_FAILURE:
      return "DPL_FAILURE";
    case DPL_ENOENT:
      return "DPL_ENOENT";
    case DPL_EINVAL:
      return "DPL_EINVAL";
    case DPL_ETIMEOUT:
      return "DPL_ETIMEOUT";
    case DPL_ENOMEM:
      return "DPL_ENOMEM";
    case DPL_ESYS:
      return "DPL_ESYS";
    case DPL_EIO:
      return "DPL_EIO";
    case DPL_ELIMIT:
      return "DPL_ELIMIT";
    case DPL_ENAMETOOLONG:
      return "DPL_ENAMETOOLONG";
    case DPL_ENOTDIR:
      return "DPL_ENOTDIR";
    case DPL_ENOTEMPTY:
      return "DPL_ENOTEMPTY";
    case DPL_EISDIR:
      return "DPL_EISDIR";
    case DPL_EEXIST:
      return "DPL_EEXIST";
    }

  return "Unknown error";
}

dpl_status_t
dpl_init()
{
  SSL_library_init();
  SSL_load_error_strings();

  return DPL_SUCCESS;
}

void
dpl_free()
{
  ERR_free_strings();
}

dpl_ctx_t *
dpl_ctx_new(const char *droplet_dir,
            const char *profile_name)
{
  dpl_ctx_t *ctx;
  int ret;

  ctx = malloc(sizeof (*ctx));
  if (NULL == ctx)
    return NULL;

  memset(ctx, 0, sizeof (*ctx));

  pthread_mutex_init(&ctx->lock, NULL);

  ret = dpl_profile_load(ctx, droplet_dir, profile_name);
  if (DPL_SUCCESS != ret)
    {
      dpl_ctx_free(ctx);
      return NULL;
    }

  return ctx;
}

void
dpl_ctx_free(dpl_ctx_t *ctx)
{
  dpl_profile_free(ctx);
  pthread_mutex_destroy(&ctx->lock);
  free(ctx);
}

/*
 * eval
 */

double
dpl_price_storage(dpl_ctx_t *ctx,
                  size_t size)
{
  int i;
  struct dpl_data_pricing *datp = NULL;

  for (i = 0;i < ctx->data_pricing[DPL_DATA_TYPE_STORAGE]->n_items;i++)
    {
      datp = (struct dpl_data_pricing *) ctx->data_pricing[DPL_DATA_TYPE_STORAGE]->array[i];

      //dpl_data_pricing_print(datp);

      if (size < datp->limit)
        break ;
    }

  if (NULL == datp)
    return .0;

  return ((double) size / (double) datp->quantity) * datp->price;
}

char *
dpl_price_storage_str(dpl_ctx_t *ctx,
                      size_t size)
{
 static char str[256];

 snprintf(str, sizeof (str), "$%.03f", dpl_price_storage(ctx, size));

 return str;
}

/**
 * convenience function
 *
 * @note not thread safe since it returns a static buffer
 *
 * @param size
 *
 * @return
 */
char *
dpl_size_str(uint64_t size)
{
  static char str[256];
  char *unit;
  unsigned long long divisor;
  double size_dbl;

  divisor = 1;

  if (size < 1000)
    unit = "";
  else if (size < (1000*1000))
    {
      divisor = 1000;
      unit = "KB";
    }
  else if (size < (1000*1000*1000))
    {
      divisor = 1000*1000;
      unit = "MB";
    }
  else if (size < (1000LL*1000LL*1000LL*1000LL))
    {
      divisor = 1000LL*1000LL*1000LL;
      unit = "GB";
    }
  else
    {
      divisor = 1000LL*1000LL*1000LL*1000LL;
      unit = "PB";
    }

  size_dbl = (double)size / (double)divisor;

  snprintf(str, sizeof (str), "%.02f%s", size_dbl, unit);

  return str;
}
