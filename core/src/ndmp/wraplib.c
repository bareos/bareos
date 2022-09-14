/*
 * Copyright (c) 1998,1999,2000
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

#include "ndmos.h"
#include "wraplib.h"


int wrap_main(int ac, char* av[], struct wrap_ccb* wccb)
{
  int rc;

  rc = wrap_process_args(ac, av, wccb);
  if (rc) return rc;

  rc = wrap_main_start_index_file(wccb);
  if (rc) return rc;

  rc = wrap_main_start_image_file(wccb);
  if (rc) return rc;


  return 0;
}

int wrap_main_start_index_file(struct wrap_ccb* wccb)
{
  char* filename = wccb->I_index_file_name;
  FILE* fp;

  if (!filename) return 0;

  if (filename[0] == '#') {
    int fd = atoi(filename + 1);

    if (fd < 2 || fd > 100) {
      /* huey! */
      strcpy(wccb->errmsg, "bad -I#N");
      return -1;
    }
    fp = fdopen(fd, "w");
    if (!fp) {
      sprintf(wccb->errmsg, "failed fdopen %s", filename);
      return -1;
    }
  } else {
    fp = fopen(filename, "w");
    if (!fp) {
      sprintf(wccb->errmsg, "failed open %s", filename);
      return -1;
    }
  }

  wccb->index_fp = fp;

  return 0;
}

int wrap_main_start_image_file(struct wrap_ccb* wccb)
{
  char* filename = wccb->f_file_name;
  int fd, o_mode;

  switch (wccb->op) {
    case WRAP_CCB_OP_BACKUP:
      o_mode = O_CREAT | O_WRONLY;
      break;

    case WRAP_CCB_OP_RECOVER:
    case WRAP_CCB_OP_RECOVER_FILEHIST:
      o_mode = O_RDONLY;
      break;

    default:
      abort();
      return -1;
  }

  if (!filename) filename = "-";

  if (strcmp(filename, "-") == 0) {
    if (wccb->op == WRAP_CCB_OP_BACKUP) {
      fd = 1;
    } else {
      fd = 0;
    }
  } else if (filename[0] == '#') {
    fd = atoi(filename + 1);

    if (fd < 2 || fd > 100) {
      /* huey! */
      strcpy(wccb->errmsg, "bad -f#N");
      return -1;
    }
  } else {
    fd = open(filename, o_mode, 0666);
    if (fd < 0) {
      sprintf(wccb->errmsg, "failed open %s", filename);
      return -1;
    }
  }

  wccb->data_conn_fd = fd;

  return 0;
}

void wrap_log(struct wrap_ccb* wccb, char* fmt, ...)
{
  va_list ap;
  char buf[4096];

  if (!wccb->index_fp && wccb->d_debug < 1) return;

  sprintf(buf, "%04d ", ++wccb->log_seq_num);

  va_start(ap, fmt);
  vsnprintf(buf + 5, sizeof(buf) - 5, fmt, ap);
  va_end(ap);

  if (wccb->index_fp) wrap_send_log_message(wccb->index_fp, buf);

  if (wccb->d_debug > 0) fprintf(stderr, "LOG: %s\n", buf);
}

int wrap_set_error(struct wrap_ccb* wccb, int error)
{
  if (error == 0) error = -3;

  wccb->error = error;

  return wccb->error;
}

int wrap_set_errno(struct wrap_ccb* wccb)
{
  return wrap_set_error(wccb, errno);
}


/*
 * wrap -c [-B TYPE] [-d N] [-I FILE] [-E NAME=VALUE ...]
 * wrap -x [-B TYPE] [-d N] [-I FILE] [-E NAME=VALUE ...]
 *              ORIGINAL_NAME @pos NEW_NAME ...
 * wrap -t [-B TYPE] [-d N] [-I FILE] [-E NAME=VALUE ...]
 *              ORIGINAL_NAME @pos
 */

