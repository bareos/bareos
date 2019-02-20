/*
 * Copyright (c) 2001,2002
 *      Traakan, Inc., Los Altos, CA
 *      All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Project:  NDMJOB
 * Ident:    $Id: $
 *
 * Description:
 *
 */


#include "ndmlib.h"

void ndmfhdb_register_callbacks(struct ndmlog* ixlog,
                                struct ndm_fhdb_callbacks* callbacks)
{
  /*
   * Only allow one register.
   */
  if (!ixlog->nfc) {
    ixlog->nfc = NDMOS_API_MALLOC(sizeof(struct ndm_fhdb_callbacks));
    if (ixlog->nfc) {
      memcpy(ixlog->nfc, callbacks, sizeof(struct ndm_fhdb_callbacks));
    }
  }
}

void ndmfhdb_unregister_callbacks(struct ndmlog* ixlog)
{
  if (ixlog->nfc) {
    NDMOS_API_FREE(ixlog->nfc);
    ixlog->nfc = NULL;
  }
}

int ndmfhdb_add_file(struct ndmlog* ixlog,
                     int tagc,
                     char* raw_name,
                     ndmp9_file_stat* fstat)
{
  if (ixlog->nfc && ixlog->nfc->add_file) {
    return ixlog->nfc->add_file(ixlog, tagc, raw_name, fstat);
  } else {
    return 0;
  }
}

int ndmfhdb_add_dir(struct ndmlog* ixlog,
                    int tagc,
                    char* raw_name,
                    ndmp9_u_quad dir_node,
                    ndmp9_u_quad node)
{
  if (ixlog->nfc && ixlog->nfc->add_dir) {
    return ixlog->nfc->add_dir(ixlog, tagc, raw_name, dir_node, node);
  } else {
    return 0;
  }
}

int ndmfhdb_add_node(struct ndmlog* ixlog,
                     int tagc,
                     ndmp9_u_quad node,
                     ndmp9_file_stat* fstat)
{
  if (ixlog->nfc && ixlog->nfc->add_node) {
    return ixlog->nfc->add_node(ixlog, tagc, node, fstat);
  } else {
    return 0;
  }
}

int ndmfhdb_add_dirnode_root(struct ndmlog* ixlog,
                             int tagc,
                             ndmp9_u_quad root_node)
{
  if (ixlog->nfc && ixlog->nfc->add_dirnode_root) {
    return ixlog->nfc->add_dirnode_root(ixlog, tagc, root_node);
  } else {
    return 0;
  }
}

int ndmfhdb_add_fh_info_to_nlist(FILE* fp, ndmp9_name* nlist, int n_nlist)
{
  struct ndmfhdb _fhcb, *fhcb = &_fhcb;
  int i, rc, n_found;
  ndmp9_file_stat fstat;

  rc = ndmfhdb_open(fp, fhcb);
  if (rc != 0) { return -31; }

  n_found = 0;

  for (i = 0; i < n_nlist; i++) {
    char* name = nlist[i].original_path;

    rc = ndmfhdb_lookup(fhcb, name, &fstat);
    if (rc > 0) {
      nlist[i].fh_info = fstat.fh_info;
      if (fstat.fh_info.valid) { n_found++; }
    }
  }

  return n_found;
}

int ndmfhdb_open(FILE* fp, struct ndmfhdb* fhcb)
{
  int rc;

  NDMOS_MACRO_ZEROFILL(fhcb);

  fhcb->fp = fp;

  rc = ndmfhdb_dirnode_root(fhcb);
  if (rc > 0) {
    fhcb->use_dir_node = 1;
    return 0;
  }

  rc = ndmfhdb_file_root(fhcb);
  if (rc > 0) {
    fhcb->use_dir_node = 0;
    return 0;
  }

  return -1;
}

int ndmfhdb_lookup(struct ndmfhdb* fhcb, char* path, ndmp9_file_stat* fstat)
{
  if (fhcb->use_dir_node) {
    return ndmfhdb_dirnode_lookup(fhcb, path, fstat);
  } else {
    return ndmfhdb_file_lookup(fhcb, path, fstat);
  }
}

int ndmfhdb_dirnode_root(struct ndmfhdb* fhcb)
{
  int rc, off;
  char* p;
  char key[256];
  char linebuf[2048];

  snprintf(key, sizeof(key), "DHr ");

  p = NDMOS_API_STREND(key);
  off = p - key;

  rc = ndmbstf_first(fhcb->fp, key, linebuf, sizeof linebuf);

  if (rc <= 0) { return rc; /* error or not found */ }

  fhcb->root_node = NDMOS_API_STRTOLL(linebuf + off, &p, 0);

  if (*p != 0) { return -10; }

  return 1;
}

