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

dpl_ctx_t *ctx = NULL;
dpl_task_pool_t *pool = NULL;
char *folder = NULL;
int folder_len;
char file1_path[MAXPATHLEN];
char file2_path[MAXPATHLEN];
char file3_path[MAXPATHLEN];

pthread_mutex_t prog_lock;
pthread_cond_t prog_cond;

pthread_mutex_t list_lock;
pthread_cond_t list_cond;
int n_ok = 0;

void
free_all()
{
  fprintf(stderr, "finished\n");

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
cb_update_metadata_named_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "abnormal answer: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  dpl_async_task_free(atask);
  
  //
}

void
update_metadata_named_object()
{
  dpl_async_task_t *atask = NULL;
  dpl_status_t ret;
  dpl_option_t option;
  dpl_dict_t *metadata = NULL;

  fprintf(stderr, "6 - append metadata to existing named object\n");

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
  dpl_option_t option;

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

  fprintf(stderr, "5 - append data to nonexisting named object\n");
  
  option.mask = DPL_OPTION_APPEND_DATA;

  atask = (dpl_async_task_t *) dpl_put_async_prepare(ctx,
                                                     NULL,          //no bucket
                                                     file3_path,    //the id
                                                     &option,       //option
                                                     DPL_FTYPE_REG, //regular object
                                                     NULL,          //no condition
                                                     NULL,          //range
                                                     NULL,          //the metadata
                                                     NULL,          //no sysmd
                                                     buf);          //object body
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
  dpl_option_t option;

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

  fprintf(stderr, "4 - append data to nonexisting named object with precondition\n");
  
  condition.mask = DPL_CONDITION_IF_MATCH;
  condition.etag[0] = '*';
  condition.etag[1] = 0;

  option.mask = DPL_OPTION_APPEND_DATA;

  atask = (dpl_async_task_t *) dpl_put_async_prepare(ctx,
                                                     NULL,          //no bucket
                                                     file3_path,    //the id
                                                     &option,       //no option
                                                     DPL_FTYPE_REG, //regular object
                                                     &condition,    //expect failure if exists
                                                     NULL,          //range
                                                     NULL,          //the metadata
                                                     NULL,          //no sysmd
                                                     buf);          //object body
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

  fprintf(stderr, "3 - add existing named object with precondition\n");
  
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

  fprintf(stderr, "2bis - add existing named object\n");
  
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

  fprintf(stderr, "2 - add nonexisting named object\n");
  
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
cb_name_nameless_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "rename object failed: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  dpl_async_task_free(atask);

  add_nonexisting_named_object();
}

void
name_nameless_object(char *id)
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;

  fprintf(stderr, "1bis - bind nameless object with a name\n");

  atask = (dpl_async_task_t *) dpl_copy_id_async_prepare(ctx,
                                                         NULL,          //no src bucket
                                                         id,            //the src resource
                                                         NULL,          //no dst bucket
                                                         file1_path,    //dst resource
                                                         NULL,          //no option
                                                         DPL_FTYPE_REG, //regular file
                                                         DPL_COPY_DIRECTIVE_MOVE, //rename
                                                         NULL,          //no metadata
                                                         NULL,          //no sysmd
                                                         NULL);         //no server side condition
  
  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_name_nameless_object;
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

  fprintf(stderr, "id=%s\n", atask->u.post.sysmd_returned.id);

  name_nameless_object(atask->u.post.sysmd_returned.id);

  dpl_async_task_free(atask);
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

  fprintf(stderr, "1 - add nameless object\n");

  atask = (dpl_async_task_t *) dpl_post_id_async_prepare(ctx,           //the context
                                                         NULL,          //no bucket
                                                         NULL,          //no option
                                                         DPL_FTYPE_REG, //regular object
                                                         NULL,         //the metadata
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

  fprintf(stderr, "creating folder\n");

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
