/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, October MM
 * Extracted from other source files by Marco van Wieringen, September 2012
 */
/**
 * @file
 * Bareos File Daemon Include Exclude pattern handling.
 */

#include "bareos.h"
#include "filed.h"
#include "ch.h"

/* Forward referenced functions */
static int set_options(findFOPTS *fo, const char *opts);

/**
 * callback function for edit_job_codes
 * See ../lib/util.c, function edit_job_codes, for more remaining codes
 *
 * %D = Director
 * %m = Modification time (for incremental and differential)
 */
extern "C" char *job_code_callback_filed(JCR *jcr, const char* param)
{
   static char str[50];

   switch (param[0]) {
   case 'D':
      if (jcr->director) {
         return jcr->director->name();
      }
      break;
   case 'm':
      return edit_uint64(jcr->mtime, str);
   }

   return NULL;
}

bool init_fileset(JCR *jcr)
{
   FF_PKT *ff;
   findFILESET *fileset;

   if (!jcr->ff) {
      return false;
   }
   ff = jcr->ff;
   if (ff->fileset) {
      return false;
   }
   fileset = (findFILESET *)malloc(sizeof(findFILESET));
   memset(fileset, 0, sizeof(findFILESET));
   ff->fileset = fileset;
   fileset->state = state_none;
   fileset->include_list.init(1, true);
   fileset->exclude_list.init(1, true);
   return true;
}

static void append_file(JCR *jcr, findINCEXE *incexe,
                        const char *buf, bool is_file)
{
   if (is_file) {
      /*
       * Sanity check never append empty file patterns.
       */
      if (strlen(buf) > 0) {
         incexe->name_list.append(new_dlistString(buf));
      }
   } else if (me->plugin_directory) {
      generate_plugin_event(jcr, bEventPluginCommand, (void *)buf);
      incexe->plugin_list.append(new_dlistString(buf));
   } else {
      Jmsg(jcr, M_FATAL, 0,
           _("Plugin Directory not defined. Cannot use plugin: \"%s\"\n"),
           buf);
   }
}

/**
 * Add fname to include/exclude fileset list. First check for
 * | and < and if necessary perform command.
 */
void add_file_to_fileset(JCR *jcr, const char *fname, bool is_file)
{
   findFILESET *fileset = jcr->ff->fileset;
   char *p;
   BPIPE *bpipe;
   POOLMEM *fn;
   FILE *ffd;
   char buf[1000];
   int ch;
   int status;

   p = (char *)fname;
   ch = (uint8_t)*p;
   switch (ch) {
   case '|':
      p++;                            /* skip over | */
      fn = get_pool_memory(PM_FNAME);
      fn = edit_job_codes(jcr, fn, p, "", job_code_callback_filed);
      bpipe = open_bpipe(fn, 0, "r");
      if (!bpipe) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0, _("Cannot run program: %s. ERR=%s\n"),
            p, be.bstrerror());
         free_pool_memory(fn);
         return;
      }
      free_pool_memory(fn);
      while (fgets(buf, sizeof(buf), bpipe->rfd)) {
         strip_trailing_junk(buf);
         append_file(jcr, fileset->incexe, buf, is_file);
      }
      if ((status = close_bpipe(bpipe)) != 0) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0, _("Error running program: %s. status=%d: ERR=%s\n"),
            p, be.code(status), be.bstrerror(status));
         return;
      }
      break;
   case '<':
      Dmsg1(100, "Doing < of '%s' include on client.\n", p + 1);
      p++;                      /* skip over < */
      if ((ffd = fopen(p, "rb")) == NULL) {
         berrno be;
         Jmsg(jcr, M_FATAL, 0,
              _("Cannot open FileSet input file: %s. ERR=%s\n"),
            p, be.bstrerror());
         return;
      }
      while (fgets(buf, sizeof(buf), ffd)) {
         strip_trailing_junk(buf);
         append_file(jcr, fileset->incexe, buf, is_file);
      }
      fclose(ffd);
      break;
   default:
      append_file(jcr, fileset->incexe, fname, is_file);
      break;
   }
}