int wrap_process_args(int argc, char* argv[], struct wrap_ccb* wccb)
{
  int c;
  enum wrap_ccb_op op;
  char* p;

  NDMOS_MACRO_ZEROFILL(wccb);

  wccb->progname = argv[0];

  if (argc < 2) {
    strcpy(wccb->errmsg, "too few arguments");
    return -1;
  }

  while ((c = getopt(argc, argv, "cxtB:d:I:E:f:o:")) != EOF) {
    switch (c) {
      case 'c':
        op = WRAP_CCB_OP_BACKUP;
        goto set_op;

      case 't':
        op = WRAP_CCB_OP_RECOVER_FILEHIST;
        goto set_op;

      case 'x':
        op = WRAP_CCB_OP_RECOVER;
        goto set_op;

      set_op:
        if (wccb->op != WRAP_CCB_OP_NONE) {
          strcpy(wccb->errmsg, "only one of -c, -x, -t");
          return -1;
        }
        wccb->op = op;
        break;

      case 'B':
        if (wccb->B_butype) {
          strcpy(wccb->errmsg, "only one -B allowed");
          return -1;
        }
        wccb->B_butype = optarg;
        break;

      case 'd':
        wccb->d_debug = atoi(optarg);
        break;

      case 'E':
        if (wccb->n_env >= WRAP_MAX_ENV) {
          strcpy(wccb->errmsg, "-E overflow");
          return -1;
        }
        p = strchr(optarg, '=');
        if (p) {
          *p++ = 0;
        } else {
          p = "";
        }
        wccb->env[wccb->n_env].name = optarg;
        wccb->env[wccb->n_env].value = p;
        wccb->n_env++;
        break;

      case 'f':
        if (wccb->f_file_name) {
          strcpy(wccb->errmsg, "only one -f allowed");
          return -1;
        }
        wccb->f_file_name = optarg;
        break;

      case 'I':
        if (wccb->I_index_file_name) {
          strcpy(wccb->errmsg, "only one -I allowed");
          return -1;
        }
        wccb->I_index_file_name = optarg;
        break;

      case 'o':
        if (wccb->n_o_option >= WRAP_MAX_O_OPTION) {
          strcpy(wccb->errmsg, "-o overflow");
          return -1;
        }
        wccb->o_option[wccb->n_o_option] = optarg;
        wccb->n_o_option++;
        break;

      default:
        strcpy(wccb->errmsg, "unknown option");
        return -1;
    }
  }

  switch (wccb->op) {
    default:
      abort(); /* just can't happen */

    case WRAP_CCB_OP_NONE:
      strcpy(wccb->errmsg, "one of -c, -x, or -t required");
      return -1;

    case WRAP_CCB_OP_BACKUP:
      if (optind < argc) {
        strcpy(wccb->errmsg, "extra args not allowed for -c");
        return -1;
      }
      break;

    case WRAP_CCB_OP_RECOVER:
    case WRAP_CCB_OP_RECOVER_FILEHIST:
      break;
  }

  for (c = optind; c + 2 < argc; c += 3) {
    p = argv[c + 1];

    if (p[0] != '@') {
      sprintf(wccb->errmsg, "malformed fhinfo %s", p);
      return -1;
    }

    if (wccb->n_file >= WRAP_MAX_FILE) {
      strcpy(wccb->errmsg, "file table overflow");
      return -1;
    }

    if (strcmp(p, "@-") == 0) {
      wccb->file[wccb->n_file].fhinfo = WRAP_INVALID_FHINFO;
    } else {
      wccb->file[wccb->n_file].fhinfo = NDMOS_API_STRTOLL(p + 1, &p, 0);
      if (*p != 0) {
        sprintf(wccb->errmsg, "malformed fhinfo %s", p);
        return -1;
      }
    }

    wccb->file[wccb->n_file].original_name = argv[c];
    wccb->file[wccb->n_file].save_to_name = argv[c + 2];

    wccb->n_file++;
  }

  if (c < argc) {
    strcpy(wccb->errmsg, "superfluous args at end");
    return -1;
  }

  p = wrap_find_env(wccb, "HIST");
  if (p) {
    switch (*p) {
      case 'y':
      case 'Y':
        p = wrap_find_env(wccb, "HIST_TYPE");
        if (!p) { p = "y"; }
        break;
    }

    switch (*p) {
      case 'y':
      case 'Y':
        wccb->hist_enable = 'y';
        break;

      case 'd':
      case 'D':
        wccb->hist_enable = 'd';
        break;

      case 'f':
      case 'F':
        wccb->hist_enable = 'f';
        break;

      default:
        /* gripe? */
        break;
    }
  }

  p = wrap_find_env(wccb, "DIRECT");
  if (p) {
    if (*p == 'y') { wccb->direct_enable = 1; }
  }

  p = wrap_find_env(wccb, "FILESYSTEM");
  if (!p) p = wrap_find_env(wccb, "PREFIX");
  if (!p) p = "/";

  wccb->backup_root = p;

  return 0;
}

char* wrap_find_env(struct wrap_ccb* wccb, char* name)
{
  int i;

  for (i = 0; i < wccb->n_env; i++) {
    if (strcmp(wccb->env[i].name, name) == 0) return wccb->env[i].value;
  }

  return 0;
}


int wrap_cmd_add_with_escapes(char* cmd, char* word, char* special)
{
  char* cmd_lim = &cmd[WRAP_MAX_COMMAND - 3];
  char* p;
  int c;

  p = cmd;
  while (*p) p++;
  if (p != cmd) *p++ = ' ';

  while ((c = *word++) != 0) {
    if (p >= cmd_lim) return -1; /* overflow */
    if (c == '\\' || strchr(special, c)) *p++ = '\\';
    *p++ = c;
  }
  *p = 0;

  return 0;
}

