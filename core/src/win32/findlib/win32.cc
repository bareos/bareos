/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
/**
 * @file
 * Windows specific functions.
 */

#include "bareos.h"

#if defined(HAVE_WIN32)

#include "jcr.h"
#include "find.h"
#include "lib/cbuf.h"
#include "findlib/drivetype.h"
#include "findlib/fstype.h"
#include "win32/findlib/win32.h"

/**
 * We need to analyze if a fileset contains onefs=no as option, because only then
 * we need to snapshot submounted vmps
 */
bool win32_onefs_is_disabled(findFILESET *fileset)
{
   findIncludeExcludeItem *incexe;

   for (int i = 0; i < fileset->include_list.size(); i++) {
      incexe = (findIncludeExcludeItem *)fileset->include_list.get(i);
      /*
       * Look through all files and check
       */
      for (int j = 0; j < incexe->opts_list.size(); j++) {
         findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);
         if (bit_is_set(FO_MULTIFS, fo->flags)) {
            return true;
         }
      }
   }

   return false;
}

/**
 * For VSS we need to know which windows drives are used, because we create a snapshot
 * of all used drives. This function returns the number of used drives and fills
 * szDrives with up to 26 (A..Z) drive names.
 */
int get_win32_driveletters(findFILESET *fileset, char *szDrives)
{
   int i;
   int nCount;
   char *fname, ch;
   char drive[4], dt[16];
   struct stat sb;
   dlistString *node;
   findIncludeExcludeItem *incexe;

   /*
    * szDrives must be at least 27 bytes long
    * Can be already filled by plugin, so check that all
    *   letters are in upper case. There should be no duplicates.
    */
   for (nCount = 0; nCount < 27 && szDrives[nCount]; nCount++) {
      szDrives[nCount] = toupper(szDrives[nCount]);
   }

   /*
    * First check if there are any non fixed drives in the list
    * filled by the plugin. VSS can only snapshot fixed drives.
    */
   for (nCount = 0; nCount < 27 && szDrives[nCount]; nCount++) {
      bsnprintf(drive, sizeof(drive), "%c:\\", szDrives[nCount]);
      if (drivetype(drive, dt, sizeof(dt))) {
         if (bstrcmp(dt, "fixed")) {
            continue;
         }

         /*
          * Inline copy the rest of the string over the current
          * drive letter.
          */
         bstrinlinecpy(szDrives + nCount, szDrives + nCount + 1);
      }
   }

   if (fileset) {
      for (i = 0; i < fileset->include_list.size(); i++) {
         incexe = (findIncludeExcludeItem *)fileset->include_list.get(i);

         /*
          * Look through all files and check
          */
         foreach_dlist(node, &incexe->name_list) {
            fname = node->c_str();

            /*
             * See if the entry doesn't have the FILE_ATTRIBUTE_VOLUME_MOUNT_POINT flag set.
             */
            if (stat(fname, &sb) == 0) {
               if (sb.st_rdev & FILE_ATTRIBUTE_VOLUME_MOUNT_POINT) {
                  continue;
               }
            }

            /*
             * fname should match x:/
             */
            if (strlen(fname) >= 2 && B_ISALPHA(fname[0]) && fname[1] == ':') {
               /*
                * VSS can only snapshot fixed drives.
                */
               bstrncpy(drive, fname, sizeof(drive));
               drive[2] = '\\';
               drive[3] = '\0';

               /*
                * Lookup the drive type.
                */
               if (drivetype(drive, dt, sizeof(dt))) {
                  if (bstrcmp(dt, "fixed")) {
                     /*
                      * Always add in uppercase
                      */
                     ch = toupper(fname[0]);

                     /*
                      * If not found in string, add drive letter
                      */
                     if (!strchr(szDrives, ch)) {
                        szDrives[nCount] = ch;
                        szDrives[nCount + 1] = 0;
                        nCount++;
                     }
                  }
               }
            }
         }
      }
   }

   return nCount;
}

/**
 * For VSS we need to know which windows virtual mountpoints are used, because we create
 * a snapshot of all used drives and virtual mountpoints. This function returns the number
 * of used virtual mountpoints and fills szVmps with a list of all virtual mountpoints.
 */
