#include <lib/mem_pool.h>
#include <winsock2.h>
#include <windows.h>
namespace old_path_conversion {
  typedef bool (*t_pVSSPathConvert)(const char* szFilePath,
				    char* szShadowPath,
				    int nBuflen);
  typedef bool (*t_pVSSPathConvertW)(const wchar_t* szFilePath,
				     wchar_t* szShadowPath,
				     int nBuflen);
  struct thread_vss_path_convert {
    t_pVSSPathConvert pPathConvert;
    t_pVSSPathConvertW pPathConvertW;
  };

  void Win32SetPathConvert(t_pVSSPathConvert, t_pVSSPathConvertW);
  void unix_name_to_win32(POOLMEM*& win32_name, const char* name);
  int make_win32_path_UTF8_2_wchar(POOLMEM*& pszUCS,
				   const char* pszUTF,
				   BOOL* pBIsRawPath = NULL);
};