int wrap_cmd_add_with_sh_escapes(char* cmd, char* word)
{
  return wrap_cmd_add_with_escapes(cmd, word, " \t`'\"*?[]$");
}

int wrap_cmd_add_allow_file_wildcards(char* cmd, char* word)
{
  return wrap_cmd_add_with_escapes(cmd, word, " \t`'\"$");
}


int wrap_pipe_fork_exec(char* cmd, int fdmap[3])
{
  int pipes[3][2];
  int child_fdmap[3];
  int nullfd = -1;
  int i;
  int rc = -1;

  for (i = 0; i < 3; i++) {
    pipes[i][0] = -1;
    pipes[i][1] = -1;
    child_fdmap[i] = -1;
  }

  for (i = 0; i < 3; i++) {
    if (fdmap[i] >= 0) {
      child_fdmap[i] = fdmap[i];
      continue;
    }
    switch (fdmap[i]) {
      case WRAP_FDMAP_DEV_NULL:
        if (nullfd < 0) {
          nullfd = open("/dev/null", 2);
          if (nullfd < 0) { goto bail_out; }
        }
        child_fdmap[i] = nullfd;
        break;

      case WRAP_FDMAP_INPUT_PIPE:
        rc = pipe(pipes[i]);
        if (rc != 0) { goto bail_out; }
        child_fdmap[i] = pipes[i][0];
        break;

      case WRAP_FDMAP_OUTPUT_PIPE:
        rc = pipe(pipes[i]);
        if (rc != 0) { goto bail_out; }
        child_fdmap[i] = pipes[i][1];
        break;

      default:
        goto bail_out;
    }
  }

  rc = fork();
  if (rc < 0) { goto bail_out; }

  if (rc == 0) {
    /* child */
    dup2(child_fdmap[2], 2);
    dup2(child_fdmap[1], 1);
    dup2(child_fdmap[0], 0);

    for (rc = 3; rc < 100; rc++) close(rc);

    execl("/bin/sh", "sh", "-c", cmd, NULL);

    fprintf(stderr, "EXEC FAILED %s\n", cmd);
    exit(127);
  }

  if (nullfd >= 0) close(nullfd);

  for (i = 0; i < 3; i++) {
    if (fdmap[i] >= 0) { continue; }
    switch (fdmap[i]) {
      case WRAP_FDMAP_DEV_NULL:
        break;

      case WRAP_FDMAP_INPUT_PIPE:
        close(pipes[i][0]);
        fdmap[i] = pipes[i][1];
        break;

      case WRAP_FDMAP_OUTPUT_PIPE:
        close(pipes[i][1]);
        fdmap[i] = pipes[i][0];
        break;

      default:
        abort();
    }
  }

  return rc; /* PID */

bail_out:
  if (nullfd >= 0) close(nullfd);

  for (i = 0; i < 3; i++) {
    if (pipes[i][0] >= 0) close(pipes[i][0]);
    if (pipes[i][1] >= 0) close(pipes[i][1]);
  }

  return -1;
}


int wrap_parse_msg(char* buf, struct wrap_msg_buf* wmsg)
{
  int c1, c2;

  c1 = buf[0];
  c2 = buf[1];

  if (buf[2] != ' ') { return -1; }

  if (c1 == 'L' && c2 == 'x') { /* log_message */
    return wrap_parse_log_message_msg(buf, wmsg);
  }

  if (c1 == 'H' && c2 == 'F') { /* add_file */
    return wrap_parse_add_file_msg(buf, wmsg);
  }

  if (c1 == 'H' && c2 == 'D') { /* add_dirent */
    return wrap_parse_add_dirent_msg(buf, wmsg);
  }

  if (c1 == 'H' && c2 == 'N') { /* add_node */
    return wrap_parse_add_node_msg(buf, wmsg);
  }

  if (c1 == 'D' && c2 == 'E') { /* add_env */
    return wrap_parse_add_env_msg(buf, wmsg);
  }

  if (c1 == 'D' && c2 == 'R') { /* data_read */
    return wrap_parse_data_read_msg(buf, wmsg);
  }

  return -1;
}

int wrap_parse_log_message_msg(char* buf, struct wrap_msg_buf* wmsg)
{
  struct wrap_log_message* res = &wmsg->body.log_message;
  char* scan = buf + 3;
  int rc;

  wmsg->msg_type = WRAP_MSGTYPE_LOG_MESSAGE;

  while (*scan && *scan == ' ') scan++;

  rc = wrap_cstr_to_str(scan, res->message, sizeof res->message);
  if (rc < 0) return -2;

  return 0;
}

int wrap_send_log_message(FILE* fp, char* message)
{
  struct wrap_msg_buf wmsg;
  struct wrap_log_message* res = &wmsg.body.log_message;

  if (!fp) return -1;

  wrap_cstr_from_str(message, res->message, sizeof res->message);
  fprintf(fp, "Lx %s\n", res->message);

  return 0;
}

