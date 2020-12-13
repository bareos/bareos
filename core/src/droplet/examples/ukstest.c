/*
 * simple buffered example which use ID capable REST services (emulate blocks with UKS)
 */

#include <droplet.h>
#include <droplet/utils.h>
#include <droplet/uks/uks.h>
#include <assert.h>

int
main(int argc,
     char **argv)
{
  int ret;
  dpl_ctx_t *ctx;
  BIGNUM *id = NULL;
  char *id_str;
  
  if (1 != argc)
    {
      fprintf(stderr, "usage: ukstest\n");
      ret = 1;
      goto end;
    }

  ret = dpl_init();           //init droplet library
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_init failed\n");
      ret = 1;
      goto end;
    }

  //open default profile
  ctx = dpl_ctx_new(NULL,     //droplet directory, default: "~/.droplet"
                    NULL);    //droplet profile, default:   "default"
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl_ctx_new failed\n");
      ret = 1;
      goto free_dpl;
    }

  //ctx->trace_level = ~0;
  //ctx->trace_buffers = 1;

  id = BN_new();
  if (NULL == id)
    {
      perror("BN_new");
      ret = 1;
      goto free_all;
    }

  ret = dpl_uks_gen_key(id, 
			42, //oid
			43, //volid
			0xc0, //serviceid
			0); //specific
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_uks_gen_key failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  ret = dpl_uks_set_class(id,
			  2);             //means 3 replicas
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_uks_set_class failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  id_str = BN_bn2hex(id);
  printf("%s\n", id_str);
  free(id_str);

  ret = dpl_uks_gen_key_ext(id, 
			    DPL_UKS_MASK_SPECIFIC,
			    -1, //oid (unused)
			    -1, //volid (unused)
			    -1, //serviceid (unused)
			    1); //specific (overriden)
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_uks_gen_key failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  id_str = BN_bn2hex(id);
  printf("%s\n", id_str);
  free(id_str);

  ret = 0;

 free_all:

  if (NULL != id)
    BN_free(id);

  dpl_ctx_free(ctx); //free context

 free_dpl:
  dpl_free();        //free droplet library

 end:
  return ret;
}
