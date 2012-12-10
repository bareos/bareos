
#include <droplet.h>

int
main(int argc,
     char **argv)
{
  int ret;
  dpl_ctx_t *ctx;
  char *data_buf_returned = NULL;
  u_int data_len_returned;
  dpl_range_t range;
  dpl_sysmd_t sysmd;
  char *force_id = NULL;

  if (2 == argc)
    {
      force_id = argv[1];
    }
  else
    {
      fprintf(stderr, "usage: restrangetest path\n");
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

  /**/
  
  fprintf(stderr, "creating empty file\n");

#if 0
  ret = dpl_put(ctx,           //the context
                   NULL,          //no bucket
                   force_id,      //the id
                   NULL,          //no option
                   DPL_FTYPE_REG, //regular object
                   NULL,          //no condition
                   NULL,          //no range
                   NULL,          //the metadata
                   NULL,          //no sysmd
                   NULL,      //object body
                   0); //object length
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_put_id failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }
#endif

#undef DATA_BUF
#define DATA_BUF "foobarbazqux"

  range.start = 0;
  range.end = 11;

  ret = dpl_put(ctx,           //the context
                   NULL,          //no bucket
                   force_id,      //the id
                   NULL,          //no option
                   DPL_FTYPE_REG, //regular object
                   NULL,          //no condition
                   &range,        //no range
                   NULL,          //the metadata
                   NULL,          //no sysmd
                   DATA_BUF,      //object body
                   strlen(DATA_BUF)); //object length
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_put failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

#undef DATA_BUF
#define DATA_BUF "abcdef012345"

  range.start = 12;
  range.end = 23;

  ret = dpl_put(ctx,           //the context
                   NULL,          //no bucket
                   force_id,      //the id
                   NULL,          //no option
                   DPL_FTYPE_REG, //regular object
                   NULL,          //no condition
                   &range,        //no range
                   NULL,          //the metadata
                   NULL,          //no sysmd
                   DATA_BUF,      //object body
                   strlen(DATA_BUF)); //object length
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_put failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

#undef DATA_BUF
#define DATA_BUF "hello world!"

  range.start = 24;
  range.end = 35;

  ret = dpl_put(ctx,           //the context
                   NULL,          //no bucket
                   force_id,      //the id
                   NULL,          //no option
                   DPL_FTYPE_REG, //regular object
                   NULL,          //no condition
                   &range,        //no range
                   NULL,          //the metadata
                   NULL,          //no sysmd
                   DATA_BUF,      //object body
                   strlen(DATA_BUF)); //object length
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_put failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  /**/

  fprintf(stderr, "getting partial object+MD\n");

  ret = dpl_get(ctx,           //the context
                   NULL,          //no bucket
                   force_id,      //the key
                   NULL,          //no option
                   DPL_FTYPE_REG, //object type
                   NULL,          //no condition of operation
                   NULL,          //range
                   &data_buf_returned,  //data object
                   &data_len_returned,  //data object length
                   NULL,               //no metadata
                   NULL);              //no sysmd
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_get_id failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  fprintf(stderr, "checking object\n");

  write(1, data_buf_returned, data_len_returned);
  write(1, "\n", 1);

  /**/

  ret = 0;

 free_all:

  if (NULL != data_buf_returned)
    free(data_buf_returned);

  dpl_ctx_free(ctx); //free context

 free_dpl:
  dpl_free();        //free droplet library

 end:
  return ret;
}
