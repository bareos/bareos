/*
 * Example of proprietary SRWS key generation.
 * unfortunately key generation is proprietary to the REST system, driver must
 * embed its own routines
 */

#include <droplet.h>
#include <droplet/backend.h>
#include <droplet/srws/backend.h>

void
usage()
{
  fprintf(stderr, "usage: srwskey [cos]\n");
  exit(1);
}

int
main(int argc,
     char **argv)
{
  int ret;
  dpl_ctx_t *ctx;
  char *bucket = NULL;
  BIGNUM *bn = NULL;
  char *tmp_str = NULL;
  int cos = 1;

  if (2 == argc)
    {
      if (!isdigit(argv[1][0]))
        usage();
      cos = atoi(argv[1]);
    }
  else if (1 != argc)
    {
      usage();
    }

  bucket = argv[1];

  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_init failed\n");
      ret = 1;
      goto end;
    }

  ctx = dpl_ctx_new(NULL, NULL);
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl_ctx_new failed\n");
      ret = 1;
      goto free_dpl;
    }

  bn = BN_new();

  //generate a random UKS key
  ret = dpl_srws_gen_key(bn,             //the key
                         dpl_rand_u64(), //the 64-bits object ID
                         dpl_rand_u32(), //the 32-bits volume ID
                         0,              //the 8-bits service ID (0 -> test)
                         0);             //the 24-bits specific information
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "key gen failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }
  
  ret = dpl_srws_set_class(bn,          //the key
                           cos);        //the class of service (1 -> 2 copies)
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "set class failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  tmp_str = BN_bn2hex(bn);
  printf("%s\n", tmp_str);
  
  ret = 0;

 free_all:
  
  if (NULL != tmp_str)
    free(tmp_str);

  if (NULL != bn)
    BN_free(bn);

  dpl_ctx_free(ctx);

 free_dpl:
  dpl_free();

 end:
  return ret;
}
