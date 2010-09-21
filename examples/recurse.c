/*
 * simple example which recurses a directory tree
 */

#include <droplet.h>

dpl_status_t
recurse(dpl_ctx_t *ctx,
        char *dir,
        int level)
{
  void *dir_hdl;
  dpl_dirent_t dirent;
  int ret;

  ret = dpl_chdir(ctx, dir);
  if (DPL_SUCCESS != ret)
    return ret;

  ret = dpl_opendir(ctx, ".", &dir_hdl);
  if (DPL_SUCCESS != ret)
    return ret;

  while (!dpl_vdir_eof(dir_hdl))
    {
      ret = dpl_vdir_readdir(dir_hdl, &dirent);
      if (DPL_SUCCESS != ret)
        return ret;

      if (strcmp(dirent.name, "."))
        {
          int i;

          for (i = 0;i < level;i++)
            printf(" ");
          printf("%s\n", dirent.name);
          
          if (DPL_FTYPE_DIR == dirent.type)
            {
              ret = recurse(ctx, dirent.name, level + 1);
              if (DPL_SUCCESS != ret)
                return ret;
            }
        }
    }
  
  dpl_vdir_closedir(dir_hdl);

  if (level > 0)
    {
      ret = dpl_chdir(ctx, "..");
      if (DPL_SUCCESS != ret)
        return ret;
    }

  return DPL_SUCCESS;
}

int
main(int argc, 
     char **argv)
{
  int ret;
  dpl_ctx_t *ctx;
  char *bucket = NULL;

  if (2 != argc)
    {
      fprintf(stderr, "usage: recurse bucket\n");
      exit(1);
    }

  bucket = argv[1];

  ret = dpl_init();
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dpl_init failed\n");
      exit(1);
    }

  ctx = dpl_ctx_new(NULL, NULL);
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl_ctx_new failed\n");
      exit(1);
    }
  
  ctx->cur_bucket = bucket;

  ret = recurse(ctx, "/", 0);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "error recursing\n");
      exit(1);
    }

  dpl_ctx_free(ctx);
  dpl_free();
  
  return 0;
}
