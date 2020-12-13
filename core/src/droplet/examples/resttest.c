/*
 * simple example which operates into a folder (creation of a folder, atomic file creation + get/set data/metadata + listing of a folder + getattr + deletion)
 */

#include <droplet.h>
#include <assert.h>
#include <sys/param.h>

int
main(int argc,
     char **argv)
{
  int ret;
  dpl_ctx_t *ctx;
  char *folder = NULL;
  int folder_len;
  dpl_dict_t *metadata = NULL;
  char *data_buf = NULL;
  size_t data_len;
  char *data_buf_returned = NULL;
  u_int data_len_returned;
  dpl_dict_t *metadata_returned = NULL;
  dpl_dict_t *metadata2_returned = NULL;
  dpl_dict_var_t *metadatum = NULL;
  dpl_sysmd_t sysmd;
  char new_path[MAXPATHLEN];
  dpl_vec_t *files = NULL;
  dpl_vec_t *sub_directories = NULL;
  int i;

  if (2 != argc)
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

  //ctx->trace_level = ~0;
  //ctx->trace_buffers = 1;

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
      goto free_all;
    }

  /**/

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
  
  fprintf(stderr, "atomic creation of an object+MD\n");

  ret = dpl_post(ctx,           //the context
                 NULL,          //no bucket
                 folder,        //the folder
                 NULL,          //no option
                 DPL_FTYPE_REG, //regular object
                 NULL,          //condition
                 NULL,          //range
                 metadata,      //the metadata
                 NULL,          //no sysmd
                 data_buf,      //object body
                 data_len,      //object length
                 NULL,          //no query params
                 &sysmd);       //the returned sysmd
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_post failed: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  if (!(sysmd.mask & DPL_SYSMD_MASK_PATH))
    {
      fprintf(stderr, "path is absent from sysmd\n");
      ret = 1;
      goto free_all;
    }

  fprintf(stderr, "resource path %s\n", sysmd.path);

  snprintf(new_path, sizeof (new_path), "%su.1", folder);

  ret = dpl_copy(ctx,
                 NULL,          //no src bucket
                 sysmd.path,    //the src resource
                 NULL,          //no dst bucket
                 new_path,      //dst resource
                 NULL,          //no option
                 DPL_FTYPE_REG, //regular file
                 DPL_COPY_DIRECTIVE_MOVE, //rename
                 NULL,          //no metadata
                 NULL,          //no sysmd
                 NULL);         //no server side condition
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_move %s to %s failed: %s (%d)\n", sysmd.path, new_path, dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  /**/

  fprintf(stderr, "getting object+MD\n");

  ret = dpl_get(ctx,           //the context
                NULL,          //no bucket
                new_path,      //the key
                NULL,          //no opion
                DPL_FTYPE_REG, //object type
                NULL,          //no condition
                NULL,          //no range
                &data_buf_returned,  //data object
                &data_len_returned,  //data object length
                &metadata_returned, //metadata
                NULL);              //sysmd
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

  ret = dpl_copy(ctx,           //the context
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
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error updating metadata: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  /**/

  fprintf(stderr, "getting MD only\n");

  ret = dpl_head(ctx,      //the context
                 NULL,     //no bucket,
                 new_path, //the key
                 NULL,     //no option
                 DPL_FTYPE_UNDEF, //no matter the file type
                 NULL,     //no condition,
                 &metadata2_returned,
                 NULL);
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

  fprintf(stderr, "listing of folder\n");

  ret = dpl_list_bucket(ctx, 
                        NULL,
                        folder,
                        "/",
                        -1,
                        &files,
                        &sub_directories);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error listing folder: %s (%d)\n", dpl_status_str(ret), ret);
      ret = 1;
      goto free_all;
    }

  for (i = 0;i < files->n_items;i++)
    {
      dpl_object_t *obj = (dpl_object_t *) dpl_vec_get(files, i);
      dpl_sysmd_t obj_sysmd;
      dpl_dict_t *obj_md = NULL;
      
      ret = dpl_head(ctx, 
                     NULL, //no bucket
                     obj->path, 
                     NULL, //option
                     DPL_FTYPE_UNDEF, //no matter the file type
                     NULL, //condition
                     &obj_md, //user metadata
                     &obj_sysmd); //system metadata
      if (DPL_SUCCESS != ret)
        {
          fprintf(stderr, "getattr error on %s: %s (%d)\n", obj->path, dpl_status_str(ret), ret);
          ret = 1;
          goto free_all;
        }

      fprintf(stderr, "file %s: size=%ld mtime=%lu\n", obj->path, obj_sysmd.size, obj_sysmd.mtime);
      //dpl_dict_print(obj_md, stderr, 5);
      dpl_dict_free(obj_md);
    }

  for (i = 0;i < sub_directories->n_items;i++)
    {
      dpl_common_prefix_t *dir = (dpl_common_prefix_t *) dpl_vec_get(sub_directories, i);

      fprintf(stderr, "dir %s\n", dir->prefix);
    }

  /**/

  fprintf(stderr, "delete object+MD\n");

  ret = dpl_delete(ctx,       //the context
                   NULL,      //no bucket
                   new_path,  //the key
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

  if (NULL != sub_directories)
    dpl_vec_common_prefixes_free(sub_directories);

  if (NULL != files)
    dpl_vec_objects_free(files);

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
