#include <check.h>

#include "dropletp.h"
#include "droplet/s3/s3.h"

#include "utest_main.h"

/* Fri, 24 May 2013 00:00:00 GMT */
static struct tm test_date = {
  .tm_sec = 0,
  .tm_min = 0,
  .tm_hour = 0,
  .tm_mday = 24,
  .tm_mon = 4,
  .tm_year = 113,
  .tm_wday = 5
};

static dpl_ctx_t *
create_ctx_for_test(unsigned char version)
{
  dpl_ctx_t     *ctx;
  dpl_dict_t    *profile;
  dpl_status_t  ret;

  profile = dpl_dict_new(32);
  dpl_assert_ptr_not_null(profile);

  ret = dpl_dict_add(profile, "backend", "s3", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_add(profile, "use_https", "false", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_add(profile, "host", "s3.amazonaws.com", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  {
    char        version_str[4];

    snprintf(version_str, sizeof(version_str), "%d", version);
    ret = dpl_dict_add(profile, "aws_auth_sign_version", version_str, 0);

    dpl_assert_int_eq(DPL_SUCCESS, ret);
  }

  ret = dpl_dict_add(profile, "aws_region", "us-east-1", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_add(profile, "access_key", "AKIAIOSFODNN7EXAMPLE", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_add(profile, "secret_key", "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ctx = dpl_ctx_new_from_dict(profile);
  dpl_assert_ptr_not_null(ctx);

  dpl_dict_free(profile);

  return ctx;
}

static dpl_req_t *
create_req_for_test(dpl_ctx_t *ctx, dpl_method_t method,
                    const char *bucket, const char *resource,
                    dpl_dict_t **headers)
{
  dpl_status_t  ret;
  dpl_req_t     *req;
  char          *host, *port, virtual_host[1024];

  req = dpl_req_new(ctx);
  dpl_assert_ptr_not_null(req);

  dpl_req_set_method(req, method);

  ret = dpl_req_set_bucket(req, bucket);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_req_set_resource(req, resource);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_s3_req_build(req, 0u, headers);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_addrlist_get_nth(ctx->addrlist, ctx->cur_host,
                             &host, &port, NULL, NULL);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  snprintf(virtual_host, sizeof(virtual_host), "%s.%s", req->bucket, host);

  ret = dpl_req_set_host(req, virtual_host);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_req_set_port(req, port);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_add_host_to_headers(req, *headers);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  return req;
}

START_TEST(s3_auth_headers_test1)
{
  dpl_ctx_t             *ctx;
  dpl_req_t             *req;
  dpl_dict_t            *headers = NULL;
  dpl_status_t          ret;
  dpl_dict_var_t        *var;
  char                  *authorization;

  ctx = create_ctx_for_test(4);
  req = create_req_for_test(ctx, DPL_METHOD_GET, "examplebucket", "/test.txt", &headers);

  {
    dpl_dict_var_t      *var;

    var = dpl_dict_get(headers, "Connection");
    if (var != NULL)
      dpl_dict_remove(headers, var);

    var = dpl_dict_get(headers, "Date");
    if (var != NULL)
      dpl_dict_remove(headers, var);

    ret = dpl_dict_add(headers, "Range", "bytes=0-9", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);
  }

  ret = dpl_s3_add_authorization_to_headers(req, headers, NULL, &test_date);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(headers, "Authorization");
  dpl_assert_ptr_not_null(var);

  authorization = dpl_sbuf_get_str(var->val->string);
  dpl_assert_str_eq(authorization, "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request,SignedHeaders=host;range;x-amz-content-sha256;x-amz-date,Signature=f0e8bdb87c964420e857bd35b5d6ed310bd44f0170aba48dd91039c6036bdb41");

  if (headers != NULL)
    dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

START_TEST(s3_auth_headers_test2)
{
  dpl_ctx_t             *ctx;
  dpl_req_t             *req;
  dpl_dict_t            *headers = NULL;
  dpl_status_t          ret;
  dpl_dict_var_t        *var;
  char                  *authorization;

  ctx = create_ctx_for_test(4);
  req = create_req_for_test(ctx, DPL_METHOD_PUT, "examplebucket", "/test$file.text", &headers);

  {
    dpl_dict_var_t      *var;
    char                date_str[128];

    var = dpl_dict_get(headers, "Connection");
    if (var != NULL)
      dpl_dict_remove(headers, var);

    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", &test_date);

    ret = dpl_dict_add(headers, "Date", date_str, 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "x-amz-storage-class", "REDUCED_REDUNDANCY", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "x-amz-content-sha256", "44ce7dd67c959e0d3524ffac1771dfbba87d2b6b4b4e99e42034a8b803f8b072", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);
  }

  ret = dpl_s3_add_authorization_to_headers(req, headers, NULL, &test_date);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(headers, "Authorization");
  dpl_assert_ptr_not_null(var);

  authorization = dpl_sbuf_get_str(var->val->string);
  dpl_assert_str_eq(authorization, "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request,SignedHeaders=date;host;x-amz-content-sha256;x-amz-date;x-amz-storage-class,Signature=98ad721746da40c64f1a55b78f14c238d841ea1380cd77a1b5971af0ece108bd");

  if (headers != NULL)
    dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

START_TEST(s3_auth_headers_test3)
{
  dpl_ctx_t             *ctx;
  dpl_req_t             *req;
  dpl_dict_t            *headers = NULL, *query_params;
  dpl_status_t          ret;
  dpl_dict_var_t        *var;
  char                  *authorization;

  ctx = create_ctx_for_test(4);
  req = create_req_for_test(ctx, DPL_METHOD_GET, "examplebucket", "/", &headers);

  query_params = dpl_dict_new(32);
  dpl_assert_ptr_not_null(query_params);

  ret = dpl_dict_add(query_params, "lifecycle", "", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  {
    dpl_dict_var_t      *var;

    var = dpl_dict_get(headers, "Connection");
    if (var != NULL)
      dpl_dict_remove(headers, var);

    var = dpl_dict_get(headers, "Date");
    if (var != NULL)
      dpl_dict_remove(headers, var);
  }

  ret = dpl_s3_add_authorization_to_headers(req, headers, query_params, &test_date);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(headers, "Authorization");
  dpl_assert_ptr_not_null(var);

  authorization = dpl_sbuf_get_str(var->val->string);
  dpl_assert_str_eq(authorization, "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date,Signature=fea454ca298b7da1c68078a5d1bdbfbbe0d65c699e0f91ac7a200a0136783543");

  dpl_dict_free(query_params);

  if (headers != NULL)
    dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

START_TEST(s3_auth_headers_test4)
{
  dpl_ctx_t             *ctx;
  dpl_req_t             *req;
  dpl_dict_t            *headers = NULL, *query_params;
  dpl_status_t          ret;
  dpl_dict_var_t        *var;
  char                  *authorization;

  ctx = create_ctx_for_test(4);
  req = create_req_for_test(ctx, DPL_METHOD_GET, "examplebucket", "/", &headers);

  query_params = dpl_dict_new(32);
  dpl_assert_ptr_not_null(query_params);

  ret = dpl_dict_add(query_params, "max-keys", "2", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_add(query_params, "prefix", "J", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  {
    dpl_dict_var_t      *var;

    var = dpl_dict_get(headers, "Connection");
    if (var != NULL)
      dpl_dict_remove(headers, var);

    var = dpl_dict_get(headers, "Date");
    if (var != NULL)
      dpl_dict_remove(headers, var);
  }

  ret = dpl_s3_add_authorization_to_headers(req, headers, query_params, &test_date);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(headers, "Authorization");
  dpl_assert_ptr_not_null(var);

  authorization = dpl_sbuf_get_str(var->val->string);
  dpl_assert_str_eq(authorization, "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date,Signature=34b48302e7b5fa45bde8084f4b7868a86f0a534bc59db6670ed5711ef69dc6f7");

  dpl_dict_free(query_params);

  if (headers != NULL)
    dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

START_TEST(s3_auth_params_test1)
{
  dpl_ctx_t             *ctx;
  dpl_req_t             *req;
  dpl_dict_t            *headers = NULL;
  dpl_status_t          ret;
  dpl_dict_var_t        *var;
  dpl_dict_t            *query_params;
  time_t                t;

  query_params = dpl_dict_new(32);
  dpl_assert_ptr_not_null(query_params);

  ctx = create_ctx_for_test(4);
  req = create_req_for_test(ctx, DPL_METHOD_GET, "examplebucket", "/test.txt", &headers);

  dpl_req_set_expires(req, time(&t) + 86400);

  ret = dpl_s3_get_authorization_v4_params(req, query_params, NULL, &test_date);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(query_params, "X-Amz-Algorithm");
  dpl_assert_ptr_not_null(var);
  dpl_assert_str_eq(dpl_sbuf_get_str(var->val->string), "AWS4-HMAC-SHA256");

  var = dpl_dict_get(query_params, "X-Amz-Credential");
  dpl_assert_ptr_not_null(var);
  dpl_assert_str_eq(dpl_sbuf_get_str(var->val->string), "AKIAIOSFODNN7EXAMPLE%2F20130524%2Fus-east-1%2Fs3%2Faws4_request");

  var = dpl_dict_get(query_params, "X-Amz-Date");
  dpl_assert_ptr_not_null(var);
  dpl_assert_str_eq(dpl_sbuf_get_str(var->val->string), "20130524T000000Z");

  var = dpl_dict_get(query_params, "X-Amz-Expires");
  dpl_assert_ptr_not_null(var);
  dpl_assert_str_eq(dpl_sbuf_get_str(var->val->string), "86400");

  var = dpl_dict_get(query_params, "X-Amz-SignedHeaders");
  dpl_assert_ptr_not_null(var);
  dpl_assert_str_eq(dpl_sbuf_get_str(var->val->string), "host");

  var = dpl_dict_get(query_params, "X-Amz-Signature");
  dpl_assert_ptr_not_null(var);
  dpl_assert_str_eq(dpl_sbuf_get_str(var->val->string), "aeeed9bbccd4d02ee5c0109b86d86835f995330da4c265957d157751f604d404");

  dpl_dict_free(query_params);

  if (headers != NULL)
    dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

Suite *
s3_auth_suite(void)
{
  Suite *s = suite_create("s3/auth");
  TCase *d = tcase_create("base");
  tcase_add_test(d, s3_auth_headers_test1);
  tcase_add_test(d, s3_auth_headers_test2);
  tcase_add_test(d, s3_auth_headers_test3);
  tcase_add_test(d, s3_auth_headers_test4);
  tcase_add_test(d, s3_auth_params_test1);
  suite_add_tcase(s, d);
  return s;
}
