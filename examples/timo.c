/*
 * Timo use cases:
 *
 * 1) adding object without name, returns a unique 128bit(?) ID for it
 * 2) adding object with name (path), overwriting existing value
 * 3) adding object with name (path), failing if the object already exists
 * 4) appending data to an existing object, failing if it doesn't already exist
 * 5) appending data to an existing object, or automatically creating it if it doesn't already exist and adding the data
 * 6) adding metadata to added objects (for named/unnamed objects)
 * 7) getting object contents (and metadata) by ID
 * 8) getting object contents (and metadata) by name
 * 9) getting object contents only partially (e.g. first 1000 bytes)
 * 10) getting object metadata without its contents
 * 11) copying ID object with changed metadata (is the content duplicated in the storage?)
 * 12) copying named object with changed metadata (is the content duplicated in the storage?)
 */

#include <droplet.h>
#include <droplet/async.h>
#include <assert.h>
#include <sys/param.h>

#define DATA_LEN 10000

#define FILE1 "u.1"
#define FILE2 "u.2"
#define FILE3 "u.3"
#define FILE4 "u.4"

dpl_ctx_t *ctx = NULL;
dpl_task_pool_t *pool = NULL;
char *folder = NULL;
int folder_len;
char id1[DPL_SYSMD_ID_SIZE+1];
char file1_path[MAXPATHLEN];
char file2_path[MAXPATHLEN];
char file3_path[MAXPATHLEN];
char file4_path[MAXPATHLEN];

pthread_mutex_t prog_lock;
pthread_cond_t prog_cond;

pthread_mutex_t list_lock;
pthread_cond_t list_cond;
int n_ok = 0;

void
banner(char *str)
{
  int i;
  int len = strlen(str);

  for (i = 0;i < len+4;i++)
    fprintf(stderr, "*");
  fprintf(stderr, "\n* %s *\n", str);
  for (i = 0;i < len+4;i++)
    fprintf(stderr, "*");
  fprintf(stderr, "\n");
}

void
free_all()
{
  banner("finished");

  if (NULL != pool)
    {
      //dpl_task_pool_cancel(pool);
      //dpl_task_pool_destroy(pool);
    }

  dpl_ctx_free(ctx); //free context

  dpl_free();        //free droplet library

  pthread_cond_signal(&prog_cond);
}

void
cb_copy_named_object_with_new_md(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "rename object failed: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  dpl_async_task_free(atask);

  free_all();
}

void
copy_named_object_with_new_md()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;
  dpl_dict_t *metadata = NULL;

  banner("12 - copy named object with new metadata");

  metadata = dpl_dict_new(13);
  if (NULL == metadata)
    {
      ret = DPL_ENOMEM;
      exit(1);
    }

  ret = dpl_dict_add(metadata, "qux", "baz", 0);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error updating metadatum: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }

  atask = (dpl_async_task_t *) dpl_copy_async_prepare(ctx,
                                                      NULL,          //no src bucket
                                                      file1_path,    //the src resource
                                                      NULL,          //no dst bucket
                                                      file4_path,    //dst resource
                                                      NULL,          //no option
                                                      DPL_FTYPE_REG, //regular file
                                                      DPL_COPY_DIRECTIVE_COPY, //rename
                                                      metadata,      //metadata
                                                      NULL,          //no sysmd
                                                      NULL);         //no server side condition
  
  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_copy_named_object_with_new_md;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_copy_nameless_object_with_new_md(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "rename object failed: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  dpl_async_task_free(atask);

  copy_named_object_with_new_md();
}

