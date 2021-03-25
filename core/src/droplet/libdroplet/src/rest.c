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

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt, ...)

/**
 * @defgroup rest RESTful API
 * @addtogroup rest
 * @{
 * Functions specific to RESTful API (S3, CDMI, sproxyd, SRWS, etc)
 *
 * This is the abstraction layer fo all backends (S3, CDMI, sproxyd, SRWS, etc).
 * Mainly, this API is used for PUTting, GETting and DELETing objects, but also
 * manipulating the object attributes. For backends that has bucket support,
 * this API allows to create/list/destroy buckets,
 *
 * Users are supposed to call these functions as opposed to using the backend
 * functions directly.
 *
 * Configuring the backend is done is the profile dictionnary or the profile
 * configuration file (see dpl_ctx_new())
 */

/**
 * return the name of the backend currently used
 *
 * @param ctx
 *
 * @return the backend name
 */
const char* dpl_get_backend_name(dpl_ctx_t* ctx) { return ctx->backend->name; }

/**
 * @brief get the backend capabilities
 *
 * @param ctx droplet context
 * @param[out] maskp a pointer to @a mask
 *
 */
dpl_status_t dpl_get_capabilities(dpl_ctx_t* ctx, dpl_capability_t* maskp)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_REST, "get_capabilities");

  if (NULL == ctx->backend->get_capabilities) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->get_capabilities(ctx, maskp);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  return ret;
}

/**
 * @brief login
 *
 * @param ctx droplet context
 *
 */
dpl_status_t dpl_login(dpl_ctx_t* ctx)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_REST, "login");

  if (NULL == ctx->backend->login) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->login(ctx);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  return ret;
}

/**
 * @brief convert the absolute URI in @a location string to a relative
 * resource (possibly removing ctx->base_path) and subresource
 *
 * @note this function modifies @a location string
 *
 * @param ctx droplet context
 * @param location absolute URI
 * @param[out] resourcep a pointer to @a location which is the
 * relative path of the resource.
 * @param[out] subresourcep a pointer to @a location which is the
 * query-string without the '?'
 *
 */
static void dpl_location_to_resource(dpl_ctx_t* ctx,
                                     char* location,
                                     char** resourcep,
                                     char** subresourcep)
{
  char* resource;
  char* subresource;
  size_t base_path_len;

  if (0 == strcmp(ctx->base_path, "/")) {
    resource = location;
  } else {
    base_path_len = strlen(ctx->base_path);
    if (0 == strncmp(location, ctx->base_path, base_path_len))
      resource = location + base_path_len;
    else
      resource = location;
  }

  subresource = strchr(resource, '?');
  if (NULL != subresource) {
    subresource[0] = '\0';
    ++subresource;
  }

  if (NULL != resourcep) *resourcep = resource;
  if (NULL != subresourcep) *subresourcep = subresource;
}

