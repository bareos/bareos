/*
 * simple example which recurses a directory tree in using VFS abstraction
 */

#include <droplet.h>
#include <droplet/vfs.h>

dpl_status_t recurse(dpl_ctx_t* ctx, char* dir, int level)
{
  void* dir_hdl;
  dpl_dirent_t dirent;
  int ret;

  ret = dpl_chdir(ctx, dir);
  if (DPL_SUCCESS != ret) return ret;

  ret = dpl_opendir(ctx, ".", &dir_hdl);
  if (DPL_SUCCESS != ret) return ret;

  while (!dpl_eof(dir_hdl)) {
    ret = dpl_readdir(dir_hdl, &dirent);
    if (DPL_SUCCESS != ret) return ret;

    if (strcmp(dirent.name, ".")) {
      int i;

      for (i = 0; i < level; i++) printf(" ");
      printf("%s\n", dirent.name);

      if (DPL_FTYPE_DIR == dirent.type) {
        ret = recurse(ctx, dirent.name, level + 1);
        if (DPL_SUCCESS != ret) return ret;
      }
    }
  }

  dpl_closedir(dir_hdl);

  if (level > 0) {
    ret = dpl_chdir(ctx, "..");
    if (DPL_SUCCESS != ret) return ret;
  }

  return DPL_SUCCESS;
}

int main(int argc, char** argv)
{
  int ret;
  dpl_ctx_t* ctx;
  char* bucket = NULL;

  if (2 != argc) {
    fprintf(stderr, "usage: recurse bucket\n");
    ret = 1;
    goto end;
  }

  bucket = argv[1];

  ret = dpl_init();
  if (DPL_SUCCESS != ret) {
    fprintf(stderr, "dpl_init failed\n");
    ret = 1;
    goto end;
  }

  ctx = dpl_ctx_new(NULL, NULL);
  if (NULL == ctx) {
    fprintf(stderr, "dpl_ctx_new failed\n");
    ret = 1;
    goto free_dpl;
  }

  ctx->cur_bucket = strdup(bucket);
  if (!ctx->cur_bucket) {
    perror("strdup");
    ret = 1;
    goto free_all;
  }

  ret = recurse(ctx, "/", 0);
  if (DPL_SUCCESS != ret) {
    fprintf(stderr, "error recursing\n");
    ret = 1;
    goto free_all;
  }

  ret = 0;

free_all:
  dpl_ctx_free(ctx);

free_dpl:
  dpl_free();

end:
  return ret;
}
