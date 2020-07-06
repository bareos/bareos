#include <droplet.h>
#include <assert.h>
#include <sys/param.h>

/*
 * Scenario: simple putdir (folder)
 */

int
main(int argc,
     char **argv)
{
  int ret;
  dpl_ctx_t *ctx = NULL;
  char *folder = NULL;
  int folder_len;
  int dpl_inited = 0;

  if (2 != argc)
    {
      fprintf(stderr, "usage: %s folder\n", argv[0]);
      ret = 1;
      goto end;
    }

  folder = argv[1];
  folder_len = strlen(folder);
  if (folder_len < 1)
    {
      fprintf(stderr, "bad folder\n");
      ret = 1;
      goto end;
    }
  if (folder[folder_len-1] != '/')
    {
      fprintf(stderr, "folder name must end with a slash\n");
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
                folder,        //the folder
                NULL,          //no option
                DPL_FTYPE_DIR, //directory
                NULL,          //no condition
                NULL,          //no range
                NULL,          //no metadata
                NULL,          //no sysmd
                NULL,          //object body
                0);            //object length
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_put failed: %s (%d)\n", dpl_status_str(ret), ret);
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