int wrap_parse_add_file_msg(char* buf, struct wrap_msg_buf* wmsg)
{
  struct wrap_add_file* res = &wmsg->body.add_file;
  char* scan = buf + 3;
  char* p;
  int rc;

  wmsg->msg_type = WRAP_MSGTYPE_ADD_FILE;

  res->fstat.valid = 0;
  res->fhinfo = WRAP_INVALID_FHINFO;

  while (*scan && *scan == ' ') scan++;
  if (*scan == 0) return -1;

  p = scan;
  while (*scan && *scan != ' ') scan++;

  if (*scan) {
    *scan = 0;
    rc = wrap_cstr_to_str(p, res->path, sizeof res->path);
    *scan++ = ' ';
  } else {
    rc = wrap_cstr_to_str(p, res->path, sizeof res->path);
  }
  if (rc < 0) return -2;

  while (*scan) {
    p = scan + 1;
    switch (*scan) {
      case ' ':
        scan++;
        continue;

      case '@':
        res->fhinfo = NDMOS_API_STRTOLL(p, &scan, 0);
        break;

      default:
        rc = wrap_parse_fstat_subr(&scan, &res->fstat);
        if (rc < 0) return rc;
        break;
    }

    if (*scan != ' ' && *scan != 0) {
      /* bogus */
      return -1;
    }
  }

  return 0;
}

int wrap_send_add_file(FILE* fp,
                       char* path,
                       uint64_t fhinfo,
                       struct wrap_fstat* fstat)
{
  struct wrap_msg_buf wmsg;
  struct wrap_add_file* res = &wmsg.body.add_file;

  if (!fp) return -1;

  wrap_cstr_from_str(path, res->path, sizeof res->path);
  fprintf(fp, "HF %s", res->path);

  if (fhinfo != WRAP_INVALID_FHINFO) fprintf(fp, " @%llu", fhinfo);

  wrap_send_fstat_subr(fp, fstat);

  fprintf(fp, "\n");

  return 0;
}

int wrap_parse_add_dirent_msg(char* buf, struct wrap_msg_buf* wmsg)
{
  struct wrap_add_dirent* res = &wmsg->body.add_dirent;
  char* scan = buf + 3;
  char* p;
  int rc;

  wmsg->msg_type = WRAP_MSGTYPE_ADD_DIRENT;

  res->fhinfo = WRAP_INVALID_FHINFO;

  while (*scan && *scan == ' ') scan++;
  if (*scan == 0) return -1;

  res->dir_fileno = NDMOS_API_STRTOLL(scan, &scan, 0);
  if (*scan != ' ') return -1;

  while (*scan == ' ') scan++;

  if (*scan == 0) return -1;

  p = scan;
  while (*scan && *scan != ' ') scan++;

  if (*scan) {
    *scan = 0;
    rc = wrap_cstr_to_str(p, res->name, sizeof res->name);
    *scan++ = ' ';
  } else {
    rc = wrap_cstr_to_str(p, res->name, sizeof res->name);
  }
  if (rc < 0) return -2;

  res->fileno = NDMOS_API_STRTOLL(scan, &scan, 0);
  if (*scan != ' ' && *scan != 0) return -1;

  while (*scan == ' ') scan++;

  if (*scan == '@') { res->fhinfo = NDMOS_API_STRTOLL(scan + 1, &scan, 0); }

  if (*scan != ' ' && *scan != 0) return -1;

  while (*scan == ' ') scan++;

  if (*scan) return -1;

  return 0;
}

int wrap_send_add_dirent(FILE* fp,
                         char* name,
                         uint64_t fhinfo,
                         uint64_t dir_fileno,
                         uint64_t fileno)
{
  struct wrap_msg_buf wmsg;
  struct wrap_add_dirent* res = &wmsg.body.add_dirent;

  if (!fp) return -1;

  wrap_cstr_from_str(name, res->name, sizeof res->name);
  fprintf(fp, "HD %llu %s %llu", dir_fileno, res->name, fileno);

  if (fhinfo != WRAP_INVALID_FHINFO) fprintf(fp, " @%llu", fhinfo);

  fprintf(fp, "\n");

  return 0;
}


