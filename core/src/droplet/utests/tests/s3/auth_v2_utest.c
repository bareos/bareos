/* check-sources:disable-copyright-check */
#include <check.h>

#include "dropletp.h"
#include "droplet/s3/s3.h"

#include "utest_main.h"

dpl_ctx_t* create_ctx_for_test(unsigned char);
dpl_req_t* create_req_for_test(dpl_ctx_t*,
                               dpl_method_t,
                               const char*,
                               const char*,
                               dpl_dict_t**);

START_TEST(s3_auth_headers_test1)
{
  dpl_ctx_t* ctx;
  dpl_req_t* req;
  dpl_dict_t* headers = NULL;
  dpl_status_t ret;
  dpl_dict_var_t* var;
  char* authorization;

  ctx = create_ctx_for_test(2);
  req = create_req_for_test(ctx, DPL_METHOD_GET, "johnsmith",
                            "/photos/puppy.jpg", &headers);

  /* Tue, 27 Mar 2007 19:36:42 +0000 */
  struct tm test_date = {.tm_sec = 42,
                         .tm_min = 36,
                         .tm_hour = 19,
                         .tm_mday = 27,
                         .tm_mon = 2,
                         .tm_year = 107,
                         .tm_wday = 2};

  {
    dpl_dict_var_t* var;
    char date_str[128];

    var = dpl_dict_get(headers, "Connection");
    if (var != NULL) dpl_dict_remove(headers, var);

    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S +0000",
             &test_date);

    ret = dpl_dict_add(headers, "Date", date_str, 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);
  }

  ret = dpl_s3_add_authorization_to_headers(req, headers, NULL, &test_date);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(headers, "Authorization");
  dpl_assert_ptr_not_null(var);

  authorization = dpl_sbuf_get_str(var->val->string);
  dpl_assert_str_eq(authorization,
                    "AWS AKIAIOSFODNN7EXAMPLE:bWq2s1WEIj+Ydj0vQ697zp+IXMU=");

  if (headers != NULL) dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

START_TEST(s3_auth_headers_test2)
{
  dpl_ctx_t* ctx;
  dpl_req_t* req;
  dpl_dict_t* headers = NULL;
  dpl_status_t ret;
  dpl_dict_var_t* var;
  char* authorization;

  ctx = create_ctx_for_test(2);
  req = create_req_for_test(ctx, DPL_METHOD_PUT, "johnsmith",
                            "/photos/puppy.jpg", &headers);

  /* Tue, 27 Mar 2007 21:15:45 +0000 */
  struct tm test_date = {.tm_sec = 45,
                         .tm_min = 15,
                         .tm_hour = 21,
                         .tm_mday = 27,
                         .tm_mon = 2,
                         .tm_year = 107,
                         .tm_wday = 2};

  {
    dpl_dict_var_t* var;
    char date_str[128];

    var = dpl_dict_get(headers, "Connection");
    if (var != NULL) dpl_dict_remove(headers, var);

    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S +0000",
             &test_date);

    ret = dpl_dict_add(headers, "Date", date_str, 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "Content-Type", "image/jpeg", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "Content-Length", "94328", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);
  }

  ret = dpl_s3_add_authorization_to_headers(req, headers, NULL, &test_date);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(headers, "Authorization");
  dpl_assert_ptr_not_null(var);

  authorization = dpl_sbuf_get_str(var->val->string);
  dpl_assert_str_eq(authorization,
                    "AWS AKIAIOSFODNN7EXAMPLE:MyyxeRY7whkBe+bq8fHCL/2kKUg=");

  if (headers != NULL) dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

