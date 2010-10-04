/*
 * Droplet, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplet
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
dpl_ctx_new(char *droplet_dir,
            char *profile_name)
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