/**
 * list all buckets
 *
 * @param ctx the droplect context
 * @param vecp a vector of dpl_bucket_t *
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_list_all_my_buckets(dpl_ctx_t* ctx, dpl_vec_t** vecp)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_REST, "list_all_my_buckets");

  if (NULL == ctx->backend->list_all_my_buckets) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->list_all_my_buckets(ctx, vecp, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "REQUEST", "LIST", 0);

  return ret;
}

/**
 * list bucket or directory
 *
 * @param ctx the droplet context
 * @param bucket can be NULL
 * @param prefix directory can be NULL
 * @param delimiter e.g. "/" can be NULL
 * @param max number of keys we want to get (-1 for no limit)
 * @param pointer to a dpl_dict_t filled with metadata (to be freed by caller)
 * @param pointer to a preallocated sysmd to be filled with system metadata
 * @param objectsp vector of dpl_object_t * (files)
 * @param prefixesp vector of dpl_common_prefix_t * (directories)
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_list_bucket_attrs(dpl_ctx_t* ctx,
                                   const char* bucket,
                                   const char* prefix,
                                   const char* delimiter,
                                   const int max_keys,
                                   dpl_dict_t** metadatap,
                                   dpl_sysmd_t* sysmdp,
                                   dpl_vec_t** objectsp,
                                   dpl_vec_t** common_prefixesp)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_REST, "list_bucket bucket=%s prefix=%s delimiter=%s",
            bucket, prefix, delimiter);

  if (NULL == ctx->backend->list_bucket) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->list_bucket_attrs(ctx, bucket, prefix, delimiter,
                                         max_keys, metadatap, sysmdp, objectsp,
                                         common_prefixesp, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "REQUEST", "LIST", 0);

  return ret;
}

/**
 * list bucket or directory
 *
 * @param ctx the droplet context
 * @param bucket can be NULL
 * @param prefix directory can be NULL
 * @param delimiter e.g. "/" can be NULL
 * @param max number of keys we want to get (-1 for no limit)
 * @param objectsp vector of dpl_object_t * (files)
 * @param prefixesp vector of dpl_common_prefix_t * (directories)
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_list_bucket(dpl_ctx_t* ctx,
                             const char* bucket,
                             const char* prefix,
                             const char* delimiter,
                             const int max_keys,
                             dpl_vec_t** objectsp,
                             dpl_vec_t** common_prefixesp)
{
  return dpl_list_bucket_attrs(ctx, bucket, prefix, delimiter, max_keys, NULL,
                               NULL, objectsp, common_prefixesp);
}

/**
 * make a bucket
 *
 * @param ctx the droplet context
 * @param bucket can be NULL
 * @param location_constraint geographic location
 * @param canned_acl simplified ACL
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_make_bucket(dpl_ctx_t* ctx,
                             const char* bucket,
                             dpl_location_constraint_t location_constraint,
                             dpl_canned_acl_t canned_acl)
{
  dpl_status_t ret, ret2;
  dpl_sysmd_t sysmd;

  DPL_TRACE(ctx, DPL_TRACE_REST, "makebucket bucket=%s", bucket);

  if (NULL == ctx->backend->make_bucket) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  memset(&sysmd, 0, sizeof(sysmd));
  sysmd.mask = DPL_SYSMD_MASK_CANNED_ACL | DPL_SYSMD_MASK_LOCATION_CONSTRAINT;
  sysmd.canned_acl = canned_acl;
  sysmd.location_constraint = location_constraint;

  ret2 = ctx->backend->make_bucket(ctx, bucket, &sysmd, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "DATA", "PUT", 0);

  return ret;
}

/**
 * delete a path
 *
 * @param ctx the droplet context
 * @param bucket can be NULL
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_delete_bucket(dpl_ctx_t* ctx, const char* bucket)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_REST, "delete_bucket bucket=%s", bucket);

  if (NULL == ctx->backend->delete_bucket) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->delete_bucket(ctx, bucket, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "DATA", "DELETE", 0);

  return ret;
}

/**
 * create or post data into a path
 *
 * @note this function is expected to return a newly created object
 *
 * @param ctx the droplet context
 * @param bucket can be NULL
 * @param path can be NULL
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_REG create a file
 * @param metadata the user metadata. optional
 * @param sysmd the system metadata. optional
 * @param data_buf the data buffer
 * @param data_len the data length
 * @param query_params can be NULL
 * @param returned_sysmdp the returned system metadata passed through stack
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_post(dpl_ctx_t* ctx,
                      const char* bucket,
                      const char* path,
                      const dpl_option_t* option,
                      dpl_ftype_t object_type,
                      const dpl_condition_t* condition,
                      const dpl_range_t* range,
                      const dpl_dict_t* metadata,
                      const dpl_sysmd_t* sysmd,
                      const char* data_buf,
                      unsigned int data_len,
                      const dpl_dict_t* query_params,
                      dpl_sysmd_t* returned_sysmdp)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_REST, "put bucket=%s path=%s", bucket, path);

  if (NULL == ctx->backend->post) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->post(ctx, bucket, path, NULL, option, object_type,
                            condition, range, metadata, sysmd, data_buf,
                            data_len, query_params, returned_sysmdp, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "DATA", "IN", data_len);

  return ret;
}

/**
 * put a path
 *
 * @param ctx the droplet context
 * @param bucket optional
 * @param path mandatory
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_REG create a file
 * @param object_type DPL_FTYPE_DIR create a directory
 * @param condition the optional condition
 * @param range optional range
 * @param metadata the optional user metadata
 * @param sysmd the optional system metadata
 * @param data_buf the data buffer
 * @param data_len the data length
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_EEXIST
 */
