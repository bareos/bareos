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
  dpl_dict_t *metadata = NULL;
  char *data_buf = NULL;
  size_t data_len;
  char *data_buf_returned = NULL;
  u_int data_len_returned;
  dpl_dict_t *metadata_returned = NULL;
  dpl_dict_t *metadata2_returned = NULL;
  dpl_var_t *metadatum = NULL;

  if (2 != argc)
    {
      fprintf(stderr, "usage: idtest key\n");
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

  data_len = 10000;
  data_buf = malloc(data_len);
  if (NULL == data_buf)
    {
      fprintf(stderr, "alloc data failed\n");
      ret = 1;
      goto free_all;
    }

  memset(data_buf, 'z', data_len);

  metadata = dpl_dict_new(13);
  if (NULL == metadata)
    {
      fprintf(stderr, "dpl_dict_new failed\n");
      ret = 1;
      goto free_all;
    }
 
  ret = dpl_dict_add(metadata, "foo", "bar", 0);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_dict_add failed\n");
      ret = 1;
      goto free_all;
    }

  ret = dpl_dict_add(metadata, "foo2", "qux", 0);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_dict_add failed\n");
      ret = 1;
      goto free_all;
    }

  /**/
  
  fprintf(stderr, "setting object+MD\n");

  ret = dpl_put_id(ctx,           //the context
                   NULL,          //no bucket
                   id,            //the key
                   NULL,          //no subresource
                   DPL_FTYPE_REG, //regular object
                   metadata,      //the metadata
                   NULL,          //no sysmd
                   data_buf,      //object body
                   data_len);     //object length
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_put_id failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  /**/

  fprintf(stderr, "getting object+MD\n");

  ret = dpl_get_id(ctx,           //the context
                   NULL,          //no bucket
                   id,            //the key
                   NULL,          //no subresource
                   DPL_FTYPE_REG, //object type
                   NULL,          //no condition
                   &data_buf_returned,  //data object
                   &data_len_returned,  //data object length
                   &metadata_returned); //metadata
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_get_id failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  fprintf(stderr, "checking object\n");

  if (data_len != data_len_returned)
    {
      fprintf(stderr, "data lengths mismatch\n");
      ret = 1;
      goto free_all;
    }

  if (0 != memcmp(data_buf, data_buf_returned, data_len))
    {
      fprintf(stderr, "data content mismatch\n");
      ret = 1;
      goto free_all;
    }

  fprintf(stderr, "checking metadata\n");

  metadatum = dpl_dict_get(metadata_returned, "foo");
  if (NULL == metadatum)
    {
      fprintf(stderr, "missing metadatum\n");
      ret = 1;
      goto free_all;
    }

  if (strcmp(metadatum->value, "bar"))
    {
      fprintf(stderr, "bad value in metadatum\n");
      ret = 1;
      goto free_all;
    }

  metadatum = dpl_dict_get(metadata_returned, "foo2");
  if (NULL == metadatum)
    {
      fprintf(stderr, "missing metadatum\n");
      ret = 1;
      goto free_all;
    }

  if (strcmp(metadatum->value, "qux"))
    {
      fprintf(stderr, "bad value in metadatum\n");
      ret = 1;
      goto free_all;
    }

  /**/

  fprintf(stderr, "setting MD only\n");

  ret = dpl_dict_update_value(metadata, "foo", "bar2");
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error updating metadatum: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  ret = dpl_copy_id(ctx,           //the context
                    NULL,          //no src bucket
                    id,            //the key
                    NULL,          //no subresource
                    NULL,          //no dst bucket
                    id,            //the same key
                    NULL,          //no subresource
                    DPL_FTYPE_REG, //object type
                    DPL_METADATA_DIRECTIVE_REPLACE,  //tell server to replace metadata
                    metadata,      //the updated metadata
                    NULL,          //no sysmd
                    NULL);         //no condition
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error updating metadata: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  /**/

  fprintf(stderr, "getting MD only\n");

  ret = dpl_head_id(ctx,      //the context
                    NULL,     //no bucket,
                    id,       //the key
                    NULL,     //no subresource
                    NULL,     //no condition,
                    &metadata2_returned);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error getting metadata: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  fprintf(stderr, "checking metadata\n");

  metadatum = dpl_dict_get(metadata2_returned, "foo");
  if (NULL == metadatum)
    {
      fprintf(stderr, "missing metadatum\n");
      ret = 1;
      goto free_all;
    }

  if (strcmp(metadatum->value, "bar2"))
    {
      fprintf(stderr, "bad value in metadatum\n");
      ret = 1;
      goto free_all;
    }

  metadatum = dpl_dict_get(metadata2_returned, "foo2");
  if (NULL == metadatum)
    {
      fprintf(stderr, "missing metadatum\n");
      ret = 1;
      goto free_all;
    }

  if (strcmp(metadatum->value, "qux"))
    {
      fprintf(stderr, "bad value in metadatum\n");
      ret = 1;
      goto free_all;
    }

  /**/

  fprintf(stderr, "delete object+MD\n");

  ret = dpl_delete_id(ctx,       //the context
                      NULL,      //no bucket
                      id,        //the key
                      NULL);     //no subresource
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error deleting object: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  ret = 0;

 free_all:

  if (NULL != metadata2_returned)
    dpl_dict_free(metadata2_returned);

  if (NULL != metadata_returned)
    dpl_dict_free(metadata_returned);

  if (NULL != data_buf_returned)
    free(data_buf_returned);

  if (NULL != metadata)
    dpl_dict_free(metadata);

  if (NULL != data_buf)
    free(data_buf);

  dpl_ctx_free(ctx); //free context

 free_dpl:
  dpl_free();        //free droplet library

 end:
  return ret;
}
