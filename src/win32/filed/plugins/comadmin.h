/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the w64 mingw-runtime package.
 * No warranty is given; refer to the file DISCLAIMER within this package.
 */
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error This stub requires an updated version of <rpcndr.h>
#endif

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif

#ifndef __comadmin_h__
#define __comadmin_h__

#ifndef __ICOMAdminCatalog_FWD_DEFINED__
#define __ICOMAdminCatalog_FWD_DEFINED__
typedef struct ICOMAdminCatalog ICOMAdminCatalog;
#endif

#ifndef __ICOMAdminCatalog2_FWD_DEFINED__
#define __ICOMAdminCatalog2_FWD_DEFINED__
typedef struct ICOMAdminCatalog2 ICOMAdminCatalog2;
#endif

#ifndef __ICatalogObject_FWD_DEFINED__
#define __ICatalogObject_FWD_DEFINED__
typedef struct ICatalogObject ICatalogObject;
#endif

#ifndef __ICatalogCollection_FWD_DEFINED__
#define __ICatalogCollection_FWD_DEFINED__
typedef struct ICatalogCollection ICatalogCollection;
#endif

#ifndef __COMAdminCatalog_FWD_DEFINED__
#define __COMAdminCatalog_FWD_DEFINED__
#ifdef __cplusplus
typedef class COMAdminCatalog COMAdminCatalog;
#else
typedef struct COMAdminCatalog COMAdminCatalog;
#endif
#endif

#ifndef __COMAdminCatalogObject_FWD_DEFINED__
#define __COMAdminCatalogObject_FWD_DEFINED__
#ifdef __cplusplus
typedef class COMAdminCatalogObject COMAdminCatalogObject;
#else
typedef struct COMAdminCatalogObject COMAdminCatalogObject;
#endif
#endif

#ifndef __COMAdminCatalogCollection_FWD_DEFINED__
#define __COMAdminCatalogCollection_FWD_DEFINED__
#ifdef __cplusplus
typedef class COMAdminCatalogCollection COMAdminCatalogCollection;
#else
typedef struct COMAdminCatalogCollection COMAdminCatalogCollection;
#endif
#endif

#include "unknwn.h"
#include "oaidl.h"