dpl_status_t dpl_put(dpl_ctx_t* ctx,
                     const char* bucket,
                     const char* path,
                     const dpl_option_t* option,
                     dpl_ftype_t object_type,
                     const dpl_condition_t* condition,
                     const dpl_range_t* range,
                     const dpl_dict_t* metadata,
                     const dpl_sysmd_t* sysmd,
                     const char* data_buf,
                     unsigned int data_len)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_REST, "put bucket=%s path=%s", bucket, path);

  if (NULL == ctx->backend->put) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->put(ctx, bucket, path, NULL, option, object_type,
                           condition, range, metadata, sysmd, data_buf,
                           data_len, NULL, NULL, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "DATA", "IN", data_len);

  return ret;
}

/**
 * get a path with range
 *
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param path the mandat path
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_ANY get any type of path
 * @param condition the optional condition
 * @param range the optional range
 * @param data_bufp the returned data buffer client shall free
 * @param data_lenp the returned data length
 * @param metadatap the returned user metadata client shall free
 * @param sysmdp the returned system metadata passed through stack
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT path does not exist
 */
dpl_status_t dpl_get(dpl_ctx_t* ctx,
                     const char* bucket,
                     const char* path,
                     const dpl_option_t* option,
                     dpl_ftype_t object_type,
                     const dpl_condition_t* condition,
                     const dpl_range_t* range,
                     char** data_bufp,
                     unsigned int* data_lenp,
                     dpl_dict_t** metadatap,
                     dpl_sysmd_t* sysmdp)
{
  dpl_status_t ret, ret2;
  u_int data_len;
  char* new_location = NULL;
  char* new_location_resource;
  char* new_location_subresource;

  DPL_TRACE(ctx, DPL_TRACE_REST, "get bucket=%s path=%s", bucket, path);

  if (NULL == ctx->backend->get) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  if (NULL != data_lenp) data_len = *data_lenp;

  ret2 = ctx->backend->get(ctx, bucket, path, NULL, option, object_type,
                           condition, range, data_bufp, &data_len, metadatap,
                           sysmdp, &new_location);

  if (DPL_EREDIRECT == ret2) {
    dpl_location_to_resource(ctx, new_location, &new_location_resource,
                             &new_location_subresource);

    if (NULL != data_lenp) data_len = *data_lenp;

    ret2 = ctx->backend->get(ctx, bucket, new_location_resource,
                             new_location_subresource, option, object_type,
                             condition, range, data_bufp, &data_len, metadatap,
                             sysmdp, NULL);

    free(new_location);
  }

  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  if (NULL != data_lenp) *data_lenp = data_len;

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "DATA", "OUT", data_len);

  return ret;
}

/**
 * get a path for SYMLINKS
 *
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param path the mandat path
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_ANY get any type of path
 * @param condition the optional condition
 * @param data_bufp the returned data buffer client shall free
 * @param data_lenp the returned data length
 * @param metadatap the returned user metadata client shall free
 * @param sysmdp the returned system metadata passed through stack
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT path does not exist
 */
dpl_status_t dpl_get_noredirect(dpl_ctx_t* ctx,
                                const char* bucket,
                                const char* path,
                                dpl_ftype_t object_type,
                                char** locationp)
{
  dpl_status_t ret, ret2;
  int loclen = 0;
  char* new_location = NULL;
  char* new_location_resource = NULL;
  char* new_location_subresource = NULL;

  DPL_TRACE(ctx, DPL_TRACE_REST, "get bucket=%s path=%s", bucket, path);

  if (NULL == ctx->backend->get) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->get(ctx, bucket, path, NULL, NULL, object_type, NULL,
                           NULL, NULL, NULL, NULL, NULL, &new_location);
  if (DPL_EREDIRECT != ret2) {
    if (ret2 == DPL_SUCCESS) {
      ret = DPL_ENOTSUPP;
      goto end;
    }
    ret = ret2;
    goto end;
  }

  dpl_location_to_resource(ctx, new_location, &new_location_resource,
                           &new_location_subresource);
  loclen = strlen(new_location_resource);
  if (NULL != locationp) {
    *locationp = strdup(new_location_resource);
    if (NULL == *locationp) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  ret = DPL_SUCCESS;

end:
  if (new_location) free(new_location);

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "LINKDATA", "OUT", loclen);

  return ret;
}

/**
 * get user and system metadata
 *
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param path the mandat path
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_ANY get any type of path
 * @param condition the optional condition
 * @param metadatap the returned user metadata client shall free
 * @param sysmdp the returned system metadata passed through stack
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT path does not exist
 */