int ndmfhdb_dirnode_lookup(struct ndmfhdb* fhcb,
                           char* path,
                           ndmp9_file_stat* fstat)
{
  int rc;
  char* p;
  char* q;
  char component[256 + 128];
  uint64_t dir_node;
  uint64_t node;

  /* classic path name reduction */
  node = dir_node = fhcb->root_node;
  p = path;
  for (;;) {
    if (*p == '/') {
      p++;
      continue;
    }
    if (*p == 0) { break; }
    q = component;
    while (*p != 0 && *p != '/') { *q++ = *p++; }
    *q = 0;

    dir_node = node;
    rc = ndmfhdb_dir_lookup(fhcb, dir_node, component, &node);
    if (rc <= 0) return rc; /* error or not found */
  }

  rc = ndmfhdb_node_lookup(fhcb, node, fstat);

  return rc;
}

int ndmfhdb_dir_lookup(struct ndmfhdb* fhcb,
                       uint64_t dir_node,
                       char* name,
                       uint64_t* node_p)
{
  int rc, off;
  char* p;
  char key[256 + 128];
  char linebuf[2048];

  snprintf(key, sizeof(key), "DHd %llu ", dir_node);
  p = NDMOS_API_STREND(key);

  ndmcstr_from_str(name, p, sizeof key - (p - key) - 10);

  strcat(p, " UNIX ");

  p = NDMOS_API_STREND(key);
  off = p - key;

  rc = ndmbstf_first(fhcb->fp, key, linebuf, sizeof linebuf);

  if (rc <= 0) { return rc; /* error or not found */ }

  *node_p = NDMOS_API_STRTOLL(linebuf + off, &p, 0);

  if (*p != 0) { return -10; }

  return 1;
}

int ndmfhdb_node_lookup(struct ndmfhdb* fhcb,
                        uint64_t node,
                        ndmp9_file_stat* fstat)
{
  int rc, off;
  char* p;
  char key[128];
  char linebuf[2048];

  snprintf(key, sizeof(key), "DHn %llu UNIX ", node);

  p = NDMOS_API_STREND(key);
  off = p - key;


  rc = ndmbstf_first(fhcb->fp, key, linebuf, sizeof linebuf);

  if (rc <= 0) { return rc; /* error or not found */ }


  rc = ndm_fstat_from_str(fstat, linebuf + off);
  if (rc < 0) { return rc; }

  return 1;
}

int ndmfhdb_file_root(struct ndmfhdb* fhcb)
{
  int rc;
  ndmp9_file_stat fstat;

  rc = ndmfhdb_file_lookup(fhcb, "/", &fstat);
  if (rc > 0) {
    if (fstat.node.valid) fhcb->root_node = fstat.node.value;
  }

  return rc;
}

int ndmfhdb_file_lookup(struct ndmfhdb* fhcb,
                        char* path,
                        ndmp9_file_stat* fstat)
{
  int rc, off;
  char* p;
  char key[2048];
  char linebuf[2048];

  snprintf(key, sizeof(key), "DHf ");
  p = NDMOS_API_STREND(key);

  ndmcstr_from_str(path, p, sizeof key - (p - key) - 10);

  strcat(p, " UNIX ");

  p = NDMOS_API_STREND(key);
  off = p - key;

  rc = ndmbstf_first(fhcb->fp, key, linebuf, sizeof linebuf);

  if (rc <= 0) { return rc; /* error or not found */ }

  rc = ndm_fstat_from_str(fstat, linebuf + off);
  if (rc < 0) { return rc; }

  return 1;
}

/*
 * Same codes as wraplib.[ch] wrap_parse_fstat_subr()
 * and wrap_send_fstat_subr().
 */