#ifdef __cplusplus
extern "C"{
#endif

  void *__RPC_API MIDL_user_allocate(size_t);
  void __RPC_API MIDL_user_free(void *);

#include <objbase.h>

  extern RPC_IF_HANDLE __MIDL_itf_comadmin_0000_v0_0_c_ifspec;
  extern RPC_IF_HANDLE __MIDL_itf_comadmin_0000_v0_0_s_ifspec;

#ifndef __ICOMAdminCatalog_INTERFACE_DEFINED__
#define __ICOMAdminCatalog_INTERFACE_DEFINED__
  EXTERN_C const IID IID_ICOMAdminCatalog;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct ICOMAdminCatalog : public IDispatch {
  public:
    virtual HRESULT WINAPI GetCollection(BSTR bstrCollName,IDispatch **ppCatalogCollection) = 0;
    virtual HRESULT WINAPI Connect(BSTR bstrCatalogServerName,IDispatch **ppCatalogCollection) = 0;
    virtual HRESULT WINAPI get_MajorVersion(long *plMajorVersion) = 0;
    virtual HRESULT WINAPI get_MinorVersion(long *plMinorVersion) = 0;
    virtual HRESULT WINAPI GetCollectionByQuery(BSTR bstrCollName,SAFEARRAY **ppsaVarQuery,IDispatch **ppCatalogCollection) = 0;
    virtual HRESULT WINAPI ImportComponent(BSTR bstrApplIDOrName,BSTR bstrCLSIDOrProgID) = 0;
    virtual HRESULT WINAPI InstallComponent(BSTR bstrApplIDOrName,BSTR bstrDLL,BSTR bstrTLB,BSTR bstrPSDLL) = 0;
    virtual HRESULT WINAPI ShutdownApplication(BSTR bstrApplIDOrName) = 0;
    virtual HRESULT WINAPI ExportApplication(BSTR bstrApplIDOrName,BSTR bstrApplicationFile,long lOptions) = 0;
    virtual HRESULT WINAPI InstallApplication(BSTR bstrApplicationFile,BSTR bstrDestinationDirectory,long lOptions,BSTR bstrUserId,BSTR bstrPassword,BSTR bstrRSN) = 0;
    virtual HRESULT WINAPI StopRouter(void) = 0;
    virtual HRESULT WINAPI RefreshRouter(void) = 0;
    virtual HRESULT WINAPI StartRouter(void) = 0;
    virtual HRESULT WINAPI Reserved1(void) = 0;
    virtual HRESULT WINAPI Reserved2(void) = 0;
    virtual HRESULT WINAPI InstallMultipleComponents(BSTR bstrApplIDOrName,SAFEARRAY **ppsaVarFileNames,SAFEARRAY **ppsaVarCLSIDs) = 0;
    virtual HRESULT WINAPI GetMultipleComponentsInfo(BSTR bstrApplIdOrName,SAFEARRAY **ppsaVarFileNames,SAFEARRAY **ppsaVarCLSIDs,SAFEARRAY **ppsaVarClassNames,SAFEARRAY **ppsaVarFileFlags,SAFEARRAY **ppsaVarComponentFlags) = 0;
    virtual HRESULT WINAPI RefreshComponents(void) = 0;
    virtual HRESULT WINAPI BackupREGDB(BSTR bstrBackupFilePath) = 0;
    virtual HRESULT WINAPI RestoreREGDB(BSTR bstrBackupFilePath) = 0;
    virtual HRESULT WINAPI QueryApplicationFile(BSTR bstrApplicationFile,BSTR *pbstrApplicationName,BSTR *pbstrApplicationDescription,VARIANT_BOOL *pbHasUsers,VARIANT_BOOL *pbIsProxy,SAFEARRAY **ppsaVarFileNames) = 0;
    virtual HRESULT WINAPI StartApplication(BSTR bstrApplIdOrName) = 0;
    virtual HRESULT WINAPI ServiceCheck(long lService,long *plStatus) = 0;
    virtual HRESULT WINAPI InstallMultipleEventClasses(BSTR bstrApplIdOrName,SAFEARRAY **ppsaVarFileNames,SAFEARRAY **ppsaVarCLSIDS) = 0;
    virtual HRESULT WINAPI InstallEventClass(BSTR bstrApplIdOrName,BSTR bstrDLL,BSTR bstrTLB,BSTR bstrPSDLL) = 0;
    virtual HRESULT WINAPI GetEventClassesForIID(BSTR bstrIID,SAFEARRAY **ppsaVarCLSIDs,SAFEARRAY **ppsaVarProgIDs,SAFEARRAY **ppsaVarDescriptions) = 0;
  };
#else
  typedef struct ICOMAdminCatalogVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(ICOMAdminCatalog *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(ICOMAdminCatalog *This);
      ULONG (WINAPI *Release)(ICOMAdminCatalog *This);
      HRESULT (WINAPI *GetTypeInfoCount)(ICOMAdminCatalog *This,UINT *pctinfo);
      HRESULT (WINAPI *GetTypeInfo)(ICOMAdminCatalog *This,UINT iTInfo,LCID lcid,ITypeInfo **ppTInfo);
      HRESULT (WINAPI *GetIDsOfNames)(ICOMAdminCatalog *This,REFIID riid,LPOLESTR *rgszNames,UINT cNames,LCID lcid,DISPID *rgDispId);
      HRESULT (WINAPI *Invoke)(ICOMAdminCatalog *This,DISPID dispIdMember,REFIID riid,LCID lcid,WORD wFlags,DISPPARAMS *pDispParams,VARIANT *pVarResult,EXCEPINFO *pExcepInfo,UINT *puArgErr);
      HRESULT (WINAPI *GetCollection)(ICOMAdminCatalog *This,BSTR bstrCollName,IDispatch **ppCatalogCollection);
      HRESULT (WINAPI *Connect)(ICOMAdminCatalog *This,BSTR bstrCatalogServerName,IDispatch **ppCatalogCollection);
      HRESULT (WINAPI *get_MajorVersion)(ICOMAdminCatalog *This,long *plMajorVersion);
      HRESULT (WINAPI *get_MinorVersion)(ICOMAdminCatalog *This,long *plMinorVersion);
      HRESULT (WINAPI *GetCollectionByQuery)(ICOMAdminCatalog *This,BSTR bstrCollName,SAFEARRAY **ppsaVarQuery,IDispatch **ppCatalogCollection);
      HRESULT (WINAPI *ImportComponent)(ICOMAdminCatalog *This,BSTR bstrApplIDOrName,BSTR bstrCLSIDOrProgID);
      HRESULT (WINAPI *InstallComponent)(ICOMAdminCatalog *This,BSTR bstrApplIDOrName,BSTR bstrDLL,BSTR bstrTLB,BSTR bstrPSDLL);
      HRESULT (WINAPI *ShutdownApplication)(ICOMAdminCatalog *This,BSTR bstrApplIDOrName);
      HRESULT (WINAPI *ExportApplication)(ICOMAdminCatalog *This,BSTR bstrApplIDOrName,BSTR bstrApplicationFile,long lOptions);
      HRESULT (WINAPI *InstallApplication)(ICOMAdminCatalog *This,BSTR bstrApplicationFile,BSTR bstrDestinationDirectory,long lOptions,BSTR bstrUserId,BSTR bstrPassword,BSTR bstrRSN);
      HRESULT (WINAPI *StopRouter)(ICOMAdminCatalog *This);
      HRESULT (WINAPI *RefreshRouter)(ICOMAdminCatalog *This);
      HRESULT (WINAPI *StartRouter)(ICOMAdminCatalog *This);
      HRESULT (WINAPI *Reserved1)(ICOMAdminCatalog *This);
      HRESULT (WINAPI *Reserved2)(ICOMAdminCatalog *This);
      HRESULT (WINAPI *InstallMultipleComponents)(ICOMAdminCatalog *This,BSTR bstrApplIDOrName,SAFEARRAY **ppsaVarFileNames,SAFEARRAY **ppsaVarCLSIDs);
      HRESULT (WINAPI *GetMultipleComponentsInfo)(ICOMAdminCatalog *This,BSTR bstrApplIdOrName,SAFEARRAY **ppsaVarFileNames,SAFEARRAY **ppsaVarCLSIDs,SAFEARRAY **ppsaVarClassNames,SAFEARRAY **ppsaVarFileFlags,SAFEARRAY **ppsaVarComponentFlags);
      HRESULT (WINAPI *RefreshComponents)(ICOMAdminCatalog *This);
      HRESULT (WINAPI *BackupREGDB)(ICOMAdminCatalog *This,BSTR bstrBackupFilePath);
      HRESULT (WINAPI *RestoreREGDB)(ICOMAdminCatalog *This,BSTR bstrBackupFilePath);
      HRESULT (WINAPI *QueryApplicationFile)(ICOMAdminCatalog *This,BSTR bstrApplicationFile,BSTR *pbstrApplicationName,BSTR *pbstrApplicationDescription,VARIANT_BOOL *pbHasUsers,VARIANT_BOOL *pbIsProxy,SAFEARRAY **ppsaVarFileNames);
      HRESULT (WINAPI *StartApplication)(ICOMAdminCatalog *This,BSTR bstrApplIdOrName);
      HRESULT (WINAPI *ServiceCheck)(ICOMAdminCatalog *This,long lService,long *plStatus);
      HRESULT (WINAPI *InstallMultipleEventClasses)(ICOMAdminCatalog *This,BSTR bstrApplIdOrName,SAFEARRAY **ppsaVarFileNames,SAFEARRAY **ppsaVarCLSIDS);
      HRESULT (WINAPI *InstallEventClass)(ICOMAdminCatalog *This,BSTR bstrApplIdOrName,BSTR bstrDLL,BSTR bstrTLB,BSTR bstrPSDLL);
      HRESULT (WINAPI *GetEventClassesForIID)(ICOMAdminCatalog *This,BSTR bstrIID,SAFEARRAY **ppsaVarCLSIDs,SAFEARRAY **ppsaVarProgIDs,SAFEARRAY **ppsaVarDescriptions);
    END_INTERFACE
  } ICOMAdminCatalogVtbl;
  struct ICOMAdminCatalog {
    CONST_VTBL struct ICOMAdminCatalogVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define ICOMAdminCatalog_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define ICOMAdminCatalog_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICOMAdminCatalog_Release(This) (This)->lpVtbl->Release(This)
#define ICOMAdminCatalog_GetTypeInfoCount(This,pctinfo) (This)->lpVtbl->GetTypeInfoCount(This,pctinfo)
#define ICOMAdminCatalog_GetTypeInfo(This,iTInfo,lcid,ppTInfo) (This)->lpVtbl->GetTypeInfo(This,iTInfo,lcid,ppTInfo)
#define ICOMAdminCatalog_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) (This)->lpVtbl->GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)
#define ICOMAdminCatalog_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) (This)->lpVtbl->Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)
#define ICOMAdminCatalog_GetCollection(This,bstrCollName,ppCatalogCollection) (This)->lpVtbl->GetCollection(This,bstrCollName,ppCatalogCollection)
#define ICOMAdminCatalog_Connect(This,bstrCatalogServerName,ppCatalogCollection) (This)->lpVtbl->Connect(This,bstrCatalogServerName,ppCatalogCollection)
#define ICOMAdminCatalog_get_MajorVersion(This,plMajorVersion) (This)->lpVtbl->get_MajorVersion(This,plMajorVersion)
#define ICOMAdminCatalog_get_MinorVersion(This,plMinorVersion) (This)->lpVtbl->get_MinorVersion(This,plMinorVersion)
#define ICOMAdminCatalog_GetCollectionByQuery(This,bstrCollName,ppsaVarQuery,ppCatalogCollection) (This)->lpVtbl->GetCollectionByQuery(This,bstrCollName,ppsaVarQuery,ppCatalogCollection)
#define ICOMAdminCatalog_ImportComponent(This,bstrApplIDOrName,bstrCLSIDOrProgID) (This)->lpVtbl->ImportComponent(This,bstrApplIDOrName,bstrCLSIDOrProgID)
#define ICOMAdminCatalog_InstallComponent(This,bstrApplIDOrName,bstrDLL,bstrTLB,bstrPSDLL) (This)->lpVtbl->InstallComponent(This,bstrApplIDOrName,bstrDLL,bstrTLB,bstrPSDLL)
#define ICOMAdminCatalog_ShutdownApplication(This,bstrApplIDOrName) (This)->lpVtbl->ShutdownApplication(This,bstrApplIDOrName)
#define ICOMAdminCatalog_ExportApplication(This,bstrApplIDOrName,bstrApplicationFile,lOptions) (This)->lpVtbl->ExportApplication(This,bstrApplIDOrName,bstrApplicationFile,lOptions)
#define ICOMAdminCatalog_InstallApplication(This,bstrApplicationFile,bstrDestinationDirectory,lOptions,bstrUserId,bstrPassword,bstrRSN) (This)->lpVtbl->InstallApplication(This,bstrApplicationFile,bstrDestinationDirectory,lOptions,bstrUserId,bstrPassword,bstrRSN)
#define ICOMAdminCatalog_StopRouter(This) (This)->lpVtbl->StopRouter(This)
#define ICOMAdminCatalog_RefreshRouter(This) (This)->lpVtbl->RefreshRouter(This)
#define ICOMAdminCatalog_StartRouter(This) (This)->lpVtbl->StartRouter(This)
#define ICOMAdminCatalog_Reserved1(This) (This)->lpVtbl->Reserved1(This)
#define ICOMAdminCatalog_Reserved2(This) (This)->lpVtbl->Reserved2(This)
#define ICOMAdminCatalog_InstallMultipleComponents(This,bstrApplIDOrName,ppsaVarFileNames,ppsaVarCLSIDs) (This)->lpVtbl->InstallMultipleComponents(This,bstrApplIDOrName,ppsaVarFileNames,ppsaVarCLSIDs)
#define ICOMAdminCatalog_GetMultipleComponentsInfo(This,bstrApplIdOrName,ppsaVarFileNames,ppsaVarCLSIDs,ppsaVarClassNames,ppsaVarFileFlags,ppsaVarComponentFlags) (This)->lpVtbl->GetMultipleComponentsInfo(This,bstrApplIdOrName,ppsaVarFileNames,ppsaVarCLSIDs,ppsaVarClassNames,ppsaVarFileFlags,ppsaVarComponentFlags)
#define ICOMAdminCatalog_RefreshComponents(This) (This)->lpVtbl->RefreshComponents(This)
#define ICOMAdminCatalog_BackupREGDB(This,bstrBackupFilePath) (This)->lpVtbl->BackupREGDB(This,bstrBackupFilePath)
#define ICOMAdminCatalog_RestoreREGDB(This,bstrBackupFilePath) (This)->lpVtbl->RestoreREGDB(This,bstrBackupFilePath)
#define ICOMAdminCatalog_QueryApplicationFile(This,bstrApplicationFile,pbstrApplicationName,pbstrApplicationDescription,pbHasUsers,pbIsProxy,ppsaVarFileNames) (This)->lpVtbl->QueryApplicationFile(This,bstrApplicationFile,pbstrApplicationName,pbstrApplicationDescription,pbHasUsers,pbIsProxy,ppsaVarFileNames)
#define ICOMAdminCatalog_StartApplication(This,bstrApplIdOrName) (This)->lpVtbl->StartApplication(This,bstrApplIdOrName)
#define ICOMAdminCatalog_ServiceCheck(This,lService,plStatus) (This)->lpVtbl->ServiceCheck(This,lService,plStatus)
#define ICOMAdminCatalog_InstallMultipleEventClasses(This,bstrApplIdOrName,ppsaVarFileNames,ppsaVarCLSIDS) (This)->lpVtbl->InstallMultipleEventClasses(This,bstrApplIdOrName,ppsaVarFileNames,ppsaVarCLSIDS)
#define ICOMAdminCatalog_InstallEventClass(This,bstrApplIdOrName,bstrDLL,bstrTLB,bstrPSDLL) (This)->lpVtbl->InstallEventClass(This,bstrApplIdOrName,bstrDLL,bstrTLB,bstrPSDLL)
#define ICOMAdminCatalog_GetEventClassesForIID(This,bstrIID,ppsaVarCLSIDs,ppsaVarProgIDs,ppsaVarDescriptions) (This)->lpVtbl->GetEventClassesForIID(This,bstrIID,ppsaVarCLSIDs,ppsaVarProgIDs,ppsaVarDescriptions)
#endif
#endif
  HRESULT WINAPI ICOMAdminCatalog_GetCollection_Proxy(ICOMAdminCatalog *This,BSTR bstrCollName,IDispatch **ppCatalogCollection);
  void __RPC_STUB ICOMAdminCatalog_GetCollection_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_Connect_Proxy(ICOMAdminCatalog *This,BSTR bstrCatalogServerName,IDispatch **ppCatalogCollection);
  void __RPC_STUB ICOMAdminCatalog_Connect_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_get_MajorVersion_Proxy(ICOMAdminCatalog *This,long *plMajorVersion);
  void __RPC_STUB ICOMAdminCatalog_get_MajorVersion_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_get_MinorVersion_Proxy(ICOMAdminCatalog *This,long *plMinorVersion);
  void __RPC_STUB ICOMAdminCatalog_get_MinorVersion_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_GetCollectionByQuery_Proxy(ICOMAdminCatalog *This,BSTR bstrCollName,SAFEARRAY **ppsaVarQuery,IDispatch **ppCatalogCollection);
  void __RPC_STUB ICOMAdminCatalog_GetCollectionByQuery_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_ImportComponent_Proxy(ICOMAdminCatalog *This,BSTR bstrApplIDOrName,BSTR bstrCLSIDOrProgID);
  void __RPC_STUB ICOMAdminCatalog_ImportComponent_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_InstallComponent_Proxy(ICOMAdminCatalog *This,BSTR bstrApplIDOrName,BSTR bstrDLL,BSTR bstrTLB,BSTR bstrPSDLL);
  void __RPC_STUB ICOMAdminCatalog_InstallComponent_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_ShutdownApplication_Proxy(ICOMAdminCatalog *This,BSTR bstrApplIDOrName);
  void __RPC_STUB ICOMAdminCatalog_ShutdownApplication_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_ExportApplication_Proxy(ICOMAdminCatalog *This,BSTR bstrApplIDOrName,BSTR bstrApplicationFile,long lOptions);
  void __RPC_STUB ICOMAdminCatalog_ExportApplication_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_InstallApplication_Proxy(ICOMAdminCatalog *This,BSTR bstrApplicationFile,BSTR bstrDestinationDirectory,long lOptions,BSTR bstrUserId,BSTR bstrPassword,BSTR bstrRSN);
  void __RPC_STUB ICOMAdminCatalog_InstallApplication_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_StopRouter_Proxy(ICOMAdminCatalog *This);
  void __RPC_STUB ICOMAdminCatalog_StopRouter_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_RefreshRouter_Proxy(ICOMAdminCatalog *This);
  void __RPC_STUB ICOMAdminCatalog_RefreshRouter_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_StartRouter_Proxy(ICOMAdminCatalog *This);
  void __RPC_STUB ICOMAdminCatalog_StartRouter_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_Reserved1_Proxy(ICOMAdminCatalog *This);
  void __RPC_STUB ICOMAdminCatalog_Reserved1_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_Reserved2_Proxy(ICOMAdminCatalog *This);
  void __RPC_STUB ICOMAdminCatalog_Reserved2_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_InstallMultipleComponents_Proxy(ICOMAdminCatalog *This,BSTR bstrApplIDOrName,SAFEARRAY **ppsaVarFileNames,SAFEARRAY **ppsaVarCLSIDs);
  void __RPC_STUB ICOMAdminCatalog_InstallMultipleComponents_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_GetMultipleComponentsInfo_Proxy(ICOMAdminCatalog *This,BSTR bstrApplIdOrName,SAFEARRAY **ppsaVarFileNames,SAFEARRAY **ppsaVarCLSIDs,SAFEARRAY **ppsaVarClassNames,SAFEARRAY **ppsaVarFileFlags,SAFEARRAY **ppsaVarComponentFlags);
  void __RPC_STUB ICOMAdminCatalog_GetMultipleComponentsInfo_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_RefreshComponents_Proxy(ICOMAdminCatalog *This);
  void __RPC_STUB ICOMAdminCatalog_RefreshComponents_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_BackupREGDB_Proxy(ICOMAdminCatalog *This,BSTR bstrBackupFilePath);
  void __RPC_STUB ICOMAdminCatalog_BackupREGDB_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_RestoreREGDB_Proxy(ICOMAdminCatalog *This,BSTR bstrBackupFilePath);
  void __RPC_STUB ICOMAdminCatalog_RestoreREGDB_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_QueryApplicationFile_Proxy(ICOMAdminCatalog *This,BSTR bstrApplicationFile,BSTR *pbstrApplicationName,BSTR *pbstrApplicationDescription,VARIANT_BOOL *pbHasUsers,VARIANT_BOOL *pbIsProxy,SAFEARRAY **ppsaVarFileNames);
  void __RPC_STUB ICOMAdminCatalog_QueryApplicationFile_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_StartApplication_Proxy(ICOMAdminCatalog *This,BSTR bstrApplIdOrName);
  void __RPC_STUB ICOMAdminCatalog_StartApplication_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_ServiceCheck_Proxy(ICOMAdminCatalog *This,long lService,long *plStatus);
  void __RPC_STUB ICOMAdminCatalog_ServiceCheck_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_InstallMultipleEventClasses_Proxy(ICOMAdminCatalog *This,BSTR bstrApplIdOrName,SAFEARRAY **ppsaVarFileNames,SAFEARRAY **ppsaVarCLSIDS);
  void __RPC_STUB ICOMAdminCatalog_InstallMultipleEventClasses_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_InstallEventClass_Proxy(ICOMAdminCatalog *This,BSTR bstrApplIdOrName,BSTR bstrDLL,BSTR bstrTLB,BSTR bstrPSDLL);
  void __RPC_STUB ICOMAdminCatalog_InstallEventClass_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog_GetEventClassesForIID_Proxy(ICOMAdminCatalog *This,BSTR bstrIID,SAFEARRAY **ppsaVarCLSIDs,SAFEARRAY **ppsaVarProgIDs,SAFEARRAY **ppsaVarDescriptions);
  void __RPC_STUB ICOMAdminCatalog_GetEventClassesForIID_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

  typedef enum COMAdminInUse {
    COMAdminNotInUse = 0,COMAdminInUseByCatalog = 0x1,COMAdminInUseByRegistryUnknown = 0x2,COMAdminInUseByRegistryProxyStub = 0x3,
    COMAdminInUseByRegistryTypeLib = 0x4,COMAdminInUseByRegistryClsid = 0x5
  } COMAdminInUse;

  extern RPC_IF_HANDLE __MIDL_itf_comadmin_0116_v0_0_c_ifspec;
  extern RPC_IF_HANDLE __MIDL_itf_comadmin_0116_v0_0_s_ifspec;

#ifndef __ICOMAdminCatalog2_INTERFACE_DEFINED__
#define __ICOMAdminCatalog2_INTERFACE_DEFINED__
  EXTERN_C const IID IID_ICOMAdminCatalog2;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct ICOMAdminCatalog2 : public ICOMAdminCatalog {
  public:
    virtual HRESULT WINAPI GetCollectionByQuery2(BSTR bstrCollectionName,VARIANT *pVarQueryStrings,IDispatch **ppCatalogCollection) = 0;
    virtual HRESULT WINAPI GetApplicationInstanceIDFromProcessID(long lProcessID,BSTR *pbstrApplicationInstanceID) = 0;
    virtual HRESULT WINAPI ShutdownApplicationInstances(VARIANT *pVarApplicationInstanceID) = 0;
    virtual HRESULT WINAPI PauseApplicationInstances(VARIANT *pVarApplicationInstanceID) = 0;
    virtual HRESULT WINAPI ResumeApplicationInstances(VARIANT *pVarApplicationInstanceID) = 0;
    virtual HRESULT WINAPI RecycleApplicationInstances(VARIANT *pVarApplicationInstanceID,long lReasonCode) = 0;
    virtual HRESULT WINAPI AreApplicationInstancesPaused(VARIANT *pVarApplicationInstanceID,VARIANT_BOOL *pVarBoolPaused) = 0;
    virtual HRESULT WINAPI DumpApplicationInstance(BSTR bstrApplicationInstanceID,BSTR bstrDirectory,long lMaxImages,BSTR *pbstrDumpFile) = 0;
    virtual HRESULT WINAPI get_IsApplicationInstanceDumpSupported(VARIANT_BOOL *pVarBoolDumpSupported) = 0;
    virtual HRESULT WINAPI CreateServiceForApplication(BSTR bstrApplicationIDOrName,BSTR bstrServiceName,BSTR bstrStartType,BSTR bstrErrorControl,BSTR bstrDependencies,BSTR bstrRunAs,BSTR bstrPassword,VARIANT_BOOL bDesktopOk) = 0;
    virtual HRESULT WINAPI DeleteServiceForApplication(BSTR bstrApplicationIDOrName) = 0;
    virtual HRESULT WINAPI GetPartitionID(BSTR bstrApplicationIDOrName,BSTR *pbstrPartitionID) = 0;
    virtual HRESULT WINAPI GetPartitionName(BSTR bstrApplicationIDOrName,BSTR *pbstrPartitionName) = 0;
    virtual HRESULT WINAPI put_CurrentPartition(BSTR bstrPartitionIDOrName) = 0;
    virtual HRESULT WINAPI get_CurrentPartitionID(BSTR *pbstrPartitionID) = 0;
    virtual HRESULT WINAPI get_CurrentPartitionName(BSTR *pbstrPartitionName) = 0;
    virtual HRESULT WINAPI get_GlobalPartitionID(BSTR *pbstrGlobalPartitionID) = 0;
    virtual HRESULT WINAPI FlushPartitionCache(void) = 0;
    virtual HRESULT WINAPI CopyApplications(BSTR bstrSourcePartitionIDOrName,VARIANT *pVarApplicationID,BSTR bstrDestinationPartitionIDOrName) = 0;
    virtual HRESULT WINAPI CopyComponents(BSTR bstrSourceApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,BSTR bstrDestinationApplicationIDOrName) = 0;
    virtual HRESULT WINAPI MoveComponents(BSTR bstrSourceApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,BSTR bstrDestinationApplicationIDOrName) = 0;
    virtual HRESULT WINAPI AliasComponent(BSTR bstrSrcApplicationIDOrName,BSTR bstrCLSIDOrProgID,BSTR bstrDestApplicationIDOrName,BSTR bstrNewProgId,BSTR bstrNewClsid) = 0;
    virtual HRESULT WINAPI IsSafeToDelete(BSTR bstrDllName,COMAdminInUse *pCOMAdminInUse) = 0;
    virtual HRESULT WINAPI ImportUnconfiguredComponents(BSTR bstrApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,VARIANT *pVarComponentType) = 0;
    virtual HRESULT WINAPI PromoteUnconfiguredComponents(BSTR bstrApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,VARIANT *pVarComponentType) = 0;
    virtual HRESULT WINAPI ImportComponents(BSTR bstrApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,VARIANT *pVarComponentType) = 0;
    virtual HRESULT WINAPI get_Is64BitCatalogServer(VARIANT_BOOL *pbIs64Bit) = 0;
    virtual HRESULT WINAPI ExportPartition(BSTR bstrPartitionIDOrName,BSTR bstrPartitionFileName,long lOptions) = 0;
    virtual HRESULT WINAPI InstallPartition(BSTR bstrFileName,BSTR bstrDestDirectory,long lOptions,BSTR bstrUserID,BSTR bstrPassword,BSTR bstrRSN) = 0;
    virtual HRESULT WINAPI QueryApplicationFile2(BSTR bstrApplicationFile,IDispatch **ppFilesForImport) = 0;
    virtual HRESULT WINAPI GetComponentVersionCount(BSTR bstrCLSIDOrProgID,long *plVersionCount) = 0;
  };
#else
  typedef struct ICOMAdminCatalog2Vtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(ICOMAdminCatalog2 *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(ICOMAdminCatalog2 *This);
      ULONG (WINAPI *Release)(ICOMAdminCatalog2 *This);
      HRESULT (WINAPI *GetTypeInfoCount)(ICOMAdminCatalog2 *This,UINT *pctinfo);
      HRESULT (WINAPI *GetTypeInfo)(ICOMAdminCatalog2 *This,UINT iTInfo,LCID lcid,ITypeInfo **ppTInfo);
      HRESULT (WINAPI *GetIDsOfNames)(ICOMAdminCatalog2 *This,REFIID riid,LPOLESTR *rgszNames,UINT cNames,LCID lcid,DISPID *rgDispId);
      HRESULT (WINAPI *Invoke)(ICOMAdminCatalog2 *This,DISPID dispIdMember,REFIID riid,LCID lcid,WORD wFlags,DISPPARAMS *pDispParams,VARIANT *pVarResult,EXCEPINFO *pExcepInfo,UINT *puArgErr);
      HRESULT (WINAPI *GetCollection)(ICOMAdminCatalog2 *This,BSTR bstrCollName,IDispatch **ppCatalogCollection);
      HRESULT (WINAPI *Connect)(ICOMAdminCatalog2 *This,BSTR bstrCatalogServerName,IDispatch **ppCatalogCollection);
      HRESULT (WINAPI *get_MajorVersion)(ICOMAdminCatalog2 *This,long *plMajorVersion);
      HRESULT (WINAPI *get_MinorVersion)(ICOMAdminCatalog2 *This,long *plMinorVersion);
      HRESULT (WINAPI *GetCollectionByQuery)(ICOMAdminCatalog2 *This,BSTR bstrCollName,SAFEARRAY **ppsaVarQuery,IDispatch **ppCatalogCollection);
      HRESULT (WINAPI *ImportComponent)(ICOMAdminCatalog2 *This,BSTR bstrApplIDOrName,BSTR bstrCLSIDOrProgID);
      HRESULT (WINAPI *InstallComponent)(ICOMAdminCatalog2 *This,BSTR bstrApplIDOrName,BSTR bstrDLL,BSTR bstrTLB,BSTR bstrPSDLL);
      HRESULT (WINAPI *ShutdownApplication)(ICOMAdminCatalog2 *This,BSTR bstrApplIDOrName);
      HRESULT (WINAPI *ExportApplication)(ICOMAdminCatalog2 *This,BSTR bstrApplIDOrName,BSTR bstrApplicationFile,long lOptions);
      HRESULT (WINAPI *InstallApplication)(ICOMAdminCatalog2 *This,BSTR bstrApplicationFile,BSTR bstrDestinationDirectory,long lOptions,BSTR bstrUserId,BSTR bstrPassword,BSTR bstrRSN);
      HRESULT (WINAPI *StopRouter)(ICOMAdminCatalog2 *This);
      HRESULT (WINAPI *RefreshRouter)(ICOMAdminCatalog2 *This);
      HRESULT (WINAPI *StartRouter)(ICOMAdminCatalog2 *This);
      HRESULT (WINAPI *Reserved1)(ICOMAdminCatalog2 *This);
      HRESULT (WINAPI *Reserved2)(ICOMAdminCatalog2 *This);
      HRESULT (WINAPI *InstallMultipleComponents)(ICOMAdminCatalog2 *This,BSTR bstrApplIDOrName,SAFEARRAY **ppsaVarFileNames,SAFEARRAY **ppsaVarCLSIDs);
      HRESULT (WINAPI *GetMultipleComponentsInfo)(ICOMAdminCatalog2 *This,BSTR bstrApplIdOrName,SAFEARRAY **ppsaVarFileNames,SAFEARRAY **ppsaVarCLSIDs,SAFEARRAY **ppsaVarClassNames,SAFEARRAY **ppsaVarFileFlags,SAFEARRAY **ppsaVarComponentFlags);
      HRESULT (WINAPI *RefreshComponents)(ICOMAdminCatalog2 *This);
      HRESULT (WINAPI *BackupREGDB)(ICOMAdminCatalog2 *This,BSTR bstrBackupFilePath);
      HRESULT (WINAPI *RestoreREGDB)(ICOMAdminCatalog2 *This,BSTR bstrBackupFilePath);
      HRESULT (WINAPI *QueryApplicationFile)(ICOMAdminCatalog2 *This,BSTR bstrApplicationFile,BSTR *pbstrApplicationName,BSTR *pbstrApplicationDescription,VARIANT_BOOL *pbHasUsers,VARIANT_BOOL *pbIsProxy,SAFEARRAY **ppsaVarFileNames);
      HRESULT (WINAPI *StartApplication)(ICOMAdminCatalog2 *This,BSTR bstrApplIdOrName);
      HRESULT (WINAPI *ServiceCheck)(ICOMAdminCatalog2 *This,long lService,long *plStatus);
      HRESULT (WINAPI *InstallMultipleEventClasses)(ICOMAdminCatalog2 *This,BSTR bstrApplIdOrName,SAFEARRAY **ppsaVarFileNames,SAFEARRAY **ppsaVarCLSIDS);
      HRESULT (WINAPI *InstallEventClass)(ICOMAdminCatalog2 *This,BSTR bstrApplIdOrName,BSTR bstrDLL,BSTR bstrTLB,BSTR bstrPSDLL);
      HRESULT (WINAPI *GetEventClassesForIID)(ICOMAdminCatalog2 *This,BSTR bstrIID,SAFEARRAY **ppsaVarCLSIDs,SAFEARRAY **ppsaVarProgIDs,SAFEARRAY **ppsaVarDescriptions);
      HRESULT (WINAPI *GetCollectionByQuery2)(ICOMAdminCatalog2 *This,BSTR bstrCollectionName,VARIANT *pVarQueryStrings,IDispatch **ppCatalogCollection);
      HRESULT (WINAPI *GetApplicationInstanceIDFromProcessID)(ICOMAdminCatalog2 *This,long lProcessID,BSTR *pbstrApplicationInstanceID);
      HRESULT (WINAPI *ShutdownApplicationInstances)(ICOMAdminCatalog2 *This,VARIANT *pVarApplicationInstanceID);
      HRESULT (WINAPI *PauseApplicationInstances)(ICOMAdminCatalog2 *This,VARIANT *pVarApplicationInstanceID);
      HRESULT (WINAPI *ResumeApplicationInstances)(ICOMAdminCatalog2 *This,VARIANT *pVarApplicationInstanceID);
      HRESULT (WINAPI *RecycleApplicationInstances)(ICOMAdminCatalog2 *This,VARIANT *pVarApplicationInstanceID,long lReasonCode);
      HRESULT (WINAPI *AreApplicationInstancesPaused)(ICOMAdminCatalog2 *This,VARIANT *pVarApplicationInstanceID,VARIANT_BOOL *pVarBoolPaused);
      HRESULT (WINAPI *DumpApplicationInstance)(ICOMAdminCatalog2 *This,BSTR bstrApplicationInstanceID,BSTR bstrDirectory,long lMaxImages,BSTR *pbstrDumpFile);
      HRESULT (WINAPI *get_IsApplicationInstanceDumpSupported)(ICOMAdminCatalog2 *This,VARIANT_BOOL *pVarBoolDumpSupported);
      HRESULT (WINAPI *CreateServiceForApplication)(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName,BSTR bstrServiceName,BSTR bstrStartType,BSTR bstrErrorControl,BSTR bstrDependencies,BSTR bstrRunAs,BSTR bstrPassword,VARIANT_BOOL bDesktopOk);
      HRESULT (WINAPI *DeleteServiceForApplication)(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName);
      HRESULT (WINAPI *GetPartitionID)(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName,BSTR *pbstrPartitionID);
      HRESULT (WINAPI *GetPartitionName)(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName,BSTR *pbstrPartitionName);
      HRESULT (WINAPI *put_CurrentPartition)(ICOMAdminCatalog2 *This,BSTR bstrPartitionIDOrName);
      HRESULT (WINAPI *get_CurrentPartitionID)(ICOMAdminCatalog2 *This,BSTR *pbstrPartitionID);
      HRESULT (WINAPI *get_CurrentPartitionName)(ICOMAdminCatalog2 *This,BSTR *pbstrPartitionName);
      HRESULT (WINAPI *get_GlobalPartitionID)(ICOMAdminCatalog2 *This,BSTR *pbstrGlobalPartitionID);
      HRESULT (WINAPI *FlushPartitionCache)(ICOMAdminCatalog2 *This);
      HRESULT (WINAPI *CopyApplications)(ICOMAdminCatalog2 *This,BSTR bstrSourcePartitionIDOrName,VARIANT *pVarApplicationID,BSTR bstrDestinationPartitionIDOrName);
      HRESULT (WINAPI *CopyComponents)(ICOMAdminCatalog2 *This,BSTR bstrSourceApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,BSTR bstrDestinationApplicationIDOrName);
      HRESULT (WINAPI *MoveComponents)(ICOMAdminCatalog2 *This,BSTR bstrSourceApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,BSTR bstrDestinationApplicationIDOrName);
      HRESULT (WINAPI *AliasComponent)(ICOMAdminCatalog2 *This,BSTR bstrSrcApplicationIDOrName,BSTR bstrCLSIDOrProgID,BSTR bstrDestApplicationIDOrName,BSTR bstrNewProgId,BSTR bstrNewClsid);
      HRESULT (WINAPI *IsSafeToDelete)(ICOMAdminCatalog2 *This,BSTR bstrDllName,COMAdminInUse *pCOMAdminInUse);
      HRESULT (WINAPI *ImportUnconfiguredComponents)(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,VARIANT *pVarComponentType);
      HRESULT (WINAPI *PromoteUnconfiguredComponents)(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,VARIANT *pVarComponentType);
      HRESULT (WINAPI *ImportComponents)(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,VARIANT *pVarComponentType);
      HRESULT (WINAPI *get_Is64BitCatalogServer)(ICOMAdminCatalog2 *This,VARIANT_BOOL *pbIs64Bit);
      HRESULT (WINAPI *ExportPartition)(ICOMAdminCatalog2 *This,BSTR bstrPartitionIDOrName,BSTR bstrPartitionFileName,long lOptions);
      HRESULT (WINAPI *InstallPartition)(ICOMAdminCatalog2 *This,BSTR bstrFileName,BSTR bstrDestDirectory,long lOptions,BSTR bstrUserID,BSTR bstrPassword,BSTR bstrRSN);
      HRESULT (WINAPI *QueryApplicationFile2)(ICOMAdminCatalog2 *This,BSTR bstrApplicationFile,IDispatch **ppFilesForImport);
      HRESULT (WINAPI *GetComponentVersionCount)(ICOMAdminCatalog2 *This,BSTR bstrCLSIDOrProgID,long *plVersionCount);
    END_INTERFACE
  } ICOMAdminCatalog2Vtbl;
  struct ICOMAdminCatalog2 {
    CONST_VTBL struct ICOMAdminCatalog2Vtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define ICOMAdminCatalog2_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define ICOMAdminCatalog2_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICOMAdminCatalog2_Release(This) (This)->lpVtbl->Release(This)
#define ICOMAdminCatalog2_GetTypeInfoCount(This,pctinfo) (This)->lpVtbl->GetTypeInfoCount(This,pctinfo)
#define ICOMAdminCatalog2_GetTypeInfo(This,iTInfo,lcid,ppTInfo) (This)->lpVtbl->GetTypeInfo(This,iTInfo,lcid,ppTInfo)
#define ICOMAdminCatalog2_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) (This)->lpVtbl->GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)
#define ICOMAdminCatalog2_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) (This)->lpVtbl->Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)
#define ICOMAdminCatalog2_GetCollection(This,bstrCollName,ppCatalogCollection) (This)->lpVtbl->GetCollection(This,bstrCollName,ppCatalogCollection)
#define ICOMAdminCatalog2_Connect(This,bstrCatalogServerName,ppCatalogCollection) (This)->lpVtbl->Connect(This,bstrCatalogServerName,ppCatalogCollection)
#define ICOMAdminCatalog2_get_MajorVersion(This,plMajorVersion) (This)->lpVtbl->get_MajorVersion(This,plMajorVersion)
#define ICOMAdminCatalog2_get_MinorVersion(This,plMinorVersion) (This)->lpVtbl->get_MinorVersion(This,plMinorVersion)
#define ICOMAdminCatalog2_GetCollectionByQuery(This,bstrCollName,ppsaVarQuery,ppCatalogCollection) (This)->lpVtbl->GetCollectionByQuery(This,bstrCollName,ppsaVarQuery,ppCatalogCollection)
#define ICOMAdminCatalog2_ImportComponent(This,bstrApplIDOrName,bstrCLSIDOrProgID) (This)->lpVtbl->ImportComponent(This,bstrApplIDOrName,bstrCLSIDOrProgID)
#define ICOMAdminCatalog2_InstallComponent(This,bstrApplIDOrName,bstrDLL,bstrTLB,bstrPSDLL) (This)->lpVtbl->InstallComponent(This,bstrApplIDOrName,bstrDLL,bstrTLB,bstrPSDLL)
#define ICOMAdminCatalog2_ShutdownApplication(This,bstrApplIDOrName) (This)->lpVtbl->ShutdownApplication(This,bstrApplIDOrName)
#define ICOMAdminCatalog2_ExportApplication(This,bstrApplIDOrName,bstrApplicationFile,lOptions) (This)->lpVtbl->ExportApplication(This,bstrApplIDOrName,bstrApplicationFile,lOptions)
#define ICOMAdminCatalog2_InstallApplication(This,bstrApplicationFile,bstrDestinationDirectory,lOptions,bstrUserId,bstrPassword,bstrRSN) (This)->lpVtbl->InstallApplication(This,bstrApplicationFile,bstrDestinationDirectory,lOptions,bstrUserId,bstrPassword,bstrRSN)
#define ICOMAdminCatalog2_StopRouter(This) (This)->lpVtbl->StopRouter(This)
#define ICOMAdminCatalog2_RefreshRouter(This) (This)->lpVtbl->RefreshRouter(This)
#define ICOMAdminCatalog2_StartRouter(This) (This)->lpVtbl->StartRouter(This)
#define ICOMAdminCatalog2_Reserved1(This) (This)->lpVtbl->Reserved1(This)
#define ICOMAdminCatalog2_Reserved2(This) (This)->lpVtbl->Reserved2(This)
#define ICOMAdminCatalog2_InstallMultipleComponents(This,bstrApplIDOrName,ppsaVarFileNames,ppsaVarCLSIDs) (This)->lpVtbl->InstallMultipleComponents(This,bstrApplIDOrName,ppsaVarFileNames,ppsaVarCLSIDs)
#define ICOMAdminCatalog2_GetMultipleComponentsInfo(This,bstrApplIdOrName,ppsaVarFileNames,ppsaVarCLSIDs,ppsaVarClassNames,ppsaVarFileFlags,ppsaVarComponentFlags) (This)->lpVtbl->GetMultipleComponentsInfo(This,bstrApplIdOrName,ppsaVarFileNames,ppsaVarCLSIDs,ppsaVarClassNames,ppsaVarFileFlags,ppsaVarComponentFlags)
#define ICOMAdminCatalog2_RefreshComponents(This) (This)->lpVtbl->RefreshComponents(This)
#define ICOMAdminCatalog2_BackupREGDB(This,bstrBackupFilePath) (This)->lpVtbl->BackupREGDB(This,bstrBackupFilePath)
#define ICOMAdminCatalog2_RestoreREGDB(This,bstrBackupFilePath) (This)->lpVtbl->RestoreREGDB(This,bstrBackupFilePath)
#define ICOMAdminCatalog2_QueryApplicationFile(This,bstrApplicationFile,pbstrApplicationName,pbstrApplicationDescription,pbHasUsers,pbIsProxy,ppsaVarFileNames) (This)->lpVtbl->QueryApplicationFile(This,bstrApplicationFile,pbstrApplicationName,pbstrApplicationDescription,pbHasUsers,pbIsProxy,ppsaVarFileNames)
#define ICOMAdminCatalog2_StartApplication(This,bstrApplIdOrName) (This)->lpVtbl->StartApplication(This,bstrApplIdOrName)
#define ICOMAdminCatalog2_ServiceCheck(This,lService,plStatus) (This)->lpVtbl->ServiceCheck(This,lService,plStatus)
#define ICOMAdminCatalog2_InstallMultipleEventClasses(This,bstrApplIdOrName,ppsaVarFileNames,ppsaVarCLSIDS) (This)->lpVtbl->InstallMultipleEventClasses(This,bstrApplIdOrName,ppsaVarFileNames,ppsaVarCLSIDS)
#define ICOMAdminCatalog2_InstallEventClass(This,bstrApplIdOrName,bstrDLL,bstrTLB,bstrPSDLL) (This)->lpVtbl->InstallEventClass(This,bstrApplIdOrName,bstrDLL,bstrTLB,bstrPSDLL)
#define ICOMAdminCatalog2_GetEventClassesForIID(This,bstrIID,ppsaVarCLSIDs,ppsaVarProgIDs,ppsaVarDescriptions) (This)->lpVtbl->GetEventClassesForIID(This,bstrIID,ppsaVarCLSIDs,ppsaVarProgIDs,ppsaVarDescriptions)
#define ICOMAdminCatalog2_GetCollectionByQuery2(This,bstrCollectionName,pVarQueryStrings,ppCatalogCollection) (This)->lpVtbl->GetCollectionByQuery2(This,bstrCollectionName,pVarQueryStrings,ppCatalogCollection)
#define ICOMAdminCatalog2_GetApplicationInstanceIDFromProcessID(This,lProcessID,pbstrApplicationInstanceID) (This)->lpVtbl->GetApplicationInstanceIDFromProcessID(This,lProcessID,pbstrApplicationInstanceID)
#define ICOMAdminCatalog2_ShutdownApplicationInstances(This,pVarApplicationInstanceID) (This)->lpVtbl->ShutdownApplicationInstances(This,pVarApplicationInstanceID)
#define ICOMAdminCatalog2_PauseApplicationInstances(This,pVarApplicationInstanceID) (This)->lpVtbl->PauseApplicationInstances(This,pVarApplicationInstanceID)
#define ICOMAdminCatalog2_ResumeApplicationInstances(This,pVarApplicationInstanceID) (This)->lpVtbl->ResumeApplicationInstances(This,pVarApplicationInstanceID)
#define ICOMAdminCatalog2_RecycleApplicationInstances(This,pVarApplicationInstanceID,lReasonCode) (This)->lpVtbl->RecycleApplicationInstances(This,pVarApplicationInstanceID,lReasonCode)
#define ICOMAdminCatalog2_AreApplicationInstancesPaused(This,pVarApplicationInstanceID,pVarBoolPaused) (This)->lpVtbl->AreApplicationInstancesPaused(This,pVarApplicationInstanceID,pVarBoolPaused)
#define ICOMAdminCatalog2_DumpApplicationInstance(This,bstrApplicationInstanceID,bstrDirectory,lMaxImages,pbstrDumpFile) (This)->lpVtbl->DumpApplicationInstance(This,bstrApplicationInstanceID,bstrDirectory,lMaxImages,pbstrDumpFile)
#define ICOMAdminCatalog2_get_IsApplicationInstanceDumpSupported(This,pVarBoolDumpSupported) (This)->lpVtbl->get_IsApplicationInstanceDumpSupported(This,pVarBoolDumpSupported)
#define ICOMAdminCatalog2_CreateServiceForApplication(This,bstrApplicationIDOrName,bstrServiceName,bstrStartType,bstrErrorControl,bstrDependencies,bstrRunAs,bstrPassword,bDesktopOk) (This)->lpVtbl->CreateServiceForApplication(This,bstrApplicationIDOrName,bstrServiceName,bstrStartType,bstrErrorControl,bstrDependencies,bstrRunAs,bstrPassword,bDesktopOk)
#define ICOMAdminCatalog2_DeleteServiceForApplication(This,bstrApplicationIDOrName) (This)->lpVtbl->DeleteServiceForApplication(This,bstrApplicationIDOrName)
#define ICOMAdminCatalog2_GetPartitionID(This,bstrApplicationIDOrName,pbstrPartitionID) (This)->lpVtbl->GetPartitionID(This,bstrApplicationIDOrName,pbstrPartitionID)
#define ICOMAdminCatalog2_GetPartitionName(This,bstrApplicationIDOrName,pbstrPartitionName) (This)->lpVtbl->GetPartitionName(This,bstrApplicationIDOrName,pbstrPartitionName)
#define ICOMAdminCatalog2_put_CurrentPartition(This,bstrPartitionIDOrName) (This)->lpVtbl->put_CurrentPartition(This,bstrPartitionIDOrName)
#define ICOMAdminCatalog2_get_CurrentPartitionID(This,pbstrPartitionID) (This)->lpVtbl->get_CurrentPartitionID(This,pbstrPartitionID)
#define ICOMAdminCatalog2_get_CurrentPartitionName(This,pbstrPartitionName) (This)->lpVtbl->get_CurrentPartitionName(This,pbstrPartitionName)
#define ICOMAdminCatalog2_get_GlobalPartitionID(This,pbstrGlobalPartitionID) (This)->lpVtbl->get_GlobalPartitionID(This,pbstrGlobalPartitionID)
#define ICOMAdminCatalog2_FlushPartitionCache(This) (This)->lpVtbl->FlushPartitionCache(This)
#define ICOMAdminCatalog2_CopyApplications(This,bstrSourcePartitionIDOrName,pVarApplicationID,bstrDestinationPartitionIDOrName) (This)->lpVtbl->CopyApplications(This,bstrSourcePartitionIDOrName,pVarApplicationID,bstrDestinationPartitionIDOrName)
#define ICOMAdminCatalog2_CopyComponents(This,bstrSourceApplicationIDOrName,pVarCLSIDOrProgID,bstrDestinationApplicationIDOrName) (This)->lpVtbl->CopyComponents(This,bstrSourceApplicationIDOrName,pVarCLSIDOrProgID,bstrDestinationApplicationIDOrName)
#define ICOMAdminCatalog2_MoveComponents(This,bstrSourceApplicationIDOrName,pVarCLSIDOrProgID,bstrDestinationApplicationIDOrName) (This)->lpVtbl->MoveComponents(This,bstrSourceApplicationIDOrName,pVarCLSIDOrProgID,bstrDestinationApplicationIDOrName)
#define ICOMAdminCatalog2_AliasComponent(This,bstrSrcApplicationIDOrName,bstrCLSIDOrProgID,bstrDestApplicationIDOrName,bstrNewProgId,bstrNewClsid) (This)->lpVtbl->AliasComponent(This,bstrSrcApplicationIDOrName,bstrCLSIDOrProgID,bstrDestApplicationIDOrName,bstrNewProgId,bstrNewClsid)
#define ICOMAdminCatalog2_IsSafeToDelete(This,bstrDllName,pCOMAdminInUse) (This)->lpVtbl->IsSafeToDelete(This,bstrDllName,pCOMAdminInUse)
#define ICOMAdminCatalog2_ImportUnconfiguredComponents(This,bstrApplicationIDOrName,pVarCLSIDOrProgID,pVarComponentType) (This)->lpVtbl->ImportUnconfiguredComponents(This,bstrApplicationIDOrName,pVarCLSIDOrProgID,pVarComponentType)
#define ICOMAdminCatalog2_PromoteUnconfiguredComponents(This,bstrApplicationIDOrName,pVarCLSIDOrProgID,pVarComponentType) (This)->lpVtbl->PromoteUnconfiguredComponents(This,bstrApplicationIDOrName,pVarCLSIDOrProgID,pVarComponentType)
#define ICOMAdminCatalog2_ImportComponents(This,bstrApplicationIDOrName,pVarCLSIDOrProgID,pVarComponentType) (This)->lpVtbl->ImportComponents(This,bstrApplicationIDOrName,pVarCLSIDOrProgID,pVarComponentType)
#define ICOMAdminCatalog2_get_Is64BitCatalogServer(This,pbIs64Bit) (This)->lpVtbl->get_Is64BitCatalogServer(This,pbIs64Bit)
#define ICOMAdminCatalog2_ExportPartition(This,bstrPartitionIDOrName,bstrPartitionFileName,lOptions) (This)->lpVtbl->ExportPartition(This,bstrPartitionIDOrName,bstrPartitionFileName,lOptions)
#define ICOMAdminCatalog2_InstallPartition(This,bstrFileName,bstrDestDirectory,lOptions,bstrUserID,bstrPassword,bstrRSN) (This)->lpVtbl->InstallPartition(This,bstrFileName,bstrDestDirectory,lOptions,bstrUserID,bstrPassword,bstrRSN)
#define ICOMAdminCatalog2_QueryApplicationFile2(This,bstrApplicationFile,ppFilesForImport) (This)->lpVtbl->QueryApplicationFile2(This,bstrApplicationFile,ppFilesForImport)
#define ICOMAdminCatalog2_GetComponentVersionCount(This,bstrCLSIDOrProgID,plVersionCount) (This)->lpVtbl->GetComponentVersionCount(This,bstrCLSIDOrProgID,plVersionCount)
#endif
#endif
  HRESULT WINAPI ICOMAdminCatalog2_GetCollectionByQuery2_Proxy(ICOMAdminCatalog2 *This,BSTR bstrCollectionName,VARIANT *pVarQueryStrings,IDispatch **ppCatalogCollection);
  void __RPC_STUB ICOMAdminCatalog2_GetCollectionByQuery2_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_GetApplicationInstanceIDFromProcessID_Proxy(ICOMAdminCatalog2 *This,long lProcessID,BSTR *pbstrApplicationInstanceID);
  void __RPC_STUB ICOMAdminCatalog2_GetApplicationInstanceIDFromProcessID_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_ShutdownApplicationInstances_Proxy(ICOMAdminCatalog2 *This,VARIANT *pVarApplicationInstanceID);
  void __RPC_STUB ICOMAdminCatalog2_ShutdownApplicationInstances_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_PauseApplicationInstances_Proxy(ICOMAdminCatalog2 *This,VARIANT *pVarApplicationInstanceID);
  void __RPC_STUB ICOMAdminCatalog2_PauseApplicationInstances_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_ResumeApplicationInstances_Proxy(ICOMAdminCatalog2 *This,VARIANT *pVarApplicationInstanceID);
  void __RPC_STUB ICOMAdminCatalog2_ResumeApplicationInstances_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_RecycleApplicationInstances_Proxy(ICOMAdminCatalog2 *This,VARIANT *pVarApplicationInstanceID,long lReasonCode);
  void __RPC_STUB ICOMAdminCatalog2_RecycleApplicationInstances_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_AreApplicationInstancesPaused_Proxy(ICOMAdminCatalog2 *This,VARIANT *pVarApplicationInstanceID,VARIANT_BOOL *pVarBoolPaused);
  void __RPC_STUB ICOMAdminCatalog2_AreApplicationInstancesPaused_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_DumpApplicationInstance_Proxy(ICOMAdminCatalog2 *This,BSTR bstrApplicationInstanceID,BSTR bstrDirectory,long lMaxImages,BSTR *pbstrDumpFile);
  void __RPC_STUB ICOMAdminCatalog2_DumpApplicationInstance_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_get_IsApplicationInstanceDumpSupported_Proxy(ICOMAdminCatalog2 *This,VARIANT_BOOL *pVarBoolDumpSupported);
  void __RPC_STUB ICOMAdminCatalog2_get_IsApplicationInstanceDumpSupported_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_CreateServiceForApplication_Proxy(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName,BSTR bstrServiceName,BSTR bstrStartType,BSTR bstrErrorControl,BSTR bstrDependencies,BSTR bstrRunAs,BSTR bstrPassword,VARIANT_BOOL bDesktopOk);
  void __RPC_STUB ICOMAdminCatalog2_CreateServiceForApplication_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_DeleteServiceForApplication_Proxy(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName);
  void __RPC_STUB ICOMAdminCatalog2_DeleteServiceForApplication_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_GetPartitionID_Proxy(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName,BSTR *pbstrPartitionID);
  void __RPC_STUB ICOMAdminCatalog2_GetPartitionID_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_GetPartitionName_Proxy(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName,BSTR *pbstrPartitionName);
  void __RPC_STUB ICOMAdminCatalog2_GetPartitionName_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_put_CurrentPartition_Proxy(ICOMAdminCatalog2 *This,BSTR bstrPartitionIDOrName);
  void __RPC_STUB ICOMAdminCatalog2_put_CurrentPartition_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_get_CurrentPartitionID_Proxy(ICOMAdminCatalog2 *This,BSTR *pbstrPartitionID);
  void __RPC_STUB ICOMAdminCatalog2_get_CurrentPartitionID_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_get_CurrentPartitionName_Proxy(ICOMAdminCatalog2 *This,BSTR *pbstrPartitionName);
  void __RPC_STUB ICOMAdminCatalog2_get_CurrentPartitionName_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_get_GlobalPartitionID_Proxy(ICOMAdminCatalog2 *This,BSTR *pbstrGlobalPartitionID);
  void __RPC_STUB ICOMAdminCatalog2_get_GlobalPartitionID_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_FlushPartitionCache_Proxy(ICOMAdminCatalog2 *This);
  void __RPC_STUB ICOMAdminCatalog2_FlushPartitionCache_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_CopyApplications_Proxy(ICOMAdminCatalog2 *This,BSTR bstrSourcePartitionIDOrName,VARIANT *pVarApplicationID,BSTR bstrDestinationPartitionIDOrName);
  void __RPC_STUB ICOMAdminCatalog2_CopyApplications_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_CopyComponents_Proxy(ICOMAdminCatalog2 *This,BSTR bstrSourceApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,BSTR bstrDestinationApplicationIDOrName);
  void __RPC_STUB ICOMAdminCatalog2_CopyComponents_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_MoveComponents_Proxy(ICOMAdminCatalog2 *This,BSTR bstrSourceApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,BSTR bstrDestinationApplicationIDOrName);
  void __RPC_STUB ICOMAdminCatalog2_MoveComponents_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_AliasComponent_Proxy(ICOMAdminCatalog2 *This,BSTR bstrSrcApplicationIDOrName,BSTR bstrCLSIDOrProgID,BSTR bstrDestApplicationIDOrName,BSTR bstrNewProgId,BSTR bstrNewClsid);
  void __RPC_STUB ICOMAdminCatalog2_AliasComponent_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_IsSafeToDelete_Proxy(ICOMAdminCatalog2 *This,BSTR bstrDllName,COMAdminInUse *pCOMAdminInUse);
  void __RPC_STUB ICOMAdminCatalog2_IsSafeToDelete_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_ImportUnconfiguredComponents_Proxy(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,VARIANT *pVarComponentType);
  void __RPC_STUB ICOMAdminCatalog2_ImportUnconfiguredComponents_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_PromoteUnconfiguredComponents_Proxy(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,VARIANT *pVarComponentType);
  void __RPC_STUB ICOMAdminCatalog2_PromoteUnconfiguredComponents_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_ImportComponents_Proxy(ICOMAdminCatalog2 *This,BSTR bstrApplicationIDOrName,VARIANT *pVarCLSIDOrProgID,VARIANT *pVarComponentType);
  void __RPC_STUB ICOMAdminCatalog2_ImportComponents_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_get_Is64BitCatalogServer_Proxy(ICOMAdminCatalog2 *This,VARIANT_BOOL *pbIs64Bit);
  void __RPC_STUB ICOMAdminCatalog2_get_Is64BitCatalogServer_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_ExportPartition_Proxy(ICOMAdminCatalog2 *This,BSTR bstrPartitionIDOrName,BSTR bstrPartitionFileName,long lOptions);
  void __RPC_STUB ICOMAdminCatalog2_ExportPartition_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_InstallPartition_Proxy(ICOMAdminCatalog2 *This,BSTR bstrFileName,BSTR bstrDestDirectory,long lOptions,BSTR bstrUserID,BSTR bstrPassword,BSTR bstrRSN);
  void __RPC_STUB ICOMAdminCatalog2_InstallPartition_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_QueryApplicationFile2_Proxy(ICOMAdminCatalog2 *This,BSTR bstrApplicationFile,IDispatch **ppFilesForImport);
  void __RPC_STUB ICOMAdminCatalog2_QueryApplicationFile2_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICOMAdminCatalog2_GetComponentVersionCount_Proxy(ICOMAdminCatalog2 *This,BSTR bstrCLSIDOrProgID,long *plVersionCount);
  void __RPC_STUB ICOMAdminCatalog2_GetComponentVersionCount_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __ICatalogObject_INTERFACE_DEFINED__
#define __ICatalogObject_INTERFACE_DEFINED__
  EXTERN_C const IID IID_ICatalogObject;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct ICatalogObject : public IDispatch {
  public:
    virtual HRESULT WINAPI get_Value(BSTR bstrPropName,VARIANT *pvarRetVal) = 0;
    virtual HRESULT WINAPI put_Value(BSTR bstrPropName,VARIANT val) = 0;
    virtual HRESULT WINAPI get_Key(VARIANT *pvarRetVal) = 0;
    virtual HRESULT WINAPI get_Name(VARIANT *pvarRetVal) = 0;
    virtual HRESULT WINAPI IsPropertyReadOnly(BSTR bstrPropName,VARIANT_BOOL *pbRetVal) = 0;
    virtual HRESULT WINAPI get_Valid(VARIANT_BOOL *pbRetVal) = 0;
    virtual HRESULT WINAPI IsPropertyWriteOnly(BSTR bstrPropName,VARIANT_BOOL *pbRetVal) = 0;
  };
#else
  typedef struct ICatalogObjectVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(ICatalogObject *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(ICatalogObject *This);
      ULONG (WINAPI *Release)(ICatalogObject *This);
      HRESULT (WINAPI *GetTypeInfoCount)(ICatalogObject *This,UINT *pctinfo);
      HRESULT (WINAPI *GetTypeInfo)(ICatalogObject *This,UINT iTInfo,LCID lcid,ITypeInfo **ppTInfo);
      HRESULT (WINAPI *GetIDsOfNames)(ICatalogObject *This,REFIID riid,LPOLESTR *rgszNames,UINT cNames,LCID lcid,DISPID *rgDispId);
      HRESULT (WINAPI *Invoke)(ICatalogObject *This,DISPID dispIdMember,REFIID riid,LCID lcid,WORD wFlags,DISPPARAMS *pDispParams,VARIANT *pVarResult,EXCEPINFO *pExcepInfo,UINT *puArgErr);
      HRESULT (WINAPI *get_Value)(ICatalogObject *This,BSTR bstrPropName,VARIANT *pvarRetVal);
      HRESULT (WINAPI *put_Value)(ICatalogObject *This,BSTR bstrPropName,VARIANT val);
      HRESULT (WINAPI *get_Key)(ICatalogObject *This,VARIANT *pvarRetVal);
      HRESULT (WINAPI *get_Name)(ICatalogObject *This,VARIANT *pvarRetVal);
      HRESULT (WINAPI *IsPropertyReadOnly)(ICatalogObject *This,BSTR bstrPropName,VARIANT_BOOL *pbRetVal);
      HRESULT (WINAPI *get_Valid)(ICatalogObject *This,VARIANT_BOOL *pbRetVal);
      HRESULT (WINAPI *IsPropertyWriteOnly)(ICatalogObject *This,BSTR bstrPropName,VARIANT_BOOL *pbRetVal);
    END_INTERFACE
  } ICatalogObjectVtbl;
  struct ICatalogObject {
    CONST_VTBL struct ICatalogObjectVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define ICatalogObject_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define ICatalogObject_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICatalogObject_Release(This) (This)->lpVtbl->Release(This)
#define ICatalogObject_GetTypeInfoCount(This,pctinfo) (This)->lpVtbl->GetTypeInfoCount(This,pctinfo)
#define ICatalogObject_GetTypeInfo(This,iTInfo,lcid,ppTInfo) (This)->lpVtbl->GetTypeInfo(This,iTInfo,lcid,ppTInfo)
#define ICatalogObject_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) (This)->lpVtbl->GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)
#define ICatalogObject_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) (This)->lpVtbl->Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)
#define ICatalogObject_get_Value(This,bstrPropName,pvarRetVal) (This)->lpVtbl->get_Value(This,bstrPropName,pvarRetVal)
#define ICatalogObject_put_Value(This,bstrPropName,val) (This)->lpVtbl->put_Value(This,bstrPropName,val)
#define ICatalogObject_get_Key(This,pvarRetVal) (This)->lpVtbl->get_Key(This,pvarRetVal)
#define ICatalogObject_get_Name(This,pvarRetVal) (This)->lpVtbl->get_Name(This,pvarRetVal)
#define ICatalogObject_IsPropertyReadOnly(This,bstrPropName,pbRetVal) (This)->lpVtbl->IsPropertyReadOnly(This,bstrPropName,pbRetVal)
#define ICatalogObject_get_Valid(This,pbRetVal) (This)->lpVtbl->get_Valid(This,pbRetVal)
#define ICatalogObject_IsPropertyWriteOnly(This,bstrPropName,pbRetVal) (This)->lpVtbl->IsPropertyWriteOnly(This,bstrPropName,pbRetVal)
#endif
#endif
  HRESULT WINAPI ICatalogObject_get_Value_Proxy(ICatalogObject *This,BSTR bstrPropName,VARIANT *pvarRetVal);
  void __RPC_STUB ICatalogObject_get_Value_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogObject_put_Value_Proxy(ICatalogObject *This,BSTR bstrPropName,VARIANT val);
  void __RPC_STUB ICatalogObject_put_Value_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogObject_get_Key_Proxy(ICatalogObject *This,VARIANT *pvarRetVal);
  void __RPC_STUB ICatalogObject_get_Key_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogObject_get_Name_Proxy(ICatalogObject *This,VARIANT *pvarRetVal);
  void __RPC_STUB ICatalogObject_get_Name_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogObject_IsPropertyReadOnly_Proxy(ICatalogObject *This,BSTR bstrPropName,VARIANT_BOOL *pbRetVal);
  void __RPC_STUB ICatalogObject_IsPropertyReadOnly_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogObject_get_Valid_Proxy(ICatalogObject *This,VARIANT_BOOL *pbRetVal);
  void __RPC_STUB ICatalogObject_get_Valid_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogObject_IsPropertyWriteOnly_Proxy(ICatalogObject *This,BSTR bstrPropName,VARIANT_BOOL *pbRetVal);
  void __RPC_STUB ICatalogObject_IsPropertyWriteOnly_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __ICatalogCollection_INTERFACE_DEFINED__
#define __ICatalogCollection_INTERFACE_DEFINED__
  EXTERN_C const IID IID_ICatalogCollection;
#if defined(__cplusplus) && !defined(CINTERFACE)
  struct ICatalogCollection : public IDispatch {
  public:
    virtual HRESULT WINAPI get__NewEnum(IUnknown **ppEnumVariant) = 0;
    virtual HRESULT WINAPI get_Item(long lIndex,IDispatch **ppCatalogObject) = 0;
    virtual HRESULT WINAPI get_Count(long *plObjectCount) = 0;
    virtual HRESULT WINAPI Remove(long lIndex) = 0;
    virtual HRESULT WINAPI Add(IDispatch **ppCatalogObject) = 0;
    virtual HRESULT WINAPI Populate(void) = 0;
    virtual HRESULT WINAPI SaveChanges(long *pcChanges) = 0;
    virtual HRESULT WINAPI GetCollection(BSTR bstrCollName,VARIANT varObjectKey,IDispatch **ppCatalogCollection) = 0;
    virtual HRESULT WINAPI get_Name(VARIANT *pVarNamel) = 0;
    virtual HRESULT WINAPI get_AddEnabled(VARIANT_BOOL *pVarBool) = 0;
    virtual HRESULT WINAPI get_RemoveEnabled(VARIANT_BOOL *pVarBool) = 0;
    virtual HRESULT WINAPI GetUtilInterface(IDispatch **ppIDispatch) = 0;
    virtual HRESULT WINAPI get_DataStoreMajorVersion(long *plMajorVersion) = 0;
    virtual HRESULT WINAPI get_DataStoreMinorVersion(long *plMinorVersionl) = 0;
    virtual HRESULT WINAPI PopulateByKey(SAFEARRAY *psaKeys) = 0;
    virtual HRESULT WINAPI PopulateByQuery(BSTR bstrQueryString,long lQueryType) = 0;
  };
#else
  typedef struct ICatalogCollectionVtbl {
    BEGIN_INTERFACE
      HRESULT (WINAPI *QueryInterface)(ICatalogCollection *This,REFIID riid,void **ppvObject);
      ULONG (WINAPI *AddRef)(ICatalogCollection *This);
      ULONG (WINAPI *Release)(ICatalogCollection *This);
      HRESULT (WINAPI *GetTypeInfoCount)(ICatalogCollection *This,UINT *pctinfo);
      HRESULT (WINAPI *GetTypeInfo)(ICatalogCollection *This,UINT iTInfo,LCID lcid,ITypeInfo **ppTInfo);
      HRESULT (WINAPI *GetIDsOfNames)(ICatalogCollection *This,REFIID riid,LPOLESTR *rgszNames,UINT cNames,LCID lcid,DISPID *rgDispId);
      HRESULT (WINAPI *Invoke)(ICatalogCollection *This,DISPID dispIdMember,REFIID riid,LCID lcid,WORD wFlags,DISPPARAMS *pDispParams,VARIANT *pVarResult,EXCEPINFO *pExcepInfo,UINT *puArgErr);
      HRESULT (WINAPI *get__NewEnum)(ICatalogCollection *This,IUnknown **ppEnumVariant);
      HRESULT (WINAPI *get_Item)(ICatalogCollection *This,long lIndex,IDispatch **ppCatalogObject);
      HRESULT (WINAPI *get_Count)(ICatalogCollection *This,long *plObjectCount);
      HRESULT (WINAPI *Remove)(ICatalogCollection *This,long lIndex);
      HRESULT (WINAPI *Add)(ICatalogCollection *This,IDispatch **ppCatalogObject);
      HRESULT (WINAPI *Populate)(ICatalogCollection *This);
      HRESULT (WINAPI *SaveChanges)(ICatalogCollection *This,long *pcChanges);
      HRESULT (WINAPI *GetCollection)(ICatalogCollection *This,BSTR bstrCollName,VARIANT varObjectKey,IDispatch **ppCatalogCollection);
      HRESULT (WINAPI *get_Name)(ICatalogCollection *This,VARIANT *pVarNamel);
      HRESULT (WINAPI *get_AddEnabled)(ICatalogCollection *This,VARIANT_BOOL *pVarBool);
      HRESULT (WINAPI *get_RemoveEnabled)(ICatalogCollection *This,VARIANT_BOOL *pVarBool);
      HRESULT (WINAPI *GetUtilInterface)(ICatalogCollection *This,IDispatch **ppIDispatch);
      HRESULT (WINAPI *get_DataStoreMajorVersion)(ICatalogCollection *This,long *plMajorVersion);
      HRESULT (WINAPI *get_DataStoreMinorVersion)(ICatalogCollection *This,long *plMinorVersionl);
      HRESULT (WINAPI *PopulateByKey)(ICatalogCollection *This,SAFEARRAY *psaKeys);
      HRESULT (WINAPI *PopulateByQuery)(ICatalogCollection *This,BSTR bstrQueryString,long lQueryType);
    END_INTERFACE
  } ICatalogCollectionVtbl;
  struct ICatalogCollection {
    CONST_VTBL struct ICatalogCollectionVtbl *lpVtbl;
  };
#ifdef COBJMACROS
#define ICatalogCollection_QueryInterface(This,riid,ppvObject) (This)->lpVtbl->QueryInterface(This,riid,ppvObject)
#define ICatalogCollection_AddRef(This) (This)->lpVtbl->AddRef(This)
#define ICatalogCollection_Release(This) (This)->lpVtbl->Release(This)
#define ICatalogCollection_GetTypeInfoCount(This,pctinfo) (This)->lpVtbl->GetTypeInfoCount(This,pctinfo)
#define ICatalogCollection_GetTypeInfo(This,iTInfo,lcid,ppTInfo) (This)->lpVtbl->GetTypeInfo(This,iTInfo,lcid,ppTInfo)
#define ICatalogCollection_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) (This)->lpVtbl->GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)
#define ICatalogCollection_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) (This)->lpVtbl->Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)
#define ICatalogCollection_get__NewEnum(This,ppEnumVariant) (This)->lpVtbl->get__NewEnum(This,ppEnumVariant)
#define ICatalogCollection_get_Item(This,lIndex,ppCatalogObject) (This)->lpVtbl->get_Item(This,lIndex,ppCatalogObject)
#define ICatalogCollection_get_Count(This,plObjectCount) (This)->lpVtbl->get_Count(This,plObjectCount)
#define ICatalogCollection_Remove(This,lIndex) (This)->lpVtbl->Remove(This,lIndex)
#define ICatalogCollection_Add(This,ppCatalogObject) (This)->lpVtbl->Add(This,ppCatalogObject)
#define ICatalogCollection_Populate(This) (This)->lpVtbl->Populate(This)
#define ICatalogCollection_SaveChanges(This,pcChanges) (This)->lpVtbl->SaveChanges(This,pcChanges)
#define ICatalogCollection_GetCollection(This,bstrCollName,varObjectKey,ppCatalogCollection) (This)->lpVtbl->GetCollection(This,bstrCollName,varObjectKey,ppCatalogCollection)
#define ICatalogCollection_get_Name(This,pVarNamel) (This)->lpVtbl->get_Name(This,pVarNamel)
#define ICatalogCollection_get_AddEnabled(This,pVarBool) (This)->lpVtbl->get_AddEnabled(This,pVarBool)
#define ICatalogCollection_get_RemoveEnabled(This,pVarBool) (This)->lpVtbl->get_RemoveEnabled(This,pVarBool)
#define ICatalogCollection_GetUtilInterface(This,ppIDispatch) (This)->lpVtbl->GetUtilInterface(This,ppIDispatch)
#define ICatalogCollection_get_DataStoreMajorVersion(This,plMajorVersion) (This)->lpVtbl->get_DataStoreMajorVersion(This,plMajorVersion)
#define ICatalogCollection_get_DataStoreMinorVersion(This,plMinorVersionl) (This)->lpVtbl->get_DataStoreMinorVersion(This,plMinorVersionl)
#define ICatalogCollection_PopulateByKey(This,psaKeys) (This)->lpVtbl->PopulateByKey(This,psaKeys)
#define ICatalogCollection_PopulateByQuery(This,bstrQueryString,lQueryType) (This)->lpVtbl->PopulateByQuery(This,bstrQueryString,lQueryType)
#endif
#endif
  HRESULT WINAPI ICatalogCollection_get__NewEnum_Proxy(ICatalogCollection *This,IUnknown **ppEnumVariant);
  void __RPC_STUB ICatalogCollection_get__NewEnum_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_get_Item_Proxy(ICatalogCollection *This,long lIndex,IDispatch **ppCatalogObject);
  void __RPC_STUB ICatalogCollection_get_Item_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_get_Count_Proxy(ICatalogCollection *This,long *plObjectCount);
  void __RPC_STUB ICatalogCollection_get_Count_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_Remove_Proxy(ICatalogCollection *This,long lIndex);
  void __RPC_STUB ICatalogCollection_Remove_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_Add_Proxy(ICatalogCollection *This,IDispatch **ppCatalogObject);
  void __RPC_STUB ICatalogCollection_Add_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_Populate_Proxy(ICatalogCollection *This);
  void __RPC_STUB ICatalogCollection_Populate_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_SaveChanges_Proxy(ICatalogCollection *This,long *pcChanges);
  void __RPC_STUB ICatalogCollection_SaveChanges_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_GetCollection_Proxy(ICatalogCollection *This,BSTR bstrCollName,VARIANT varObjectKey,IDispatch **ppCatalogCollection);
  void __RPC_STUB ICatalogCollection_GetCollection_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_get_Name_Proxy(ICatalogCollection *This,VARIANT *pVarNamel);
  void __RPC_STUB ICatalogCollection_get_Name_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_get_AddEnabled_Proxy(ICatalogCollection *This,VARIANT_BOOL *pVarBool);
  void __RPC_STUB ICatalogCollection_get_AddEnabled_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_get_RemoveEnabled_Proxy(ICatalogCollection *This,VARIANT_BOOL *pVarBool);
  void __RPC_STUB ICatalogCollection_get_RemoveEnabled_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_GetUtilInterface_Proxy(ICatalogCollection *This,IDispatch **ppIDispatch);
  void __RPC_STUB ICatalogCollection_GetUtilInterface_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_get_DataStoreMajorVersion_Proxy(ICatalogCollection *This,long *plMajorVersion);
  void __RPC_STUB ICatalogCollection_get_DataStoreMajorVersion_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_get_DataStoreMinorVersion_Proxy(ICatalogCollection *This,long *plMinorVersionl);
  void __RPC_STUB ICatalogCollection_get_DataStoreMinorVersion_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_PopulateByKey_Proxy(ICatalogCollection *This,SAFEARRAY *psaKeys);
  void __RPC_STUB ICatalogCollection_PopulateByKey_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
  HRESULT WINAPI ICatalogCollection_PopulateByQuery_Proxy(ICatalogCollection *This,BSTR bstrQueryString,long lQueryType);
  void __RPC_STUB ICatalogCollection_PopulateByQuery_Stub(IRpcStubBuffer *This,IRpcChannelBuffer *_pRpcChannelBuffer,PRPC_MESSAGE _pRpcMessage,DWORD *_pdwStubPhase);
#endif

#ifndef __COMAdmin_LIBRARY_DEFINED__
#define __COMAdmin_LIBRARY_DEFINED__
  typedef enum COMAdminComponentType {
    COMAdmin32BitComponent = 0x1,COMAdmin64BitComponent = 0x2
  } COMAdminComponentType;

  typedef enum COMAdminApplicationInstallOptions {
    COMAdminInstallNoUsers = 0,COMAdminInstallUsers = 1,COMAdminInstallForceOverwriteOfFiles = 2
  } COMAdminApplicationInstallOptions;

  typedef enum COMAdminApplicationExportOptions {
    COMAdminExportNoUsers = 0,COMAdminExportUsers = 1,COMAdminExportApplicationProxy = 2,COMAdminExportForceOverwriteOfFiles = 4,
    COMAdminExportIn10Format = 16
  } COMAdminApplicationExportOptions;

  typedef enum COMAdminThreadingModels {
    COMAdminThreadingModelApartment = 0,COMAdminThreadingModelFree = 1,COMAdminThreadingModelMain = 2,COMAdminThreadingModelBoth = 3,
    COMAdminThreadingModelNeutral = 4,COMAdminThreadingModelNotSpecified = 5
  } COMAdminThreadingModels;

  typedef enum COMAdminTransactionOptions {
    COMAdminTransactionIgnored = 0,COMAdminTransactionNone = 1,COMAdminTransactionSupported = 2,COMAdminTransactionRequired = 3,
    COMAdminTransactionRequiresNew = 4
  } COMAdminTransactionOptions;

  typedef enum COMAdminTxIsolationLevelOptions {
    COMAdminTxIsolationLevelAny = 0,COMAdminTxIsolationLevelReadUnCommitted = COMAdminTxIsolationLevelAny + 1,
    COMAdminTxIsolationLevelReadCommitted = COMAdminTxIsolationLevelReadUnCommitted + 1,
    COMAdminTxIsolationLevelRepeatableRead = COMAdminTxIsolationLevelReadCommitted + 1,
    COMAdminTxIsolationLevelSerializable = COMAdminTxIsolationLevelRepeatableRead + 1
  } COMAdminTxIsolationLevelOptions;

  typedef enum COMAdminSynchronizationOptions {
    COMAdminSynchronizationIgnored = 0,COMAdminSynchronizationNone = 1,COMAdminSynchronizationSupported = 2,COMAdminSynchronizationRequired = 3,
    COMAdminSynchronizationRequiresNew = 4
  } COMAdminSynchronizationOptions;

  typedef enum COMAdminActivationOptions {
    COMAdminActivationInproc = 0,COMAdminActivationLocal = 1
  } COMAdminActivationOptions;

  typedef enum COMAdminAccessChecksLevelOptions {
    COMAdminAccessChecksApplicationLevel = 0,COMAdminAccessChecksApplicationComponentLevel = 1
  } COMAdminAccessChecksLevelOptions;

  typedef enum COMAdminAuthenticationLevelOptions {
    COMAdminAuthenticationDefault = 0,COMAdminAuthenticationNone = 1,COMAdminAuthenticationConnect = 2,COMAdminAuthenticationCall = 3,
    COMAdminAuthenticationPacket = 4,COMAdminAuthenticationIntegrity = 5,COMAdminAuthenticationPrivacy = 6
  } COMAdminAuthenticationLevelOptions;

  typedef enum COMAdminImpersonationLevelOptions {
    COMAdminImpersonationAnonymous = 1,COMAdminImpersonationIdentify = 2,COMAdminImpersonationImpersonate = 3,COMAdminImpersonationDelegate = 4
  } COMAdminImpersonationLevelOptions;

  typedef enum COMAdminAuthenticationCapabilitiesOptions {
    COMAdminAuthenticationCapabilitiesNone = 0,COMAdminAuthenticationCapabilitiesSecureReference = 0x2,
    COMAdminAuthenticationCapabilitiesStaticCloaking = 0x20,COMAdminAuthenticationCapabilitiesDynamicCloaking = 0x40
  } COMAdminAuthenticationCapabilitiesOptions;

  typedef enum COMAdminOS {
    COMAdminOSNotInitialized = 0,COMAdminOSWindows3_1 = 1,COMAdminOSWindows9x = 2,COMAdminOSWindows2000 = 3,
    COMAdminOSWindows2000AdvancedServer = 4,COMAdminOSWindows2000Unknown = 5,COMAdminOSUnknown = 6,COMAdminOSWindowsXPPersonal = 11,
    COMAdminOSWindowsXPProfessional = 12,COMAdminOSWindowsNETStandardServer = 13,COMAdminOSWindowsNETEnterpriseServer = 14,
    COMAdminOSWindowsNETDatacenterServer = 15,COMAdminOSWindowsNETWebServer = 16,COMAdminOSWindowsLonghornPersonal = 17,
    COMAdminOSWindowsLonghornProfessional = 18,COMAdminOSWindowsLonghornStandardServer = 19,COMAdminOSWindowsLonghornEnterpriseServer = 20,
    COMAdminOSWindowsLonghornDatacenterServer = 21,COMAdminOSWindowsLonghornWebServer = 22
  } COMAdminOS;

  typedef enum COMAdminServiceOptions {
    COMAdminServiceLoadBalanceRouter = 1
  } COMAdminServiceOptions;

  typedef enum COMAdminServiceStatusOptions {
    COMAdminServiceStopped = 0,COMAdminServiceStartPending = COMAdminServiceStopped + 1,COMAdminServiceStopPending = COMAdminServiceStartPending + 1,
    COMAdminServiceRunning = COMAdminServiceStopPending + 1,COMAdminServiceContinuePending = COMAdminServiceRunning + 1,
    COMAdminServicePausePending = COMAdminServiceContinuePending + 1,COMAdminServicePaused = COMAdminServicePausePending + 1,
    COMAdminServiceUnknownState = COMAdminServicePaused + 1
  } COMAdminServiceStatusOptions;

  typedef enum COMAdminQCMessageAuthenticateOptions {
    COMAdminQCMessageAuthenticateSecureApps = 0,COMAdminQCMessageAuthenticateOff = 1,COMAdminQCMessageAuthenticateOn = 2
  } COMAdminQCMessageAuthenticateOptions;

  typedef enum COMAdminFileFlags {
    COMAdminFileFlagLoadable = 0x1,COMAdminFileFlagCOM = 0x2,COMAdminFileFlagContainsPS = 0x4,COMAdminFileFlagContainsComp = 0x8,
    COMAdminFileFlagContainsTLB = 0x10,COMAdminFileFlagSelfReg = 0x20,COMAdminFileFlagSelfUnReg = 0x40,COMAdminFileFlagUnloadableDLL = 0x80,
    COMAdminFileFlagDoesNotExist = 0x100,COMAdminFileFlagAlreadyInstalled = 0x200,COMAdminFileFlagBadTLB = 0x400,
    COMAdminFileFlagGetClassObjFailed = 0x800,COMAdminFileFlagClassNotAvailable = 0x1000,COMAdminFileFlagRegistrar = 0x2000,
    COMAdminFileFlagNoRegistrar = 0x4000,COMAdminFileFlagDLLRegsvrFailed = 0x8000,COMAdminFileFlagRegTLBFailed = 0x10000,
    COMAdminFileFlagRegistrarFailed = 0x20000,COMAdminFileFlagError = 0x40000
  } COMAdminFileFlags;

  typedef enum COMAdminComponentFlags {
    COMAdminCompFlagTypeInfoFound = 0x1,COMAdminCompFlagCOMPlusPropertiesFound = 0x2,COMAdminCompFlagProxyFound = 0x4,
    COMAdminCompFlagInterfacesFound = 0x8,COMAdminCompFlagAlreadyInstalled = 0x10,COMAdminCompFlagNotInApplication = 0x20
  } COMAdminComponentFlags;

#define COMAdminCollectionRoot ("Root")
#define COMAdminCollectionApplications ("Applications")
#define COMAdminCollectionComponents ("Components")
#define COMAdminCollectionComputerList ("ComputerList")
#define COMAdminCollectionApplicationCluster ("ApplicationCluster")
#define COMAdminCollectionLocalComputer ("LocalComputer")
#define COMAdminCollectionInprocServers ("InprocServers")
#define COMAdminCollectionRelatedCollectionInfo ("RelatedCollectionInfo")
#define COMAdminCollectionPropertyInfo ("PropertyInfo")
#define COMAdminCollectionRoles ("Roles")
#define COMAdminCollectionErrorInfo ("ErrorInfo")
#define COMAdminCollectionInterfacesForComponent ("InterfacesForComponent")
#define COMAdminCollectionRolesForComponent ("RolesForComponent")
#define COMAdminCollectionMethodsForInterface ("MethodsForInterface")
#define COMAdminCollectionRolesForInterface ("RolesForInterface")
#define COMAdminCollectionRolesForMethod ("RolesForMethod")
#define COMAdminCollectionUsersInRole ("UsersInRole")
#define COMAdminCollectionDCOMProtocols ("DCOMProtocols")
#define COMAdminCollectionPartitions ("Partitions")

  typedef enum COMAdminErrorCodes {
    COMAdminErrObjectErrors = (HRESULT)0x80110401L,COMAdminErrObjectInvalid = (HRESULT)0x80110402L,COMAdminErrKeyMissing = (HRESULT)0x80110403L,
    COMAdminErrAlreadyInstalled = (HRESULT)0x80110404L,COMAdminErrAppFileWriteFail = (HRESULT)0x80110407L,
    COMAdminErrAppFileReadFail = (HRESULT)0x80110408L,COMAdminErrAppFileVersion = (HRESULT)0x80110409L,COMAdminErrBadPath = (HRESULT)0x8011040aL,
    COMAdminErrApplicationExists = (HRESULT)0x8011040bL,COMAdminErrRoleExists = (HRESULT)0x8011040cL,COMAdminErrCantCopyFile = (HRESULT)0x8011040dL,
    COMAdminErrNoUser = (HRESULT)0x8011040fL,COMAdminErrInvalidUserids = (HRESULT)0x80110410L,COMAdminErrNoRegistryCLSID = (HRESULT)0x80110411L,
    COMAdminErrBadRegistryProgID = (HRESULT)0x80110412L,COMAdminErrAuthenticationLevel = (HRESULT)0x80110413L,
    COMAdminErrUserPasswdNotValid = (HRESULT)0x80110414L,COMAdminErrCLSIDOrIIDMismatch = (HRESULT)0x80110418L,
    COMAdminErrRemoteInterface = (HRESULT)0x80110419L,COMAdminErrDllRegisterServer = (HRESULT)0x8011041aL,
    COMAdminErrNoServerShare = (HRESULT)0x8011041bL,COMAdminErrDllLoadFailed = (HRESULT)0x8011041dL,COMAdminErrBadRegistryLibID = (HRESULT)0x8011041eL,
    COMAdminErrAppDirNotFound = (HRESULT)0x8011041fL,COMAdminErrRegistrarFailed = (HRESULT)0x80110423L,
    COMAdminErrCompFileDoesNotExist = (HRESULT)0x80110424L,COMAdminErrCompFileLoadDLLFail = (HRESULT)0x80110425L,
    COMAdminErrCompFileGetClassObj = (HRESULT)0x80110426L,COMAdminErrCompFileClassNotAvail = (HRESULT)0x80110427L,
    COMAdminErrCompFileBadTLB = (HRESULT)0x80110428L,COMAdminErrCompFileNotInstallable = (HRESULT)0x80110429L,
    COMAdminErrNotChangeable = (HRESULT)0x8011042aL,COMAdminErrNotDeletable = (HRESULT)0x8011042bL,COMAdminErrSession = (HRESULT)0x8011042cL,
    COMAdminErrCompMoveLocked = (HRESULT)0x8011042dL,COMAdminErrCompMoveBadDest = (HRESULT)0x8011042eL,COMAdminErrRegisterTLB = (HRESULT)0x80110430L,
    COMAdminErrSystemApp = (HRESULT)0x80110433L,COMAdminErrCompFileNoRegistrar = (HRESULT)0x80110434L,
    COMAdminErrCoReqCompInstalled = (HRESULT)0x80110435L,COMAdminErrServiceNotInstalled = (HRESULT)0x80110436L,
    COMAdminErrPropertySaveFailed = (HRESULT)0x80110437L,COMAdminErrObjectExists = (HRESULT)0x80110438L,
    COMAdminErrComponentExists = (HRESULT)0x80110439L,COMAdminErrRegFileCorrupt = (HRESULT)0x8011043bL,
    COMAdminErrPropertyOverflow = (HRESULT)0x8011043cL,COMAdminErrNotInRegistry = (HRESULT)0x8011043eL,
    COMAdminErrObjectNotPoolable = (HRESULT)0x8011043fL,COMAdminErrApplidMatchesClsid = (HRESULT)0x80110446L,
    COMAdminErrRoleDoesNotExist = (HRESULT)0x80110447L,COMAdminErrStartAppNeedsComponents = (HRESULT)0x80110448L,
    COMAdminErrRequiresDifferentPlatform = (HRESULT)0x80110449L,COMAdminErrQueuingServiceNotAvailable = (HRESULT)0x80110602L,
    COMAdminErrObjectParentMissing = (HRESULT)0x80110808L,COMAdminErrObjectDoesNotExist = (HRESULT)0x80110809L,
    COMAdminErrCanNotExportAppProxy = (HRESULT)0x8011044aL,COMAdminErrCanNotStartApp = (HRESULT)0x8011044bL,
    COMAdminErrCanNotExportSystemApp = (HRESULT)0x8011044cL,COMAdminErrCanNotSubscribeToComponent = (HRESULT)0x8011044dL,
    COMAdminErrAppNotRunning = (HRESULT)0x8011080aL,COMAdminErrEventClassCannotBeSubscriber = (HRESULT)0x8011044eL,
    COMAdminErrLibAppProxyIncompatible = (HRESULT)0x8011044fL,COMAdminErrBasePartitionOnly = (HRESULT)0x80110450L,
    COMAdminErrDuplicatePartitionName = (HRESULT)0x80110457L,COMAdminErrPartitionInUse = (HRESULT)0x80110459L,
    COMAdminErrImportedComponentsNotAllowed = (HRESULT)0x8011045bL,COMAdminErrRegdbNotInitialized = (HRESULT)0x80110472L,
    COMAdminErrRegdbNotOpen = (HRESULT)0x80110473L,COMAdminErrRegdbSystemErr = (HRESULT)0x80110474L,
    COMAdminErrRegdbAlreadyRunning = (HRESULT)0x80110475L,COMAdminErrMigVersionNotSupported = (HRESULT)0x80110480L,
    COMAdminErrMigSchemaNotFound = (HRESULT)0x80110481L,COMAdminErrCatBitnessMismatch = (HRESULT)0x80110482L,
    COMAdminErrCatUnacceptableBitness = (HRESULT)0x80110483L,COMAdminErrCatWrongAppBitnessBitness = (HRESULT)0x80110484L,
    COMAdminErrCatPauseResumeNotSupported = (HRESULT)0x80110485L,COMAdminErrCatServerFault = (HRESULT)0x80110486L,
    COMAdminErrCantRecycleLibraryApps = (HRESULT)0x8011080fL,COMAdminErrCantRecycleServiceApps = (HRESULT)0x80110811L,
    COMAdminErrProcessAlreadyRecycled = (HRESULT)0x80110812L,COMAdminErrPausedProcessMayNotBeRecycled = (HRESULT)0x80110813L,
    COMAdminErrInvalidPartition = (HRESULT)0x8011080bL,COMAdminErrPartitionMsiOnly = (HRESULT)0x80110819L,
    COMAdminErrStartAppDisabled = (HRESULT)0x80110451L,COMAdminErrCompMoveSource = (HRESULT)0x8011081cL,
    COMAdminErrCompMoveDest = (HRESULT)0x8011081dL,COMAdminErrCompMovePrivate = (HRESULT)0x8011081eL,
    COMAdminErrCannotCopyEventClass = (HRESULT)0x80110820L
  };

  EXTERN_C const IID LIBID_COMAdmin;
  EXTERN_C const CLSID CLSID_COMAdminCatalog;
#ifdef __cplusplus
  class COMAdminCatalog;
#endif
  EXTERN_C const CLSID CLSID_COMAdminCatalogObject;
#ifdef __cplusplus
  class COMAdminCatalogObject;
#endif
  EXTERN_C const CLSID CLSID_COMAdminCatalogCollection;
#ifdef __cplusplus
  class COMAdminCatalogCollection;
#endif
#endif

  unsigned long __RPC_API BSTR_UserSize(unsigned long *,unsigned long,BSTR *);
  unsigned char *__RPC_API BSTR_UserMarshal(unsigned long *,unsigned char *,BSTR *);
  unsigned char *__RPC_API BSTR_UserUnmarshal(unsigned long *,unsigned char *,BSTR *);
  void __RPC_API BSTR_UserFree(unsigned long *,BSTR *);
  unsigned long __RPC_API LPSAFEARRAY_UserSize(unsigned long *,unsigned long,LPSAFEARRAY *);
  unsigned char *__RPC_API LPSAFEARRAY_UserMarshal(unsigned long *,unsigned char *,LPSAFEARRAY *);
  unsigned char *__RPC_API LPSAFEARRAY_UserUnmarshal(unsigned long *,unsigned char *,LPSAFEARRAY *);
  void __RPC_API LPSAFEARRAY_UserFree(unsigned long *,LPSAFEARRAY *);
  unsigned long __RPC_API VARIANT_UserSize(unsigned long *,unsigned long,VARIANT *);
  unsigned char *__RPC_API VARIANT_UserMarshal(unsigned long *,unsigned char *,VARIANT *);
  unsigned char *__RPC_API VARIANT_UserUnmarshal(unsigned long *,unsigned char *,VARIANT *);
  void __RPC_API VARIANT_UserFree(unsigned long *,VARIANT *);

#ifdef __cplusplus
}
#endif
#endif