dpl_status_t dpl_head(dpl_ctx_t* ctx,
                      const char* bucket,
                      const char* path,
                      const dpl_option_t* option,
                      dpl_ftype_t object_type,
                      const dpl_condition_t* condition,
                      dpl_dict_t** metadatap,
                      dpl_sysmd_t* sysmdp)
{
  dpl_status_t ret, ret2;
  char* new_location = NULL;
  char* new_location_resource;
  char* new_location_subresource;

  DPL_TRACE(ctx, DPL_TRACE_REST, "head bucket=%s path=%s", bucket, path);

  if (NULL == ctx->backend->head) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->head(ctx, bucket, path, NULL, option, object_type,
                            condition, metadatap, sysmdp, &new_location);

  if (DPL_EREDIRECT == ret2) {
    dpl_location_to_resource(ctx, new_location, &new_location_resource,
                             &new_location_subresource);

    ret2 = ctx->backend->head(ctx, bucket, new_location_resource,
                              new_location_subresource, option, object_type,
                              condition, metadatap, sysmdp, NULL);

    free(new_location);
  }

  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "DATA", "GET", 0);

  return ret;
}

/**
 * get raw metadata
 *
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param path the mandat path
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param condition the optional condition
 * @param metadatap the returned metadata client shall free
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT
 */
dpl_status_t dpl_head_raw(dpl_ctx_t* ctx,
                          const char* bucket,
                          const char* path,
                          const dpl_option_t* option,
                          dpl_ftype_t object_type,
                          const dpl_condition_t* condition,
                          dpl_dict_t** metadatap)
{
  dpl_status_t ret, ret2;
  char* new_location = NULL;
  char* new_location_resource;
  char* new_location_subresource;

  DPL_TRACE(ctx, DPL_TRACE_REST, "head_raw bucket=%s path=%s", bucket, path);

  if (NULL == ctx->backend->head_raw) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->head_raw(ctx, bucket, path, NULL, option, object_type,
                                condition, metadatap, &new_location);

  if (DPL_EREDIRECT == ret2) {
    dpl_location_to_resource(ctx, new_location, &new_location_resource,
                             &new_location_subresource);

    ret2 = ctx->backend->head_raw(ctx, bucket, new_location_resource,
                                  new_location_subresource, option, object_type,
                                  condition, metadatap, NULL);

    free(new_location);
  }

  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "DATA", "GET", 0);

  return ret;
}

/**
 * delete a path
 *
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param path the mandat path
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param condition the optional condition
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT path does not exist
 */
dpl_status_t dpl_delete(dpl_ctx_t* ctx,
                        const char* bucket,
                        const char* path,
                        const dpl_option_t* option,
                        dpl_ftype_t object_type,
                        const dpl_condition_t* condition)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_REST, "delete bucket=%s path=%s", bucket, path);

  if (NULL == ctx->backend->deletef) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->deletef(ctx, bucket, path, NULL, option, object_type,
                               condition, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "DATA", "DELETE", 0);

  return ret;
}

/**
 * delete multiple path
 *
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type
 * @param condition the optional condition
 *
 * @return DPL_ENOTSUPP
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_delete_all(dpl_ctx_t* ctx,
                            const char* bucket,
                            dpl_locators_t* locators,
                            const dpl_option_t* option,
                            const dpl_condition_t* condition,
                            dpl_vec_t** objectsp)
{
  dpl_status_t ret;

  if (ctx->backend->delete_all == NULL) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret = ctx->backend->delete_all(ctx, bucket, locators, option, condition,
                                 objectsp);
  if (DPL_SUCCESS != ret) goto end;

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "DATA", "DELETE", 0);

  return ret;
}

/**
 * delete multiple id
 *
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param ressource path of ID (ex: /proxy/arc) for REST backend
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type
 * @param condition the optional condition
 *
 * @return DPL_ENOTSUPP
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_delete_all_id(dpl_ctx_t* ctx,
                               const char* bucket,
                               const char* ressource,
                               dpl_locators_t* locators,
                               const dpl_option_t* option,
                               const dpl_condition_t* condition,
                               dpl_vec_t** objectsp)
{
  dpl_status_t ret;

  if (ctx->backend->delete_all_id == NULL) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret = ctx->backend->delete_all_id(ctx, bucket, ressource, locators, option,
                                    condition, objectsp);
  if (DPL_SUCCESS != ret) goto end;

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "DATA", "DELETE", 0);

  return ret;
}

/**
 * create or post data into a path
 *
 * @note this function is expected to return a newly created object
 *
 * @param ctx
 * @param bucket
 * @param path can be NULL
 * @param object_type
 * @param metadata
 * @param canned_acl
 * @param data_buf
 * @param data_len
 * @param query_params can be NULL
 * @param returned_sysmdp
 *
 * @return
 */