int get_win32_virtualmountpoints(findFILESET *fileset, dlist **szVmps)
{
   int i, cnt;
   char *fname;
   struct stat sb;
   dlistString *node;
   findIncludeExcludeItem *incexe;
   POOLMEM *devicename;

   cnt = 0;
   if (fileset) {
      devicename = get_pool_memory(PM_FNAME);
      for (i = 0; i < fileset->include_list.size(); i++) {
         incexe = (findIncludeExcludeItem *)fileset->include_list.get(i);
         /*
          * Look through all files and check
          */
         foreach_dlist(node, &incexe->name_list) {
            fname = node->c_str();

            /*
             * See if the entry has the FILE_ATTRIBUTE_VOLUME_MOUNT_POINT flag set.
             */
            if (stat(fname, &sb) == 0) {
               if (!(sb.st_rdev & FILE_ATTRIBUTE_VOLUME_MOUNT_POINT)) {
                  continue;
               }
            } else {
               continue;
            }

            if (win32_get_vmp_devicename(fname, devicename)) {
               /*
                * See if we need to allocate a new dlist.
                */
               if (!cnt) {
                  if (!*szVmps) {
                     *szVmps = (dlist *)malloc(sizeof(dlist));
                     (*szVmps)->init();
                  }
               }

               (*szVmps)->append(new_dlistString(devicename));
               cnt++;
            }
         }
      }
      free_pool_memory(devicename);
   }

   return cnt;
}

static inline bool wanted_drive_type(const char *drive, findIncludeExcludeItem *incexe)
{
   int i,j;
   char dt[16];
   findFOPTS *fo;
   bool done = false;
   bool wanted = true;

   /*
    * Lookup the drive type.
    */
   if (!drivetype(drive, dt, sizeof(dt))) {
      return false;
   }

   /*
    * We start the loop with done set to false and wanted
    * to true so when there are no drivetype selections we
    * select any drivetype.
    */
   for (i = 0; !done && i < incexe->opts_list.size(); i++) {
      fo = (findFOPTS *)incexe->opts_list.get(i);

      /*
       * If there is any drivetype selection set the default
       * selection to false.
       */
      if (fo->drivetype.size()) {
         wanted = false;
      }

      for (j = 0; !done && j < fo->drivetype.size(); j++) {
         if (bstrcasecmp(dt, (char *)fo->drivetype.get(j))) {
            wanted = true;
            done = true;
         }
      }
   }

   return wanted;
}

bool expand_win32_fileset(findFILESET *fileset)
{
   int i;
   char *bp;
   dlistString *node;
   findIncludeExcludeItem *incexe;
   char drives[MAX_NAME_LENGTH];

   for (i = 0; i < fileset->include_list.size(); i++) {
      incexe = (findIncludeExcludeItem *)fileset->include_list.get(i);
      foreach_dlist(node, &incexe->name_list) {
         Dmsg1(100, "Checking %s\n", node->c_str());
         if (bstrcmp(node->c_str(), "/")) {
            /*
             * Request for auto expansion but no support for it.
             */
            if (!p_GetLogicalDriveStringsA) {
               return false;
            }

            /*
             * we want to add all available local drives to our fileset
             * if we have "/" specified in the fileset. We want to remove
             * the "/" pattern itself that gets expanded into all
             * available drives.
             */
            incexe->name_list.remove(node);
            if (p_GetLogicalDriveStringsA(sizeof(drives), drives) != 0) {
               bp = drives;
               while (bp && strlen(bp) > 0) {
                  /*
                   * Apply any drivetype selection to the currently
                   * processed item.
                   */
                  if (wanted_drive_type(bp, incexe)) {
                     if (*(bp + 2) == '\\') {
                        *(bp + 2) = '/';       /* 'x:\' -> 'x:/' */
                     }
                     Dmsg1(100, "adding drive %s\n", bp);
                     incexe->name_list.append(new_dlistString(bp));
                  }
                  if ((bp = strchr(bp, '\0'))) {
                     bp++;
                  }
               }
            } else {
               return false;
            }

            /*
             * No need to search further in the include list when we have
             * found what we were looking for.
             */
            break;
         }
      }
   }
   return true;
}

