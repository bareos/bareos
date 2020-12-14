#include <droplet.h>
#include <assert.h>
#include <sys/param.h>

int
main(void)
{
  int ret;
  dpl_ctx_t *ctx = NULL;
  int dpl_inited = 0;

  ret = dpl_init();           //init droplet library
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_init failed\n");
      ret = 1;
      goto end;
    }

  dpl_inited = 1;

  //open default profile
  ctx = dpl_ctx_new(NULL,     //droplet directory, default: "~/.droplet"
                    NULL);    //droplet profile, default:   "default"
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl_ctx_new failed\n");
      ret = 1;
      goto end;
    }


  ret = 0;

 end:

  if (ctx)
    dpl_ctx_free(ctx); //free context

  if (dpl_inited)
    dpl_free();        //free droplet library

  return ret;
}
