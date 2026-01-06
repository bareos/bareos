/*
   BAREOS® - Backup Archiving REcovery Open Sourced

Copyright (C) 2023-2026 Bareos GmbH & Co. KG

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

#include "fd_sendfileset.h"

#include "lib/bsock_tcp.h"
#include "lib/util.h"
#include "lib/berrno.h"
#include "lib/bnet.h"
#include "dird/director_jcr_impl.h"
#include "lib/bpipe.h"

namespace directordaemon {

static bool SendListItem(JobControlRecord* jcr,
                         const char* code,
                         char* item,
                         BareosSocket* fd)
{
  Bpipe* bpipe;
  FILE* ffd;
  char buf[2000];
  int optlen, status;
  char* p = item;

  switch (*p) {
    case '|':
      p++; /* skip over the | */
      fd->msg = edit_job_codes(jcr, fd->msg, p, "");
      bpipe = OpenBpipe(fd->msg, 0, "r");
      if (!bpipe) {
        BErrNo be;
        Jmsg(jcr, M_FATAL, 0, T_("Cannot run program: %s. ERR=%s\n"), p,
             be.bstrerror());
        return false;
      }
      bstrncpy(buf, code, sizeof(buf));
      Dmsg1(500, "code=%s\n", buf);
      optlen = strlen(buf);
      while (fgets(buf + optlen, sizeof(buf) - optlen, bpipe->rfd)) {
        fd->message_length = Mmsg(fd->msg, "%s", buf);
        Dmsg2(500, "Inc/exc len=%d: %s", fd->message_length, fd->msg);
        if (!BnetSend(fd)) {
          Jmsg(jcr, M_FATAL, 0, T_(">filed: write error on socket\n"));
          return false;
        }
      }
      if ((status = CloseBpipe(bpipe)) != 0) {
        BErrNo be;
        Jmsg(jcr, M_FATAL, 0, T_("Error running program: %s. ERR=%s\n"), p,
             be.bstrerror(status));
        return false;
      }
      break;
    case '<':
      p++; /* skip over < */
      if ((ffd = fopen(p, "rb")) == NULL) {
        BErrNo be;
        Jmsg(jcr, M_FATAL, 0, T_("Cannot open included file: %s. ERR=%s\n"), p,
             be.bstrerror());
        return false;
      }
      bstrncpy(buf, code, sizeof(buf));
      Dmsg1(500, "code=%s\n", buf);
      optlen = strlen(buf);
      while (fgets(buf + optlen, sizeof(buf) - optlen, ffd)) {
        fd->message_length = Mmsg(fd->msg, "%s", buf);
        if (!BnetSend(fd)) {
          Jmsg(jcr, M_FATAL, 0, T_(">filed: write error on socket\n"));
          fclose(ffd);
          return false;
        }
      }
      fclose(ffd);
      break;
    case '\\':
      p++; /* skip over \ */
      [[fallthrough]];
    default:
      PmStrcpy(fd->msg, code);
      fd->message_length = PmStrcat(fd->msg, p);
      Dmsg1(500, "Inc/Exc name=%s\n", fd->msg);
      if (!fd->send()) {
        Jmsg(jcr, M_FATAL, 0, T_(">filed: write error on socket\n"));
        return false;
      }
      break;
  }
  return true;
}