static inline int count_include_list_file_entries(FindFilesPacket *ff)
{
   int cnt = 0;
   findFILESET *fileset;
   findIncludeExcludeItem *incexe;

   fileset = ff->fileset;
   if (fileset) {
      for (int i = 0; i < fileset->include_list.size(); i++) {
         incexe = (findIncludeExcludeItem *)fileset->include_list.get(i);
         cnt += incexe->name_list.size();
      }
   }

   return cnt;
}

/**
 * Automatically exclude all files and paths defined in Registry Key
 * "SYSTEM\\CurrentControlSet\\Control\\BackupRestore\\FilesNotToBackup"
 *
 * Files/directories with wildcard characters are added to an
 * options block with flags "exclude=yes" as "wild=dir/file".
 * This options block is created and prepended by us.
 *
 */

#define MAX_VALUE_NAME 16383
#define REGISTRY_KEY "SYSTEM\\CurrentControlSet\\Control\\BackupRestore\\FilesNotToBackup"

bool exclude_win32_not_to_backup_registry_entries(JobControlRecord *jcr, FindFilesPacket *ff)
{
   bool retval = false;
   uint32_t wild_count = 0;
   DWORD retCode;
   HKEY hKey;

   /*
    * If we do not have "File = " directives (e.g. only plugin calls)
    * we do not create excludes for the NotForBackup RegKey
    */
   if (count_include_list_file_entries(ff) == 0 ) {
      Qmsg(jcr, M_INFO, 1, _("Fileset has no \"File=\" directives, ignoring FilesNotToBackup Registry key\n"));
      return true;
   }

   /*
    * If autoexclude is set to no in fileset options somewhere, we do not
    * automatically exclude files from FilesNotToBackup Registry Key
    */
   for (int i = 0; i < ff->fileset->include_list.size(); i++) {
      findIncludeExcludeItem *incexe = (findIncludeExcludeItem *)ff->fileset->include_list.get(i);

      for (int j = 0; j < incexe->opts_list.size(); j++) {
         findFOPTS *fo = (findFOPTS *)incexe->opts_list.get(j);

         if (bit_is_set(FO_NO_AUTOEXCL, fo->flags)) {
            Qmsg(jcr, M_INFO, 1, _("Fileset has autoexclude disabled, ignoring FilesNotToBackup Registry key\n"));
            return true;
         }
      }
   }

   retCode = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT(REGISTRY_KEY), 0, KEY_READ, &hKey);
   if (retCode == ERROR_SUCCESS ) {
      PoolMem achClass(PM_MESSAGE),
               achValue(PM_MESSAGE),
               dwKeyEn(PM_MESSAGE),
               expandedKey(PM_MESSAGE),
               destination(PM_MESSAGE);
      DWORD cchClassName;            /* Size of class string */
      DWORD cSubKeys = 0;            /* Number of subkeys */
      DWORD cbMaxSubKey;             /* Longest subkey size */
      DWORD cchMaxClass;             /* Longest class string */
      DWORD cValues;                 /* Number of values for key */
      DWORD cchMaxValue;             /* Longest value name */
      DWORD cbMaxValueData;          /* Longest value data */
      DWORD cbSecurityDescriptor;    /* Size of security descriptor */
      FILETIME ftLastWriteTime;      /* Last write time */
      DWORD cchValue;                /* Size of value string */
      findIncludeExcludeItem *include;

      /*
       * Make sure the variable are big enough to contain the data.
       */
      achClass.check_size(MAX_PATH);

      /*
       * Get the class name and the value count.
       */
      cchClassName = achClass.size();
      retCode = RegQueryInfoKey(hKey,                  /* Key handle */
                                achClass.c_str(),      /* Buffer for class name */
                                &cchClassName,         /* Size of class string */
                                NULL,                  /* Reserved */
                                &cSubKeys,             /* Number of subkeys */
                                &cbMaxSubKey,          /* Longest subkey size */
                                &cchMaxClass,          /* Longest class string */
                                &cValues,              /* Number of values for this key */
                                &cchMaxValue,          /* Longest value name */
                                &cbMaxValueData,       /* Longest value data */
                                &cbSecurityDescriptor, /* Security descriptor */
                                &ftLastWriteTime);     /* Last write time */

      if (cValues) {
         findFOPTS *fo;

         /*
          * Prepare include block to do exclusion via wildcards in options
          */
         new_preinclude(ff->fileset);

         include = (findIncludeExcludeItem*)ff->fileset->include_list.get(0);

         if (include->opts_list.size() == 0) {
            /*
             * Create new options block in include block for the wildcard excludes
             */
            Dmsg0(100, "prepending new options block\n");
            new_options(ff, ff->fileset->incexe);
         } else {
            Dmsg0(100, "reusing existing options block\n");
         }
         fo = (findFOPTS *)include->opts_list.get(0);
         set_bit(FO_EXCLUDE, fo->flags);               /* exclude = yes */
         set_bit(FO_IGNORECASE, fo->flags);            /* ignore case = yes */

         /*
          * Make sure the variables are big enough to contain the data.
          */
         achValue.check_size(MAX_VALUE_NAME);
         dwKeyEn.check_size(MAX_PATH);
         expandedKey.check_size(MAX_PATH);
         destination.check_size(MAX_PATH);

         for (unsigned int i = 0; i < cValues; i++) {
            pm_strcpy(achValue, "");
            cchValue = achValue.size();
            retCode = RegEnumValue(hKey, i, achValue.c_str(), &cchValue, NULL, NULL, NULL, NULL);

            if (retCode == ERROR_SUCCESS ){
               DWORD dwLen;

               Dmsg2(100 , "(%d) \"%s\" : \n", i + 1, achValue.c_str());

               dwLen = dwKeyEn.size();
               retCode = RegQueryValueEx(hKey, achValue.c_str(), 0, NULL, (LPBYTE)dwKeyEn.c_str(), &dwLen);
               if (retCode == ERROR_SUCCESS) {
                  char *lpValue;

                  /*
                   * Iterate over each string, expand the %xxx% variables and
                   * process them for addition to exclude block or wildcard options block
                   */
                  for (lpValue = dwKeyEn.c_str();
                       lpValue && *lpValue;
                       lpValue = strchr(lpValue, '\0') + 1) {
                     char *s, *d;

                     ExpandEnvironmentStrings(lpValue, expandedKey.c_str(), expandedKey.size());
                     Dmsg1(100, "        \"%s\"\n", expandedKey.c_str());

                     /*
                      * Never add single character entries. Probably known buggy DRM Entry in Windows XP / 2003
                      */
                     if (strlen(expandedKey.c_str()) <= 1) {
                        Dmsg0(100, TEXT("Single character entry ignored. Probably reading buggy DRM entry on Windows XP/2003 SP 1 and later\n"));
                     } else {
                        pm_strcpy(destination, "");

                        /*
                         * Do all post processing.
                         * - Replace '\' by '/'
                         * - Strip any trailing /s patterns.
                         * - Check if wildcards are used.
                         */
                        s = expandedKey.c_str();
                        d = destination.c_str();

                        /*
                         * Begin with \ means all drives
                         */
                        if (*s == '\\') {
                           d += pm_strcpy(destination, "[A-Z]:");
                        }

                        while (*s) {
                           switch (*s) {
                              case '\\':
                                 *d++ = '/';
                                 s++;
                                 break;
                              case ' ':
                                 if (bstrcmp(s, " /s")) {
                                    *d = '\0';
                                    *s = '\0';
                                    continue;
                                 }
                                 /* FALLTHROUGH */
                              default:
                                 *d++ = *s++;
                                 break;
                           }
                        }
                        *d = '\0';
                        Dmsg1(100, "    ->  \"%s\"\n", destination.c_str() );
                        fo->wild.append(bstrdup(destination.c_str()));
                        wild_count++;
                     }
                  }
               } else {
                  Dmsg0(100, TEXT("RegGetValue failed \n"));
               }
            }
         }
      }

      Qmsg(jcr, M_INFO, 0, _("Created %d wildcard excludes from FilesNotToBackup Registry key\n"), wild_count);
      RegCloseKey(hKey);
      retval = true;
   } else {
      Qmsg(jcr, M_ERROR, 0, _("Failed to open FilesNotToBackup Registry Key\n"));
   }

   return retval;
}

