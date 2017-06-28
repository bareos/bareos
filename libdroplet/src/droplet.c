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

/** @file */

int dpl_header_size;

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
    case DPL_ENOTSUPP:
      return "DPL_ENOTSUPP";
    case DPL_EREDIRECT:
      return "DPL_EREDIRECT";
    case DPL_ETOOMANYREDIR:
      return "DPL_ETOOMANYREDIR";
    case DPL_ECONNECT:
      return "DPL_ECONNECT";
    case DPL_EPERM:
      return "DPL_EPERM";
    case DPL_EPRECOND:
      return "DPL_EPRECOND";
    case DPL_ECONFLICT:
      return "DPL_ECONFLICT";
    case DPL_ERANGEUNAVAIL:
      return "DPL_ERANGEUNAVAIL";
    }

  return "Unknown error";
}

/**
 * @defgroup init Initialization
 * @addtogroup init
 * @{
 * Initializing the Droplet library.
 *
 * These functions are used to initialize global data used by Droplet
 * library.
 */

/**
 * Initialize Droplet global data.
 *
 * Initializes global data used by the library.  Must be called once
 * and only once before any other Droplet library functions.  The next
 * step after calling `dpl_init()` is probably `dpl_ctx_new()`.
 *
 * @retval DPL_SUCCESS this function cannot currently fail
 */
dpl_status_t
dpl_init()
{
  SSL_library_init();
  SSL_load_error_strings();
  ERR_load_crypto_strings();

  int ret = RAND_status();
  if (0 == ret) {
    DPL_LOG(NULL, DPL_WARNING, "PRNG not properly seeded, seeding it...");
    RAND_poll();
    ret = RAND_status();
    DPL_LOG(NULL, DPL_INFO, "PRNG state after seeding: %d", ret);
  } else if (1 == ret) {
    DPL_LOG(NULL, DPL_INFO, "PRNG has been seeded with enough data");
  }

  dpl_base64_init();

  return DPL_SUCCESS;
}

/**
 * Free Droplet global data.
 *
 * Must be called once and only once after all Droplet library calls
 * have stopped.
 */
void
dpl_free()
{
  ERR_clear_error();
  ERR_remove_state(0);

  ERR_free_strings();
  EVP_cleanup();
  sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
  CRYPTO_cleanup_all_ex_data();
}

/** @} */

/**
 * @defgroup ctx Contexts
 * @addtogroup ctx
 * @{
 * State for communicating with a cloud service
 *
 * The `dpl_ctx_t` object, or "context", holds all the state and options
 * needed to communicate with a cloud service instance.  All other
 * functions that perform operations on a cloud service require a
 * context.  Thus, creating a context is one of the very first things which
 * a program will do when using Droplet, just after calling `dpl_init()`.
 *
 * Most programs will only ever create a single context object and will
 * keep it for the lifetime of the program, only freeing it just before
 * calling `dpl_free()` and exiting.  This is not the only possible approach
 * but it makes the most sense because
 *
 * @arg The context holds some static configuration data which is read
 *      from a profile file when the context is created.  Reading this
 *      data is expensive, and it does not change.
 * @arg The context caches connections to cloud service hosts, which
 *      allows re-use of connections between requests and thus amortises
 *      connection setup latency (such as DNS and TCP roundtrip latency)
 *      over multiple requests.
 * @arg Most programs only need to communicate with a single cloud
 *      storage service instance, so there's no need to create a
 *      second context.
 *
 * An important parameter of the context is the droplet directory, which
 * is established when the context is created.  Several important files
 * are located relative to this directory, notably the profile file.
 * See the description of `dpl_ctx_new()` for details.
 *
 * The context object is thread-safe and contains an internal mutex
 * which is used to safely share access between multiple threads.  This
 * mutex is not held while requests are being made to the back end.  If
 * you need to make requests to a cloud service from multiple threads in
 * the same process, just create a single context object and share it
 * between all threads.
 */

void
dpl_ctx_lock(dpl_ctx_t *ctx)
{
  pthread_mutex_lock(&ctx->lock);
  assert(0 == ctx->canary);
  ctx->canary++;
}

void
dpl_ctx_unlock(dpl_ctx_t *ctx)
{
  ctx->canary--;
  assert(0 == ctx->canary);
  pthread_mutex_unlock(&ctx->lock);
}

static dpl_ctx_t *
dpl_ctx_alloc(void)
{
  dpl_ctx_t *ctx;

  ctx = malloc(sizeof (*ctx));
  if (NULL == ctx)
    return NULL;

  memset(ctx, 0, sizeof (*ctx));

  pthread_mutex_init(&ctx->lock, NULL);

  return ctx;
}

static void
dpl_ctx_post_load(dpl_ctx_t *ctx)
{
  char *str;

  if ((str = getenv("DPL_TRACE_LEVEL")))
    ctx->trace_level = strtoul(str, NULL, 0);

  if ((str = getenv("DPL_TRACE_BUFFERS")))
    ctx->trace_buffers = atoi(str);

  if ((str = getenv("DPL_TRACE_BINARY")))
    ctx->trace_binary = atoi(str);

  dpl_header_size = ctx->header_size;
}

