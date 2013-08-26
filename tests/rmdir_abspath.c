#include <droplet.h>
#include <assert.h>
#include <sys/param.h>

/*
 * Scenario: non-regression test (ticket #8441)
 * - mkdir foo
 * - cd foo
 * - mkdir bar
 * - cd foo
 * - rmdir bar
 */

int
main(int argc,
     char **argv)
{
  int ret;
  dpl_ctx_t *ctx = NULL;
  char *folder = NULL;
  int folder_len;

  if (3 != argc)
    {
      fprintf(stderr, "usage: restrest folder\n");
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

  //open default profile
  ctx = dpl_ctx_new(NULL,     //droplet directory, default: "~/.droplet"
                    NULL);    //droplet profile, default:   "default"
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl_ctx_new failed\n");
      ret = 1;
      goto free_dpl;
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

 free_all:

  dpl_ctx_free(ctx); //free context

 free_dpl:
  dpl_free();        //free droplet library

 end:
  return ret;
}