/**
 * Windows specific code for restoring EFS data.
 */
struct CopyThreadSaveData {
   uint32_t data_len;                     /* Length of Data */
   POOLMEM *data;                         /* Data */
};

struct CopyThreadContext {
   BareosWinFilePacket *bfd;                            /* Filehandle */
   int nr_save_elements;                  /* Number of save items in save_data */
   CopyThreadSaveData *save_data;        /* To save data (cached structure build during restore) */
   circbuf *cb;                           /* Circular buffer for passing work to copy thread */
   bool started;                          /* Copy thread consuming data */
   bool flushed;                          /* Copy thread flushed data */
   pthread_t thread_id;                   /* Id of copy thread */
   pthread_mutex_t lock;                  /* Lock the structure */
   pthread_cond_t start;                  /* Start consuming data from the Circular buffer */
   pthread_cond_t flush;                  /* Flush data from the Circular buffer */
};

/**
 * Callback method for WriteEncryptedFileRaw()
 */
static DWORD WINAPI receive_efs_data(PBYTE pbData, PVOID pvCallbackContext, PULONG ulLength)
{
   CopyThreadSaveData *save_data;
   CopyThreadContext *context = (CopyThreadContext *)pvCallbackContext;

   /*
    * Dequeue an item from the circular buffer.
    */
   save_data = (CopyThreadSaveData *)context->cb->dequeue();

   if (save_data) {
      if (save_data->data_len > *ulLength) {
         Dmsg2(100, "Restore of data bigger then allowed EFS buffer %d vs %d\n", save_data->data_len, *ulLength);
         *ulLength = 0;
      } else {
         memcpy(pbData, save_data->data, save_data->data_len);
         *ulLength = save_data->data_len;
      }
   } else {
      *ulLength = 0;
   }

   return ERROR_SUCCESS;
}