void
copy_nameless_object_with_new_md()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;
  dpl_dict_t *metadata = NULL;

  banner("11 - copy nameless object with new metadata");

  metadata = dpl_dict_new(13);
  if (NULL == metadata)
    {
      ret = DPL_ENOMEM;
      exit(1);
    }

  ret = dpl_dict_add(metadata, "bar", "qux", 0);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error updating metadatum: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }

  /* 
   * note: With Dewpoint, it would be possible to copy nameless object into another nameless object.
   * Does it make sense ? for now we copy it into a named object
   */
  atask = (dpl_async_task_t *) dpl_copy_id_async_prepare(ctx,
                                                         NULL,          //no src bucket
                                                         id1,           //the src resource
                                                         NULL,          //no dst bucket
                                                         file1_path,    //dst resource
                                                         NULL,          //no option
                                                         DPL_FTYPE_REG, //regular file
                                                         DPL_COPY_DIRECTIVE_COPY, //rename
                                                         metadata,      //metadata
                                                         NULL,          //no sysmd
                                                         NULL);         //no server side condition
  
  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_copy_nameless_object_with_new_md;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_get_object_meta(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;
  int i;
  dpl_dict_var_t *metadatum = NULL;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "dpl_get failed: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  printf("metadata:\n");
  dpl_dict_print(atask->u.head.metadata, stdout, 0);

  dpl_async_task_free(atask);
  
  copy_nameless_object_with_new_md();
}

void
get_object_meta()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;

  banner("10 - get object meta");
  
  atask = (dpl_async_task_t *) dpl_head_async_prepare(ctx,           //the context
                                                      NULL,          //no bucket
                                                      file3_path,    //the key
                                                      NULL,          //no opion
                                                      DPL_FTYPE_REG, //object type
                                                      NULL);         //no condition

  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_get_object_meta;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_get_named_object_partially(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;
  int i;
  dpl_dict_var_t *metadatum = NULL;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "dpl_get failed: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  printf("data len: %d\n", dpl_buf_size(atask->u.get.buf));
  printf("metadata:\n");
  dpl_dict_print(atask->u.get.metadata, stdout, 0);

  dpl_async_task_free(atask);

  get_object_meta();
}

void
get_named_object_partially()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;
  dpl_range_t range;

  banner("9 - get named object partially");
  
  //first 1000 bytes
  range.start = 0;
  range.end = 999;
  atask = (dpl_async_task_t *) dpl_get_async_prepare(ctx,           //the context
                                                     NULL,          //no bucket
                                                     file3_path,    //the key
                                                     NULL,          //no option
                                                     DPL_FTYPE_REG, //object type
                                                     NULL,          //no condition
                                                     &range);       //range

  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_get_named_object_partially;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_get_named_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;
  int i;
  dpl_dict_var_t *metadatum = NULL;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "dpl_get failed: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  printf("data:\n");
  write(1, dpl_buf_ptr(atask->u.get.buf), MIN(dpl_buf_size(atask->u.get.buf), 10));
  printf("...\nmetadata:\n");
  dpl_dict_print(atask->u.get.metadata, stdout, 0);

  dpl_async_task_free(atask);

  get_named_object_partially();
}

void
get_named_object()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;

  banner("8 - get named object data+metadata");
  
  atask = (dpl_async_task_t *) dpl_get_async_prepare(ctx,           //the context
                                                     NULL,          //no bucket
                                                     file3_path,    //the key
                                                     NULL,          //no opion
                                                     DPL_FTYPE_REG, //object type
                                                     NULL,          //no condition
                                                     NULL);         //no range

  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_get_named_object;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_get_nameless_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;
  int i;
  dpl_dict_var_t *metadatum = NULL;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "dpl_get failed: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  printf("data:\n");
  write(1, dpl_buf_ptr(atask->u.get.buf), MIN(dpl_buf_size(atask->u.get.buf), 10));
  printf("...\nmetadata:\n");
  dpl_dict_print(atask->u.get.metadata, stdout, 0);

  dpl_async_task_free(atask);

  get_named_object();
}

void
get_nameless_object()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;

  banner("7 - get nameless object data+metadata");

  atask = (dpl_async_task_t *) dpl_get_id_async_prepare(ctx,           //the context
                                                        NULL,          //no bucket
                                                        id1,           //the key
                                                        NULL,          //no opion
                                                        DPL_FTYPE_REG, //object type
                                                        NULL,          //no condition
                                                        NULL);         //no range

  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_get_nameless_object;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_update_metadata_nameless_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "abnormal answer: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  dpl_async_task_free(atask);
  
  get_nameless_object();
}

