/*
 * simple example which operates into a folder (creation of a folder, atomic file creation + get/set data/metadata + listing of a folder + getattr + deletion)
 */

#include <droplet.h>
#include <droplet/async.h>
#include <assert.h>
#include <sys/param.h>

#define DATA_LEN 10000

dpl_ctx_t *ctx = NULL;
dpl_task_pool_t *pool = NULL;
char *folder = NULL;
int folder_len;
dpl_dict_t *metadata = NULL;
char new_path[MAXPATHLEN];

pthread_mutex_t prog_lock;
pthread_cond_t prog_cond;

pthread_mutex_t list_lock;
pthread_cond_t list_cond;
int n_ok = 0;

void
free_all()
{
  fprintf(stderr, "finished\n");

  if (NULL != metadata)
    dpl_dict_free(metadata);

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
cb_delete_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  dpl_async_task_free(atask);

  free_all();
}

void
delete_object()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;

  fprintf(stderr, "delete object+MD\n");

  atask = (dpl_async_task_t *) dpl_delete_async_prepare(ctx,       //the context
                                                        NULL,      //no bucket
                                                        new_path,  //the key
                                                        NULL,      //no option
                                                        DPL_FTYPE_UNDEF, //no matter the file type
                                                        NULL);     //no condition

  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_delete_object;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_head_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "getattr error on %s: %s (%d)\n", atask->u.head.resource, dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }
  
  fprintf(stderr, "file %s: size=%ld mtime=%lu\n", atask->u.head.resource, atask->u.head.sysmd.size, atask->u.head.sysmd.mtime);
  //dpl_dict_print(atask->u.head.metadata, stderr, 5);

  dpl_async_task_free(atask);

  pthread_mutex_lock(&list_lock);
  n_ok++;
  pthread_cond_signal(&list_cond);
  pthread_mutex_unlock(&list_lock);
}

void
cb_list_bucket(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;
  int i;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "error listing folder: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  pthread_mutex_init(&list_lock, NULL);
  pthread_cond_init(&list_cond, NULL);

  for (i = 0;i < atask->u.list_bucket.objects->n_items;i++)
    {
      dpl_object_t *obj = (dpl_object_t *) dpl_vec_get(atask->u.list_bucket.objects, i);
      dpl_sysmd_t obj_sysmd;
      dpl_dict_t *obj_md = NULL;
      dpl_async_task_t *sub_atask = NULL;

      fprintf(stderr, "getting md\n");
      
      sub_atask = (dpl_async_task_t *) dpl_head_async_prepare(ctx, 
                                                              NULL, //no bucket
                                                              obj->path, 
                                                              NULL, //option
                                                              DPL_FTYPE_UNDEF, //no matter the file type
                                                              NULL); //condition

      if (NULL == sub_atask)
        {
          fprintf(stderr, "error preparing task\n");
          exit(1);
        }
      
      sub_atask->cb_func = cb_head_object;
      sub_atask->cb_arg = sub_atask;
      
      dpl_task_pool_put(pool, (dpl_task_t *) sub_atask);
    }

  for (i = 0;i < atask->u.list_bucket.common_prefixes->n_items;i++)
    {
      dpl_common_prefix_t *dir = (dpl_common_prefix_t *) dpl_vec_get(atask->u.list_bucket.common_prefixes, i);

      fprintf(stderr, "dir %s\n", dir->prefix);
    }

 again:
  
  pthread_cond_wait(&list_cond, &list_lock);
  
  //printf("n_ok=%d\n", n_ok);

  if (n_ok != atask->u.list_bucket.objects->n_items)
    goto again;

  dpl_async_task_free(atask);

  delete_object();
}

void
list_bucket()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  int i;

  fprintf(stderr, "listing of folder\n");

  atask = (dpl_async_task_t *) dpl_list_bucket_async_prepare(ctx, 
                                                             NULL,
                                                             folder,
                                                             "/");

  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_list_bucket;
  atask->cb_arg = atask;

  dpl_task_pool_put(pool, (dpl_task_t *) atask);

}

void
cb_get_metadata(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;
  dpl_dict_var_t *metadatum = NULL;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "error getting metadata: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  fprintf(stderr, "checking metadata\n");

  metadatum = dpl_dict_get(atask->u.head.metadata, "foo");
  if (NULL == metadatum)
    {
      fprintf(stderr, "missing metadatum\n");
      exit(1);
    }

  assert(metadatum->val->type == DPL_VALUE_STRING);
  if (strcmp(dpl_sbuf_get_str(metadatum->val->string), "bar2"))
    {
      fprintf(stderr, "bad value in metadatum\n");
      exit(1);
    }

  metadatum = dpl_dict_get(atask->u.head.metadata, "foo2");
  if (NULL == metadatum)
    {
      fprintf(stderr, "missing metadatum\n");
      exit(1);
    }

  assert(metadatum->val->type == DPL_VALUE_STRING);
  if (strcmp(dpl_sbuf_get_str(metadatum->val->string), "qux"))
    {
      fprintf(stderr, "bad value in metadatum\n");
      exit(1);
    }

  dpl_async_task_free(atask);

  list_bucket();
}

void
get_metadata()
{
  dpl_async_task_t *atask = NULL;

  fprintf(stderr, "getting MD only\n");

  atask = (dpl_async_task_t *) dpl_head_async_prepare(ctx,      //the context
                                                      NULL,     //no bucket,
                                                      new_path, //the key
                                                      NULL,     //no option
                                                      DPL_FTYPE_UNDEF, //no matter the file type
                                                      NULL);    //no condition,

  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_get_metadata;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
 
}

void
cb_update_metadata(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  dpl_async_task_free(atask);

  get_metadata();
}