dpl_status_t dpl_post_id(dpl_ctx_t* ctx,
                         const char* bucket,
                         const char* id,
                         const dpl_option_t* option,
                         dpl_ftype_t object_type,
                         const dpl_condition_t* condition,
                         const dpl_range_t* range,
                         const dpl_dict_t* metadata,
                         const dpl_sysmd_t* sysmd,
                         const char* data_buf,
                         unsigned int data_len,
                         const dpl_dict_t* query_params,
                         dpl_sysmd_t* returned_sysmdp)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_ID, "post_id bucket=%s", bucket);

  if (NULL == ctx->backend->post_id) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->post_id(ctx, bucket, id, NULL, option, object_type,
                               condition, range, metadata, sysmd, data_buf,
                               data_len, query_params, returned_sysmdp, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);

  return ret;
}

dpl_status_t dpl_put_id(dpl_ctx_t* ctx,
                        const char* bucket,
                        const char* id,
                        const dpl_option_t* option,
                        dpl_ftype_t object_type,
                        const dpl_condition_t* condition,
                        const dpl_range_t* range,
                        const dpl_dict_t* metadata,
                        const dpl_sysmd_t* sysmd,
                        const char* data_buf,
                        unsigned int data_len)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_ID, "put_id bucket=%s id=%s", bucket, id);

  if (NULL == ctx->backend->put_id) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->put_id(ctx, bucket, id, NULL, option, object_type,
                              condition, range, metadata, sysmd, data_buf,
                              data_len, NULL, NULL, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);

  return ret;
}

dpl_status_t dpl_get_id(dpl_ctx_t* ctx,
                        const char* bucket,
                        const char* id,
                        const dpl_option_t* option,
                        dpl_ftype_t object_type,
                        const dpl_condition_t* condition,
                        const dpl_range_t* range,
                        char** data_bufp,
                        unsigned int* data_lenp,
                        dpl_dict_t** metadatap,
                        dpl_sysmd_t* sysmdp)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_ID, "get_id bucket=%s id=%s", bucket, id);

  if (NULL == ctx->backend->get_id) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->get_id(ctx, bucket, id, NULL, option, object_type,
                              condition, range, data_bufp, data_lenp, metadatap,
                              sysmdp, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);

  return ret;
}

dpl_status_t dpl_head_id(dpl_ctx_t* ctx,
                         const char* bucket,
                         const char* id,
                         const dpl_option_t* option,
                         dpl_ftype_t object_type,
                         const dpl_condition_t* condition,
                         dpl_dict_t** metadatap,
                         dpl_sysmd_t* sysmdp)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_ID, "head_id bucket=%s id=%s", bucket, id);

  if (NULL == ctx->backend->head_id) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->head_id(ctx, bucket, id, NULL, option, object_type,
                               condition, metadatap, sysmdp, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);

  return ret;
}

dpl_status_t dpl_head_raw_id(dpl_ctx_t* ctx,
                             const char* bucket,
                             const char* id,
                             const dpl_option_t* option,
                             dpl_ftype_t object_type,
                             const dpl_condition_t* condition,
                             dpl_dict_t** metadatap)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_ID, "head_raw_id bucket=%s id=%s", bucket, id);

  if (NULL == ctx->backend->head_id_raw) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->head_id_raw(ctx, bucket, id, NULL, option, object_type,
                                   condition, metadatap, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);

  return ret;
}

dpl_status_t dpl_delete_id(dpl_ctx_t* ctx,
                           const char* bucket,
                           const char* id,
                           const dpl_option_t* option,
                           dpl_ftype_t object_type,
                           const dpl_condition_t* condition)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_ID, "delete bucket=%s id=%s", bucket, id);

  if (NULL == ctx->backend->delete_id) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->delete_id(ctx, bucket, id, NULL, option, object_type,
                                 condition, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);

  return ret;
}