void
update_metadata_nameless_object()
{
  dpl_async_task_t *atask = NULL;
  dpl_status_t ret;
  dpl_option_t option;
  dpl_dict_t *metadata = NULL;

  banner("6bis - append metadata to existing nameless object");

  metadata = dpl_dict_new(13);
  if (NULL == metadata)
    {
      ret = DPL_ENOMEM;
      exit(1);
    }

  ret = dpl_dict_add(metadata, "foo", "bar", 0);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error updating metadatum: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }
  
  option.mask = DPL_OPTION_APPEND_METADATA;

  atask = (dpl_async_task_t *) dpl_put_id_async_prepare(ctx,
                                                        NULL,          //no bucket
                                                        id1,           //the id
                                                        &option,       //option
                                                        DPL_FTYPE_REG, //regular object
                                                        NULL,          //condition
                                                        NULL,          //range
                                                        metadata,      //the metadata
                                                        NULL,          //no sysmd
                                                        NULL);         //object body

  dpl_dict_free(metadata);

  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_update_metadata_nameless_object;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_update_metadata_named_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "abnormal answer: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  dpl_async_task_free(atask);
  
  update_metadata_nameless_object();
}

void
update_metadata_named_object()
{
  dpl_async_task_t *atask = NULL;
  dpl_status_t ret;
  dpl_option_t option;
  dpl_dict_t *metadata = NULL;

  banner("6 - append metadata to existing named object");

  metadata = dpl_dict_new(13);
  if (NULL == metadata)
    {
      ret = DPL_ENOMEM;
      exit(1);
    }

  ret = dpl_dict_add(metadata, "foo", "bar", 0);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error updating metadatum: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }
  
  option.mask = DPL_OPTION_APPEND_METADATA;

  atask = (dpl_async_task_t *) dpl_put_async_prepare(ctx,
                                                     NULL,          //no bucket
                                                     file3_path,    //the id
                                                     &option,       //option
                                                     DPL_FTYPE_REG, //regular object
                                                     NULL,          //condition
                                                     NULL,          //range
                                                     metadata,      //the metadata
                                                     NULL,          //no sysmd
                                                     NULL);         //object body

  dpl_dict_free(metadata);

  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_update_metadata_named_object;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_append_to_nonexisting_named_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "abnormal answer: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  dpl_async_task_free(atask);
  
  update_metadata_named_object();
}

void
append_to_nonexisting_named_object()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;

  buf = dpl_buf_new();
  if (NULL == buf)
    exit(1);

  buf->size = DATA_LEN;
  buf->ptr = malloc(buf->size);
  if (NULL == buf->ptr)
    {
      fprintf(stderr, "alloc data failed\n");
      exit(1);
    }

  memset(buf->ptr, 'z', buf->size);

  banner("5 - append data to nonexisting named object");
  
  atask = (dpl_async_task_t *) dpl_post_async_prepare(ctx,
                                                      NULL,          //no bucket
                                                      file3_path,    //the id
                                                      NULL,          //option
                                                      DPL_FTYPE_REG, //regular object
                                                      NULL,          //no condition
                                                      NULL,          //range
                                                      NULL,          //the metadata
                                                      NULL,          //no sysmd
                                                      buf,           //object body
                                                      NULL);         //query params
  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_append_to_nonexisting_named_object;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_append_to_nonexisting_named_object_precond(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_EPRECOND != atask->ret)
    {
      fprintf(stderr, "abnormal answer: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      //exit(1);
    }

  dpl_async_task_free(atask);

  append_to_nonexisting_named_object();
}

