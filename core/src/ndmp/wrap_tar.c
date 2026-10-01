/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

/*
 * wrap_tar -- NDMP data agent formatter for the ndmjob emulator.
 *
 * The ndmjob DATA agent runs "wrap_<BUTYPE>" to create and restore the
 * backup image (see ndma_data.c). This program implements that
 * interface (see wraplib.h) and is installed as wrap_tar and wrap_dump.
 *
 * The image is a POSIX ustar archive (with GNU long name extensions),
 * readable by GNU tar. File history is reported with the byte offset of
 * each member as fhinfo. When a LEVEL is given (dump semantics), only
 * files changed since the last backup of a lower level are saved. The
 * backup dates are kept in a dumpdates file ($WRAP_TAR_DUMPDATES or
 * ./wrap_tar.dumpdates).
 *
 * It is meant for testing only.
 */

#include "ndmos.h"
#include "wraplib.h"

#include <dirent.h>
#include <stddef.h>
#include <sys/time.h>

#define TAR_BLOCK 512
#define TAR_RECORD (20 * TAR_BLOCK)
#define TAR_LONGLINK_NAME "././@LongLink"
#define DUMPDATES_DEFAULT "wrap_tar.dumpdates"
#define DUMPDATES_MAX_LINE (WRAP_MAX_PATH + 64)

#define WRAP_TAR_LENGTH_INFINITY (~0ULL)

#ifndef MIN
#  define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