int wrap_parse_add_node_msg(char* buf, struct wrap_msg_buf* wmsg)
{
  struct wrap_add_node* res = &wmsg->body.add_node;
  char* scan = buf + 3;
  char* p;
  int rc;

  wmsg->msg_type = WRAP_MSGTYPE_ADD_NODE;

  res->fstat.valid = 0;
  res->fhinfo = WRAP_INVALID_FHINFO;

  while (*scan && *scan == ' ') scan++;
  if (*scan == 0) return -1;

  res->fstat.fileno = NDMOS_API_STRTOLL(scan, &scan, 0);
  if (*scan != ' ' && *scan != 0) return -1;

  res->fstat.valid |= WRAP_FSTAT_VALID_FILENO;

  while (*scan) {
    p = scan + 1;
    switch (*scan) {
      case ' ':
        scan++;
        continue;

      case '@':
        res->fhinfo = NDMOS_API_STRTOLL(p, &scan, 0);
        break;

      default:
        rc = wrap_parse_fstat_subr(&scan, &res->fstat);
        if (rc < 0) return rc;
        break;
    }

    if (*scan != ' ' && *scan != 0) {
      /* bogus */
      return -1;
    }
  }

  if ((res->fstat.valid & WRAP_FSTAT_VALID_FILENO) == 0) return -5;

  return 0;
}

int wrap_send_add_node(FILE* fp, uint64_t fhinfo, struct wrap_fstat* fstat)
{
  uint32_t save_valid;

  if (!fp) return -1;

  if (fstat->valid & WRAP_FSTAT_VALID_FILENO) {
    fprintf(fp, "HN %llu", fstat->fileno);
  } else {
    fprintf(fp, "HN 0000000000");
  }

  if (fhinfo != WRAP_INVALID_FHINFO) fprintf(fp, " @%llu", fhinfo);

  /* suppress iFILENO */
  save_valid = fstat->valid;
  fstat->valid &= ~WRAP_FSTAT_VALID_FILENO;
  wrap_send_fstat_subr(fp, fstat);
  fstat->valid = save_valid;

  fprintf(fp, "\n");

  return 0;
}


int wrap_parse_fstat_subr(char** scanp, struct wrap_fstat* fstat)
{
  char* scan = *scanp;
  char* p = scan + 1;
  uint32_t valid = 0;

  valid = 0;
  switch (*scan) {
    case 's': /* size */
      valid = WRAP_FSTAT_VALID_SIZE;
      fstat->size = NDMOS_API_STRTOLL(p, &scan, 0);
      break;

    case 'i': /* fileno (inum) */
      valid = WRAP_FSTAT_VALID_FILENO;
      fstat->fileno = NDMOS_API_STRTOLL(p, &scan, 0);
      break;

    case 'm': /* mode low twelve bits */
      valid = WRAP_FSTAT_VALID_MODE;
      fstat->mode = strtol(p, &scan, 8);
      break;

    case 'l': /* link count */
      valid = WRAP_FSTAT_VALID_LINKS;
      fstat->links = strtol(p, &scan, 0);
      break;

    case 'u': /* uid */
      valid = WRAP_FSTAT_VALID_UID;
      fstat->uid = strtol(p, &scan, 0);
      break;

    case 'g': /* gid */
      valid = WRAP_FSTAT_VALID_GID;
      fstat->gid = strtol(p, &scan, 0);
      break;

    case 't': /* one of the times */
      p = scan + 2;
      switch (scan[1]) {
        case 'm': /* mtime */
          valid = WRAP_FSTAT_VALID_MTIME;
          fstat->mtime = strtol(p, &scan, 0);
          break;

        case 'a': /* atime */
          valid = WRAP_FSTAT_VALID_ATIME;
          fstat->atime = strtol(p, &scan, 0);
          break;

        case 'c': /* ctime */
          valid = WRAP_FSTAT_VALID_CTIME;
          fstat->ctime = strtol(p, &scan, 0);
          break;

        default:
          return -3;
      }
      break;

    case 'f': /* ftype (file type) */
      valid = WRAP_FSTAT_VALID_FTYPE;
      switch (scan[1]) {
        case 'd':
          fstat->ftype = WRAP_FTYPE_DIR;
          break;
        case 'p':
          fstat->ftype = WRAP_FTYPE_FIFO;
          break;
        case 'c':
          fstat->ftype = WRAP_FTYPE_CSPEC;
          break;
        case 'b':
          fstat->ftype = WRAP_FTYPE_BSPEC;
          break;
        case '-':
          fstat->ftype = WRAP_FTYPE_REG;
          break;
        case 'l':
          fstat->ftype = WRAP_FTYPE_SLINK;
          break;
        case 's':
          fstat->ftype = WRAP_FTYPE_SOCK;
          break;
        case 'R':
          fstat->ftype = WRAP_FTYPE_REGISTRY;
          break;
        case 'o':
          fstat->ftype = WRAP_FTYPE_OTHER;
          break;
        default:
          fstat->ftype = WRAP_FTYPE_INVALID;
          return -5;
      }
      scan += 2;
      break;

    default:
      return -3;
  }

  if (*scan != ' ' && *scan != 0) return -1;

  fstat->valid |= valid;
  *scanp = scan;

  return 0;
}