/**
 * Create a context.
 *
 * Creates and returns a new `dpl_ctx_t` object, or NULL on error.
 *
 * Possible errors include memory allocation failure, failure to find
 * the profile, and failure to parse the profile.  @note If this function
 * fails there is no way for the caller to discover what went wrong.
 *
 * The droplet directory is used as the base directory for finding several
 * important files, notably the profile file.  It is the first non-NULL pathname of:
 * @arg the parameter @a droplet_dir, or
 * @arg the environmental variable `DPLDIR`, or
 * @arg `"~/.droplet/"`.
 *
 * A profile file is read to set up options for the context.  The profile
 * file is named `<name>.profile` where `<name>` is the profile's name,
 * and is located in the droplet directory.  The profile's name is the
 * first non-NULL name of:
 * @arg the @a profile_name parameter, or
 * @arg the environment variable `DPLPROFILE`, or
 * @arg `"default"`.
 *
 * Thus, if both parameters are passed `NULL` the profile will be read
 * from the file `~/.droplet/default.profile`.
 *
 * See @ref profile "Profile File" for details of the profile file format.
 *
 * @param droplet_dir pathname of directory containing Droplet state, or `NULL`
 * @param profile_name name of the profile to use, or `NULL`
 * @return a new context, or NULL on error
 */
dpl_ctx_t *
dpl_ctx_new(const char *droplet_dir,
            const char *profile_name)
{
  dpl_ctx_t *ctx;
  int ret;

  ctx = dpl_ctx_alloc();
  if (NULL == ctx)
    {
      DPL_LOG(NULL, DPL_ERROR, "No memory for droplet context creation.");
      return NULL;
    }

  ret = dpl_profile_load(ctx, droplet_dir, profile_name);
  if (DPL_SUCCESS != ret)
    {
      dpl_ctx_free(ctx);
      return NULL;
    }

  dpl_ctx_post_load(ctx);

  return ctx;
}

/**
 * Creates a new context with the profile specified in a dict.
 *
 * Creates a new profile and sets up the the profile information from a
 * dict containing profile variable settings as if they had been read in
 * from a profile file, without reading a file.  This function is provided
 * for applications which need to use the Droplet library without a
 * profile file.
 *
 * Note that to create a context without needing a droplet directory at
 * all, applications should set the profile variable @c pricing_dir to
 * an empty string.
 *
 * See @ref profile "Profile File" for details of the profile variables.
 * The droplet directory is set by a special variable in the dict called
 * @c droplet_dir, and the profile name by a special variable called @c
 * profile_name.
 *
 * @param profile a dict containing profile variables
 * @return a new context or NULL on error
 */
dpl_ctx_t *
dpl_ctx_new_from_dict(const dpl_dict_t *profile)
{
  dpl_ctx_t     *ctx;
  int           ret;

  ctx = dpl_ctx_alloc();
  if (NULL == ctx)
    return NULL;

  ret = dpl_profile_set_from_dict(ctx, profile);
  if (DPL_SUCCESS != ret) {
    dpl_ctx_free(ctx);
    return NULL;
  }

  dpl_ctx_post_load(ctx);

  return ctx;
}

/**
 * Free a context.
 *
 * Free a context created earlier by `dpl_ctx_new()`.  All resources
 * associated with the context (e.g. profile data and cached connections)
 * are freed.
 *
 * @note If you use the connection API to create your own connections,
 * you @b must release all your connections created from this context, by
 * calling either `dpl_conn_release()` or `dpl_conn_terminate()`, @b before
 * calling `dpl_ctx_free()`.
 */

void
dpl_ctx_free(dpl_ctx_t *ctx)
{
  dpl_profile_free(ctx);
  pthread_mutex_destroy(&ctx->lock);
  free(ctx);
}

/** @} */

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
      datp = (struct dpl_data_pricing *) dpl_vec_get(ctx->data_pricing[DPL_DATA_TYPE_STORAGE], i);

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

/*
 *
 */

extern dpl_backend_t dpl_backend_s3;
extern dpl_backend_t dpl_backend_cdmi;
extern dpl_backend_t dpl_backend_swift;
extern dpl_backend_t dpl_backend_srws;
extern dpl_backend_t dpl_backend_sproxyd;
extern dpl_backend_t dpl_backend_posix;

int
dpl_backend_set(dpl_ctx_t *ctx, const char *name)
{
  int ret = 0;

  if (!strcmp(name, "s3"))
    ctx->backend = &dpl_backend_s3;
  else if (!strcmp(name, "cdmi"))
  {
    ctx->preserve_root_path = 1;
    ctx->backend = &dpl_backend_cdmi;
  }
  else if (!strcmp(name, "swift"))
    ctx->backend = &dpl_backend_swift;
  else if (!strcmp(name, "srws"))
    ctx->backend = &dpl_backend_srws;
  else if (!strcmp(name, "sproxyd"))
    ctx->backend = &dpl_backend_sproxyd;
  else if (!strcmp(name, "posix"))
    ctx->backend = &dpl_backend_posix;
  else
      ret = -1;

  return ret;
}