findINCEXE *get_incexe(JCR *jcr)
{
   if (jcr->ff && jcr->ff->fileset) {
      return jcr->ff->fileset->incexe;
   }

   return NULL;
}

void set_incexe(JCR *jcr, findINCEXE *incexe)
{
   findFILESET *fileset = jcr->ff->fileset;
   fileset->incexe = incexe;
}

/**
 * Add a regex to the current fileset
 */
int add_regex_to_fileset(JCR *jcr, const char *item, int type)
{
   findFOPTS *current_opts = start_options(jcr->ff);
   regex_t *preg;
   int rc;
   char prbuf[500];

   preg = (regex_t *)malloc(sizeof(regex_t));
   if (bit_is_set(FO_IGNORECASE, current_opts->flags)) {
      rc = regcomp(preg, item, REG_EXTENDED|REG_ICASE);
   } else {
      rc = regcomp(preg, item, REG_EXTENDED);
   }
   if (rc != 0) {
      regerror(rc, preg, prbuf, sizeof(prbuf));
      regfree(preg);
      free(preg);
      Jmsg(jcr, M_FATAL, 0, _("REGEX %s compile error. ERR=%s\n"), item, prbuf);
      return state_error;
   }
   if (type == ' ') {
      current_opts->regex.append(preg);
   } else if (type == 'D') {
      current_opts->regexdir.append(preg);
   } else if (type == 'F') {
      current_opts->regexfile.append(preg);
   } else {
      return state_error;
   }

   return state_options;
}

/**
 * Add a wild card to the current fileset
 */
int add_wild_to_fileset(JCR *jcr, const char *item, int type)
{
   findFOPTS *current_opts = start_options(jcr->ff);

   if (type == ' ') {
      current_opts->wild.append(bstrdup(item));
   } else if (type == 'D') {
      current_opts->wilddir.append(bstrdup(item));
   } else if (type == 'F') {
      current_opts->wildfile.append(bstrdup(item));
   } else if (type == 'B') {
      current_opts->wildbase.append(bstrdup(item));
   } else {
      return state_error;
   }

   return state_options;
}

/**
 * Add options to the current fileset
 */
int add_options_to_fileset(JCR *jcr, const char *item)
{
   findFOPTS *current_opts = start_options(jcr->ff);

   set_options(current_opts, item);

   return state_options;
}

void add_fileset(JCR *jcr, const char *item)
{
   FF_PKT *ff = jcr->ff;
   findFILESET *fileset = ff->fileset;
   int code, subcode;
   int state = fileset->state;
   findFOPTS *current_opts;

   /*
    * Get code, optional subcode, and position item past the dividing space
    */
   Dmsg1(100, "%s\n", item);
   code = item[0];
   if (code != '\0') {
      ++item;
   }

   subcode = ' ';                    /* A space is always a valid subcode */
   if (item[0] != '\0' && item[0] != ' ') {
      subcode = item[0];
      ++item;
   }

   if (*item == ' ') {
      ++item;
   }

   /*
    * Skip all lines we receive after an error
    */
   if (state == state_error) {
      Dmsg0(100, "State=error return\n");
      return;
   }

   /**
    * The switch tests the code for validity.
    * The subcode is always good if it is a space, otherwise we must confirm.
    * We set state to state_error first assuming the subcode is invalid,
    * requiring state to be set in cases below that handle subcodes.
    */
   if (subcode != ' ') {
      state = state_error;
      Dmsg0(100, "Set state=error or double code.\n");
   }
   switch (code) {
   case 'I':
      (void)new_include(jcr->ff->fileset);
      break;
   case 'E':
      (void)new_exclude(jcr->ff->fileset);
      break;
   case 'N':                             /* Null */
      state = state_none;
      break;
   case 'F':                             /* File */
      state = state_include;
      add_file_to_fileset(jcr, item, true);
      break;
   case 'P':                             /* Plugin */
      state = state_include;
      add_file_to_fileset(jcr, item, false);
      break;
   case 'R':                             /* Regex */
      state = add_regex_to_fileset(jcr, item, subcode);
      break;
   case 'B':
      current_opts = start_options(ff);
      current_opts->base.append(bstrdup(item));
      state = state_options;
      break;
   case 'X':                             /* Filetype or Drive type */
      current_opts = start_options(ff);
      state = state_options;
      if (subcode == ' ') {
         current_opts->fstype.append(bstrdup(item));
      } else if (subcode == 'D') {
         current_opts->drivetype.append(bstrdup(item));
      } else {
         state = state_error;
      }
      break;
   case 'W':                             /* Wild cards */
      state = add_wild_to_fileset(jcr, item, subcode);
      break;
   case 'O':                             /* Options */
      state = add_options_to_fileset(jcr, item);
      break;
   case 'Z':                             /* Ignore dir */
      state = state_include;
      fileset->incexe->ignoredir.append(bstrdup(item));
      break;
   case 'D':
      current_opts = start_options(ff);
      state = state_options;
      break;
   case 'T':
      current_opts = start_options(ff);
      state = state_options;
      break;
   case 'G':                             /* Plugin command for this Option block */
      current_opts = start_options(ff);
      current_opts->plugin = bstrdup(item);
      state = state_options;
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Invalid FileSet command: %s\n"), item);
      state = state_error;
      break;
   }
   ff->fileset->state = state;
}