START_TEST(s3_auth_headers_test3)
{
  dpl_ctx_t* ctx;
  dpl_req_t* req;
  dpl_dict_t* headers = NULL;
  dpl_status_t ret;
  dpl_dict_var_t* var;
  char* authorization;
  dpl_dict_t* query_params;

  ctx = create_ctx_for_test(2);
  req = create_req_for_test(ctx, DPL_METHOD_GET, "johnsmith", "/", &headers);

  /* Tue, 27 Mar 2007 19:42:41 +0000 */
  struct tm test_date = {.tm_sec = 41,
                         .tm_min = 42,
                         .tm_hour = 19,
                         .tm_mday = 27,
                         .tm_mon = 2,
                         .tm_year = 107,
                         .tm_wday = 2};

  query_params = dpl_dict_new(32);
  dpl_assert_ptr_not_null(query_params);

  ret = dpl_dict_add(headers, "prefix", "photos", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_add(headers, "max-keys", "50", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_add(headers, "marker", "puppy", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  {
    dpl_dict_var_t* var;
    char date_str[128];

    var = dpl_dict_get(headers, "Connection");
    if (var != NULL) dpl_dict_remove(headers, var);

    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S +0000",
             &test_date);

    ret = dpl_dict_add(headers, "Date", date_str, 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "User-Agent", "Mozilla/5.0", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);
  }

  ret = dpl_s3_add_authorization_to_headers(req, headers, query_params,
                                            &test_date);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(headers, "Authorization");
  dpl_assert_ptr_not_null(var);

  authorization = dpl_sbuf_get_str(var->val->string);
  dpl_assert_str_eq(authorization,
                    "AWS AKIAIOSFODNN7EXAMPLE:htDYFYduRNen8P9ZfE/s9SuKy0U=");

  dpl_dict_free(query_params);

  if (headers != NULL) dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

START_TEST(s3_auth_headers_test4)
{
  dpl_ctx_t* ctx;
  dpl_req_t* req;
  dpl_dict_t* headers = NULL;
  dpl_status_t ret;
  dpl_dict_var_t* var;
  char* authorization;

  /* Tue, 27 Mar 2007 19:44:46 +0000 */
  struct tm test_date = {.tm_sec = 46,
                         .tm_min = 44,
                         .tm_hour = 19,
                         .tm_mday = 27,
                         .tm_mon = 2,
                         .tm_year = 107,
                         .tm_wday = 2};

  ctx = create_ctx_for_test(2);
  req = create_req_for_test(ctx, DPL_METHOD_GET, "johnsmith", "/", &headers);

  ret = dpl_req_set_subresource(req, "acl");
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  {
    dpl_dict_var_t* var;
    char date_str[128];

    var = dpl_dict_get(headers, "Connection");
    if (var != NULL) dpl_dict_remove(headers, var);

    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S +0000",
             &test_date);

    ret = dpl_dict_add(headers, "Date", date_str, 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);
  }

  ret = dpl_s3_add_authorization_to_headers(req, headers, NULL, &test_date);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(headers, "Authorization");
  dpl_assert_ptr_not_null(var);

  authorization = dpl_sbuf_get_str(var->val->string);
  dpl_assert_str_eq(authorization,
                    "AWS AKIAIOSFODNN7EXAMPLE:c2WLPFtWHVgbEmeEG93a4cG37dM=");

  if (headers != NULL) dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

START_TEST(s3_auth_headers_test5)
{
  dpl_ctx_t* ctx;
  dpl_req_t* req;
  dpl_dict_t* headers = NULL;
  dpl_status_t ret;
  dpl_dict_var_t* var;
  char* authorization;

  /* Tue, 27 Mar 2007 21:20:27 +0000 */
  struct tm test_date = {.tm_sec = 27,
                         .tm_min = 20,
                         .tm_hour = 21,
                         .tm_mday = 27,
                         .tm_mon = 2,
                         .tm_year = 107,
                         .tm_wday = 2};

  ctx = create_ctx_for_test(2);
  req = create_req_for_test(ctx, DPL_METHOD_DELETE, "johnsmith",
                            "/photos/puppy.jpg", &headers);

  {
    dpl_dict_var_t* var;
    char date_str[128];

    var = dpl_dict_get(headers, "Connection");
    if (var != NULL) dpl_dict_remove(headers, var);

    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S +0000",
             &test_date);

    ret = dpl_dict_add(headers, "Date", date_str, 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "User-Agent", "dotnet", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "x-amz-date", "Tue, 27 Mar 2007 21:20:26 +0000",
                       0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);
  }

  ret = dpl_s3_add_authorization_to_headers(req, headers, NULL, &test_date);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(headers, "Authorization");
  dpl_assert_ptr_not_null(var);

  authorization = dpl_sbuf_get_str(var->val->string);
  dpl_assert_str_eq(authorization,
                    "AWS AKIAIOSFODNN7EXAMPLE:lx3byBScXR6KzyMaifNkardMwNk=");

  if (headers != NULL) dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

/**
 * @todo Fix dpl_s3_make_signature_v2() for support uppercase x-amz-* header
 */

START_TEST(s3_auth_headers_test6)
{
  dpl_ctx_t* ctx;
  dpl_req_t* req;
  dpl_dict_t* headers = NULL;
  dpl_status_t ret;
  dpl_dict_var_t* var;
  char* authorization;

  /* Tue, 27 Mar 2007 21:06:08 +0000 */
  struct tm test_date = {.tm_sec = 8,
                         .tm_min = 6,
                         .tm_hour = 21,
                         .tm_mday = 27,
                         .tm_mon = 2,
                         .tm_year = 107,
                         .tm_wday = 2};

  ctx = create_ctx_for_test(2);
  req = create_req_for_test(ctx, DPL_METHOD_PUT, "johnsmith",
                            "/db-backup.dat.gz", &headers);

  {
    dpl_dict_var_t* var;
    char date_str[128];

    var = dpl_dict_get(headers, "Connection");
    if (var != NULL) dpl_dict_remove(headers, var);

    ret = dpl_dict_add(headers, "User-Agent", "curl/7.15.5", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "Host", "static.johnsmith.net:8080", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S +0000",
             &test_date);

    ret = dpl_dict_add(headers, "Date", date_str, 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "x-amz-acl", "public-read", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "Content-Type", "application/x-download", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "Content-MD5", "4gJE4saaMU4BqNR0kLY+lw==", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "X-Amz-Meta-ReviewedBy",
                       "joe@johnsmith.net,jane@johnsmith.net", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "X-Amz-Meta-FileChecksum", "0x02661779", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "X-Amz-Meta-ChecksumAlgorithm", "crc32", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "Content-Disposition",
                       "attachment; filename=database.dat", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "Content-Encoding", "gzip", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);

    ret = dpl_dict_add(headers, "Content-Length", "5913339", 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);
  }

  ret = dpl_s3_add_authorization_to_headers(req, headers, NULL, &test_date);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(headers, "Authorization");
  dpl_assert_ptr_not_null(var);

  authorization = dpl_sbuf_get_str(var->val->string);
  dpl_assert_str_eq(authorization,
                    "AWS AKIAIOSFODNN7EXAMPLE:ilyl83RwaSoYIEdixDQcA4OnAnc=");

  if (headers != NULL) dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

START_TEST(s3_auth_headers_test7)
{
  dpl_ctx_t* ctx;
  dpl_req_t* req;
  dpl_dict_t* headers = NULL;
  dpl_status_t ret;
  dpl_dict_var_t* var;
  char* authorization;

  /* Wed, 28 Mar 2007 01:29:59 +0000 */
  struct tm test_date = {.tm_sec = 59,
                         .tm_min = 29,
                         .tm_hour = 1,
                         .tm_mday = 28,
                         .tm_mon = 2,
                         .tm_year = 107,
                         .tm_wday = 3};

  ctx = create_ctx_for_test(2);
  req = create_req_for_test(ctx, DPL_METHOD_GET, NULL, "/", &headers);

  {
    dpl_dict_var_t* var;
    char date_str[128];

    var = dpl_dict_get(headers, "Connection");
    if (var != NULL) dpl_dict_remove(headers, var);

    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S +0000",
             &test_date);

    ret = dpl_dict_add(headers, "Date", date_str, 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);
  }

  ret = dpl_s3_add_authorization_to_headers(req, headers, NULL, &test_date);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(headers, "Authorization");
  dpl_assert_ptr_not_null(var);

  authorization = dpl_sbuf_get_str(var->val->string);
  dpl_assert_str_eq(authorization,
                    "AWS AKIAIOSFODNN7EXAMPLE:qGdzdERIC03wnaRNKh6OqZehG9s=");

  if (headers != NULL) dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

/**
 * @todo Correct dpl_url_encode() for support already encoded character
 */

START_TEST(s3_auth_headers_test8)
{
  dpl_ctx_t* ctx;
  dpl_req_t* req;
  dpl_dict_t* headers = NULL;
  dpl_status_t ret;
  dpl_dict_var_t* var;
  char* authorization;

  /* Wed, 28 Mar 2007 01:49:49 +0000 */
  struct tm test_date = {.tm_sec = 49,
                         .tm_min = 49,
                         .tm_hour = 1,
                         .tm_mday = 28,
                         .tm_mon = 2,
                         .tm_year = 107,
                         .tm_wday = 3};

  ctx = create_ctx_for_test(2);
  req = create_req_for_test(ctx, DPL_METHOD_GET, NULL,
                            "/dictionary/fran%C3%A7ais/pr%c3%a9f%c3%a8re",
                            &headers);

  {
    dpl_dict_var_t* var;
    char date_str[128];

    var = dpl_dict_get(headers, "Connection");
    if (var != NULL) dpl_dict_remove(headers, var);

    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S +0000",
             &test_date);

    ret = dpl_dict_add(headers, "Date", date_str, 0);
    dpl_assert_int_eq(DPL_SUCCESS, ret);
  }

  ret = dpl_s3_add_authorization_to_headers(req, headers, NULL, &test_date);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(headers, "Authorization");
  dpl_assert_ptr_not_null(var);

  authorization = dpl_sbuf_get_str(var->val->string);
  dpl_assert_str_eq(authorization,
                    "AWS AKIAIOSFODNN7EXAMPLE:DNEZGsoieTZ92F3bUfSPQcbGmlM=");

  if (headers != NULL) dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

START_TEST(s3_auth_params_test1)
{
  dpl_ctx_t* ctx;
  dpl_req_t* req;
  dpl_dict_t* headers = NULL;
  dpl_status_t ret;
  dpl_dict_var_t* var;
  dpl_dict_t* query_params;

  query_params = dpl_dict_new(32);
  dpl_assert_ptr_not_null(query_params);

  ctx = create_ctx_for_test(2);
  req = create_req_for_test(ctx, DPL_METHOD_GET, "johnsmith",
                            "/photos/puppy.jpg", &headers);

  dpl_req_set_expires(req, 1175139620);

  ret = dpl_s3_get_authorization_v2_params(req, query_params, NULL);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  var = dpl_dict_get(query_params, "AWSAccessKeyId");
  dpl_assert_ptr_not_null(var);
  dpl_assert_str_eq(dpl_sbuf_get_str(var->val->string), "AKIAIOSFODNN7EXAMPLE");

  var = dpl_dict_get(query_params, "Signature");
  dpl_assert_ptr_not_null(var);
  dpl_assert_str_eq(dpl_sbuf_get_str(var->val->string),
                    "NpgCjnDzrM%2BWFzoENXmpNDUsSn8%3D");

  var = dpl_dict_get(query_params, "Expires");
  dpl_assert_ptr_not_null(var);
  dpl_assert_str_eq(dpl_sbuf_get_str(var->val->string), "1175139620");

  if (headers != NULL) dpl_dict_free(headers);

  dpl_req_free(req);
  dpl_ctx_free(ctx);
}
END_TEST

Suite* s3_auth_v2_suite(void)
{
  Suite* s = suite_create("s3/auth/v2");
  TCase* d = tcase_create("base");
  tcase_add_test(d, s3_auth_headers_test1);
  tcase_add_test(d, s3_auth_headers_test2);
  tcase_add_test(d, s3_auth_headers_test3);
  tcase_add_test(d, s3_auth_headers_test4);
  tcase_add_test(d, s3_auth_headers_test5);
  tcase_add_test(d, s3_auth_headers_test6);  // FAILED !
  tcase_add_test(d, s3_auth_headers_test7);
  tcase_add_test(d, s3_auth_headers_test8);  // FAILED !
  tcase_add_test(d, s3_auth_params_test1);
  suite_add_tcase(s, d);
  return s;
}