/**
 * Copy thread cancel handler.
 */
static void copy_cleanup_thread(void *data)
{
   CopyThreadContext *context = (CopyThreadContext *)data;

   pthread_mutex_unlock(&context->lock);
}

/**
 * Actual copy thread that restores EFS data.
 */
static void *copy_thread(void *data)
{
   CopyThreadContext *context = (CopyThreadContext *)data;

   if (pthread_mutex_lock(&context->lock) != 0) {
      goto bail_out;
   }

   /*
    * When we get canceled make sure we run the cleanup function.
    */
   pthread_cleanup_push(copy_cleanup_thread, data);

   while (1) {
      /*
       * Wait for the moment we are supposed to start.
       * We are signalled by the restore thread.
       */
      pthread_cond_wait(&context->start, &context->lock);
      context->started = true;

      pthread_mutex_unlock(&context->lock);

      if (p_WriteEncryptedFileRaw((PFE_IMPORT_FUNC)receive_efs_data, context, context->bfd->pvContext)) {
         goto bail_out;
      }

      /*
       * Need to synchronize the main thread and this one so the main thread cannot miss the conditional signal.
       */
      if (pthread_mutex_lock(&context->lock) != 0) {
         goto bail_out;
      }

      /*
       * Signal the main thread we flushed the data and the BFD can be closed.
       */
      pthread_cond_signal(&context->flush);

      context->started = false;
      context->flushed = true;

   }

   pthread_cleanup_pop(1);

bail_out:
   return NULL;
}

/**
 * Create a copy thread that restores the EFS data.
 */