void
update_metadata()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;

  fprintf(stderr, "setting MD only\n");

  ret = dpl_dict_add(metadata, "foo", "bar2", 0);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error updating metadatum: %s (%d)\n", dpl_status_str(ret), ret);
      exit(1);
    }

  atask = (dpl_async_task_t *) dpl_copy_async_prepare(ctx,           //the context
                                                      NULL,          //no src bucket
                                                      new_path,      //the key
                                                      NULL,          //no dst bucket
                                                      new_path,      //the same key
                                                      NULL,          //no option
                                                      DPL_FTYPE_REG, //object type
                                                      DPL_COPY_DIRECTIVE_METADATA_REPLACE,  //tell server to replace metadata
                                                      metadata,      //the updated metadata
                                                      NULL,          //no sysmd
                                                      NULL);         //no condition
  
  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_update_metadata;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_get_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;
  int i;
  dpl_dict_var_t *metadatum = NULL;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "dpl_get failed: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  fprintf(stderr, "checking object\n");

  if (DATA_LEN != atask->u.get.buf->size)
    {
      fprintf(stderr, "data lengths mismatch\n");
      exit(1);
    }

  for (i = 0;i < DATA_LEN;i++)
    if (atask->u.get.buf->ptr[i] != 'z')
      {
        fprintf(stderr, "data content mismatch\n");
        exit(1);
      }

  fprintf(stderr, "checking metadata\n");

  metadatum = dpl_dict_get(atask->u.get.metadata, "foo");
  if (NULL == metadatum)
    {
      fprintf(stderr, "missing metadatum\n");
      exit(1);
    }

  assert(metadatum->val->type == DPL_VALUE_STRING);
  if (strcmp(dpl_sbuf_get_str(metadatum->val->string), "bar"))
    {
      fprintf(stderr, "bad value in metadatum\n");
      exit(1);
    }
  
  metadatum = dpl_dict_get(atask->u.get.metadata, "foo2");
  if (NULL == metadatum)
    {
      fprintf(stderr, "missing metadatum\n");
      exit(1);
    }

  assert(metadatum->val->type == DPL_VALUE_STRING);
  if (strcmp(dpl_sbuf_get_str(metadatum->val->string), "qux"))
    {
      fprintf(stderr, "bad value in metadatum\n");
      exit(1);
    }

  dpl_async_task_free(atask);

  update_metadata();
}

void
get_object()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;

  fprintf(stderr, "getting object+MD\n");

  atask = (dpl_async_task_t *) dpl_get_async_prepare(ctx,           //the context
                                                     NULL,          //no bucket
                                                     new_path,      //the key
                                                     NULL,          //no opion
                                                     DPL_FTYPE_REG, //object type
                                                     NULL,          //no condition
                                                     NULL);         //no range

  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_get_object;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_rename_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "rename object failed: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  dpl_async_task_free(atask);

  get_object();
}

void
rename_object(char *resource_path)
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;

  snprintf(new_path, sizeof (new_path), "%su.1", folder);

  atask = (dpl_async_task_t *) dpl_copy_async_prepare(ctx,
                                                      NULL,          //no src bucket
                                                      resource_path, //the src resource
                                                      NULL,          //no dst bucket
                                                      new_path,      //dst resource
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

  atask->cb_func = cb_rename_object;
  atask->cb_arg = atask;
  
  dpl_task_pool_put(pool, (dpl_task_t *) atask);
}

void
cb_create_object(void *handle)
{
  dpl_async_task_t *atask = (dpl_async_task_t *) handle;

  if (DPL_SUCCESS != atask->ret)
    {
      fprintf(stderr, "create object failed: %s (%d)\n", dpl_status_str(atask->ret), atask->ret);
      exit(1);
    }

  if (!(atask->u.post.sysmd_returned.mask & DPL_SYSMD_MASK_PATH))
    {
      fprintf(stderr, "path is missing from sysmd\n");
      exit(1);
    }

  fprintf(stderr, "resource path %s\n", atask->u.post.sysmd_returned.path);

  rename_object(atask->u.post.sysmd_returned.path);
  
  dpl_async_task_free(atask);
}

void
create_object()
{
  dpl_async_task_t *atask = NULL;
  dpl_buf_t *buf = NULL;
  dpl_status_t ret;

  buf = dpl_buf_new();
  if (NULL == buf)
    exit(1);

  buf->size = DATA_LEN;
  buf->ptr = malloc(DATA_LEN);
  if (NULL == buf->ptr)
    {
      fprintf(stderr, "alloc data failed\n");
      exit(1);
    }

  memset(buf->ptr, 'z', buf->size);

  metadata = dpl_dict_new(13);
  if (NULL == metadata)
    {
      fprintf(stderr, "dpl_dict_new failed\n");
      exit(1);
    }
 
  ret = dpl_dict_add(metadata, "foo", "bar", 0);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_dict_add failed\n");
      exit(1);
    }

  ret = dpl_dict_add(metadata, "foo2", "qux", 0);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_dict_add failed\n");
      exit(1);
    }

  fprintf(stderr, "atomic creation of an object+MD\n");

  atask = (dpl_async_task_t *) dpl_post_async_prepare(ctx,           //the context
                                                      NULL,          //no bucket
                                                      folder,        //the folder
                                                      NULL,          //no option
                                                      DPL_FTYPE_REG, //regular object
                                                      NULL,          //condition
                                                      NULL,          //range
                                                      metadata,      //the metadata
                                                      NULL,          //no sysmd
                                                      buf,           //object body
                                                      NULL);         //no query params
  if (NULL == atask)
    {
      fprintf(stderr, "error preparing task\n");
      exit(1);
    }

  atask->cb_func = cb_create_object;
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

  create_object();
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
