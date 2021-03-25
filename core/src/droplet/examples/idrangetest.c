/* check-sources:disable-copyright-check */
/*
 * simple example which use ID capable REST services
 */

#include <droplet.h>

int main(int argc, char** argv)
{
  int ret;
  dpl_ctx_t* ctx;
  char* data_buf_returned = NULL;
  u_int data_len_returned;
  dpl_range_t range;
  dpl_sysmd_t sysmd;
  char* force_id = NULL;

  if (2 == argc) {
    force_id = argv[1];
  } else if (1 != argc) {
    fprintf(stderr, "usage: idrangetest [key]\n");
    ret = 1;
    goto end;
  }

  ret = dpl_init();  // init droplet library
  if (DPL_SUCCESS != ret) {
    fprintf(stderr, "dpl_init failed\n");
    ret = 1;
    goto end;
  }

  // open default profile
  ctx = dpl_ctx_new(NULL,   // droplet directory, default: "~/.droplet"
                    NULL);  // droplet profile, default:   "default"
  if (NULL == ctx) {
    fprintf(stderr, "dpl_ctx_new failed\n");
    ret = 1;
    goto free_dpl;
  }

  // ctx->trace_level = ~0;

  /**/

  fprintf(stderr, "setting object+MD\n");

#define DATA_BUF "foobarbaz"

  if (force_id) {
    // we have a broken cloud storage with no POST
    ret = dpl_put_id(ctx,                // the context
                     NULL,               // no bucket
                     force_id,           // the id
                     NULL,               // no option
                     DPL_FTYPE_REG,      // regular object
                     NULL,               // no condition
                     NULL,               // no range
                     NULL,               // the metadata
                     NULL,               // no sysmd
                     DATA_BUF,           // object body
                     strlen(DATA_BUF));  // object length
    if (DPL_SUCCESS != ret) {
      fprintf(stderr, "dpl_put_id failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

    // emulate returned sysmd
    memset(&sysmd, 0, sizeof(sysmd));
    strncpy(sysmd.id, force_id, sizeof(sysmd.id));
  } else {
    ret = dpl_post_id(ctx,               // the context
                      NULL,              // no bucket
                      NULL,              // no id
                      NULL,              // no option
                      DPL_FTYPE_REG,     // regular object
                      NULL,              // condition
                      NULL,              // range
                      NULL,              // the metadata
                      NULL,              // no sysmd
                      DATA_BUF,          // object body
                      strlen(DATA_BUF),  // object length
                      NULL,              // no query params
                      &sysmd);           // the returned sysmd
    if (DPL_SUCCESS != ret) {
      fprintf(stderr, "dpl_post_id failed: %s (%d)\n", dpl_status_str(ret),
              ret);
      ret = 1;
      goto free_all;
    }

    if (!(sysmd.mask & DPL_SYSMD_MASK_ID)) {
      fprintf(stderr, "backend is not capable of retrieving resource id\n");
      exit(1);
    }

    fprintf(stderr, "id=%s\n", sysmd.id);
  }

  /**/

  fprintf(stderr, "getting partial object+MD\n");

  range.start = 3LL;
  range.end = 5LL;
  ret = dpl_get_id(ctx,                 // the context
                   NULL,                // no bucket
                   sysmd.id,            // the key
                   NULL,                // no option
                   DPL_FTYPE_REG,       // object type
                   NULL,                // no condition of operation
                   &range,              // range
                   &data_buf_returned,  // data object
                   &data_len_returned,  // data object length
                   NULL,                // no metadata
                   NULL);               // no sysmd
  if (DPL_SUCCESS != ret) {
    fprintf(stderr, "dpl_get_id failed: %s (%d)\n", dpl_status_str(ret), ret);
    ret = 1;
    goto free_all;
  }

  fprintf(stderr, "checking object\n");

  if (data_len_returned != 3) {
    fprintf(stderr, "bad content length %d\n", data_len_returned);
    ret = 1;
    goto free_all;
  }

  if (memcmp(data_buf_returned, "bar", 3)) {
    fprintf(stderr, "bad content\n");
    ret = 1;
    goto free_all;
  }

  /**/

  fprintf(stderr, "delete object+MD\n");

  ret = dpl_delete_id(ctx,              // the context
                      NULL,             // no bucket
                      sysmd.id,         // the key
                      NULL,             // no option
                      DPL_FTYPE_UNDEF,  // no matter the file type
                      NULL);            // no condition
  if (DPL_SUCCESS != ret) {
    fprintf(stderr, "error deleting object: %s (%d)\n", dpl_status_str(ret),
            ret);
    ret = 1;
    goto free_all;
  }

  ret = 0;

free_all:

  if (NULL != data_buf_returned) free(data_buf_returned);

  dpl_ctx_free(ctx);  // free context

free_dpl:
  dpl_free();  // free droplet library

end:
  return ret;
}