bool term_fileset(JCR *jcr)
{
   findFILESET *fileset;

   fileset = jcr->ff->fileset;
#ifdef HAVE_WIN32
   /*
    * Expand the fileset to include all drive letters when the fileset includes a File = / entry.
    */
   if (!expand_win32_fileset(jcr->ff->fileset)) {
      return false;
   }

   /*
    * Exclude entries in NotToBackup registry key
    */
   if (!exclude_win32_not_to_backup_registry_entries(jcr, jcr->ff)) {
      return false;
   }
#endif

#ifdef xxx_DEBUG_CODE
   for (int i = 0; i < fileset->include_list.size(); i++) {
      dlistString *node;
      findINCEXE *incexe = (findINCEXE *)fileset->include_list.get(i);

      Dmsg0(400, "I\n");
      for (int j = 0; j < incexe->opts_list.size(); j++) {
         findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);

         for (int k = 0; k < fo->regex.size(); k++) {
            Dmsg1(400, "R %s\n", (char *)fo->regex.get(k));
         }
         for (int k = 0; k < fo->regexdir.size(); k++) {
            Dmsg1(400, "RD %s\n", (char *)fo->regexdir.get(k));
         }
         for (int k = 0; k < fo->regexfile.size(); k++) {
            Dmsg1(400, "RF %s\n", (char *)fo->regexfile.get(k));
         }
         for (int k = 0; k < fo->wild.size(); k++) {
            Dmsg1(400, "W %s\n", (char *)fo->wild.get(k));
         }
         for (int k = 0; k < fo->wilddir.size(); k++) {
            Dmsg1(400, "WD %s\n", (char *)fo->wilddir.get(k));
         }
         for (int k = 0; k < fo->wildfile.size(); k++) {
            Dmsg1(400, "WF %s\n", (char *)fo->wildfile.get(k));
         }
         for (int k = 0; k < fo->wildbase.size(); k++) {
            Dmsg1(400, "WB %s\n", (char *)fo->wildbase.get(k));
         }
         for (int k = 0; k < fo->base.size(); k++) {
            Dmsg1(400, "B %s\n", (char *)fo->base.get(k));
         }
         for (int k = 0; k < fo->fstype.size(); k++) {
            Dmsg1(400, "X %s\n", (char *)fo->fstype.get(k));
         }
         for (int k = 0; k < fo->drivetype.size(); k++) {
            Dmsg1(400, "XD %s\n", (char *)fo->drivetype.get(k));
         }
         if (fo->plugin) {
            Dmsg1(400, "G %s\n", (char *)fo->plugin);
         }
      }

      for (int k = 0; k < incexe->ignoredir.size(); k++) {
         Dmsg1(400, "Z %s\n", (char *)incexe->ignoredir.get(k));
      }

      foreach_dlist(node, &incexe->name_list) {
         Dmsg1(400, "F %s\n", node->c_str());
      }

      foreach_dlist(node, &incexe->plugin_list) {
         Dmsg1(400, "P %s\n", node->c_str());
      }
   }
   for (int i = 0; i < fileset->exclude_list.size(); i++) {
      findINCEXE *incexe = (findINCEXE *)fileset->exclude_list.get(i);

      Dmsg0(400, "E\n");
      for (int j = 0; j < incexe->opts_list.size(); j++) {
         findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);
         for (int k = 0; k < fo->regex.size(); k++) {
            Dmsg1(400, "R %s\n", (char *)fo->regex.get(k));
         }
         for (int k = 0; k < fo->regexdir.size(); k++) {
            Dmsg1(400, "RD %s\n", (char *)fo->regexdir.get(k));
         }
         for (int k = 0; k < fo->regexfile.size(); k++) {
            Dmsg1(400, "RF %s\n", (char *)fo->regexfile.get(k));
         }
         for (int k = 0; k < fo->wild.size(); k++) {
            Dmsg1(400, "W %s\n", (char *)fo->wild.get(k));
         }
         for (int k = 0; k < fo->wilddir.size(); k++) {
            Dmsg1(400, "WD %s\n", (char *)fo->wilddir.get(k));
         }
         for (int k = 0; k < fo->wildfile.size(); k++) {
            Dmsg1(400, "WF %s\n", (char *)fo->wildfile.get(k));
         }
         for (int k = 0; k < fo->wildbase.size(); k++) {
            Dmsg1(400, "WB %s\n", (char *)fo->wildbase.get(k));
         }
         for (int k = 0; k<fo->base.size(); k++) {
            Dmsg1(400, "B %s\n", (char *)fo->base.get(k));
         }
         for (int k = 0; k < fo->fstype.size(); k++) {
            Dmsg1(400, "X %s\n", (char *)fo->fstype.get(k));
         }
         for (int k = 0; k < fo->drivetype.size(); k++) {
            Dmsg1(400, "XD %s\n", (char *)fo->drivetype.get(k));
         }
      }
      dlistString *node;
      foreach_dlist(node, &incexe->name_list) {
         Dmsg1(400, "F %s\n", node->c_str());
      }
      foreach_dlist(node, &incexe->plugin_list) {
         Dmsg1(400, "P %s\n", node->c_str());
      }
   }