void
append_to_nonexisting_named_object_precond()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;
  dpl_condition_t condition;

  buf = dpl_buf_new();
  if (NULL == buf)
    exit(1);

  buf->size = DATA_LEN;
  buf->ptr = malloc(buf->size);
  if (NULL == buf->ptr)
    {
      fprintf(stderr, "alloc data failed\n");
      exit(1);
    }
  
  memset(buf->ptr, 'z', buf->size);

  banner("4 - append data to nonexisting named object with precondition");
  
  condition.mask = DPL_CONDITION_IF_MATCH;
  condition.etag[0] = '*';
  condition.etag[1] = 0;

  atask = (dpl_async_task_t *) dpl_post_async_prepare(ctx,
                                                      NULL,          //no bucket
                                                      file3_path,    //the id
                                                      NULL,          //option
                                                      DPL_FTYPE_REG, //regular object
                                                      &condition,    //expect failure if exists
                                                      NULL,          //range
                                                      NULL,          //the metadata
                                                      NULL,          //no sysmd
                                                      buf,           //object body
                                                      NULL);         //query params
  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_append_to_nonexisting_named_object_precond;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_add_existing_named_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_EPRECOND != atask->ret)
    {
      fprintf(stderr, "abnormal answer: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  dpl_async_task_free(atask);

  append_to_nonexisting_named_object_precond();
}

void
add_existing_named_object()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;
  dpl_condition_t condition;

  buf = dpl_buf_new();
  if (NULL == buf)
    exit(1);

  buf->size = DATA_LEN;
  buf->ptr = malloc(buf->size);
  if (NULL == buf->ptr)
    {
      fprintf(stderr, "alloc data failed\n");
      exit(1);
    }

  memset(buf->ptr, 'y', buf->size);

  banner("3 - add existing named object with precondition");
  
  condition.mask = DPL_CONDITION_IF_NONE_MATCH;
  condition.etag[0] = '*';
  condition.etag[1] = 0;

  atask = (dpl_async_task_t *) dpl_put_async_prepare(ctx,
                                                     NULL,          //no bucket
                                                     file2_path,    //the id
                                                     NULL,          //no option
                                                     DPL_FTYPE_REG, //regular object
                                                     &condition,    //expect failure if exists
                                                     NULL,          //no range
                                                     NULL,          //the metadata
                                                     NULL,          //no sysmd
                                                     buf);          //object body
  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_add_existing_named_object;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_add_existing_named_object_no_precond(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_EPRECOND != atask->ret)
    {
      fprintf(stderr, "abnormal answer: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  dpl_async_task_free(atask);

  add_existing_named_object();
}

void
add_existing_named_object_no_precond()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;

  buf = dpl_buf_new();
  if (NULL == buf)
    exit(1);

  buf->size = DATA_LEN;
  buf->ptr = malloc(buf->size);
  if (NULL == buf->ptr)
    {
      fprintf(stderr, "alloc data failed\n");
      exit(1);
    }

  memset(buf->ptr, 'y', buf->size);

  banner("2bis - add existing named object");
  
  atask = (dpl_async_task_t *) dpl_put_async_prepare(ctx,
                                                     NULL,          //no bucket
                                                     file2_path,    //the id
                                                     NULL,          //no option
                                                     DPL_FTYPE_REG, //regular object
                                                     NULL,         //no condition
                                                     NULL,          //no range
                                                     NULL,          //the metadata
                                                     NULL,          //no sysmd
                                                     buf);          //object body
  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_add_existing_named_object;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_add_nonexisting_named_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "add named object failed: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  dpl_async_task_free(atask);

  add_existing_named_object();
}

void
add_nonexisting_named_object()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;

  buf = dpl_buf_new();
  if (NULL == buf)
    exit(1);

  buf->size = DATA_LEN;
  buf->ptr = malloc(buf->size);
  if (NULL == buf->ptr)
    {
      fprintf(stderr, "alloc data failed\n");
      exit(1);
    }

  memset(buf->ptr, 'y', buf->size);

  banner("2 - add nonexisting named object");
  
  atask = (dpl_async_task_t *) dpl_put_async_prepare(ctx,
                                                     NULL,          //no bucket
                                                     file2_path,    //the id
                                                     NULL,          //no option
                                                     DPL_FTYPE_REG, //regular object
                                                     NULL,          //no condition
                                                     NULL,          //no range
                                                     NULL,          //the metadata
                                                     NULL,          //no sysmd
                                                     buf);          //object body
  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_add_nonexisting_named_object;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_add_nameless_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "add object failed: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  fprintf(stderr, "id=%s path=%s\n", atask->u.post.sysmd_returned.id, atask->u.post.sysmd_returned.path);

  strcpy(id1, atask->u.post.sysmd_returned.id);

  dpl_async_task_free(atask);

  add_nonexisting_named_object();
}

