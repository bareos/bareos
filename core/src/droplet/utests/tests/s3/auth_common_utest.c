#include <check.h>

#include "dropletp.h"
#include "droplet/s3/s3.h"

#include "utest_main.h"

dpl_ctx_t* create_ctx_for_test(unsigned char version)
{
  dpl_ctx_t* ctx;
  dpl_dict_t* profile;
  dpl_status_t ret;

  profile = dpl_dict_new(32);
  dpl_assert_ptr_not_null(profile);

  ret = dpl_dict_add(profile, "backend", "s3", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_add(profile, "use_https", "false", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_add(profile, "host", "s3.amazonaws.com", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  {
    char version_str[4];

    snprintf(version_str, sizeof(version_str), "%d", version);
    ret = dpl_dict_add(profile, "aws_auth_sign_version", version_str, 0);

    dpl_assert_int_eq(DPL_SUCCESS, ret);
  }

  ret = dpl_dict_add(profile, "aws_region", "us-east-1", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_add(profile, "access_key", "AKIAIOSFODNN7EXAMPLE", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_dict_add(profile, "secret_key",
                     "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY", 0);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ctx = dpl_ctx_new_from_dict(profile);
  dpl_assert_ptr_not_null(ctx);

  dpl_dict_free(profile);

  return ctx;
}

dpl_req_t* create_req_for_test(dpl_ctx_t* ctx,
                               dpl_method_t method,
                               const char* bucket,
                               const char* resource,
                               dpl_dict_t** headers)
{
  dpl_addr_t* addrp = NULL;
  dpl_status_t ret;
  dpl_req_t* req;
  char virtual_host[1024];

  req = dpl_req_new(ctx);
  dpl_assert_ptr_not_null(req);

  dpl_req_set_method(req, method);

  if (bucket != NULL) {
    ret = dpl_req_set_bucket(req, bucket);
    dpl_assert_int_eq(DPL_SUCCESS, ret);
  }

  ret = dpl_req_set_resource(req, resource);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_s3_req_build(req, 0u, headers);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_addrlist_get_nth(ctx->addrlist, ctx->cur_host, &addrp);
  dpl_assert_int_eq(DPL_SUCCESS, ret);
  dpl_assert_ptr_not_null(addrp);

  if (req->bucket != NULL)
    snprintf(virtual_host, sizeof(virtual_host), "%s.%s", req->bucket,
             addrp->host);
  else
    strncpy(virtual_host, addrp->host, sizeof(virtual_host));

  ret = dpl_req_set_host(req, virtual_host);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_req_set_port(req, addrp->portstr);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  ret = dpl_add_host_to_headers(req, *headers);
  dpl_assert_int_eq(DPL_SUCCESS, ret);

  return req;
}