/**
 * generate a valid URL for sharing object
 *
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param path the mandat path
 * @param option unused
 * @param expires expire time of URL
 * @param buf URL is created in this buffer
 * @param len buffer length
 * @param lenp real buffer returned
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_genurl(dpl_ctx_t* ctx,
                        const char* bucket,
                        const char* path,
                        const dpl_option_t* option,
                        time_t expires,
                        char* buf,
                        unsigned int len,
                        unsigned int* lenp)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_REST, "genurl bucket=%s path=%s", bucket, path);

  if (NULL == ctx->backend->genurl) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->genurl(ctx, bucket, path, NULL, option, expires, buf,
                              len, lenp, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  return ret;
}

/**
 * perform various flavors of server side copies
 *
 * @param ctx the droplet context
 * @param src_bucket the optional source bucket
 * @param src_path the mandat source path
 * @param dst_bucket the optional destination bucket
 * @param dst_path the optional destination path (if dst equals src)
 * @param option unused
 * @param object_type unused
 * @param copy_directive DPL_COPY_DIRECTIVE_COPY server side copy
 * @param copy_directive DPL_COPY_DIRECTIVE_METADATA_REPLACE setattr
 * @param copy_directive DPL_COPY_DIRECTIVE_LINK hard link
 * @param copy_directive DPL_COPY_DIRECTIVE_SYMLINK reference
 * @param copy_directive DPL_COPY_DIRECTIVE_MOVE rename
 * @param copy_directive DPL_COPY_DIRECTIVE_MKDENT create a directory entry
 * @param metadata the optional user metadata
 * @param sysmd the optional system metadata
 * @param condition the optional condition
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_copy(dpl_ctx_t* ctx,
                      const char* src_bucket,
                      const char* src_path,
                      const char* dst_bucket,
                      const char* dst_path,
                      const dpl_option_t* option,
                      dpl_ftype_t object_type,
                      dpl_copy_directive_t copy_directive,
                      const dpl_dict_t* metadata,
                      const dpl_sysmd_t* sysmd,
                      const dpl_condition_t* condition)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_REST,
            "copy src_bucket=%s src_path=%s dst_bucket=%s dst_path=%s",
            src_bucket, src_path, dst_bucket, dst_path);

  if (NULL == ctx->backend->copy) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->copy(ctx, src_bucket, src_path, NULL, dst_bucket,
                            dst_path, NULL, option, object_type, copy_directive,
                            metadata, sysmd, condition, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "DATA", "PUT", 0);

  return ret;
}

dpl_status_t dpl_copy_id(dpl_ctx_t* ctx,
                         const char* src_bucket,
                         const char* src_id,
                         const char* dst_bucket,
                         const char* dst_path,
                         const dpl_option_t* option,
                         dpl_ftype_t object_type,
                         dpl_copy_directive_t copy_directive,
                         const dpl_dict_t* metadata,
                         const dpl_sysmd_t* sysmd,
                         const dpl_condition_t* condition)
{
  dpl_status_t ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_REST,
            "copy_id src_bucket=%s src_id=%s dst_bucket=%s dst_path=%s",
            src_bucket, src_id, dst_bucket, dst_path);

  if (NULL == ctx->backend->copy_id) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret2 = ctx->backend->copy_id(
      ctx, src_bucket, src_id, NULL, dst_bucket, dst_path, NULL, option,
      object_type, copy_directive, metadata, sysmd, condition, NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  if (DPL_SUCCESS == ret) dpl_log_request(ctx, "DATA", "PUT", 0);

  return ret;
}

/**
 * Create a streaming object to read from/write to using an object path
 * The returned stream is not reentrant, and should not be used by multiple
 * threads at the same time.
 *
 * @param ctx       the droplet context
 * @param bucket    the name of the bucket
 * @param path      the path of the object
 * @param option    Optional. The dict of options for this stream object
 * @param condition Optional. The condition
 * @param streamp   Mandatory. The pointer to return the stream through
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOMEM
 */