static inline bool setup_copy_thread(JobControlRecord *jcr, BareosWinFilePacket *bfd)
{
   int nr_save_elements;
   CopyThreadContext *new_context;

   new_context = (CopyThreadContext *)malloc(sizeof(CopyThreadContext));
   new_context->started = false;
   new_context->flushed = false;
   new_context->cb = New(circbuf);

   nr_save_elements = new_context->cb->capacity();
   new_context->save_data = (CopyThreadSaveData *)malloc(nr_save_elements * sizeof(CopyThreadSaveData));
   memset(new_context->save_data, 0, nr_save_elements * sizeof(CopyThreadSaveData));
   new_context->nr_save_elements = nr_save_elements;

   if (pthread_mutex_init(&new_context->lock, NULL) != 0) {
      goto bail_out;
   }

   if (pthread_cond_init(&new_context->start, NULL) != 0) {
      pthread_mutex_destroy(&new_context->lock);
      goto bail_out;
   }

   if (pthread_create(&new_context->thread_id, NULL, copy_thread, (void *)new_context) != 0) {
      pthread_cond_destroy(&new_context->start);
      pthread_mutex_destroy(&new_context->lock);
      goto bail_out;
   }

   jcr->cp_thread = new_context;
   return true;

bail_out:

   free(new_context->save_data);
   delete new_context->cb;
   free(new_context);

   return false;
}

/**
 * Send data to the copy thread that restores EFS data.
 */
int win32_send_to_copy_thread(JobControlRecord *jcr, BareosWinFilePacket *bfd, char *data, const int32_t length)
{
   circbuf *cb;
   int slotnr;
   CopyThreadSaveData *save_data;

   if (!p_WriteEncryptedFileRaw) {
      Jmsg0(jcr, M_FATAL, 0, _("Encrypted file restore but no EFS support functions\n"));
   }

   /*
    * If no copy thread started do it now.
    */
   if (!jcr->cp_thread) {
      if (!setup_copy_thread(jcr, bfd)) {
         Jmsg0(jcr, M_FATAL, 0, _("Failed to start encrypted data restore copy thread\n"));
         return -1;
      }
   }
   cb = jcr->cp_thread->cb;

   /*
    * See if the bfd changed.
    */
   if (jcr->cp_thread->bfd != bfd) {
      jcr->cp_thread->bfd = bfd;
   }

   /*
    * Find out which next slot will be used on the Circular Buffer.
    * The method will block when the circular buffer is full until a slot is available.
    */
   slotnr = cb->next_slot();
   save_data = &jcr->cp_thread->save_data[slotnr];

   /*
    * If this is the first time we use this slot we need to allocate some memory.
    */
   if (!save_data->data) {
      save_data->data = get_pool_memory(PM_BSOCK);
   }
   save_data->data = check_pool_memory_size(save_data->data, length + 1);
   memcpy(save_data->data, data, length);
   save_data->data_len = length;

   cb->enqueue(save_data);

   /*
    * Signal the copy thread its time to start if it didn't start yet.
    */
   if (!jcr->cp_thread->started) {
      pthread_cond_signal(&jcr->cp_thread->start);
   }

   return length;
}

/**
 * Flush the copy thread so we can close the BFD.
 */
void win32_flush_copy_thread(JobControlRecord *jcr)
{
   CopyThreadContext *context = jcr->cp_thread;

   if (pthread_mutex_lock(&context->lock) != 0) {
      return;
   }

   /*
    * In essence the flush should work in one shot but be a bit more conservative.
    */
   while (!context->flushed) {
      /*
       * Tell the copy thread to flush out all data.
       */
      context->cb->flush();

      /*
       * Wait for the copy thread to say it flushed the data out.
       */
      pthread_cond_wait(&context->flush, &context->lock);
   }

   context->flushed = false;

   pthread_mutex_unlock(&context->lock);
}

/**
 * Cleanup all data allocated for the copy thread.
 */
void win32_cleanup_copy_thread(JobControlRecord *jcr)
{
   int slotnr;

   /*
    * Stop the copy thread.
    */
   if (!pthread_equal(jcr->cp_thread->thread_id, pthread_self())) {
      pthread_cancel(jcr->cp_thread->thread_id);
      pthread_join(jcr->cp_thread->thread_id, NULL);
   }

   /*
    * Free all data allocated along the way.
    */
   for (slotnr = 0; slotnr < jcr->cp_thread->nr_save_elements; slotnr++) {
      if (jcr->cp_thread->save_data[slotnr].data) {
         free_pool_memory(jcr->cp_thread->save_data[slotnr].data);
      }
   }
   free(jcr->cp_thread->save_data);
   delete jcr->cp_thread->cb;
   free(jcr->cp_thread);
   jcr->cp_thread = NULL;
}
#endif