char* ndm_fstat_to_str(ndmp9_file_stat* fstat, char* buf)
{
  char* p = buf;

  *p++ = 'f';
  switch (fstat->ftype) {
    case NDMP9_FILE_DIR:
      *p++ = 'd';
      break;
    case NDMP9_FILE_FIFO:
      *p++ = 'p';
      break;
    case NDMP9_FILE_CSPEC:
      *p++ = 'c';
      break;
    case NDMP9_FILE_BSPEC:
      *p++ = 'b';
      break;
    case NDMP9_FILE_REG:
      *p++ = '-';
      break;
    case NDMP9_FILE_SLINK:
      *p++ = 'l';
      break;
    case NDMP9_FILE_SOCK:
      *p++ = 's';
      break;
    case NDMP9_FILE_REGISTRY:
      *p++ = 'R';
      break;
    case NDMP9_FILE_OTHER:
      *p++ = 'o';
      break;
    default:
      *p++ = '?';
      break;
  }

  if (fstat->mode.valid) { sprintf(p, " m%04lo", fstat->mode.value & 07777); }
  while (*p) p++;

  if (fstat->uid.valid) { sprintf(p, " u%ld", fstat->uid.value); }
  while (*p) p++;

  if (fstat->gid.valid) { sprintf(p, " g%ld", fstat->gid.value); }
  while (*p) p++;

  if (fstat->ftype == NDMP9_FILE_REG || fstat->ftype == NDMP9_FILE_SLINK) {
    if (fstat->size.valid) { sprintf(p, " s%llu", fstat->size.value); }
  } else {
    /* ignore size on other file types */
  }
  while (*p) p++;

  /* tar -t can not recover atime/ctime */
  /* they are also not particularly interesting in the index */

  if (fstat->mtime.valid) { sprintf(p, " tm%lu", fstat->mtime.value); }
  while (*p) p++;

  if (fstat->fh_info.valid) { sprintf(p, " @%lld", fstat->fh_info.value); }
  while (*p) p++;

  return buf;
}

int ndm_fstat_from_str(ndmp9_file_stat* fstat, char* buf)
{
  char* scan = buf;
  ndmp9_validity* valid_p;

  NDMOS_MACRO_ZEROFILL(fstat);

  while (*scan) {
    char* p = scan + 1;

    switch (*scan) {
      case ' ':
        scan++;
        continue;

      case '@': /* fh_info */
        fstat->fh_info.value = NDMOS_API_STRTOLL(p, &scan, 0);
        valid_p = &fstat->fh_info.valid;
        break;

      case 's': /* size */
        fstat->size.value = NDMOS_API_STRTOLL(p, &scan, 0);
        valid_p = &fstat->size.valid;
        break;

      case 'i': /* fileno (inum) */
        fstat->node.value = NDMOS_API_STRTOLL(p, &scan, 0);
        valid_p = &fstat->node.valid;
        break;

      case 'm': /* mode low twelve bits */
        fstat->mode.value = strtol(p, &scan, 8);
        valid_p = &fstat->mode.valid;
        break;

      case 'l': /* link count */
        fstat->links.value = strtol(p, &scan, 0);
        valid_p = &fstat->links.valid;
        break;

      case 'u': /* uid */
        fstat->uid.value = strtol(p, &scan, 0);
        valid_p = &fstat->uid.valid;
        break;

      case 'g': /* gid */
        fstat->gid.value = strtol(p, &scan, 0);
        valid_p = &fstat->gid.valid;
        break;

      case 't': /* one of the times */
        p = scan + 2;
        switch (scan[1]) {
          case 'm': /* mtime */
            fstat->mtime.value = strtol(p, &scan, 0);
            valid_p = &fstat->mtime.valid;
            break;

          case 'a': /* atime */
            fstat->atime.value = strtol(p, &scan, 0);
            valid_p = &fstat->atime.valid;
            break;

          case 'c': /* ctime */
            fstat->ctime.value = strtol(p, &scan, 0);
            valid_p = &fstat->ctime.valid;
            break;

          default:
            return -13;
        }
        break;

      case 'f': /* ftype (file type) */
        switch (scan[1]) {
          case 'd':
            fstat->ftype = NDMP9_FILE_DIR;
            break;
          case 'p':
            fstat->ftype = NDMP9_FILE_FIFO;
            break;
          case 'c':
            fstat->ftype = NDMP9_FILE_CSPEC;
            break;
          case 'b':
            fstat->ftype = NDMP9_FILE_BSPEC;
            break;
          case '-':
            fstat->ftype = NDMP9_FILE_REG;
            break;
          case 'l':
            fstat->ftype = NDMP9_FILE_SLINK;
            break;
          case 's':
            fstat->ftype = NDMP9_FILE_SOCK;
            break;
          case 'R':
            fstat->ftype = NDMP9_FILE_REGISTRY;
            break;
          case 'o':
            fstat->ftype = NDMP9_FILE_OTHER;
            break;
          default:
            fstat->ftype = NDMP9_FILE_OTHER;
            return -15;
        }
        scan += 2;
        valid_p = 0;
        break;

      default:
        return -13;
    }

    if (*scan != ' ' && *scan != 0) return -11;

    if (valid_p) *valid_p = NDMP9_VALIDITY_VALID;
  }

  return 0;
}