dpl_status_t dpl_stream_open(dpl_ctx_t* ctx,
                             const char* bucket,
                             const char* path,
                             const dpl_option_t* option,
                             const dpl_condition_t* condition,
                             dpl_dict_t* metadata,
                             dpl_sysmd_t* sysmd,
                             dpl_stream_t** streamp)
{
  dpl_status_t ret = DPL_FAILURE;
  dpl_stream_t* stream = NULL;

  DPL_TRACE(ctx, DPL_TRACE_REST,
            "stream_open bucket=%s path=%s"
            " option=%p condition=%p metadata=%p sysmd=%p",
            bucket, path, option, condition, metadata, sysmd);

  stream = calloc(1, sizeof(*stream));
  if (NULL == stream) {
    ret = DPL_ENOMEM;
    goto end;
  }

  stream->bucket = strdup(bucket);
  if (NULL == stream->bucket) {
    ret = DPL_ENOMEM;
    goto end;
  }

  stream->locator_is_id = 0;
  stream->locator = strdup(path);
  if (NULL == stream->locator) {
    ret = DPL_ENOMEM;
    goto end;
  }

  if (NULL != option) {
    stream->options = dpl_option_dup(option);
    if (NULL == stream->options) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  if (NULL != condition) {
    stream->condition = dpl_condition_dup(condition);
    if (NULL == stream->condition) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  if (NULL != metadata) {
    stream->md = dpl_dict_dup(metadata);
    if (NULL == stream->md) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  if (NULL != sysmd) {
    stream->sysmd = dpl_sysmd_dup(sysmd);
    if (NULL == stream->sysmd) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  *streamp = stream;
  stream = NULL;

  ret = DPL_SUCCESS;

end:

  if (NULL != stream) dpl_stream_close(ctx, stream);

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d stream=%p", ret);

  return ret;
}

/**
 * Create a streaming object to read from/write to using an object ID
 * The returned stream is not reentrant, and should not be used by multiple
 * threads at the same time.
 *
 * @param ctx       the droplet context
 * @param bucket    the name of the bucket
 * @param id        the id of the object
 * @param option    Optional. The dict of options for this stream object
 * @param condition Optional. The condition
 * @param streamp   Mandatory. The pointer to return the stream through
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */

dpl_status_t dpl_stream_openid(dpl_ctx_t* ctx,
                               const char* bucket,
                               const char* id,
                               const dpl_option_t* option,
                               const dpl_condition_t* condition,
                               dpl_dict_t* metadata,
                               dpl_sysmd_t* sysmd,
                               dpl_stream_t** streamp)
{
  dpl_status_t ret = DPL_FAILURE;
  dpl_stream_t* stream = NULL;

  DPL_TRACE(ctx, DPL_TRACE_REST,
            "stream_open bucket=%s id=%s"
            " option=%p condition=%pi metadata=%p sysmd=%p",
            bucket, id, option, condition, metadata, sysmd);

  stream = calloc(1, sizeof(*stream));
  if (NULL == stream) {
    ret = DPL_ENOMEM;
    goto end;
  }

  stream->bucket = strdup(bucket);
  if (NULL == stream->bucket) {
    ret = DPL_ENOMEM;
    goto end;
  }

  stream->locator_is_id = 1;
  stream->locator = strdup(id);
  if (NULL == stream->locator) {
    ret = DPL_ENOMEM;
    goto end;
  }

  if (NULL != option) {
    stream->options = dpl_option_dup(option);
    if (NULL == stream->options) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  if (NULL != condition) {
    stream->condition = dpl_condition_dup(condition);
    if (NULL == stream->condition) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  if (NULL != metadata) {
    stream->md = dpl_dict_dup(metadata);
    if (NULL == stream->md) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  if (NULL != sysmd) {
    stream->sysmd = dpl_sysmd_dup(sysmd);
    if (NULL == stream->sysmd) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  *streamp = stream;
  ret = DPL_SUCCESS;

end:

  if (NULL != stream) dpl_stream_close(ctx, stream);

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d stream=%p", ret);

  return ret;
}

/**
 * Resume a streaming (Write) operation using the json object returned by the
 * last successful call to a stream write operation.
 *
 * @param ctx       the droplet context
 * @param stream    the droplet stream
 * @param status    the json status object
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_stream_resume(dpl_ctx_t* ctx,
                               dpl_stream_t* stream,
                               struct json_object* status)
{
  dpl_status_t ret = DPL_FAILURE;

  DPL_TRACE(ctx, DPL_TRACE_REST, "stream_resume ctx=%p stream=%p status=%p",
            ctx, stream, status);

  if (NULL == ctx->backend->stream_resume) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret = ctx->backend->stream_resume(ctx, stream, status);
  if (DPL_SUCCESS != ret) goto end;

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  return ret;
}

/**
 * Read the Object's metadata from the stream
 *
 * @param ctx       the droplet context
 * @param stream    the droplet stream
 * @param metadatap the pointer to the metadata to return
 * @param sysmdp    the pointer to the sysmd to return
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_stream_getmd(dpl_ctx_t* ctx,
                              dpl_stream_t* stream,
                              dpl_dict_t** metadatap,
                              dpl_sysmd_t** sysmdp)
{
  dpl_status_t ret = DPL_FAILURE;

  DPL_TRACE(ctx, DPL_TRACE_REST,
            "stream_getmd ctx=%p stream=%p mdp=%p sysmdp=%p", ctx, stream,
            metadatap, sysmdp);

  if (NULL == ctx->backend->stream_getmd) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret = ctx->backend->stream_getmd(ctx, stream, metadatap, sysmdp);
  if (DPL_SUCCESS != ret) goto end;

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  return ret;
}

/**
 * Read the Object's Data into the streamed object
 *
 * @param ctx       the droplet context
 * @param stream    the droplet stream
 * @param len       the size of the data to read
 * @param data_bufp pointer to return the read data
 * @param data_lenp pointer to return the size of the data read
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_stream_get(dpl_ctx_t* ctx,
                            dpl_stream_t* stream,
                            unsigned int len,
                            char** data_bufp,
                            unsigned int* data_lenp,
                            struct json_object** statusp)
{
  dpl_status_t ret = DPL_FAILURE;

  DPL_TRACE(ctx, DPL_TRACE_REST, "stream_get ctx=%p stream=%p len=%u", ctx,
            stream, len);

  if (NULL == ctx->backend->stream_get) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret = ctx->backend->stream_get(ctx, stream, len, data_bufp, data_lenp,
                                 statusp);
  if (DPL_SUCCESS != ret) goto end;

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  return ret;
}

/**
 * Write the Object's Metadata into the streamed object
 *
 * @param ctx       the droplet context
 * @param stream    the droplet stream
 * @param metadata  the metadata to write
 * @param sysmd     the sysmd to write
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_stream_putmd(dpl_ctx_t* ctx,
                              dpl_stream_t* stream,
                              dpl_dict_t* metadata,
                              dpl_sysmd_t* sysmd)
{
  dpl_status_t ret = DPL_FAILURE;

  DPL_TRACE(ctx, DPL_TRACE_REST, "stream_putmd ctx=%p stream=%p md=%p sysmd=%p",
            ctx, stream, metadata, sysmd);

  if (NULL == ctx->backend->stream_putmd) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret = ctx->backend->stream_putmd(ctx, stream, metadata, sysmd);
  if (DPL_SUCCESS != ret) goto end;

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  return ret;
}

/**
 * Write the Object's Data into the streamed object
 *
 * @param ctx       the droplet context
 * @param stream    the droplet stream
 * @param buf       the data to write
 * @param len       the size of the data to write
 * @param statusp   The pointer to a json status object, to return
 *                  backend-specific status to caller
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_stream_put(dpl_ctx_t* ctx,
                            dpl_stream_t* stream,
                            char* buf,
                            unsigned int len,
                            struct json_object** statusp)
{
  dpl_status_t ret = DPL_FAILURE;

  DPL_TRACE(ctx, DPL_TRACE_REST,
            "stream_put ctx=%p stream=%p buf=%p len=%u status=%p", ctx, stream,
            buf, len, statusp);

  if (NULL == ctx->backend->stream_put) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret = ctx->backend->stream_put(ctx, stream, buf, len, statusp);
  if (DPL_SUCCESS != ret) goto end;

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  return ret;
}

/**
 * Flush (finalize) streamed upload
 *
 * @param ctx     the droplet context
 * @param stream  the droplet stream
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t dpl_stream_flush(dpl_ctx_t* ctx, dpl_stream_t* stream)
{
  dpl_status_t ret = DPL_FAILURE;

  DPL_TRACE(ctx, DPL_TRACE_REST, "stream_flush ctx=%p stream=%p", ctx, stream);

  if (NULL == ctx->backend->stream_flush) {
    ret = DPL_ENOTSUPP;
    goto end;
  }

  ret = ctx->backend->stream_flush(ctx, stream);
  if (DPL_SUCCESS != ret) goto end;

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);

  return ret;
}


/**
 * Releases resources associated to the stream stream
 *
 * @param ctx       the droplet context
 * @param stream    the droplet stream
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
void dpl_stream_close(dpl_ctx_t* ctx, dpl_stream_t* stream)
{
  DPL_TRACE(ctx, DPL_TRACE_REST, "stream_close ctx=%p stream=%p", ctx, stream);

  if (stream->condition) dpl_condition_free(stream->condition);

  if (stream->options) dpl_option_free(stream->options);

  if (stream->locator) free(stream->locator);

  if (stream->bucket) free(stream->bucket);

  if (stream->status) json_object_put(stream->status);

  free(stream);
}

/* @} */