void
add_nameless_object()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;

  buf = dpl_buf_new();
  if (NULL == buf)
    exit(1);

  buf->size = DATA_LEN;
  buf->ptr = malloc(buf->size);
  if (NULL == buf->ptr)
    {
      fprintf(stderr, "alloc data failed\n");
      exit(1);
    }

  memset(buf->ptr, 'x', buf->size);

  banner("1 - add nameless object");

  atask = (dpl_async_task_t *) dpl_post_id_async_prepare(ctx,           //the context
                                                         NULL,          //no bucket
                                                         NULL,          //no id
                                                         NULL,          //no option
                                                         DPL_FTYPE_REG, //regular object
                                                         NULL,          //condition
                                                         NULL,          //range
                                                         NULL,          //the metadata
                                                         NULL,          //no sysmd
                                                         buf,           //object body
                                                         NULL);         //no query params
  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_add_nameless_object;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_make_folder(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "make dir failed: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  dpl_async_task_free(atask);

  add_nameless_object();
}

void
make_folder()
{
  dpl_status_t ret, ret2;
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;

  banner("creating folder");

  buf = dpl_buf_new();
  if (NULL == buf)
    exit(1);

  atask = (dpl_async_task_t *) dpl_put_async_prepare(ctx,           //the context
                                                     NULL,          //no bucket
                                                     folder,        //the folder
                                                     NULL,          //no option
                                                     DPL_FTYPE_DIR, //directory
                                                     NULL,          //no condition
                                                     NULL,          //no range
                                                     NULL,          //no metadata
                                                     NULL,          //no sysmd
                                                     buf);         //object body
  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_make_folder;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

int
main(int argc,
     char **argv)
{
  int ret;
 
  if (2 != argc)
    {
      fprintf(stderr, "usage: restrest folder\n");
      exit(1);
    }

  folder = argv[1];
  if (folder[0] == '/')
    {
      fprintf(stderr, "folder name must not start with slash");
      exit(1);
    }

  folder_len = strlen(folder);
  if (folder_len < 1)
    {
      fprintf(stderr, "bad folder\n");
      exit(1);
    }
  if (folder[folder_len-1] != '/')
    {
      fprintf(stderr, "folder name must end with a slash\n");
      exit(1);
    }

  snprintf(file1_path, sizeof (file1_path), "%s%s", folder, FILE1);
  snprintf(file2_path, sizeof (file2_path), "%s%s", folder, FILE2);
  snprintf(file3_path, sizeof (file3_path), "%s%s", folder, FILE3);
  snprintf(file4_path, sizeof (file4_path), "%s%s", folder, FILE4);

  ret = dpl_init();           //init droplet library
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_init failed\n");
      exit(1);
    }

  //open default profile
  ctx = dpl_ctx_new(NULL,     //droplet directory, default: "~/.droplet"
                    NULL);    //droplet profile, default:   "default"
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl_ctx_new failed\n");
      ret = 1;
    }

  //ctx->trace_level = ~0;
  //ctx->trace_buffers = 1;

  pool = dpl_task_pool_create(ctx, "resttest", 10);
  if (NULL == pool)
    {
      fprintf(stderr, "error creating pool\n");
      exit(1);
    }

  /**/

  pthread_mutex_init(&prog_lock, NULL);
  pthread_cond_init(&prog_cond, NULL);

  make_folder();

  pthread_cond_wait(&prog_cond, &prog_lock);

  return 0;
}
