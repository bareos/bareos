/*
 * simple buffered example which use ID capable REST services
 */

#include <droplet.h>
#include <droplet/utils.h>
#include <assert.h>

int
main(int argc,
     char **argv)
{
  int ret;
  dpl_ctx_t *ctx;
  dpl_dict_t *metadata = NULL;
  char *data_buf = NULL;
  char *data_ptr;
  size_t data_len;
  char *data_buf_returned = NULL;
  u_int data_len_returned;
  dpl_dict_t *metadata_returned = NULL;
  dpl_dict_t *metadata2_returned = NULL;
  dpl_dict_var_t *metadatum = NULL;
  dpl_option_t option;
  dpl_sysmd_t sysmd;
  char *force_id = NULL;
  dpl_conn_t *conn = NULL;
  int block_size, remain_size, offset;
  int connection_close;
  dpl_dict_t *headers_returned;
  
  if (2 == argc)
    {
      force_id = argv[1];
    }
  else if (1 != argc)
    {
      fprintf(stderr, "usage: idtest [id]\n");
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

  if (force_id)
    {
      //we have a broken cloud storage with no POST
      ret = dpl_put_id_buffered(ctx,           //the context
				NULL,          //no bucket
				force_id,      //the id
				NULL,          //no option
				DPL_FTYPE_REG, //regular object
				NULL,          //no condition
				NULL,          //no range
				metadata,      //the metadata
				NULL,          //no sysmd
				data_len,      //object length
				&conn);        //dpl_conn_t*
      if (DPL_SUCCESS != ret)
        {
          fprintf(stderr, "dpl_put_id failed: %s (%d)\n", dpl_status_str(ret), ret);
          ret = 1;
          goto free_all;
        }

      //emulate returned sysmd
      memset(&sysmd, 0, sizeof (sysmd));
      strncpy(sysmd.id, force_id, sizeof (sysmd.id));
    }
  else
    {
      ret = dpl_post_id_buffered(ctx,           //the context
				 NULL,          //no bucket
				 NULL,          //no id
				 NULL,          //no option
				 DPL_FTYPE_REG, //regular object
				 NULL,          //condition
				 NULL,          //range
				 metadata,      //the metadata
				 NULL,          //no sysmd
				 data_len,      //object length
				 NULL,          //no query params
				 &conn);        //dpl_conn_t*
      if (DPL_SUCCESS != ret)
        {
          fprintf(stderr, "dpl_post_id_buffered failed: %s (%d)\n", dpl_status_str(ret), ret);
          ret = 1;
          goto free_all;
        }
      
      if (!(sysmd.mask & DPL_SYSMD_MASK_ID))
        {
          fprintf(stderr, "backend is not capable of retrieving resource id\n");
          exit(1);
        }
      
      fprintf(stderr, "id=%s\n", sysmd.id);
    }

  block_size = 1000;
  remain_size = data_len;
  data_ptr = data_buf;

  while (remain_size > 0)
    {
      int size = MIN(remain_size, block_size);
      struct iovec iov;

      iov.iov_base = data_ptr;
      iov.iov_len = size;
      ret = dpl_conn_writev_all(conn, &iov, 1, 10);
      if (DPL_SUCCESS != ret)
	{
	  fprintf(stderr, "dpl_conn_writev_all failed: %s (%d)\n", dpl_status_str(ret), ret);
	  exit(1);
	}

      data_ptr += size;
      remain_size -= size;
    }

  ret = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_returned, &connection_close);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "read http_reply failed %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }

  if (1 == connection_close)
    dpl_conn_terminate(conn);
  else
    dpl_conn_release(conn);

  dpl_dict_free(headers_returned);
  
  /**/

  fprintf(stderr, "getting object+MD\n");

  remain_size = data_len;
  offset = 0;

  //we known file size in advance
  data_buf_returned = malloc(data_len);
  if (NULL == data_buf_returned)
    {
      perror("malloc");
      exit(1);
    }
  data_len_returned = data_len;

  while (remain_size > 0)
    {
      int size = MIN(block_size, remain_size);
      dpl_range_t range;
      char *bufp;
      int len;

      range.start = offset;
      range.end = offset + size - 1;
      bufp = data_buf_returned + offset;
      len = size;

      option.mask = DPL_OPTION_NOALLOC; //we already allocated buffer
		     
      ret = dpl_get_id(ctx,           //the context
		       NULL,          //no bucket
		       sysmd.id,      //the key
		       &option,       //options
		       DPL_FTYPE_REG, //object type
		       NULL,          //no condition
		       &range,        //no range
		       &bufp,         //data object
		       &len,          //data object length
		       NULL,                //metadata
		       NULL);               //sysmd
      if (DPL_SUCCESS != ret)
	{
	  fprintf(stderr, "dpl_get_id failed: %s (%d)\n", dpl_status_str(ret), ret);
	  ret = 1;
	  goto free_all;
	}

      if (len != size)
	{
	  fprintf(stderr, "short read %d != %d\n", data_len_returned, size);
	  ret = 1;
	  goto free_all;
	}

      offset += size;
      remain_size -= size;
    }

  //we fetch the attributes in a separate call
  ret = dpl_head_id(ctx,      //the context
                    NULL,     //no bucket,
                    sysmd.id, //the key
                    NULL,     //option
                    DPL_FTYPE_UNDEF, //no matter the file type
                    NULL,     //no condition,
                    &metadata_returned,
                    NULL);    //no sysmd
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error getting metadata: %s (%d)\n", dpl_status_str(ret), ret);
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

  assert(metadatum->val->type == DPL_VALUE_STRING);
  if (strcmp(dpl_sbuf_get_str(metadatum->val->string), "bar"))
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

  assert(metadatum->val->type == DPL_VALUE_STRING);
  if (strcmp(dpl_sbuf_get_str(metadatum->val->string), "qux"))
    {
      fprintf(stderr, "bad value in metadatum\n");
      ret = 1;
      goto free_all;
    }

  /**/

  fprintf(stderr, "setting MD only\n");

  ret = dpl_dict_add(metadata, "foo", "bar2", 0);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error updating metadatum: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  ret = dpl_copy_id(ctx,           //the context
                    NULL,          //no src bucket
                    sysmd.id,      //the key
                    NULL,          //no dst bucket
                    sysmd.id,      //the same key
                    NULL,          //no option
                    DPL_FTYPE_REG, //object type
                    DPL_COPY_DIRECTIVE_METADATA_REPLACE,  //tell server to replace metadata
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
                    sysmd.id, //the key
                    NULL,     //option
                    DPL_FTYPE_UNDEF, //no matter the file type
                    NULL,     //no condition,
                    &metadata2_returned,
                    NULL);    //no sysmd
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

  assert(metadatum->val->type == DPL_VALUE_STRING);
  if (strcmp(dpl_sbuf_get_str(metadatum->val->string), "bar2"))
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

  assert(metadatum->val->type == DPL_VALUE_STRING);
  if (strcmp(dpl_sbuf_get_str(metadatum->val->string), "qux"))
    {
      fprintf(stderr, "bad value in metadatum\n");
      ret = 1;
      goto free_all;
    }

  /**/

  fprintf(stderr, "delete object+MD\n");

  ret = dpl_delete_id(ctx,       //the context
                      NULL,      //no bucket
                      sysmd.id,  //the key
                      NULL,      //no option
                      DPL_FTYPE_UNDEF, //no matter the file type
                      NULL);     //no condition
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
