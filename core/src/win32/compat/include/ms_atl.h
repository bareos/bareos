/*
 * Minimal replacement for class CComPtr and CComBSTR
 * Based on common public IUnknown interface only
 */
/* bareos-check-sources:disable-copyright-check */

#ifndef BAREOS_WIN32_COMPAT_INCLUDE_MS_ATL_H_
#define BAREOS_WIN32_COMPAT_INCLUDE_MS_ATL_H_

template <class T>
class CComPtr {
 public:
  /* Attribute(s) ... */
  T* p;

  /* Creation ... */
  CComPtr() { p = NULL; }

  /* Destructor ... */
  ~CComPtr()
  {
    if (p) p->Release();
  }
};

class CComBSTR {
 public:
  BSTR p;

  /* Creation ... */
  CComBSTR() { p = NULL; }

  /* Destructor ... */
  ~CComBSTR() { ::SysFreeString(p); }

  /* Address-of operator */
  BSTR* operator&() { return &p; }
};

#endif  // BAREOS_WIN32_COMPAT_INCLUDE_MS_ATL_H_
