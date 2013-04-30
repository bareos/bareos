/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2006-2012 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*                               -*- Mode: C -*-
 * vss.h --
 */
//
// Copyright transferred from MATRIX-Computer GmbH to
//   Kern Sibbald by express permission.
/*
 *
 * Author          : Thorsten Engel
 * Created On      : Fri May 06 21:44:00 2006 
 */

#ifndef __VSS_H_
#define __VSS_H_

#ifndef b_errno_win32
#define b_errno_win32 (1<<29)
#endif
 
#ifdef WIN32_VSS

#define VSS_INIT_RESTORE_AFTER_INIT   1
#define VSS_INIT_RESTORE_AFTER_GATHER 2

// some forward declarations
struct IVssAsync;

class VSSClient
{
public:
    VSSClient();
    virtual ~VSSClient();

    // Backup Process
    bool InitializeForBackup(JCR *jcr);
    bool InitializeForRestore(JCR *jcr);
    virtual bool CreateSnapshots(char* szDriveLetters) = 0;
    virtual bool CloseBackup() = 0;
    virtual bool CloseRestore() = 0;
    virtual WCHAR *GetMetadata() = 0;
    virtual const char* GetDriverName() = 0;
    bool GetShadowPath  (const char* szFilePath, char* szShadowPath, int nBuflen);
    bool GetShadowPathW (const wchar_t* szFilePath, wchar_t* szShadowPath, int nBuflen); /* nBuflen in characters */

    const size_t GetWriterCount();
    const char* GetWriterInfo(int nIndex);
    const int   GetWriterState(int nIndex);
    void DestroyWriterInfo();
    void AppendWriterInfo(int nState, const char* pszInfo);
    const bool  IsInitialized() { return m_bBackupIsInitialized; };
    HMODULE GetVssDllHandle() { return m_hLib; };
    IUnknown *GetVssObject() { return m_pVssObject; };
         
private:
    virtual bool Initialize(DWORD dwContext, bool bDuringRestore = FALSE) = 0;
    virtual bool WaitAndCheckForAsyncOperation(IVssAsync*  pAsync) = 0;
    virtual void QuerySnapshotSet(GUID snapshotSetID) = 0;

protected:
    HMODULE    m_hLib;
    JCR       *m_jcr;

    DWORD      m_dwContext;

    IUnknown*  m_pVssObject;
    GUID       m_uidCurrentSnapshotSet;

    // drive A will be stored on position 0,Z on pos. 25
    wchar_t    m_wszUniqueVolumeName[26][MAX_PATH]; // approx. 7 KB
    wchar_t    m_szShadowCopyName[26][MAX_PATH]; // approx. 7 KB
    
    alist     *m_pAlistWriterState;
    alist     *m_pAlistWriterInfoText;

    bool       m_bCoInitializeCalled;
    bool       m_bCoInitializeSecurityCalled;
    bool       m_bDuringRestore;  /* true if we are doing a restore */
    bool       m_bBackupIsInitialized;
    bool       m_bWriterStatusCurrent;

    WCHAR     *m_metadata;
};

class VSSClientXP:public VSSClient
{
public:
   VSSClientXP();
   virtual ~VSSClientXP();
   virtual bool CreateSnapshots(char* szDriveLetters);
   virtual bool CloseBackup();
   virtual bool CloseRestore();
   virtual WCHAR *GetMetadata();
#ifdef _WIN64
   virtual const char* GetDriverName() { return "Win64 VSS"; };
#else
   virtual const char* GetDriverName() { return "Win32 VSS"; };
#endif
private:
   virtual bool Initialize(DWORD dwContext, bool bDuringRestore);
   virtual bool WaitAndCheckForAsyncOperation(IVssAsync* pAsync);
   virtual void QuerySnapshotSet(GUID snapshotSetID);
   bool CheckWriterStatus();   
};

class VSSClient2003:public VSSClient
{
public:
   VSSClient2003();
   virtual ~VSSClient2003();
   virtual bool CreateSnapshots(char* szDriveLetters);
   virtual bool CloseBackup();   
   virtual bool CloseRestore();
   virtual WCHAR *GetMetadata();
#ifdef _WIN64
   virtual const char* GetDriverName() { return "Win64 VSS"; };
#else
   virtual const char* GetDriverName() { return "Win32 VSS"; };
#endif
private:
   virtual bool Initialize(DWORD dwContext, bool bDuringRestore);
   virtual bool WaitAndCheckForAsyncOperation(IVssAsync*  pAsync);
   virtual void QuerySnapshotSet(GUID snapshotSetID);
   bool CheckWriterStatus();
};

class VSSClientVista:public VSSClient
{
public:
   VSSClientVista();
   virtual ~VSSClientVista();
   virtual bool CreateSnapshots(char* szDriveLetters);
   virtual bool CloseBackup();   
   virtual bool CloseRestore();
   virtual WCHAR *GetMetadata();
#ifdef _WIN64
   virtual const char* GetDriverName() { return "Win64 VSS"; };
#else
   virtual const char* GetDriverName() { return "Win32 VSS"; };
#endif
private:
   virtual bool Initialize(DWORD dwContext, bool bDuringRestore);
   virtual bool WaitAndCheckForAsyncOperation(IVssAsync*  pAsync);
   virtual void QuerySnapshotSet(GUID snapshotSetID);
   bool CheckWriterStatus();
};


extern VSSClient *g_pVSSClient;

BOOL VSSPathConvert(const char *szFilePath, char *szShadowPath, int nBuflen);
BOOL VSSPathConvertW(const wchar_t *szFilePath, wchar_t *szShadowPath, int nBuflen);

#endif /* WIN32_VSS */

#endif /* __VSS_H_ */
