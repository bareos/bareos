/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2015 Bareos GmbH & Co. KG

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
 Copyright transferred from MATRIX-Computer GmbH to
   Kern Sibbald by express permission.
 Author          : Thorsten Engel
 Created On      : Fri May 06 21:44:00 2005
*/
/* @file
 Interface to Volume Shadow Copies (VSS)
*/

#ifdef WIN32_VSS

#include "bareos.h"
#include "filed.h"

#include "ms_atl.h"
#include <objbase.h>

/*
 * { b5946137-7b9f-4925-af80-51abd60b20d5 }
 */
static const GUID VSS_SWPRV_ProviderID = {
   0xb5946137, 0x7b9f, 0x4925, { 0xaf, 0x80, 0x51, 0xab, 0xd6, 0x0b, 0x20, 0xd5 }
};

static bool VSSPathConvert(const char *szFilePath, char *szShadowPath, int nBuflen)
{
   JCR *jcr = get_jcr_from_tsd();

   if (jcr && jcr->pVSSClient) {
      return jcr->pVSSClient->GetShadowPath(szFilePath, szShadowPath, nBuflen);
   }

   return false;
}

static bool VSSPathConvertW(const wchar_t *szFilePath, wchar_t *szShadowPath, int nBuflen)
{
   JCR *jcr = get_jcr_from_tsd();

   if (jcr && jcr->pVSSClient) {
      return jcr->pVSSClient->GetShadowPathW(szFilePath, szShadowPath, nBuflen);
   }

   return false;
}

void VSSInit(JCR *jcr)
{
   /*
    * Decide which vss class to initialize
    */
   if (g_MajorVersion == 5) {
      switch (g_MinorVersion) {
      case 1:
         jcr->pVSSClient = new VSSClientXP();
         break;
      case 2:
         jcr->pVSSClient = new VSSClient2003();
         break;
      }
   /*
    * Vista or Longhorn or later
    */
   } else if (g_MajorVersion >= 6) {
      jcr->pVSSClient = new VSSClientVista();
   }

   /*
    * Setup the callback functions.
    */
   if (!SetVSSPathConvert(VSSPathConvert, VSSPathConvertW)) {
      Jmsg(jcr, M_FATAL, 0, "Failed to setup VSS Path Conversion callbacks.\n");
   }
}

/*
 * Constructor
 */
VSSClient::VSSClient()
{
    memset(this, 0, sizeof(VSSClient));
    m_pAlistWriterState = New(alist(10, not_owned_by_alist));
    m_pAlistWriterInfoText = New(alist(10, owned_by_alist));
    m_uidCurrentSnapshotSet = GUID_NULL;
}

/*
 * Destructor
 */
VSSClient::~VSSClient()
{
   /*
    * Release the IVssBackupComponents interface
    * WARNING: this must be done BEFORE calling CoUninitialize()
    */
   if (m_pVssObject) {
//      m_pVssObject->Release();
      m_pVssObject = NULL;
   }

   DestroyWriterInfo();
   delete m_pAlistWriterState;
   delete m_pAlistWriterInfoText;

   /*
    * Call CoUninitialize if the CoInitialize was performed successfully
    */
   if (m_bCoInitializeCalled) {
      CoUninitialize();
   }
}

bool VSSClient::InitializeForBackup(JCR *jcr)
{
   m_jcr = jcr;

   generate_plugin_event(jcr, bEventVssInitializeForBackup);

   return Initialize(0);
}

bool VSSClient::InitializeForRestore(JCR *jcr)
{
   m_metadata = NULL;
   m_jcr = jcr;

   generate_plugin_event(jcr, bEventVssInitializeForRestore);

   return Initialize(0, true /*=>Restore*/);
}

bool VSSClient::GetShadowPath(const char *szFilePath, char *szShadowPath, int nBuflen)
{
   if (!m_bBackupIsInitialized)
      return false;

   /*
    * Check for valid pathname
    */
   bool bIsValidName;

   bIsValidName = strlen(szFilePath) > 3;
   if (bIsValidName)
      bIsValidName &= isalpha (szFilePath[0]) &&
                      szFilePath[1]==':' &&
                      szFilePath[2] == '\\';

   if (bIsValidName) {
      int nDriveIndex = toupper(szFilePath[0])-'A';
      if (m_szShadowCopyName[nDriveIndex][0] != 0) {

         if (WideCharToMultiByte(CP_UTF8,0,m_szShadowCopyName[nDriveIndex],-1,szShadowPath,nBuflen-1,NULL,NULL)) {
            nBuflen -= (int)strlen(szShadowPath);
            bstrncat(szShadowPath, szFilePath+2, nBuflen);
            return true;
         }
      }
   }

   bstrncpy(szShadowPath, szFilePath, nBuflen);
   errno = EINVAL;
   return false;
}

bool VSSClient::GetShadowPathW(const wchar_t *szFilePath, wchar_t *szShadowPath, int nBuflen)
{
   if (!m_bBackupIsInitialized)
      return false;

   /*
    * Check for valid pathname
    */
   bool bIsValidName;

   bIsValidName = wcslen(szFilePath) > 3;
   if (bIsValidName)
      bIsValidName &= iswalpha (szFilePath[0]) &&
                      szFilePath[1]==':' &&
                      szFilePath[2] == '\\';

   if (bIsValidName) {
      int nDriveIndex = towupper(szFilePath[0])-'A';
      if (m_szShadowCopyName[nDriveIndex][0] != 0) {
         wcsncpy(szShadowPath, m_szShadowCopyName[nDriveIndex], nBuflen);
         nBuflen -= (int)wcslen(m_szShadowCopyName[nDriveIndex]);
         wcsncat(szShadowPath, szFilePath+2, nBuflen);
         return true;
      }
   }

   wcsncpy(szShadowPath, szFilePath, nBuflen);
   errno = EINVAL;
   return false;
}

const size_t VSSClient::GetWriterCount()
{
   return m_pAlistWriterInfoText->size();
}

const char* VSSClient::GetWriterInfo(int nIndex)
{
   return (char*)m_pAlistWriterInfoText->get(nIndex);
}

const int VSSClient::GetWriterState(int nIndex)
{
   void *item = m_pAlistWriterState->get(nIndex);

   /*
    * Eliminate compiler warnings
    */
#ifdef HAVE_VSS64
   return (int64_t)(char *)item;
#else
   return (int)(char *)item;
#endif
}

void VSSClient::AppendWriterInfo(int nState, const char* pszInfo)
{
   m_pAlistWriterInfoText->push(bstrdup(pszInfo));
   m_pAlistWriterState->push((void*)nState);
}

/*
 * Note, this is called at the end of every job, so release all
 * the items in the alists, but do not delete the alist.
 */
void VSSClient::DestroyWriterInfo()
{
   while (!m_pAlistWriterInfoText->empty()) {
      free(m_pAlistWriterInfoText->pop());
   }

   while (!m_pAlistWriterState->empty()) {
      m_pAlistWriterState->pop();
   }
}
#endif
