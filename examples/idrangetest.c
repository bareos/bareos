/*
 * simple example which use ID capable REST services
 *
 * @note this test suppose you are able to generate a suitable key for your REST system
 */

#include <droplet.h>

int
main(int argc,
     char **argv)
{
  int ret;
  dpl_ctx_t *ctx;
  char *id = NULL;
  char *data_buf_returned = NULL;
  u_int data_len_returned;
  dpl_range_t range;

  if (2 != argc)
    {
      fprintf(stderr, "usage: idrangetest key\n");
      ret = 1;
      goto end;
    }

  id = argv[1];

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
  
  fprintf(stderr, "setting object+MD\n");

  ret = dpl_put_id(ctx,           //the context
                   NULL,          //no bucket
                   id,            //the key
                   0,             //enterprise number
                   NULL,          //no subresource
                   NULL,          //no option
                   DPL_FTYPE_REG, //regular object
                   NULL,          //no condition
                   NULL,          //no range
                   NULL,          //no metadata
                   NULL,          //no sysmd
                   "foobarbaz",   //object body
                   9);            //object length
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_put_id failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  /**/

  fprintf(stderr, "getting partial object+MD\n");

  range.start = 3;
  range.end = 5;
  ret = dpl_get_id(ctx,           //the context
                   NULL,          //no bucket
                   id,            //the key
                   0,             //enterprise number
                   NULL,          //no subresource
                   NULL,          //no option
                   DPL_FTYPE_REG, //object type
                   NULL,          //no condition of operation
                   &range,          //range
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

  if (data_len_returned != 3)
    {
      fprintf(stderr, "bad content length\n");
      ret = 1;
      goto free_all;
    }

  if (memcmp(data_buf_returned, "bar", 3))
    {
      fprintf(stderr, "bad content\n");
      ret = 1;
      goto free_all;
    }

  /**/

  fprintf(stderr, "delete object+MD\n");

  ret = dpl_delete_id(ctx,       //the context
                      NULL,      //no bucket
                      id,        //the key
                      0,         //enterprise number
                      NULL,      //no subresource
                      NULL,      //no option
                      NULL);     //no condition
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error deleting object: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

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