static void SendFilesetOptions(JobControlRecord* jcr,
                               IncludeExcludeItem* include_exclude_item)
{
  StorageResource* store = jcr->dir_impl->res.write_storage;
  BareosSocket* fd = jcr->file_bsock;

  for (auto file_option : include_exclude_item->file_options_list) {
    bool enhanced_wild = false;
    if (strchr(file_option->opts, 'W')) { enhanced_wild = true; }

    // Strip out compression option Zn if disallowed for this Storage
    if (store && !store->AllowCompress) {
      char newopts[sizeof(file_option->opts)];
      bool done = false; /* print warning only if compression enabled in FS */
      int l = 0;

      for (int k = 0; file_option->opts[k] != '\0'; k++) {
        /* Z compress option is followed by the single-digit compress level
         * or 'o' For fastlz its Zf with a single char selecting the actual
         * compression algo. */
        if (file_option->opts[k] == 'Z' && file_option->opts[k + 1] == 'f') {
          done = true;
          k += 2; /* skip option */
        } else if (file_option->opts[k] == 'Z') {
          done = true;
          k++; /* skip option and level */
        } else {
          newopts[l] = file_option->opts[k];
          l++;
        }
      }
      newopts[l] = '\0';

      if (done) {
        Jmsg(jcr, M_INFO, 0,
             T_("FD compression disabled for this Job because "
                "AllowCompress=No in Storage resource.\n"));
      }

      // Send the new trimmed option set without overwriting fo->opts
      fd->fsend("O %s\n", newopts);
    } else {
      // Send the original options
      fd->fsend("O %s\n", file_option->opts);
    }

    for (int k = 0; k < file_option->regex.size(); k++) {
      fd->fsend("R %s\n", file_option->regex.get(k));
    }

    for (int k = 0; k < file_option->regexdir.size(); k++) {
      fd->fsend("RD %s\n", file_option->regexdir.get(k));
    }

    for (int k = 0; k < file_option->regexfile.size(); k++) {
      fd->fsend("RF %s\n", file_option->regexfile.get(k));
    }

    for (int k = 0; k < file_option->wild.size(); k++) {
      fd->fsend("W %s\n", file_option->wild.get(k));
    }

    for (int k = 0; k < file_option->wilddir.size(); k++) {
      fd->fsend("WD %s\n", file_option->wilddir.get(k));
    }

    for (int k = 0; k < file_option->wildfile.size(); k++) {
      fd->fsend("WF %s\n", file_option->wildfile.get(k));
    }

    for (int k = 0; k < file_option->wildbase.size(); k++) {
      fd->fsend("W%c %s\n", enhanced_wild ? 'B' : 'F',
                file_option->wildbase.get(k));
    }

    for (int k = 0; k < file_option->base.size(); k++) {
      fd->fsend("B %s\n", file_option->base.get(k));
    }

    for (int k = 0; k < file_option->fstype.size(); k++) {
      fd->fsend("X %s\n", file_option->fstype.get(k));
    }

    for (int k = 0; k < file_option->Drivetype.size(); k++) {
      fd->fsend("XD %s\n", file_option->Drivetype.get(k));
    }

    if (file_option->plugin) { fd->fsend("G %s\n", file_option->plugin); }

    if (file_option->reader) { fd->fsend("D %s\n", file_option->reader); }

    if (file_option->writer) { fd->fsend("T %s\n", file_option->writer); }

    fd->fsend("N\n");
  }
}

static bool SendFilesetFileAndPlugin(JobControlRecord* jcr,
                                     IncludeExcludeItem* ie)
{
  BareosSocket* fd = jcr->file_bsock;
  char* item;
  for (int j = 0; j < ie->name_list.size(); j++) {
    item = (char*)ie->name_list.get(j);
    if (!SendListItem(jcr, "F ", item, fd)) { return false; }
  }
  fd->fsend("N\n");

  for (int j = 0; j < ie->plugin_list.size(); j++) {
    item = (char*)ie->plugin_list.get(j);
    if (!SendListItem(jcr, "P ", item, fd)) { return false; }
  }
  fd->fsend("N\n");
  return true;
}

static void SendFilesetIgnoredir(JobControlRecord* jcr,
                                 IncludeExcludeItem* include_exclude_item)
{
  BareosSocket* fd = jcr->file_bsock;
  for (int j = 0; j < include_exclude_item->ignoredir.size(); j++) {
    fd->fsend("Z %s\n", include_exclude_item->ignoredir.get(j));
  }
}

bool SendIncludeExcludeItems(JobControlRecord* jcr, FilesetResource* fileset)
{
  BareosSocket* fd = jcr->file_bsock;

  for (auto include_item : fileset->include_items) {
    fd->fsend("I\n");
    SendFilesetIgnoredir(jcr, include_item);
    SendFilesetOptions(jcr, include_item);
    if (!SendFilesetFileAndPlugin(jcr, include_item)) { return false; }
  }

  for (auto exclude_item : fileset->exclude_items) {
    fd->fsend("E\n");
    SendFilesetIgnoredir(jcr, exclude_item);
    SendFilesetOptions(jcr, exclude_item);
    if (!SendFilesetFileAndPlugin(jcr, exclude_item)) { return false; }
  }

  return true;
}
}  // namespace directordaemon