int wrap_send_fstat_subr(FILE* fp, struct wrap_fstat* fstat)
{
  if (!fp) return -1;

  if (fstat->valid & WRAP_FSTAT_VALID_FTYPE) {
    int c = 0;

    switch (fstat->ftype) {
      default:
      case WRAP_FTYPE_INVALID:
        c = 0;
        break;
      case WRAP_FTYPE_DIR:
        c = 'd';
        break;
      case WRAP_FTYPE_FIFO:
        c = 'p';
        break;
      case WRAP_FTYPE_CSPEC:
        c = 'c';
        break;
      case WRAP_FTYPE_BSPEC:
        c = 'b';
        break;
      case WRAP_FTYPE_REG:
        c = '-';
        break;
      case WRAP_FTYPE_SLINK:
        c = 'l';
        break;
      case WRAP_FTYPE_SOCK:
        c = 's';
        break;
      case WRAP_FTYPE_REGISTRY:
        c = 'R';
        break;
      case WRAP_FTYPE_OTHER:
        c = 'o';
        break;
    }

    if (c) {
      fprintf(fp, " f%c", c);
    } else {
      return -1;
    }
  }

  if (fstat->valid & WRAP_FSTAT_VALID_MODE) {
    fprintf(fp, " m%04o", fstat->mode);
  }

  if (fstat->valid & WRAP_FSTAT_VALID_LINKS) {
    fprintf(fp, " l%lu", fstat->links);
  }

  if (fstat->valid & WRAP_FSTAT_VALID_SIZE) {
    fprintf(fp, " s%llu", fstat->size);
  }

  if (fstat->valid & WRAP_FSTAT_VALID_UID) { fprintf(fp, " u%lu", fstat->uid); }

  if (fstat->valid & WRAP_FSTAT_VALID_GID) { fprintf(fp, " g%lu", fstat->gid); }

  if (fstat->valid & WRAP_FSTAT_VALID_ATIME) {
    fprintf(fp, " ta%lu", fstat->atime);
  }

  if (fstat->valid & WRAP_FSTAT_VALID_MTIME) {
    fprintf(fp, " tm%lu", fstat->mtime);
  }

  if (fstat->valid & WRAP_FSTAT_VALID_CTIME) {
    fprintf(fp, " tc%lu", fstat->ctime);
  }

  if (fstat->valid & WRAP_FSTAT_VALID_FILENO) {
    fprintf(fp, " i%llu", fstat->fileno);
  }

  return 0;
}

int wrap_parse_add_env_msg(char* buf, struct wrap_msg_buf* wmsg)
{
  struct wrap_add_env* res = &wmsg->body.add_env;
  char* scan = buf + 3;
  char* p;
  int rc;

  wmsg->msg_type = WRAP_MSGTYPE_ADD_ENV;

  while (*scan && *scan == ' ') scan++;
  if (*scan == 0) return -1;

  p = scan;
  while (*scan && *scan != ' ') scan++;

  if (*scan) {
    *scan = 0;
    rc = wrap_cstr_to_str(p, res->name, sizeof res->name);
    *scan++ = ' ';
  } else {
    rc = wrap_cstr_to_str(p, res->name, sizeof res->name);
  }
  if (rc < 0) return -2;

  while (*scan && *scan == ' ') scan++;

  p = scan;
  while (*scan && *scan != ' ') scan++;

  if (*scan) {
    *scan = 0;
    rc = wrap_cstr_to_str(p, res->value, sizeof res->value);
    *scan++ = ' ';
  } else {
    rc = wrap_cstr_to_str(p, res->value, sizeof res->value);
  }
  if (rc < 0) return -2;

  return 0;
}

int wrap_send_add_env(FILE* fp, char* name, char* value)
{
  struct wrap_msg_buf wmsg;
  struct wrap_add_env* res = &wmsg.body.add_env;

  if (!fp) return -1;

  wrap_cstr_from_str(name, res->name, sizeof res->name);
  wrap_cstr_from_str(value, res->value, sizeof res->value);

  fprintf(fp, "DE %s %s\n", res->name, res->value);

  return 0;
}

int wrap_parse_data_read_msg(char* buf, struct wrap_msg_buf* wmsg)
{
  struct wrap_data_read* res = &wmsg->body.data_read;
  char* scan = buf + 3;

  wmsg->msg_type = WRAP_MSGTYPE_DATA_READ;

  while (*scan && *scan == ' ') scan++;
  if (*scan == 0) return -1;

  res->offset = NDMOS_API_STRTOLL(scan, &scan, 0);
  if (*scan != ' ') return -1;

  while (*scan && *scan != ' ') scan++;

  if (*scan == 0) return -1;

  res->length = NDMOS_API_STRTOLL(scan, &scan, 0);

  /* tolerate trailing white */
  while (*scan && *scan != ' ') scan++;

  if (*scan != 0) return -1;

  return 0;
}

int wrap_send_data_read(FILE* fp, uint64_t offset, uint64_t length)
{
  if (!fp) return -1;

  fprintf(fp, "DR %lld %lld\n", (int64_t)offset, (int64_t)length);
  fflush(fp);

  return 0;
}

