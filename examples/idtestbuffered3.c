/*
 * simple buffered example which use ID capable REST services (emulate blocks with UKS)
 */

#include <droplet.h>
#include <droplet/utils.h>
#include <droplet/uks/uks.h>
#include <droplet/dbuf.h>
#include <droplet/scal/gc.h>
#include <assert.h>

char *
my_dpl_uks_bn2hex(const BIGNUM *id)
{
  char *id_str = NULL;
  
  id_str = malloc(DPL_UKS_BCH_LEN+1);
  if (!id_str)
    return NULL;
  
  if (dpl_uks_bn2hex(id, id_str) != DPL_SUCCESS)
    {
      free(id_str);
      return NULL;
    }
  
  return id_str;
}

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
  BIGNUM *id = NULL;
  char *main_id = NULL;
  dpl_conn_t *conn = NULL;
  int block_size, remain_size, offset;
  int connection_close;
  dpl_dict_t *headers_returned;
  uint64_t file_identifier;
  int first;
  dpl_dbuf_t *dbuf = NULL;
  char *gc_id = NULL;
  
  if (1 != argc)
    {
      fprintf(stderr, "usage: idtestbuffered3\n");
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

  file_identifier = dpl_rand_u64();

  id = BN_new();
  if (NULL == id)
    {
      perror("BN_new");
      ret = 1;
      goto free_all;
    }

  ret = dpl_uks_gen_key(id, 
			file_identifier,  //file id
			0,                //volume id
			0xc,              //customer tag
			0);               //block id
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_uks_gen_key failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  ret = dpl_uks_set_class(id,
			  2);             //means 3 replicas
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_uks_set_class failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  main_id = my_dpl_uks_bn2hex(id);
  if (NULL == main_id)
    {
      perror("my_dpl_uks_bn2hex");
      ret = 1;
      goto free_all;
    }

  first = 1;
  block_size = 1000;
  remain_size = data_len;
  offset = 0;
  while (remain_size > 0)
    {
      int size = MIN(block_size, remain_size);
      int block_nb = offset / block_size;
      char *block_id = NULL;

      ret = dpl_uks_gen_key(id, 
			    file_identifier,  //file id
			    0,                //volume id
			    0xc,              //customer tag
			    block_nb);        //block nb
      if (DPL_SUCCESS != ret)
	{
	  fprintf(stderr, "dpl_uks_gen_key failed: %s (%d)\n", dpl_status_str(ret), ret);
	  ret = 1;
	  goto free_all;
	}
      
      ret = dpl_uks_set_class(id,
			      2);             //means 3 replicas
      if (DPL_SUCCESS != ret)
	{
	  fprintf(stderr, "dpl_uks_set_class failed: %s (%d)\n", dpl_status_str(ret), ret);
	  ret = 1;
	  goto free_all;
	}

      block_id = my_dpl_uks_bn2hex(id);
      if (NULL == block_id)
	{
	  perror("my_dpl_uks_bn2hex");
	  ret = 1;
	  goto free_all;
	}

      ret = dpl_put_id(ctx,           //the context
                       NULL,          //no bucket
                       block_id,      //the id
                       NULL,          //no option
                       DPL_FTYPE_REG, //regular object
                       NULL,          //no condition
                       NULL,          //no range
                       first ? metadata : NULL,      //the metadata
                       NULL,          //no sysmd
                       data_buf + offset,      //object body
                       size);                  //object length
      if (DPL_SUCCESS != ret)
	{
	  fprintf(stderr, "dpl_put_id %s failed: %s (%d)\n", block_id, dpl_status_str(ret), ret);
	  ret = 1;
	  goto free_all;
	}

      first = 0;
      offset += size;
      remain_size -= size;

      free(block_id);
    }
  
  /**/

  fprintf(stderr, "getting object+MD\n");

  //we known file size in advance
  data_buf_returned = malloc(data_len);
  if (NULL == data_buf_returned)
    {
      perror("malloc");
      exit(1);
    }
  data_len_returned = data_len;

  first = 1;
  remain_size = data_len;
  offset = 0;
  while (remain_size > 0)
    {
      int size = MIN(block_size, remain_size);
      int block_nb = offset / block_size;
      char *block_id = NULL;
      char *bufp;
      int len;

      ret = dpl_uks_gen_key(id, 
			    file_identifier,  //file id
			    0,                //volume id
			    0xc,              //customer tag
			    block_nb);        //block nb
      if (DPL_SUCCESS != ret)
	{
	  fprintf(stderr, "dpl_uks_gen_key failed: %s (%d)\n", dpl_status_str(ret), ret);
	  ret = 1;
	  goto free_all;
	}
      
      ret = dpl_uks_set_class(id,
			      2);             //means 3 replicas
      if (DPL_SUCCESS != ret)
	{
	  fprintf(stderr, "dpl_uks_set_class failed: %s (%d)\n", dpl_status_str(ret), ret);
	  ret = 1;
	  goto free_all;
	}

      block_id = my_dpl_uks_bn2hex(id);
      if (NULL == block_id)
	{
	  perror("my_dpl_uks_bn2hex");
	  ret = 1;
	  goto free_all;
	}

      bufp = data_buf_returned + offset;
      len = size;

      option.mask = DPL_OPTION_NOALLOC; //we already allocated buffer
		     
      ret = dpl_get_id(ctx,           //the context
		       NULL,          //no bucket
		       block_id,      //the key
		       &option,       //options
		       DPL_FTYPE_REG, //object type
		       NULL,          //no condition
		       NULL,          //no range
		       &bufp,         //data object
		       &len,          //data object length
		       first ? &metadata_returned : NULL, //metadata
		       NULL);               //sysmd
      if (DPL_SUCCESS != ret)
	{
	  fprintf(stderr, "dpl_get_id %s failed: %s (%d)\n", block_id, dpl_status_str(ret), ret);
	  ret = 1;
	  goto free_all;
	}

      if (len != size)
	{
	  fprintf(stderr, "short read %d != %d\n", data_len_returned, size);
	  ret = 1;
	  goto free_all;
	}

      first = 0;
      offset += size;
      remain_size -= size;

      free(block_id);
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
                    main_id,      //the key
                    NULL,          //no dst bucket
                    main_id,      //the same key
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
                    main_id, //the key
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

  ret = dpl_scal_gc_index_init(&dbuf);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "gc init failed\n");
      ret = 1;
      goto free_all;
    }

  remain_size = data_len;
  offset = 0;
  while (remain_size > 0)
    {
      int size = MIN(block_size, remain_size);
      int block_nb = offset / block_size;

      ret = dpl_uks_gen_key(id, 
			    file_identifier,  //file id
			    0,                //volume id
			    0xc,              //customer tag
			    block_nb);        //block nb
      if (DPL_SUCCESS != ret)
	{
	  fprintf(stderr, "dpl_uks_gen_key failed: %s (%d)\n", dpl_status_str(ret), ret);
	  ret = 1;
	  goto free_all;
	}
      
      ret = dpl_uks_set_class(id,
			      2);             //means 3 replicas
      if (DPL_SUCCESS != ret)
	{
	  fprintf(stderr, "dpl_uks_set_class failed: %s (%d)\n", dpl_status_str(ret), ret);
	  ret = 1;
	  goto free_all;
	}

      ret = dpl_scal_gc_index_serialize(id,
					offset, //offset of block (not mandatory)
					block_size, //size of block (not mandatory)
					dbuf);
      if (DPL_SUCCESS != ret)
	{
	  fprintf(stderr, "error marking object for deletion: %s (%d)\n", dpl_status_str(ret), ret);
	  ret = 1;
	  goto free_all;
	}

      offset += size;
      remain_size -= size;
    }

  //generate GC chunk
  dpl_scal_gc_gen_key(id, 2);

  gc_id = my_dpl_uks_bn2hex(id);
  if (NULL == gc_id)
    {
      perror("my_dpl_uks_bn2hex");
      ret = 1;
      goto free_all;
    }

  //push GC chunk
  ret = dpl_put_id(ctx,           //the context
		   NULL,          //no bucket
		   gc_id,         //the id
		   NULL,          //no option
		   DPL_FTYPE_REG, //regular object
		   NULL,          //no condition
		   NULL,          //no range
		   NULL,          //no metadata
		   NULL,          //no sysmd
		   dbuf->data,                 //object body
		   dpl_dbuf_length(dbuf));     //object length
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_put_id %s failed: %s (%d)\n", gc_id, dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  ret = 0;

 free_all:

  if (NULL != gc_id)
    free(gc_id);

  if (NULL != main_id)
    free(main_id);
  
  if (NULL != id)
    BN_free(id);

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

  if (NULL != dbuf)
    dpl_dbuf_free(dbuf);

  dpl_ctx_free(ctx); //free context

 free_dpl:
  dpl_free();        //free droplet library

 end:
  return ret;
}