dpl_status_t
dpl_print_capabilities(dpl_ctx_t *ctx)
{
  dpl_status_t ret, ret2;
  dpl_capability_t mask;

  if (NULL == ctx->backend->get_capabilities)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret2 = ctx->backend->get_capabilities(ctx, &mask);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  printf("buckets:\t\t%d\n", mask & DPL_CAP_BUCKETS ? 1 : 0);
  printf("fnames:\t\t\t%d\n", mask & DPL_CAP_FNAMES ? 1 : 0);
  printf("ids:\t\t\t%d\n", mask & DPL_CAP_IDS ? 1 : 0);
  printf("lazy:\t\t\t%d\n", mask & DPL_CAP_LAZY ? 1 : 0);
  printf("http_compat:\t\t%d\n", mask & DPL_CAP_HTTP_COMPAT ? 1 : 0);
  printf("raw:\t\t\t%d\n", mask & DPL_CAP_RAW ? 1 : 0);
  printf("append_metadata:\t%d\n", mask & DPL_CAP_APPEND_METADATA ? 1 : 0);
  printf("consistency:\t\t%d\n", mask & DPL_CAP_CONSISTENCY ? 1 : 0);
  printf("versioning:\t\t%d\n", mask & DPL_CAP_VERSIONING ? 1 : 0);
  printf("conditions:\t\t%d\n", mask & DPL_CAP_CONDITIONS ? 1 : 0);
  printf("put_range:\t\t%d\n", mask & DPL_CAP_PUT_RANGE ? 1 : 0);

  ret = DPL_SUCCESS;
  
 end:

  return ret;
}

/* other */

void
dpl_bucket_free(dpl_bucket_t *bucket)
{
  free(bucket->name);
  free(bucket);
}

void
dpl_vec_buckets_free(dpl_vec_t *vec)
{
  int i;

  for (i = 0;i < vec->n_items;i++)
    dpl_bucket_free((dpl_bucket_t *) dpl_vec_get(vec, i));
  dpl_vec_free(vec);
}

void
dpl_object_free(dpl_object_t *object)
{
  if (NULL != object->path)
    free(object->path);

  free(object);
}

void
dpl_vec_objects_free(dpl_vec_t *vec)
{
  int i;

  for (i = 0;i < vec->n_items;i++)
    dpl_object_free((dpl_object_t *) dpl_vec_get(vec, i));
  dpl_vec_free(vec);
}

void
dpl_delete_object_free(dpl_delete_object_t *object)
{
  if (object->name != NULL)
    free(object->name);

  if (object->version_id != NULL)
    free(object->version_id);

  if (object->error != NULL)
    free(object->error);

  free(object);
}

void
dpl_vec_delete_objects_free(dpl_vec_t *vec)
{
  int i;

  for (i = 0; i < vec->n_items; i++)
    dpl_delete_object_free((dpl_delete_object_t *) dpl_vec_get(vec, i));
  dpl_vec_free(vec);
}

void
dpl_common_prefix_free(dpl_common_prefix_t *common_prefix)
{
  if (NULL != common_prefix->prefix)
    free(common_prefix->prefix);

  free(common_prefix);
}

void
dpl_vec_common_prefixes_free(dpl_vec_t *vec)
{
  int i;

  for (i = 0;i < vec->n_items;i++)
    dpl_common_prefix_free((dpl_common_prefix_t *) dpl_vec_get(vec, i));
  dpl_vec_free(vec);
}

dpl_option_t *
dpl_option_dup(const dpl_option_t *src)
{
  dpl_option_t *dst = NULL;

  dst = malloc(sizeof (*dst));
  if (NULL == dst)
    return NULL;
  
  memcpy(dst, src, sizeof (*src));
  
  return dst;
}

void
dpl_option_free(dpl_option_t *option)
{
  free(option);
}

dpl_condition_t *
dpl_condition_dup(const dpl_condition_t *src)
{
  dpl_condition_t *dst = NULL;

  dst = malloc(sizeof (*dst));
  if (NULL == dst)
    return NULL;
  
  memcpy(dst, src, sizeof (*src));
  
  return dst;
}

void
dpl_condition_free(dpl_condition_t *condition)
{
  free(condition);
}

dpl_range_t *
dpl_range_dup(const dpl_range_t *src)
{
  dpl_range_t *dst = NULL;

  dst = malloc(sizeof (*dst));
  if (NULL == dst)
    return NULL;
  
  memcpy(dst, src, sizeof (*src));
  
  return dst;
}

void
dpl_range_free(dpl_range_t *range)
{
  free(range);
}