int wrap_parse_data_stats_msg([[maybe_unused]] char* buf,
                              [[maybe_unused]] struct wrap_msg_buf* wmsg)
{
  return -1;
}

int wrap_send_data_stats(FILE* fp)
{
  if (!fp) return -1;

  fprintf(fp, "DS ...\n");
  fflush(fp);

  return 0;
}


/*
 * Recovery helpers
 ****************************************************************
 */

int wrap_reco_align_to_wanted(struct wrap_ccb* wccb)
{
  uint64_t distance;
  uint64_t unwanted_length;

top:
  // If there is an error, we're toast.
  if (wccb->error) return wccb->error;

  // If we're aligned, we're done.
  if (wccb->expect_offset == wccb->want_offset) {
    if (wccb->expect_length < wccb->want_length && wccb->reading_length == 0) {
      wrap_reco_issue_read(wccb);
    }
    return wccb->error;
  }

  // If we have a portion we don't want, consume it now
  if (wccb->have_length > 0) {
    if (wccb->have_offset < wccb->want_offset) {
      distance = wccb->want_offset - wccb->have_offset;
      if (distance < wccb->have_length) {
        /*
         * We have some of what we want.
         * Consume (discard) unwanted part.
         */
        unwanted_length = distance;
      } else {
        unwanted_length = wccb->have_length;
      }
    } else {
      unwanted_length = wccb->have_length;
    }
    wrap_reco_consume(wccb, unwanted_length);
    goto top;
  }

  if (wccb->expect_length > 0) {
    /* Incoming, but we don't have it yet. */
    wrap_reco_receive(wccb);
    goto top;
  }

  /*
   * We don't have anything. We don't expect anything.
   * Time to issue an NDMP_DATA_NOTIFY_READ via this wrapper.
   */

  wrap_reco_issue_read(wccb);

  goto top;
}

int wrap_reco_receive(struct wrap_ccb* wccb)
{
  char* iobuf_end = &wccb->iobuf[wccb->n_iobuf];
  char* have_end = wccb->have + wccb->have_length;
  unsigned n_read = iobuf_end - have_end;
  int rc;

  if (wccb->error) return wccb->error;

  if (wccb->have_length == 0) {
    wccb->have = wccb->iobuf;
    have_end = wccb->have + wccb->have_length;
  }

  if (n_read < 512 && wccb->have != wccb->iobuf) {
    /* Not much room at have_end. Front of iobuf available. */
    /* Compress */
    NDMOS_API_BCOPY(wccb->have, wccb->iobuf, wccb->have_length);
    wccb->have = wccb->iobuf;
    have_end = wccb->have + wccb->have_length;
    n_read = iobuf_end - have_end;
  }

  if (n_read > wccb->reading_length) n_read = wccb->reading_length;

  if (n_read == 0) {
    /* Hmmm. */
    abort();
    return -1;
  }

  rc = read(wccb->data_conn_fd, have_end, n_read);
  if (rc > 0) {
    wccb->have_length += rc;
    wccb->reading_offset += rc;
    wccb->reading_length -= rc;
  } else {
    /* EOF or error */
    if (rc == 0) {
      strcpy(wccb->errmsg, "EOF on data connection");
      wrap_set_error(wccb, -1);
    } else {
      sprintf(wccb->errmsg, "errno %d on data connection", errno);
      wrap_set_errno(wccb);
    }
  }

  return wccb->error;
}

int wrap_reco_consume(struct wrap_ccb* wccb, uint32_t length)
{
  assert(wccb->have_length >= length);

  wccb->have_offset += length;
  wccb->have_length -= length;
  wccb->expect_offset += length;
  wccb->expect_length -= length;
  wccb->have += length;

  if (wccb->expect_length == 0) {
    assert(wccb->have_length == 0);
    wccb->expect_offset = -1ull;
  }

  return wccb->error;
}

int wrap_reco_must_have(struct wrap_ccb* wccb, uint32_t length)
{
  if (wccb->want_length < length) wccb->want_length = length;

  wrap_reco_align_to_wanted(wccb);

  while (wccb->have_length < length && !wccb->error) {
    wrap_reco_align_to_wanted(wccb); /* triggers issue_read() */
    wrap_reco_receive(wccb);
  }

  if (wccb->have_length >= length) return 0;

  return wccb->error;
}

int wrap_reco_seek(struct wrap_ccb* wccb,
                   uint64_t want_offset,
                   uint64_t want_length,
                   uint32_t must_have_length)
{
  if (wccb->error) return wccb->error;

  wccb->want_offset = want_offset;
  wccb->want_length = want_length;

  return wrap_reco_must_have(wccb, must_have_length);
}

