#include <droplet.h>
#include <assert.h>
#include <sys/param.h>

/*
 * Scenario: simple get (file object), without metadata
 */

int
main(int argc,
     char **argv)
{
  int ret;
  dpl_ctx_t *ctx = NULL;
  char *object = NULL;
  int dpl_inited = 0;

  if (2 != argc)
    {
      fprintf(stderr, "usage: %s object\n", argv[0]);
      ret = 1;
      goto end;
    }

  object = argv[1];

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

  /**/

  fprintf(stderr, "creating folder\n");

  ret = dpl_put(ctx,           //the context
                NULL,          //no bucket
                object,        //the folder
                NULL,          //no option
                DPL_FTYPE_REG, //file
                NULL,          //no condition
                NULL,          //no range
                NULL,          //no metadata
                NULL,          //no sysmd
                NULL,          //object body
                0);            //object length
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_get failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto end;
    }

  /**/

  ret = 0;

 end:
  if (ctx)
    dpl_ctx_free(ctx); //free context

  if (dpl_inited)
    dpl_free();        //free droplet library

  return ret;
}
