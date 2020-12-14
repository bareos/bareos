#include <droplet.h>
#include <assert.h>
#include <sys/param.h>

/*
 * Scenario: simple put (object)
 */

int
main(int argc,
     char **argv)
{
  int ret;
  dpl_ctx_t *ctx = NULL;
  char *folder = NULL;
  size_t folder_len = 0;
  int dpl_inited = 0;
  size_t data_len;
  char *data_buf = NULL;
  dpl_sysmd_t sysmd;

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

  data_len = 10000;
  data_buf = malloc(data_len);
  if (NULL == data_buf)
    {
      fprintf(stderr, "alloc data failed\n");
      ret = 1;
      goto end;
    }

  memset(data_buf, 'x', data_len);

  fprintf(stderr, "putting object\n");

  ret = dpl_post(ctx,           //the context
                 NULL,          //no bucket
                 folder,        //the folder
                 NULL,          //no option
                 DPL_FTYPE_REG, //regular object
                 NULL,          //condition
                 NULL,          //range
                 NULL,          //no metadata
                 NULL,          //no sysmd
                 data_buf,      //object body
                 data_len,      //object length
                 NULL,          //no query params
                 &sysmd);       //the returned sysmd
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_post failed: %s (%d)\n", dpl_status_str(ret), ret);
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