int wrap_reco_pass(struct wrap_ccb* wccb,
                   int write_fd,
                   uint64_t length,
                   unsigned write_bsize)
{
  unsigned cnt;
  int rc;

  while (length > 0) {
    if (wccb->error) break;

    cnt = write_bsize;
    if (cnt > length) cnt = length;

    if (wccb->have_length < cnt) { wrap_reco_must_have(wccb, cnt); }

    rc = write(write_fd, wccb->have, cnt);

    length -= cnt;
    wrap_reco_consume(wccb, cnt);
  }

  return wccb->error;
}

int wrap_reco_issue_read(struct wrap_ccb* wccb)
{
  uint64_t off;
  uint64_t len;

  assert(wccb->reading_length == 0);

  if (wccb->data_conn_mode == 0) {
    struct stat st;
    int rc;

    rc = fstat(wccb->data_conn_fd, &st);
    if (rc != 0) {
      sprintf(wccb->errmsg, "Can't fstat() data conn rc=%d", rc);
      return wrap_set_errno(wccb);
    }
    if (S_ISFIFO(st.st_mode)) {
      wccb->data_conn_mode = 'p';
      if (!wccb->index_fp) {
        strcpy(wccb->errmsg, "data_conn is pipe but no -I");
        return wrap_set_error(wccb, -3);
      }
    } else if (S_ISREG(st.st_mode)) {
      wccb->data_conn_mode = 'f';
    } else {
      sprintf(wccb->errmsg, "Unsupported data_conn type %o", st.st_mode);
      return wrap_set_error(wccb, -3);
    }
  }

  off = wccb->want_offset;
  len = wccb->want_length;

  off += wccb->have_length;
  len -= wccb->have_length;

  if (len == 0) { abort(); }

  wccb->last_read_offset = off;
  wccb->last_read_length = len;

  switch (wccb->data_conn_mode) {
    default:
      abort();
      return -1;

    case 'f':
      if (lseek(wccb->data_conn_fd, off, 0) < 0) { return -1; }
      break;

    case 'p':
      wrap_send_data_read(wccb->index_fp, off, len);
      break;
  }

  wccb->reading_offset = wccb->last_read_offset;
  wccb->reading_length = wccb->last_read_length;

  if (wccb->have_length == 0) {
    wccb->expect_offset = wccb->reading_offset;
    wccb->expect_length = wccb->reading_length;
  } else {
    wccb->expect_length += len;
  }

  return wccb->error;
}


/*
 * (Note: this is hoisted from ndml_cstr.c)
 *
 * Description:
 *      Convert strings to/from a canonical strings (CSTR).
 *
 *      The main reason for this is to eliminate spaces
 *      in strings thus making multiple strings easily
 *      delimited by white space.
 *
 *      Canonical strings use the HTTP convention of
 *      percent sign followed by two hex digits (%xx).
 *      Characters outside the printable ASCII range,
 *      space, and percent sign are so converted.
 *
 *      Both interfaces return the length of the resulting
 *      string, -1 if there is an overflow, or -2
 *      there is a conversion error.
 */

int wrap_cstr_from_str(char* src, char* dst, unsigned dst_max)
{
  static char cstr_to_hex[] = "0123456789ABCDEF";
  unsigned char* p = (unsigned char*)src;
  unsigned char* q = (unsigned char*)dst;
  unsigned char* q_end = q + dst_max - 1;
  int c;

  while ((c = *p++) != 0) {
    if (c <= ' ' || c > 0x7E || c == NDMCSTR_WARN) {
      if (q + 3 > q_end) return -1;
      *q++ = NDMCSTR_WARN;
      *q++ = cstr_to_hex[(c >> 4) & 0xF];
      *q++ = cstr_to_hex[c & 0xF];
    } else {
      if (q + 1 > q_end) return -1;
      *q++ = c;
    }
  }
  *q = 0;

  return q - (unsigned char*)dst;
}

int wrap_cstr_to_str(char* src, char* dst, unsigned dst_max)
{
  unsigned char* p = (unsigned char*)src;
  unsigned char* q = (unsigned char*)dst;
  unsigned char* q_end = q + dst_max - 1;
  int c, c1, c2;

  while ((c = *p++) != 0) {
    if (q + 1 > q_end) return -1;
    if (c != NDMCSTR_WARN) {
      *q++ = c;
      continue;
    }
    c1 = wrap_cstr_from_hex(p[0]);
    c2 = wrap_cstr_from_hex(p[1]);

    if (c1 < 0 || c2 < 0) {
      /* busted conversion */
      return -2;
    }

    c = (c1 << 4) + c2;
    *q++ = c;
    p += 2;
  }
  *q = 0;

  return q - (unsigned char*)dst;
}

int wrap_cstr_from_hex(int c)
{
  if ('0' <= c && c <= '9') return c - '0';
  if ('a' <= c && c <= 'f') return (c - 'a') + 10;
  if ('A' <= c && c <= 'F') return (c - 'A') + 10;
  return -1;
}