struct tar_header {
  char name[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char chksum[8];
  char typeflag;
  char linkname[100];
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[155];
  char pad[12];
};

_Static_assert(sizeof(struct tar_header) == TAR_BLOCK, "bad tar header size");

struct wrap_tar {
  struct wrap_ccb* wccb;
  int fd;
  uint64_t offset;
  int level;
  time_t since;
  int n_errors;
  uint64_t n_files;
  int n_extract_errors; /* reported per name list entry, not fatal */
  int last_errno;
  /* recovery: per name list entry, was a member restored, first errno */
  int file_seen[WRAP_MAX_FILE];
  int file_errno[WRAP_MAX_FILE];
};

/*
 * Low level I/O
 ****************************************************************
 */

static int write_all(int fd, const void* buf, size_t len)
{
  const char* p = buf;

  while (len > 0) {
    ssize_t rc = write(fd, p, len);
    if (rc < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    p += rc;
    len -= (size_t)rc;
  }
  return 0;
}

/* returns number of bytes read, less than len only on EOF, -1 on error */
static ssize_t read_full(int fd, void* buf, size_t len)
{
  char* p = buf;
  size_t done = 0;

  while (done < len) {
    ssize_t rc = read(fd, p + done, len - done);
    if (rc < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (rc == 0) break;
    done += (size_t)rc;
  }
  return (ssize_t)done;
}

static int image_write(struct wrap_tar* wt, const void* buf, size_t len)
{
  if (write_all(wt->fd, buf, len) < 0) return -1;
  wt->offset += len;
  return 0;
}

static int image_pad(struct wrap_tar* wt, uint64_t len)
{
  static const char zeros[TAR_BLOCK];
  uint64_t rest = len % TAR_BLOCK;

  if (rest == 0) return 0;
  return image_write(wt, zeros, TAR_BLOCK - rest);
}

static int image_read(struct wrap_tar* wt, void* buf, size_t len)
{
  ssize_t rc = read_full(wt->fd, buf, len);

  if (rc < 0) return -1;
  wt->offset += (uint64_t)rc;
  return (size_t)rc == len ? 0 : -1;
}

static int image_skip(struct wrap_tar* wt, uint64_t len)
{
  char buf[64 * 1024];

  while (len > 0) {
    size_t n = len > sizeof buf ? sizeof buf : (size_t)len;
    if (image_read(wt, buf, n) < 0) return -1;
    len -= n;
  }
  return 0;
}

static uint64_t padded(uint64_t len)
{
  return (len + TAR_BLOCK - 1) / TAR_BLOCK * TAR_BLOCK;
}

/*
 * Tar header helpers
 ****************************************************************
 */

static void put_number(char* field, size_t size, uint64_t value)
{
  char tmp[32];
  uint64_t max_octal = 1;

  for (size_t i = 0; i < size - 1; i++) max_octal *= 8;

  if (value < max_octal) {
    snprintf(tmp, sizeof tmp, "%0*llo", (int)(size - 1),
             (unsigned long long)value);
    memcpy(field, tmp, size - 1);
    field[size - 1] = 0;
  } else {
    /* GNU base-256 encoding */
    memset(field, 0, size);
    field[0] = (char)0x80;
    for (size_t i = size - 1; i > 0 && value; i--) {
      field[i] = (char)(value & 0xff);
      value >>= 8;
    }
  }
}

static uint64_t get_number(const char* field, size_t size)
{
  uint64_t value = 0;
  size_t i = 0;

  if ((unsigned char)field[0] & 0x80) {
    value = (unsigned char)field[0] & 0x3f;
    for (i = 1; i < size; i++) value = (value << 8) | (unsigned char)field[i];
    return value;
  }

  while (i < size && (field[i] == ' ' || field[i] == 0)) i++;
  while (i < size && field[i] >= '0' && field[i] <= '7') {
    value = value * 8 + (uint64_t)(field[i] - '0');
    i++;
  }
  return value;
}

static unsigned header_checksum(const struct tar_header* h)
{
  const unsigned char* p = (const unsigned char*)h;
  unsigned sum = 0;

  for (size_t i = 0; i < sizeof *h; i++) {
    if (i >= offsetof(struct tar_header, chksum)
        && i < offsetof(struct tar_header, chksum) + sizeof h->chksum) {
      sum += ' ';
    } else {
      sum += p[i];
    }
  }
  return sum;
}

static void header_finish(struct tar_header* h)
{
  char tmp[16];

  memcpy(h->magic, "ustar", 6);
  memcpy(h->version, "00", 2);
  snprintf(tmp, sizeof tmp, "%06o", header_checksum(h));
  memcpy(h->chksum, tmp, 7);
  h->chksum[7] = ' ';
}

static int is_zero_block(const char* block)
{
  for (int i = 0; i < TAR_BLOCK; i++) {
    if (block[i]) return 0;
  }
  return 1;
}

static int write_longlink(struct wrap_tar* wt, char type, const char* name)
{
  struct tar_header h;
  size_t len = strlen(name) + 1;

  memset(&h, 0, sizeof h);
  strcpy(h.name, TAR_LONGLINK_NAME);
  put_number(h.mode, sizeof h.mode, 0644);
  put_number(h.uid, sizeof h.uid, 0);
  put_number(h.gid, sizeof h.gid, 0);
  put_number(h.size, sizeof h.size, len);
  put_number(h.mtime, sizeof h.mtime, 0);
  h.typeflag = type;
  header_finish(&h);

  if (image_write(wt, &h, sizeof h) < 0) return -1;
  if (image_write(wt, name, len) < 0) return -1;
  return image_pad(wt, len);
}

/*
 * File history
 ****************************************************************
 */

static void fstat_from_stat(struct wrap_fstat* fs, const struct stat* st)
{
  memset(fs, 0, sizeof *fs);

  if (S_ISDIR(st->st_mode))
    fs->ftype = WRAP_FTYPE_DIR;
  else if (S_ISREG(st->st_mode))
    fs->ftype = WRAP_FTYPE_REG;
  else if (S_ISLNK(st->st_mode))
    fs->ftype = WRAP_FTYPE_SLINK;
  else if (S_ISFIFO(st->st_mode))
    fs->ftype = WRAP_FTYPE_FIFO;
  else
    fs->ftype = WRAP_FTYPE_OTHER;

  fs->mode = st->st_mode & 07777;
  fs->links = (uint32_t)st->st_nlink;
  fs->size = (uint64_t)st->st_size;
  fs->uid = st->st_uid;
  fs->gid = st->st_gid;
  fs->atime = (uint32_t)st->st_atime;
  fs->mtime = (uint32_t)st->st_mtime;
  fs->ctime = (uint32_t)st->st_ctime;
  fs->fileno = (uint64_t)st->st_ino;
  fs->valid = WRAP_FSTAT_VALID_FTYPE | WRAP_FSTAT_VALID_MODE
              | WRAP_FSTAT_VALID_LINKS | WRAP_FSTAT_VALID_SIZE
              | WRAP_FSTAT_VALID_UID | WRAP_FSTAT_VALID_GID
              | WRAP_FSTAT_VALID_ATIME | WRAP_FSTAT_VALID_MTIME
              | WRAP_FSTAT_VALID_CTIME | WRAP_FSTAT_VALID_FILENO;
}

static void send_history(struct wrap_tar* wt,
                         const char* rel,
                         uint64_t fhinfo,
                         const struct stat* st)
{
  struct wrap_fstat fs;
  char path[WRAP_MAX_PATH];

  if (!wt->wccb->hist_enable || !wt->wccb->index_fp) return;
  /* the backup root itself is implied by FILESYSTEM */
  if (rel[0] == 0) return;

  fstat_from_stat(&fs, st);
  snprintf(path, sizeof path, "%s", rel);
  wrap_send_add_file(wt->wccb->index_fp, path, fhinfo, &fs);
}

/*
 * Dumpdates (for LEVEL based backups)
 ****************************************************************
 */

static const char* dumpdates_file(void)
{
  const char* p = getenv("WRAP_TAR_DUMPDATES");

  return (p && *p) ? p : DUMPDATES_DEFAULT;
}

/* newest date of a backup of filesystem with a level lower than level */
static time_t dumpdates_lookup(const char* filesystem, int level)
{
  FILE* fp = fopen(dumpdates_file(), "r");
  char line[DUMPDATES_MAX_LINE];
  time_t since = 0;

  if (!fp) return 0;

  while (fgets(line, sizeof line, fp)) {
    int l;
    long long date;
    int n = 0;

    line[strcspn(line, "\n")] = 0;
    if (sscanf(line, "%d %lld %n", &l, &date, &n) < 2 || n == 0) continue;
    if (strcmp(line + n, filesystem) != 0) continue;
    if (l < level && (time_t)date > since) since = (time_t)date;
  }
  fclose(fp);

  return since;
}

static int dumpdates_update(const char* filesystem, int level, time_t date)
{
  const char* name = dumpdates_file();
  char tmpname[WRAP_MAX_PATH];
  char line[DUMPDATES_MAX_LINE];
  FILE* in;
  FILE* out;

  snprintf(tmpname, sizeof tmpname, "%s.tmp.%ld", name, (long)getpid());
  out = fopen(tmpname, "w");
  if (!out) return -1;

  in = fopen(name, "r");
  if (in) {
    while (fgets(line, sizeof line, in)) {
      char copy[DUMPDATES_MAX_LINE];
      int l;
      long long d;
      int n = 0;

      snprintf(copy, sizeof copy, "%s", line);
      copy[strcspn(copy, "\n")] = 0;
      if (sscanf(copy, "%d %lld %n", &l, &d, &n) >= 2 && n > 0
          && strcmp(copy + n, filesystem) == 0 && l >= level) {
        continue;
      }
      fputs(line, out);
    }
    fclose(in);
  }

  fprintf(out, "%d %lld %s\n", level, (long long)date, filesystem);
  if (fclose(out) != 0) {
    unlink(tmpname);
    return -1;
  }
  return rename(tmpname, name);
}

/*
 * Backup
 ****************************************************************
 */

static int backup_entry(struct wrap_tar* wt,
                        const char* abs,
                        const char* rel,
                        const struct stat* st)
{
  struct tar_header h;
  char name[WRAP_MAX_PATH + 3];
  char target[WRAP_MAX_PATH];
  uint64_t fhinfo = wt->offset;
  uint64_t size = 0;

  memset(&h, 0, sizeof h);
  target[0] = 0;

  if (S_ISDIR(st->st_mode)) {
    snprintf(name, sizeof name, ".%s/", rel);
    h.typeflag = '5';
  } else if (S_ISREG(st->st_mode)) {
    snprintf(name, sizeof name, ".%s", rel);
    h.typeflag = '0';
    size = (uint64_t)st->st_size;
  } else if (S_ISLNK(st->st_mode)) {
    ssize_t n = readlink(abs, target, sizeof target - 1);
    if (n < 0) {
      wrap_log(wt->wccb, "readlink %s failed: %s", abs, strerror(errno));
      wt->n_errors++;
      return 0;
    }
    target[n] = 0;
    snprintf(name, sizeof name, ".%s", rel);
    h.typeflag = '2';
  } else if (S_ISFIFO(st->st_mode)) {
    snprintf(name, sizeof name, ".%s", rel);
    h.typeflag = '6';
  } else {
    wrap_log(wt->wccb, "skipping unsupported file type %s", abs);
    return 0;
  }

  if (strlen(name) >= sizeof h.name) {
    if (write_longlink(wt, 'L', name) < 0) return -1;
  }
  if (strlen(target) >= sizeof h.linkname) {
    if (write_longlink(wt, 'K', target) < 0) return -1;
  }

  /* truncated names are preceded by a GNU long name record */
  memcpy(h.name, name, MIN(strlen(name), sizeof h.name));
  memcpy(h.linkname, target, MIN(strlen(target), sizeof h.linkname));
  put_number(h.mode, sizeof h.mode, st->st_mode & 07777);
  put_number(h.uid, sizeof h.uid, st->st_uid);
  put_number(h.gid, sizeof h.gid, st->st_gid);
  put_number(h.size, sizeof h.size, size);
  put_number(h.mtime, sizeof h.mtime, (uint64_t)st->st_mtime);
  header_finish(&h);

  if (image_write(wt, &h, sizeof h) < 0) return -1;

  if (size > 0) {
    char buf[64 * 1024];
    uint64_t left = size;
    int fd = open(abs, O_RDONLY);

    if (fd < 0) {
      wrap_log(wt->wccb, "open %s failed: %s", abs, strerror(errno));
      wt->n_errors++;
    }
    while (left > 0) {
      size_t n = left > sizeof buf ? sizeof buf : (size_t)left;
      ssize_t rc = fd >= 0 ? read_full(fd, buf, n) : 0;

      if (rc < 0) rc = 0;
      /* the file shrunk while reading, fill up to the announced size */
      if ((size_t)rc < n) memset(buf + rc, 0, n - (size_t)rc);
      if (image_write(wt, buf, n) < 0) {
        if (fd >= 0) close(fd);
        return -1;
      }
      left -= n;
    }
    if (fd >= 0) close(fd);
    if (image_pad(wt, size) < 0) return -1;
  }

  wt->n_files++;
  send_history(wt, rel, fhinfo, st);

  return 0;
}

static int changed_since(const struct stat* st, time_t since)
{
  return since == 0 || st->st_mtime >= since || st->st_ctime >= since;
}

static int backup_tree(struct wrap_tar* wt, const char* abs, const char* rel)
{
  struct stat st;
  struct dirent** list = NULL;
  int n;
  int rc = 0;

  if (lstat(abs, &st) < 0) {
    wrap_log(wt->wccb, "lstat %s failed: %s", abs, strerror(errno));
    wt->n_errors++;
    return 0;
  }

  if (!S_ISDIR(st.st_mode)) {
    if (!changed_since(&st, wt->since)) return 0;
    return backup_entry(wt, abs, rel, &st);
  }

  /* directories are always saved, so incremental images are restorable */
  if (backup_entry(wt, abs, rel, &st) < 0) return -1;

  n = scandir(abs, &list, NULL, alphasort);
  if (n < 0) {
    wrap_log(wt->wccb, "scandir %s failed: %s", abs, strerror(errno));
    wt->n_errors++;
    return 0;
  }

  for (int i = 0; i < n; i++) {
    const char* d = list[i]->d_name;
    char child_abs[WRAP_MAX_PATH];
    char child_rel[WRAP_MAX_PATH];

    if (rc == 0 && strcmp(d, ".") != 0 && strcmp(d, "..") != 0) {
      if ((size_t)snprintf(child_abs, sizeof child_abs, "%s/%s", abs, d)
              >= sizeof child_abs
          || (size_t)snprintf(child_rel, sizeof child_rel, "%s/%s", rel, d)
                 >= sizeof child_rel) {
        wrap_log(wt->wccb, "path too long %s/%s", abs, d);
        wt->n_errors++;
      } else {
        rc = backup_tree(wt, child_abs, child_rel);
      }
    }
    free(list[i]);
  }
  free(list);

  return rc;
}

static int wrap_tar_backup(struct wrap_tar* wt)
{
  struct wrap_ccb* wccb = wt->wccb;
  char root[WRAP_MAX_PATH];
  char* level = wrap_find_env(wccb, "LEVEL");
  char* update = wrap_find_env(wccb, "UPDATE");
  time_t start = time(NULL);
  static const char zeros[2 * TAR_BLOCK];
  size_t len;

  snprintf(root, sizeof root, "%s", wccb->backup_root);
  len = strlen(root);
  while (len > 1 && root[len - 1] == '/') root[--len] = 0;

  if (level) {
    wt->level = atoi(level);
    if (wt->level > 0) wt->since = dumpdates_lookup(root, wt->level);
  }
  wrap_log(wccb, "backup of %s level %d since %lld", root, wt->level,
           (long long)wt->since);

  if (backup_tree(wt, root, "") < 0) {
    wrap_log(wccb, "writing the image failed: %s", strerror(errno));
    return -1;
  }

  if (image_write(wt, zeros, sizeof zeros) < 0) return -1;
  if (wt->offset % TAR_RECORD) {
    static const char pad[TAR_RECORD];
    if (image_write(wt, pad, TAR_RECORD - wt->offset % TAR_RECORD) < 0) {
      return -1;
    }
  }

  if (wt->n_errors) return -1;

  if (level && update && (*update == 'y' || *update == 'Y')) {
    if (dumpdates_update(root, wt->level, start) < 0) {
      wrap_log(wccb, "updating %s failed: %s", dumpdates_file(),
               strerror(errno));
      return -1;
    }
  }

  wrap_log(wccb, "backup of %s done, %llu entries, %llu bytes", root,
           (unsigned long long)wt->n_files, (unsigned long long)wt->offset);
  return 0;
}

/*
 * Recover
 ****************************************************************
 */

static int unsafe_path(const char* rel)
{
  const char* p = rel;

  while ((p = strstr(p, "..")) != NULL) {
    if ((p == rel || p[-1] == '/') && (p[2] == 0 || p[2] == '/')) return 1;
    p += 2;
  }
  return 0;
}

/* map the archive member rel (e.g. "/dir/file") to its restore path */
/*
 * Map the member rel to its destination. Returns the index of the name list
 * entry with the longest matching original name, WRAP_MAX_FILE if there is
 * no name list at all and -1 if the member is not wanted.
 */
static int map_destination(struct wrap_ccb* wccb,
                           const char* rel,
                           char* dest,
                           size_t dest_size)
{
  int best = -1;
  size_t best_len = 0;
  const char* best_suffix = NULL;

  if (wccb->n_file == 0) {
    snprintf(dest, dest_size, "%s%s", wccb->backup_root, rel);
    return WRAP_MAX_FILE;
  }

  for (int i = 0; i < wccb->n_file; i++) {
    char orig[WRAP_MAX_PATH];
    const char* suffix = NULL;
    size_t len;

    snprintf(orig, sizeof orig, "%s", wccb->file[i].original_name);
    len = strlen(orig);
    while (len > 0 && orig[len - 1] == '/') orig[--len] = 0;

    if (len == 0) {
      suffix = rel;
    } else if (orig[0] != '/') {
      if (rel[0] == '/' && strncmp(rel + 1, orig, len) == 0
          && (rel[len + 1] == 0 || rel[len + 1] == '/')) {
        suffix = rel + len + 1;
      }
    } else if (strncmp(rel, orig, len) == 0
               && (rel[len] == 0 || rel[len] == '/')) {
      suffix = rel + len;
    }

    if (suffix && (best < 0 || len > best_len)) {
      best = i;
      best_len = len;
      best_suffix = suffix;
    }
  }
  if (best >= 0) {
    snprintf(dest, dest_size, "%s%s", wccb->file[best].save_to_name,
             best_suffix);
  }
  return best;
}

static int mkdir_p(char* path, mode_t mode)
{
  for (char* p = path + 1; *p; p++) {
    if (*p != '/') continue;
    *p = 0;
    if (mkdir(path, mode) < 0 && errno != EEXIST) {
      *p = '/';
      return -1;
    }
    *p = '/';
  }
  if (mkdir(path, mode) < 0 && errno != EEXIST) return -1;
  return 0;
}

static int make_parent(const char* path)
{
  char parent[WRAP_MAX_PATH];
  char* slash;

  snprintf(parent, sizeof parent, "%s", path);
  slash = strrchr(parent, '/');
  if (!slash || slash == parent) return 0;
  *slash = 0;
  return mkdir_p(parent, 0755);
}

static void set_attributes(const char* path,
                           const struct tar_header* h,
                           int is_link)
{
  struct timeval tv[2];
  uid_t uid = (uid_t)get_number(h->uid, sizeof h->uid);
  gid_t gid = (gid_t)get_number(h->gid, sizeof h->gid);

  if (geteuid() == 0) {
    if (lchown(path, uid, gid) < 0) { /* best effort */
    }
  }
  if (is_link) return;

  if (chmod(path, (mode_t)get_number(h->mode, sizeof h->mode) & 07777) < 0) {
    /* best effort */
  }
  tv[0].tv_sec = tv[1].tv_sec = (time_t)get_number(h->mtime, sizeof h->mtime);
  tv[0].tv_usec = tv[1].tv_usec = 0;
  utimes(path, tv);
}

static int extract_member(struct wrap_tar* wt,
                          const struct tar_header* h,
                          const char* dest,
                          const char* linkname,
                          uint64_t size)
{
  struct wrap_ccb* wccb = wt->wccb;
  char path[WRAP_MAX_PATH];

  snprintf(path, sizeof path, "%s", dest);

  switch (h->typeflag) {
    case '5':
      if (mkdir_p(path, 0755) < 0) goto error;
      set_attributes(path, h, 0);
      return image_skip(wt, padded(size));

    case '2':
      if (make_parent(path) < 0) goto error;
      unlink(path);
      if (symlink(linkname, path) < 0) goto error;
      set_attributes(path, h, 1);
      return image_skip(wt, padded(size));

    case '6':
      if (make_parent(path) < 0) goto error;
      unlink(path);
      if (mkfifo(path, 0600) < 0) goto error;
      set_attributes(path, h, 0);
      return image_skip(wt, padded(size));

    case '0':
    case 0: {
      char buf[64 * 1024];
      uint64_t left = size;
      int fd;

      if (make_parent(path) < 0) goto error;
      unlink(path);
      fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
      if (fd < 0) goto error;
      while (left > 0) {
        size_t n = left > sizeof buf ? sizeof buf : (size_t)left;
        if (image_read(wt, buf, n) < 0 || write_all(fd, buf, n) < 0) {
          close(fd);
          goto error;
        }
        left -= n;
      }
      close(fd);
      set_attributes(path, h, 0);
      return image_skip(wt, padded(size) - size);
    }

    default:
      wrap_log(wccb, "skipping unsupported member type %c for %s",
               h->typeflag, dest);
      return image_skip(wt, padded(size));
  }

error:
  wt->last_errno = errno ? errno : EIO;
  wrap_log(wccb, "restore of %s failed: %s", dest, strerror(wt->last_errno));
  wt->n_extract_errors++;
  return image_skip(wt, padded(size));
}

static char* read_longlink(struct wrap_tar* wt, uint64_t size)
{
  char* buf;

  if (size == 0 || size > WRAP_MAX_PATH) return NULL;
  buf = malloc(padded(size) + 1);
  if (!buf) return NULL;
  if (image_read(wt, buf, padded(size)) < 0) {
    free(buf);
    return NULL;
  }
  buf[size] = 0;
  return buf;
}

static void history_from_header(struct wrap_tar* wt,
                                const struct tar_header* h,
                                const char* rel,
                                uint64_t fhinfo,
                                uint64_t size)
{
  struct stat st;

  memset(&st, 0, sizeof st);
  switch (h->typeflag) {
    case '5':
      st.st_mode = S_IFDIR;
      break;
    case '2':
      st.st_mode = S_IFLNK;
      break;
    case '6':
      st.st_mode = S_IFIFO;
      break;
    default:
      st.st_mode = S_IFREG;
      break;
  }
  st.st_mode |= (mode_t)get_number(h->mode, sizeof h->mode) & 07777;
  st.st_nlink = 1;
  st.st_size = (off_t)size;
  st.st_uid = (uid_t)get_number(h->uid, sizeof h->uid);
  st.st_gid = (gid_t)get_number(h->gid, sizeof h->gid);
  st.st_mtime = st.st_atime = st.st_ctime
      = (time_t)get_number(h->mtime, sizeof h->mtime);
  send_history(wt, rel, fhinfo, &st);
}

static int wrap_tar_recover(struct wrap_tar* wt, int filehist_only)
{
  struct wrap_ccb* wccb = wt->wccb;
  char* longname = NULL;
  char* longlink = NULL;
  uint64_t fhinfo = 0;
  int zero_blocks = 0;
  int rc = 0;

  /* the image arrives through a pipe only after asking the DMA for it */
  if (wccb->index_fp) {
    struct stat st;
    if (fstat(wt->fd, &st) == 0 && S_ISFIFO(st.st_mode)) {
      wrap_send_data_read(wccb->index_fp, 0, WRAP_TAR_LENGTH_INFINITY);
      fflush(wccb->index_fp);
    }
  }

  for (;;) {
    struct tar_header h;
    char name[WRAP_MAX_PATH + 256];
    char rel[WRAP_MAX_PATH + 256 + 1];
    char dest[WRAP_MAX_PATH];
    const char* p;
    uint64_t start = wt->offset;
    uint64_t size;
    size_t len;
    int entry;

    if (image_read(wt, &h, sizeof h) < 0) break;

    if (is_zero_block((const char*)&h)) {
      if (++zero_blocks >= 2) break;
      continue;
    }
    zero_blocks = 0;

    if (get_number(h.chksum, sizeof h.chksum) != header_checksum(&h)) {
      wrap_log(wccb, "bad tar header checksum at offset %llu",
               (unsigned long long)start);
      rc = -1;
      break;
    }

    size = get_number(h.size, sizeof h.size);
    if (!longname && !longlink) fhinfo = start;

    if (h.typeflag == 'L' || h.typeflag == 'K') {
      char* s = read_longlink(wt, size);
      if (!s) {
        rc = -1;
        break;
      }
      if (h.typeflag == 'L') {
        free(longname);
        longname = s;
      } else {
        free(longlink);
        longlink = s;
      }
      continue;
    }
    if (h.typeflag == 'x' || h.typeflag == 'g') {
      if (image_skip(wt, padded(size)) < 0) {
        rc = -1;
        break;
      }
      continue;
    }

    if (longname) {
      snprintf(name, sizeof name, "%s", longname);
    } else if (h.prefix[0]) {
      snprintf(name, sizeof name, "%.*s/%.*s", (int)sizeof h.prefix, h.prefix,
               (int)sizeof h.name, h.name);
    } else {
      snprintf(name, sizeof name, "%.*s", (int)sizeof h.name, h.name);
    }

    /* normalize "./a/b/" to "/a/b", the root becomes "" */
    p = name;
    if (p[0] == '.' && (p[1] == '/' || p[1] == 0)) p++;
    while (p[0] == '/' && p[1] == '/') p++;
    if (*p && *p != '/') {
      snprintf(rel, sizeof rel, "/%s", p);
    } else {
      snprintf(rel, sizeof rel, "%s", p);
    }
    len = strlen(rel);
    while (len > 0 && rel[len - 1] == '/') rel[--len] = 0;

    if (h.typeflag == '1' || h.typeflag == '3' || h.typeflag == '4') {
      /* hard links and devices are never created by our backup */
      wrap_log(wccb, "skipping unsupported member type %c for %s", h.typeflag,
               rel);
      if (image_skip(wt, padded(h.typeflag == '1' ? 0 : size)) < 0) rc = -1;
    } else if (unsafe_path(rel)) {
      wrap_log(wccb, "refusing unsafe path %s", rel);
      wt->n_errors++;
      if (image_skip(wt, padded(size)) < 0) rc = -1;
    } else if (filehist_only) {
      history_from_header(wt, &h, rel, fhinfo, size);
      if (image_skip(wt, padded(size)) < 0) rc = -1;
    } else if ((entry = map_destination(wccb, rel, dest, sizeof dest)) >= 0) {
      char link[WRAP_MAX_PATH];
      int errors = wt->n_extract_errors;

      if (longlink) {
        snprintf(link, sizeof link, "%s", longlink);
      } else {
        snprintf(link, sizeof link, "%.*s", (int)sizeof h.linkname,
                 h.linkname);
      }
      if (extract_member(wt, &h, dest, link, size) < 0) rc = -1;
      wt->n_files++;
      if (entry < WRAP_MAX_FILE) {
        wt->file_seen[entry] = 1;
        if (wt->n_extract_errors != errors && !wt->file_errno[entry]) {
          wt->file_errno[entry] = wt->last_errno;
        }
      }
    } else {
      if (image_skip(wt, padded(size)) < 0) rc = -1;
    }

    free(longname);
    free(longlink);
    longname = longlink = NULL;
    if (rc < 0) break;
  }

  free(longname);
  free(longlink);

  /*
   * Consume the record padding that is already buffered, but do not wait
   * for more: the mover pauses at the end of the image and never closes
   * the stream.
   */
  {
    char buf[64 * 1024];
    int flags = fcntl(wt->fd, F_GETFL);
    if (flags >= 0 && fcntl(wt->fd, F_SETFL, flags | O_NONBLOCK) == 0) {
      while (read(wt->fd, buf, sizeof buf) > 0) {}
    }
  }

  if (rc < 0) wrap_log(wccb, "reading the image failed");

  /* report the result of every name list entry (NDMP_LOG_FILE) */
  if (!filehist_only) {
    for (int i = 0; i < wccb->n_file; i++) {
      int err = wt->file_errno[i];
      if (!err && !wt->file_seen[i]) err = rc < 0 ? EIO : ENOENT;
      if (err) {
        wrap_log(wccb, "recovery of %s failed: %s", wccb->file[i].original_name,
                 strerror(err));
      }
      wrap_send_recovery_result(wccb->index_fp, err,
                                wccb->file[i].original_name);
    }
  }

  /* without a name list there is nobody to report extraction errors to */
  if (wt->n_errors || (wccb->n_file == 0 && wt->n_extract_errors)) rc = -1;

  wrap_log(wccb, "recover done, %llu entries", (unsigned long long)wt->n_files);
  return rc;
}

int main(int argc, char* argv[])
{
  struct wrap_ccb wccb;
  struct wrap_tar wt;
  int rc;

  if (wrap_main(argc, argv, &wccb) != 0) {
    fprintf(stderr, "%s: %s\n", argv[0], wccb.errmsg);
    return 2;
  }

  memset(&wt, 0, sizeof wt);
  wt.wccb = &wccb;
  wt.fd = wccb.data_conn_fd;

  switch (wccb.op) {
    case WRAP_CCB_OP_BACKUP:
      rc = wrap_tar_backup(&wt);
      break;
    case WRAP_CCB_OP_RECOVER:
      rc = wrap_tar_recover(&wt, 0);
      break;
    case WRAP_CCB_OP_RECOVER_FILEHIST:
      rc = wrap_tar_recover(&wt, 1);
      break;
    default:
      rc = -1;
      break;
  }

  if (wccb.index_fp) fclose(wccb.index_fp);

  if (rc < 0) {
    fprintf(stderr, "%s: operation failed\n", argv[0]);
    return 1;
  }
  return 0;
}