#endif

   /*
    * Generate bEventPluginCommand events for each Options Plugin.
    */
   for (int i = 0; i < fileset->include_list.size(); i++) {
      findINCEXE *incexe = (findINCEXE *)fileset->include_list.get(i);

      for (int j = 0; j < incexe->opts_list.size(); j++) {
         findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);

         if (fo->plugin) {
            generate_plugin_event(jcr, bEventPluginCommand, (void *)fo->plugin);
         }
      }
   }

   return jcr->ff->fileset->state != state_error;
}

/**
 * As an optimization, we should do this during
 *  "compile" time in filed/job.c, and keep only a bit mask
 *  and the Verify options.
 */
static int set_options(findFOPTS *fo, const char *opts)
{
   int j;
   const char *p;
   char strip[21];
   char size[50];

   for (p = opts; *p; p++) {
      switch (*p) {
      case 'A':
         set_bit(FO_ACL, fo->flags);
         break;
      case 'a':                          /* Alway replace */
      case '0':                          /* No option */
         break;
      case 'C':                          /* Accurate options */
         /*
          * Copy Accurate Options
          */
         for (j = 0; *p && *p != ':'; p++) {
            fo->AccurateOpts[j] = *p;
            if (j < (int)sizeof(fo->AccurateOpts) - 1) {
               j++;
            }
         }
         fo->AccurateOpts[j] = 0;
         break;
      case 'c':
         set_bit(FO_CHKCHANGES, fo->flags);
         break;
      case 'd':
         switch(*(p + 1)) {
         case '1':
            fo->shadow_type = check_shadow_local_warn;
            p++;
            break;
         case '2':
            fo->shadow_type = check_shadow_local_remove;
            p++;
            break;
         case '3':
            fo->shadow_type = check_shadow_global_warn;
            p++;
            break;
         case '4':
            fo->shadow_type = check_shadow_global_remove;
            p++;
            break;
         }
         break;
      case 'e':
         set_bit(FO_EXCLUDE, fo->flags);
         break;
      case 'E':                          /* Encryption */
         switch(*(p + 1)) {
         case '3':
            fo->Encryption_cipher = CRYPTO_CIPHER_3DES_CBC;
            p++;
            break;
         case 'a':
            switch(*(p + 2)) {
            case '1':
               fo->Encryption_cipher = CRYPTO_CIPHER_AES_128_CBC;
               p += 2;
               break;
            case '2':
               fo->Encryption_cipher = CRYPTO_CIPHER_AES_192_CBC;
               p += 2;
               break;
            case '3':
               fo->Encryption_cipher = CRYPTO_CIPHER_AES_256_CBC;
               p += 2;
               break;
            }
            break;
         case 'b':
            fo->Encryption_cipher = CRYPTO_CIPHER_BLOWFISH_CBC;
            p++;
            break;
         case 'c':
            switch(*(p + 2)) {
            case '1':
               fo->Encryption_cipher = CRYPTO_CIPHER_CAMELLIA_128_CBC;
               p += 2;
               break;
            case '2':
               fo->Encryption_cipher = CRYPTO_CIPHER_CAMELLIA_192_CBC;
               p += 2;
               break;
            case '3':
               fo->Encryption_cipher = CRYPTO_CIPHER_CAMELLIA_256_CBC;
               p += 2;
               break;
            }
            break;
         case 'f':
            set_bit(FO_FORCE_ENCRYPT, fo->flags);
            p++;
            break;
         case 'h':
            switch(*(p + 2)) {
            case '1':
               fo->Encryption_cipher = CRYPTO_CIPHER_AES_128_CBC_HMAC_SHA1;
               p += 2;
               break;
            case '2':
               fo->Encryption_cipher = CRYPTO_CIPHER_AES_256_CBC_HMAC_SHA1;
               p += 2;
               break;
            }
         }
         break;
      case 'f':
         set_bit(FO_MULTIFS, fo->flags);
         break;
      case 'H':                          /* No hard link handling */
         set_bit(FO_NO_HARDLINK, fo->flags);
         break;
      case 'h':                          /* No recursion */
         set_bit(FO_NO_RECURSION, fo->flags);
         break;
      case 'i':
         set_bit(FO_IGNORECASE, fo->flags);
         break;
      case 'J':                         /* Basejob options */
         /*
          * Copy BaseJob Options
          */
         for (j = 0; *p && *p != ':'; p++) {
            fo->BaseJobOpts[j] = *p;
            if (j < (int)sizeof(fo->BaseJobOpts) - 1) {
               j++;
            }
         }
         fo->BaseJobOpts[j] = 0;
         break;
      case 'K':
         set_bit(FO_NOATIME, fo->flags);
         break;
      case 'k':
         set_bit(FO_KEEPATIME, fo->flags);
         break;
      case 'M':                         /* MD5 */
         set_bit(FO_MD5, fo->flags);
         break;
      case 'm':
         set_bit(FO_MTIMEONLY, fo->flags);
         break;
      case 'N':
         set_bit(FO_HONOR_NODUMP, fo->flags);
         break;
      case 'n':
         set_bit(FO_NOREPLACE, fo->flags);
         break;
      case 'P':                         /* Strip path */
         /*
          * Get integer
          */
         p++;                           /* skip P */
         for (j = 0; *p && *p != ':'; p++) {
            strip[j] = *p;
            if (j < (int)sizeof(strip) - 1) {
               j++;
            }
         }
         strip[j] = 0;
         fo->strip_path = atoi(strip);
         set_bit(FO_STRIPPATH, fo->flags);
         Dmsg2(100, "strip=%s strip_path=%d\n", strip, fo->strip_path);
         break;
      case 'p':                         /* Use portable data format */
         set_bit(FO_PORTABLE, fo->flags);
         break;
      case 'R':                         /* Resource forks and Finder Info */
         set_bit(FO_HFSPLUS, fo->flags);
         break;
      case 'r':                         /* Read fifo */
         set_bit(FO_READFIFO, fo->flags);
         break;
      case 'S':
         switch(*(p + 1)) {
         case '1':
            set_bit(FO_SHA1, fo->flags);
            p++;
            break;
#ifdef HAVE_SHA2
         case '2':
            set_bit(FO_SHA256, fo->flags);
            p++;
            break;
         case '3':
            set_bit(FO_SHA512, fo->flags);
            p++;
            break;
#endif
         default:
            /*
             * If 2 or 3 is seen here, SHA2 is not configured, so eat the option, and drop back to SHA-1.
             */
            if (p[1] == '2' || p[1] == '3') {
               p++;
            }
            set_bit(FO_SHA1, fo->flags);
            break;
         }
         break;
      case 's':
         set_bit(FO_SPARSE, fo->flags);
         break;
      case 'V':                         /* verify options */
         /*
          * Copy Verify Options
          */
         for (j = 0; *p && *p != ':'; p++) {
            fo->VerifyOpts[j] = *p;
            if (j < (int)sizeof(fo->VerifyOpts) - 1) {
               j++;
            }
         }
         fo->VerifyOpts[j] = 0;
         break;
      case 'W':
         set_bit(FO_ENHANCEDWILD, fo->flags);
         break;
      case 'w':
         set_bit(FO_IF_NEWER, fo->flags);
         break;
      case 'x':
         set_bit(FO_NO_AUTOEXCL, fo->flags);
         break;
      case 'X':
         set_bit(FO_XATTR, fo->flags);
         break;
      case 'Z':                         /* Compression */
         p++;                           /* Skip Z */
         if (*p >= '0' && *p <= '9') {
            set_bit(FO_COMPRESS, fo->flags);
            fo->Compress_algo = COMPRESS_GZIP;
            fo->Compress_level = *p - '0';
         } else if (*p == 'o') {
            set_bit(FO_COMPRESS, fo->flags);
            fo->Compress_algo = COMPRESS_LZO1X;
            fo->Compress_level = 1;     /* not used with LZO */
         } else if (*p == 'f') {
	    p++;
	    if (*p == 'f') {
               set_bit(FO_COMPRESS, fo->flags);
               fo->Compress_algo = COMPRESS_FZFZ;
               fo->Compress_level = 1;     /* not used with FZFZ */
            } else if (*p == '4') {
               set_bit(FO_COMPRESS, fo->flags);
               fo->Compress_algo = COMPRESS_FZ4L;
               fo->Compress_level = 1;     /* not used with FZ4L */
            } else if (*p == 'h') {
               set_bit(FO_COMPRESS, fo->flags);
               fo->Compress_algo = COMPRESS_FZ4H;
               fo->Compress_level = 1;     /* not used with FZ4H */
            }
         }
         break;
      case 'z':                         /* Min, max or approx size or size range */
         p++;                           /* Skip z */
         for (j = 0; *p && *p != ':'; p++) {
            size[j] = *p;
            if (j < (int)sizeof(size) - 1) {
               j++;
            }
         }
         size[j] = 0;
         if (!fo->size_match) {
            fo->size_match = (struct s_sz_matching *)malloc(sizeof(struct s_sz_matching));
         }
         if (!parse_size_match(size, fo->size_match)) {
            Emsg1(M_ERROR, 0, _("Unparseable size option: %s\n"), size);
         }
         break;
      default:
         Emsg1(M_ERROR, 0, _("Unknown include/exclude option: %c\n"), *p);
         break;
      }
   }

   return state_options;
}
