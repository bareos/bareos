/*
 * Minimal replacement for class CComPtr and CComBSTR
 * Based on common public IUnknown interface only
 */

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
