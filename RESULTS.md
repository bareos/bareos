# Bareos test results

### Task 1
1) Visited url https://gist.github.com/joergsteffens/95d2f04eb0d4945da9f4cd198998cfae
2) installed pre-requirements :
`apt install acl-dev apache2 chrpath cmake dpkg-dev debhelper libacl1-dev libcap-dev liblzo2-dev qtbase5-dev libreadline-dev libssl-dev libwrap0-dev libx11-dev libxml2-dev libsqlite3-dev libpq-dev libjansson-dev libjson-c-dev mtx ncurses-dev pkg-config po-debconf python-dev zlib1g-dev`

and postgresql for database like mysql to connect to postgres
* `sudo -u postgres psql`
* `psql -d postgres`
* used ide like dbeaver to view and manage db

3) viewed project (bareos) https://docs.bareos.org/master/DeveloperGuide/BuildAndTestBareos/systemtests.html
4) cloned project using git clone
5) built project using cmake (C or C++ compiled language)
every c (or c++) project has makefile or cmakeLists that define the structure of project (like package.json or composer.json)
6) viewed database structure
I have a challenge to view database with dbeaver
so I change the password by this command
   `ALTER USER postgres PASSWORD 'postgres';`
after connect with psql
   `sudo -u postgres psql`

7) run tests

8) checked catalog if database with other credentials (MyCatalog.conf)
9) bin/bareos start
10) change levels with bconsole (ensure bareos started)

### Task 2
* to improve setdebug behaviour I suggest three improvement
1) make trace parameter interactive
2) pass personalised trace path
3) disable file trace path if trace is disabled (trace=0)

### Task 3
- the functions are : SetdebugCmd() ,
- the function is SetdebugCmd() and is located in " bareos/core/src/dird/ua_cmds.cc "
- functions called by SetdebugCmd :   Dmsg1(),DoDirectorSetdebug() ,DoStorageSetdebug() ,DoAllSetDebug()

### Task 4
* I chose to go with the first improvement i suggested in (2)
* I propose to make change on the file  "ua_cmds.cc" where the methode SetdebugCmd() is located , change in the code inside it where i can make the parametre of trace prompt so i can choose to enable or disable tracing while runnig setDebug commande .
* implementation is done in the file , test is well done in the terminal ( runned make for refreshing )

## Outputs

- cmake_build_output
```
cmake -Dpostgresql=yes -Dsystemtest_db_user=$USER -Dtraymonitor=yes ../bareos
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos
-- Found Git: /usr/bin/git (found version "2.25.1") 
-- Using version information from Git
BAREOS_NUMERIC_VERSION is 21.0.0
BAREOS_FULL_VERSION is 21.0.0~pre993.4cfdab87b
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core
-- The C compiler identification is GNU 9.3.0
-- The CXX compiler identification is GNU 9.3.0
-- Check for working C compiler: /usr/bin/cc
-- Check for working C compiler: /usr/bin/cc -- works
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Detecting C compile features
-- Detecting C compile features - done
-- Check for working CXX compiler: /usr/bin/c++
-- Check for working CXX compiler: /usr/bin/c++ -- works
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Performing Test compiler_will_suggest_override
-- Performing Test compiler_will_suggest_override - Success
-- Performing Test compiler_format_security
-- Performing Test compiler_format_security - Success
-- Performing Test compiler_error_format_security
-- Performing Test compiler_error_format_security - Success
-- Performing Test c_compiler_debug_prefix_map
-- Performing Test c_compiler_debug_prefix_map - Success
-- Performing Test cxx_compiler_debug_prefix_map
-- Performing Test cxx_compiler_debug_prefix_map - Success
-- Performing Test c_compiler_macro_prefix_map
-- Performing Test c_compiler_macro_prefix_map - Success
-- Performing Test cxx_compiler_macro_prefix_map
-- Performing Test cxx_compiler_macro_prefix_map - Success
-- Performing Test compiler_has_no_unknown_pragmas
-- Performing Test compiler_has_no_unknown_pragmas - Success
-- Found PkgConfig: /usr/bin/pkg-config (found version "0.29.1") 
-- Checking for module 'systemd'
--   Found systemd, version 245
-- systemd services install dir: /lib/systemd/system
-- Found Python2: /usr/bin/python2.7 (found version "2.7.18") found components: Interpreter Development 
-- Could NOT find Python3 (missing: Python3_INCLUDE_DIRS Development) (found version "3.8.10")
-- Python2_CC is  x86_64-linux-gnu-gcc -pthread
-- Python2_CC_FLAGS is  -pthread
-- Python2_BLDSHARED is  x86_64-linux-gnu-gcc -pthread -shared -Wl,-O1 -Wl,-Bsymbolic-functions -Wl,-Bsymbolic-functions -Wl,-z,relro -fno-strict-aliasing -DNDEBUG -g -fwrapv -O2 -Wall -Wstrict-prototypes -Wdate-time -D_FORTIFY_SOURCE=2 -g -fdebug-prefix-map=/build/python2.7-QDqKfA/python2.7-2.7.18=. -fstack-protector-strong -Wformat -Werror=format-security  
-- Python2_BLDSHARED_FLAGS is  -pthread -shared -Wl,-O1 -Wl,-Bsymbolic-functions -Wl,-Bsymbolic-functions -Wl,-z,relro -fno-strict-aliasing -DNDEBUG -g -fwrapv -O2 -Wall -Wstrict-prototypes -Wdate-time -D_FORTIFY_SOURCE=2 -g -fdebug-prefix-map=/build/python2.7-QDqKfA/python2.7-2.7.18=. -fstack-protector-strong -Wformat -Werror=format-security  
-- Python2_CFLAGS is  -fno-strict-aliasing -DNDEBUG -g -fwrapv -O2 -Wall -Wstrict-prototypes -Wdate-time -D_FORTIFY_SOURCE=2 -g -fdebug-prefix-map=/build/python2.7-QDqKfA/python2.7-2.7.18=. -fstack-protector-strong -Wformat -Werror=format-security  
-- Python2_CCSHARED is  -fPIC
-- Python2_INCLUDEPY is  /usr/include/python2.7
-- Python2_LDFLAGS is  -Wl,-Bsymbolic-functions -Wl,-z,relro
-- Python2_EXT_SUFFIX is  
-- Could NOT find PostgreSQL (missing: PostgreSQL_TYPE_INCLUDE_DIR) (found version "12.9")
-- Found MySQL: /usr/lib/x86_64-linux-gnu/libmysqlclient.so
-- Found OpenSSL: /usr/lib/x86_64-linux-gnu/libcrypto.so (found version "1.1.1f")  
-- checking for library vixDiskLib and vixDiskLib.h header ...
--   HAVE_VIXDISKLIB=0
--               ERROR:  vixDiskLib libraries NOT found. 
--               ERROR:  vixDiskLib includes  NOT found. 
--   VIXDISKLIB_FOUND=FALSE
--   VIXDISKLIB_LIBRARIES=
--   VIXDISKLIB_INCLUDE_DIRS=
--   HAVE_VIXDISKLIB=0
-- checking for library rados and rados/librados.h header ...
--   HAVE_RADOS=0
--               ERROR:  rados libraries NOT found. 
--               ERROR:  rados includes  NOT found. 
--   RADOS_FOUND=FALSE
--   RADOS_LIBRARIES=
--   RADOS_INCLUDE_DIRS=
--   HAVE_RADOS=0
-- checking for library radosstriper and radosstriper/libradosstriper.h header ...
--   HAVE_RADOSSTRIPER=0
--               ERROR:  radosstriper libraries NOT found. 
--               ERROR:  radosstriper includes  NOT found. 
--   RADOSSTRIPER_FOUND=FALSE
--   RADOSSTRIPER_LIBRARIES=
--   RADOSSTRIPER_INCLUDE_DIRS=
--   HAVE_RADOSSTRIPER=0
-- checking for library cephfs and cephfs/libcephfs.h header ...
--   HAVE_CEPHFS=0
--               ERROR:  cephfs libraries NOT found. 
--               ERROR:  cephfs includes  NOT found. 
--   CEPHFS_FOUND=FALSE
--   CEPHFS_LIBRARIES=
--   CEPHFS_INCLUDE_DIRS=
--   HAVE_CEPHFS=0
-- checking for library pthread and pthread.h header ...
--   PTHREAD_FOUND=TRUE
--   PTHREAD_LIBRARIES=/usr/lib/x86_64-linux-gnu/libpthread.so
--   PTHREAD_INCLUDE_DIRS=/usr/include
--   HAVE_PTHREAD=1
-- checking for library cap and sys/capability.h header ...
--   CAP_FOUND=TRUE
--   CAP_LIBRARIES=/usr/lib/x86_64-linux-gnu/libcap.so
--   CAP_INCLUDE_DIRS=/usr/include
--   HAVE_CAP=1
-- checking for library gfapi and glusterfs/api/glfs.h header ...
--   HAVE_GFAPI=0
--               ERROR:  gfapi libraries NOT found. 
--               ERROR:  gfapi includes  NOT found. 
--   GFAPI_FOUND=FALSE
--   GFAPI_LIBRARIES=
--   GFAPI_INCLUDE_DIRS=
--   HAVE_GFAPI=0
-- checking for library pam and security/pam_appl.h header ...
--   HAVE_PAM=0
--               ERROR:  pam libraries NOT found. 
--               ERROR:  pam includes  NOT found. 
--   PAM_FOUND=FALSE
--   PAM_LIBRARIES=
--   PAM_INCLUDE_DIRS=
--   HAVE_PAM=0
-- checking for library lzo2 and lzo/lzoconf.h header ...
--   LZO2_FOUND=TRUE
--   LZO2_LIBRARIES=/usr/lib/x86_64-linux-gnu/liblzo2.so
--   LZO2_INCLUDE_DIRS=/usr/include
--   HAVE_LZO2=1
-- checking for library tirpc...
--               ERROR:  tirpc libraries NOT found. 
--   TIRPC_FOUND=FALSE
--   TIRPC_LIBRARIES=
--   HAVE_LIBTIRPC=0
-- checking for library util...
--   UTIL_FOUND=TRUE
--   UTIL_LIBRARIES=/usr/lib/x86_64-linux-gnu/libutil.so
--   HAVE_LIBUTIL=1
-- checking for library dl...
--   DL_FOUND=TRUE
--   DL_LIBRARIES=/usr/lib/x86_64-linux-gnu/libdl.so
--   HAVE_LIBDL=1
-- checking for library acl...
--   ACL_FOUND=TRUE
--   ACL_LIBRARIES=/usr/lib/x86_64-linux-gnu/libacl.so
--   HAVE_LIBACL=1
-- Could NOT find GTest (missing: GTest_DIR)
-- Found ZLIB: /usr/lib/x86_64-linux-gnu/libz.so (found version "1.2.11") 
-- Found Readline: /usr/include  
-- Found Jansson: /usr/lib/x86_64-linux-gnu/libjansson.so (found version "2.12") 
-- Looking for pthread.h
-- Looking for pthread.h - found
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD - Failed
-- Check if compiler accepts -pthread
-- Check if compiler accepts -pthread - yes
-- Found Threads: TRUE  
-- Looking for include files pthread.h, pthread_np.h
-- Looking for include files pthread.h, pthread_np.h - not found
-- Performing Test HAVE_PTHREAD_ATTR_GET_NP
-- Performing Test HAVE_PTHREAD_ATTR_GET_NP - Failed
-- Found Intl: /usr/include  
-- set bindir to default /usr/local/bin
-- set sbindir to default /usr/local/sbin
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/droplet
-- Try to configure LibDroplet
--    Found LibXml2: /usr/lib/x86_64-linux-gnu/libxml2.so (found version "2.9.10") 
--    Found jsonc: /usr/include/json-c  
CMake Warning (dev) at /usr/share/cmake-3.16/Modules/FindPkgConfig.cmake:640 (if):
  Policy CMP0054 is not set: Only interpret if() arguments as variables or
  keywords when unquoted.  Run "cmake --help-policy CMP0054" for policy
  details.  Use the cmake_policy command to set the policy and suppress this
  warning.

  Quoted variables like "openssl" will no longer be dereferenced when the
  policy is set to NEW.  Since the policy is not set the OLD behavior will be
  used.
Call Stack (most recent call first):
  /usr/share/cmake-3.16/Modules/FindOpenSSL.cmake:85 (pkg_check_modules)
  core/src/droplet/CMakeLists.txt:32 (include)
This warning is for project developers.  Use -Wno-dev to suppress it.

   Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/droplet/libdroplet
--    LibDroplet ready to be built
-- Check if the system is big endian
-- Searching 16 bit integer
-- Looking for sys/types.h
-- Looking for sys/types.h - found
-- Looking for stdint.h
-- Looking for stdint.h - found
-- Looking for stddef.h
-- Looking for stddef.h - found
-- Check size of unsigned short
-- Check size of unsigned short - done
-- Using unsigned short
-- Check if the system is big endian - little endian
-- VERSION: C
-- PROJECT_SOURCE_DIR:         /home/hajer/Documents/Wks/bareos-local-tests/bareos/core
-- Looking for include file alloca.h
-- Looking for include file alloca.h - found
-- Looking for include file afs/afsint.h
-- Looking for include file afs/afsint.h - not found
-- Looking for include file afs/venus.h
-- Looking for include file afs/venus.h - not found
-- Looking for include file arpa/nameser.h
-- Looking for include file arpa/nameser.h - found
-- Looking for include file attr.h
-- Looking for include file attr.h - not found
-- Looking for include file demangle.h
-- Looking for include file demangle.h - not found
-- Looking for include file execinfo.h
-- Looking for include file execinfo.h - found
-- Looking for include file grp.h
-- Looking for include file grp.h - found
-- Looking for include file libutil.h
-- Looking for include file libutil.h - not found
-- Looking for include file mtio.h
-- Looking for include file mtio.h - not found
-- Looking for include file pwd.h
-- Looking for include file pwd.h - found
-- Looking for include file regex.h
-- Looking for include file regex.h - found
-- Looking for include files sys/types.h, sys/acl.h
-- Looking for include files sys/types.h, sys/acl.h - found
-- Looking for include file sys/attr.h
-- Looking for include file sys/attr.h - not found
-- Looking for include file sys/bitypes.h
-- Looking for include file sys/bitypes.h - found
-- Looking for include file sys/capability.h
-- Looking for include file sys/capability.h - found
-- Looking for include file sys/ea.h
-- Looking for include file sys/ea.h - not found
-- Looking for include files sys/types.h, sys/extattr.h
-- Looking for include files sys/types.h, sys/extattr.h - not found
-- Looking for include file sys/mtio.h
-- Looking for include file sys/mtio.h - found
-- Looking for include file sys/nvpair.h
-- Looking for include file sys/nvpair.h - not found
-- Looking for include files sys/types.h, sys/tape.h
-- Looking for include files sys/types.h, sys/tape.h - not found
-- Looking for include file sys/time.h
-- Looking for include file sys/time.h - found
-- Looking for C++ include cxxabi.h
-- Looking for C++ include cxxabi.h - found
-- Looking for include file curses.h
-- Looking for include file curses.h - found
-- Looking for include file poll.h
-- Looking for include file poll.h - found
-- Looking for include file sys/poll.h
-- Looking for include file sys/poll.h - found
-- Looking for include file sys/statvfs.h
-- Looking for include file sys/statvfs.h - found
-- Looking for include file umem.h
-- Looking for include file umem.h - not found
-- Looking for include file ucontext.h
-- Looking for include file ucontext.h - found
-- Looking for include file sys/proplist.h
-- Looking for include file sys/proplist.h - not found
-- Looking for include file sys/xattr.h
-- Looking for include file sys/xattr.h - found
-- Looking for ceph_statx
-- Looking for ceph_statx - not found
-- Looking for include file rados/librados.h
-- Looking for include file rados/librados.h - not found
-- Looking for include file radosstriper/libradosstriper.h
-- Looking for include file radosstriper/libradosstriper.h - not found
-- Looking for include file glusterfs/api/glfs.h
-- Looking for include file glusterfs/api/glfs.h - not found
-- Looking for include file sys/prctl.h
-- Looking for include file sys/prctl.h - found
-- Looking for include file zlib.h
-- Looking for include file zlib.h - found
-- Looking for include file scsi/scsi.h
-- Looking for include file scsi/scsi.h - found
-- Looking for include files stddef.h, scsi/sg.h
-- Looking for include files stddef.h, scsi/sg.h - found
-- Looking for include files sys/types.h, sys/scsi/impl/uscsi.h
-- Looking for include files sys/types.h, sys/scsi/impl/uscsi.h - not found
-- Looking for include files stdio.h, camlib.h
-- Looking for include files stdio.h, camlib.h - not found
-- Looking for include file cam/scsi/scsi_message.h
-- Looking for include file cam/scsi/scsi_message.h - not found
-- Looking for include file dev/scsipi/scsipi_all.h
-- Looking for include file dev/scsipi/scsipi_all.h - not found
-- Looking for include file scsi/uscsi_all.h
-- Looking for include file scsi/uscsi_all.h - not found
-- Looking for backtrace
-- Looking for backtrace - found
-- Looking for backtrace_symbols
-- Looking for backtrace_symbols - found
-- Looking for fchmod
-- Looking for fchmod - found
-- Looking for fchown
-- Looking for fchown - found
-- Looking for add_proplist_entry
-- Looking for add_proplist_entry - not found
-- Looking for closefrom
-- Looking for closefrom - not found
-- Looking for extattr_get_file
-- Looking for extattr_get_file - not found
-- Looking for extattr_get_link
-- Looking for extattr_get_link - not found
-- Looking for extattr_list_file
-- Looking for extattr_list_file - not found
-- Looking for extattr_list_link
-- Looking for extattr_list_link - not found
-- Looking for extattr_namespace_to_string
-- Looking for extattr_namespace_to_string - not found
-- Looking for extattr_set_file
-- Looking for extattr_set_file - not found
-- Looking for extattr_set_link
-- Looking for extattr_set_link - not found
-- Looking for extattr_string_to_namespace
-- Looking for extattr_string_to_namespace - not found
-- Looking for fchownat
-- Looking for fchownat - found
-- Looking for fdatasync
-- Looking for fdatasync - found
-- Looking for fseeko
-- Looking for fseeko - found
-- Looking for futimens
-- Looking for futimens - found
-- Looking for futimes
-- Looking for futimes - found
-- Looking for futimesat
-- Looking for futimesat - found
-- Looking for getaddrinfo
-- Looking for getaddrinfo - found
-- Looking for getea
-- Looking for getea - not found
-- Looking for gethostbyname2
-- Looking for gethostbyname2 - found
-- Looking for getmntent
-- Looking for getmntent - found
-- Looking for getmntinfo
-- Looking for getmntinfo - not found
-- Looking for getpagesize
-- Looking for getpagesize - found
-- Looking for getproplist
-- Looking for getproplist - not found
-- Looking for getxattr
-- Looking for getxattr - found
-- Looking for get_proplist_entry
-- Looking for get_proplist_entry - not found
-- Looking for glfs_readdirplus
-- Looking for glfs_readdirplus - not found
-- Looking for glob
-- Looking for glob - found
-- Looking for inet_ntop
-- Looking for inet_ntop - found
-- Looking for lchmod
-- Looking for lchmod - found
-- Looking for lchown
-- Looking for lchown - found
-- Looking for lgetea
-- Looking for lgetea - not found
-- Looking for lgetxattr
-- Looking for lgetxattr - found
-- Looking for listea
-- Looking for listea - not found
-- Looking for listxattr
-- Looking for listxattr - found
-- Looking for llistea
-- Looking for llistea - not found
-- Looking for llistxattr
-- Looking for llistxattr - found
-- Looking for localtime_r
-- Looking for localtime_r - found
-- Looking for lsetea
-- Looking for lsetea - not found
-- Looking for lsetxattr
-- Looking for lsetxattr - found
-- Looking for lutimes
-- Looking for lutimes - found
-- Looking for nanosleep
-- Looking for nanosleep - found
-- Looking for openat
-- Looking for openat - found
-- Looking for poll
-- Looking for poll - found
-- Looking for posix_fadvise
-- Looking for posix_fadvise - found
-- Looking for prctl
-- Looking for prctl - found
-- Looking for readdir_r
-- Looking for readdir_r - found
-- Looking for setea
-- Looking for setea - not found
-- Looking for setproplist
-- Looking for setproplist - not found
-- Looking for setreuid
-- Looking for setreuid - found
-- Looking for setxattr
-- Looking for setxattr - found
-- Looking for hl_loa
-- Looking for hl_loa - not found
-- Looking for sizeof_proplist_entry
-- Looking for sizeof_proplist_entry - not found
-- Looking for unlinkat
-- Looking for unlinkat - found
-- Looking for utimes
-- Looking for utimes - found
-- Looking for chflags
-- Looking for chflags - found
-- Looking for __stub_lchmod
-- Looking for __stub_lchmod - found
-- Looking for __stub___lchmod
-- Looking for __stub___lchmod - not found
-- lchmod is a stub, setting HAVE_LCHMOD to 0
-- Looking for __stub_chflags
-- Looking for __stub_chflags - found
-- lchflags is a stub, setting HAVE_CHFLAGS to 0
-- Looking for rados_ioctx_set_namespace
-- Looking for rados_ioctx_set_namespace - not found
-- Looking for rados_nobjects_list_open
-- Looking for rados_nobjects_list_open - not found
-- Performing Test HAVE_ACL_TYPE_DEFAULT_DIR
-- Performing Test HAVE_ACL_TYPE_DEFAULT_DIR - Failed
-- Performing Test HAVE_ACL_TYPE_EXTENDED
-- Performing Test HAVE_ACL_TYPE_EXTENDED - Failed
-- Performing Test HAVE_ACL_TYPE_NFS4
-- Performing Test HAVE_ACL_TYPE_NFS4 - Failed
-- CMAKE_BINARY_DIR:         /home/hajer/Documents/Wks/bareos-local-tests/build
-- CMAKE_CURRENT_BINARY_DIR: /home/hajer/Documents/Wks/bareos-local-tests/build/core
-- PROJECT_SOURCE_DIR:         /home/hajer/Documents/Wks/bareos-local-tests/bareos/core
-- CMAKE_CURRENT_SOURCE_DIR: /home/hajer/Documents/Wks/bareos-local-tests/bareos/core
-- PROJECT_BINARY_DIR: /home/hajer/Documents/Wks/bareos-local-tests/build/core
-- PROJECT_SOURCE_DIR: /home/hajer/Documents/Wks/bareos-local-tests/bareos/core
-- EXECUTABLE_OUTPUT_PATH: 
-- LIBRARY_OUTPUT_PATH:     
-- CMAKE_MODULE_PATH: /home/hajer/Documents/Wks/bareos-local-tests/bareos/cmake/home/hajer/Documents/Wks/bareos-local-tests/bareos/core/cmake/home/hajer/Documents/Wks/bareos-local-tests/bareos/core/../../SOURCES/home/hajer/Documents/Wks/bareos-local-tests/bareos/core/../SOURCES/home/hajer/Documents/Wks/bareos-local-tests/bareos/core/cmake
-- CMAKE_COMMAND: /usr/bin/cmake
-- CMAKE_VERSION: 3.16.3
-- CMAKE_ROOT: /usr/share/cmake-3.16
-- CMAKE_CURRENT_LIST_FILE: /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/CMakeLists.txt
-- CMAKE_CURRENT_LIST_LINE: 494
-- CMAKE_INCLUDE_PATH: 
-- CMAKE_LIBRARY_PATH: 
-- CMAKE_SYSTEM: Linux-5.11.0-40-generic
-- CMAKE_SYSTEM_NAME: Linux
-- CMAKE_SYSTEM_VERSION: 5.11.0-40-generic
-- CMAKE_SYSTEM_PROCESSOR: x86_64
-- UNIX: 1
-- WIN32: 
-- APPLE: 
-- MINGW: 
-- CYGWIN: 
-- BORLAND: 
-- MSVC: 
-- MSVC_IDE: 
-- MSVC60: 
-- MSVC70: 
-- MSVC71: 
-- MSVC80: 
-- CMAKE_COMPILER_2005: 
-- CMAKE_SKIP_RULE_DEPENDENCY: 
-- CMAKE_SKIP_INSTALL_ALL_DEPENDENCY: 
-- CMAKE_SKIP_RPATH: NO
-- CMAKE_VERBOSE_MAKEFILE: FALSE
-- CMAKE_SUPPRESS_REGENERATION: 
-- CCACHE_FOUND: CCACHE_FOUND-NOTFOUND
-- CMAKE_BUILD_TYPE: 
-- BUILD_SHARED_LIBS: 
-- CMAKE_C_COMPILER:           /usr/bin/cc
-- CMAKE_C_FLAGS:               -fdebug-prefix-map=/home/hajer/Documents/Wks/bareos-local-tests/bareos/core=. -fmacro-prefix-map=/home/hajer/Documents/Wks/bareos-local-tests/bareos/core=. -Werror -Wall
-- CMAKE_C_COMPILER_ID:        GNU
-- CMAKE_C_COMPILER_VERSION:   9.3.0
-- CMAKE_CXX_COMPILER:         /usr/bin/c++
-- CMAKE_CXX_FLAGS:             -Wsuggest-override -Wformat -Werror=format-security -fdebug-prefix-map=/home/hajer/Documents/Wks/bareos-local-tests/bareos/core=. -fmacro-prefix-map=/home/hajer/Documents/Wks/bareos-local-tests/bareos/core=. -Wno-unknown-pragmas -Werror -Wall
-- CMAKE_CXX_COMPILER_ID:      GNU
-- CMAKE_CXX_COMPILER_VERSION: 9.3.0
-- CMAKE_AR: /usr/bin/ar
-- CMAKE_RANLIB: /usr/bin/ranlib
-- CMAKE_INSTALL_PREFIX:         /usr/local
-- acl found, libs: /usr/lib/x86_64-linux-gnu/libacl.so
-- installing /usr/local/var/log/bareos
-- installing 
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/aix/bareos-fd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/darwin/resources/ReadMe.html
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/darwin/resources/com.bareos.bareos-fd.plist
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/darwin/resources/postinstall
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/darwin/resources/preinstall
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/darwin/resources/uninstall-bareos
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/darwin/resources/welcome.txt
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/debian/set_dbconfig_vars.sh
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/freebsd/bareos-dir
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/freebsd/bareos-fd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/freebsd/bareos-sd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/hurd/bareos-dir
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/hurd/bareos-fd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/hurd/bareos-sd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/openbsd/bareos-dir
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/openbsd/bareos-fd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/openbsd/bareos-sd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/redhat/bareos-dir
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/redhat/bareos-fd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/redhat/bareos-sd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/slackware/functions.bareos
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/slackware/rc.bareos-dir
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/slackware/rc.bareos-fd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/slackware/rc.bareos-sd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/solaris/bareos-dir
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/solaris/bareos-fd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/solaris/bareos-sd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/suse/bareos-dir
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/suse/bareos-fd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/suse/bareos-sd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/systemd/bareos-dir.service
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/systemd/bareos-fd.service
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/systemd/bareos-sd.service
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/univention/AppCenter/univention-bareos.ini
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/univention/conffiles/etc/apt/sources.list.d/60_bareos.list
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/bareos-config-lib.sh
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/bareos-config
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/bareos-ctl-dir
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/bareos-ctl-fd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/bareos-ctl-sd
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/bareos-explorer
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/bareos-glusterfind-wrapper
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/bareos
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/bconsole
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/btraceback
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/disk-changer
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/logrotate
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/logwatch/logfile.bareos.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts/mtx-changer
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/create_bareos_database
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/ddl/versions.map
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/delete_catalog_backup
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/drop_bareos_database
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/drop_bareos_tables
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/grant_bareos_privileges
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/install-default-backend
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/make_bareos_tables
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/make_catalog_backup
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/make_catalog_backup.pl
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/update_bareos_tables
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/console/bconsole.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/catalog/MyCatalog.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/client/bareos-fd.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/console/bareos-mon.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/director/bareos-dir.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/fileset/Catalog.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/fileset/LinuxAll.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/fileset/SelfTest.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/job/BackupCatalog.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/job/RestoreFiles.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/job/backup-bareos-fd.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/jobdefs/DefaultJob.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/messages/Daemon.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/messages/Standard.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-dir.d/storage/File.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-fd.d/client/myself.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-fd.d/director/bareos-dir.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-fd.d/director/bareos-mon.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-sd.d/device/FileStorage.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-sd.d/director/bareos-dir.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-sd.d/director/bareos-mon.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/bareos-sd.d/storage/bareos-sd.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/tray-monitor.d/client/FileDaemon-local.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/tray-monitor.d/director/Director-local.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/tray-monitor.d/monitor/bareos-mon.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/defaultconfigs/tray-monitor.d/storage/StorageDaemon-local.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/droplet/droplet-3.0.pc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/droplet/libdroplet.spec
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/include/config.h
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/plugins/filed/python/ldap/python-ldap-conf.d/bareos-dir.d/fileset/plugin-ldap.conf.example
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/plugins/filed/python/ovirt/python-ovirt-conf.d/bareos-dir.d/fileset/plugin-ovirt.conf.example
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/qt-tray-monitor/bareos-tray-monitor.desktop
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/bareos-configparser-tests/bareos-dir-CFG_TYPE_STR_VECTOR_OF_DIRS.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/catalog/bareos-dir.d/catalog/MyCatalog.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_disabled/bareos-dir.d/director/bareos-dir.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_disabled/bareos-dir.d/fileset/Catalog.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_disabled/bareos-dir.d/fileset/LinuxAll.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_disabled/bareos-dir.d/fileset/SelfTest.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_disabled/bareos-dir.d/job/BackupCatalog.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_disabled/bareos-dir.d/jobdefs/DefaultJob.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_disabled/bareos-dir.d/messages/Daemon.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_disabled/bareos-dir.d/messages/Standard.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_psk_default_enabled/bareos-dir.d/director/bareos-dir.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_psk_default_enabled/bareos-dir.d/fileset/Catalog.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_psk_default_enabled/bareos-dir.d/fileset/LinuxAll.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_psk_default_enabled/bareos-dir.d/fileset/SelfTest.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_psk_default_enabled/bareos-dir.d/job/BackupCatalog.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_psk_default_enabled/bareos-dir.d/jobdefs/DefaultJob.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_psk_default_enabled/bareos-dir.d/messages/Daemon.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/console-director/tls_psk_default_enabled/bareos-dir.d/messages/Standard.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/sd_backend/bareos-sd.d/device/droplet.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/sd_backend/bareos-sd.d/storage/myself.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/sd_backend/droplet.profile
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tests/configs/sd_reservation/bareos-sd.d/storage/myself.conf
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/win32/console/consoleres.rc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/win32/dird/dbcheckres.rc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/win32/dird/dirdres.rc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/win32/filed/filedres.rc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/win32/qt-tray-monitor/traymon.rc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/win32/stored/bextractres.rc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/win32/stored/blsres.rc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/win32/stored/bscanres.rc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/win32/stored/btaperes.rc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/win32/stored/storedres.rc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/win32/tools/bregexres.rc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/win32/tools/bsmtpres.rc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/win32/tools/bwildres.rc
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-bconsole.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-bconsole.postinst
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-common.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-common.postinst
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-common.preinst
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-database-common.config
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-database-common.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-database-common.postinst
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-database-mysql.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-database-postgresql.dirs
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-database-postgresql.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-database-sqlite3.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-database-tools.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-director-python-plugins-common.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-director-python2-plugin.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-director-python3-plugin.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-director.bareos-dir.init
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-director.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-director.postinst
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-director.preinst
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-director.service
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon-ceph-plugin.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon-glusterfs-plugin.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon-ldap-python-plugin.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon-libcloud-python-plugin.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon-percona-xtrabackup-python-plugin.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon-postgresql-python-plugin.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon-python-plugins-common.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon-python2-plugin.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon-python3-plugin.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon.bareos-fd.init
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon.postinst
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon.preinst
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-filedaemon.service
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-storage-ceph.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-storage-droplet.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-storage-fifo.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-storage-glusterfs.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-storage-python-plugins-common.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-storage-python2-plugin.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-storage-python3-plugin.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-storage-tape.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-storage.bareos-sd.init
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-storage.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-storage.postinst
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-storage.preinst
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-storage.service
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-tools.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-traymonitor.install
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/bareos-traymonitor.postinst
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/control
-- creating file /home/hajer/Documents/Wks/bareos-local-tests/bareos/debian/univention-bareos.postinst
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/scripts
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/manpages
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms
-- UNITDIR: 
-- BAREOS_PLATFORM: ubuntu
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/platforms/debian
-- installing startfiles to /usr/local/etc/init.d/
WILL INSTALL  /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/ddl/updates/postgresql.10_11.sql to /usr/local/share/dbconfig-common/data/bareos-database-common/upgrade/pgsql/11
WILL INSTALL  /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/ddl/updates/postgresql.11_12.sql to /usr/local/share/dbconfig-common/data/bareos-database-common/upgrade/pgsql/12
WILL INSTALL  /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/ddl/updates/postgresql.12_14.sql to /usr/local/share/dbconfig-common/data/bareos-database-common/upgrade/pgsql/14
WILL INSTALL  /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/ddl/updates/postgresql.14_2001.sql to /usr/local/share/dbconfig-common/data/bareos-database-common/upgrade/pgsql/2001
WILL INSTALL  /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/ddl/updates/postgresql.2001_2002.sql to /usr/local/share/dbconfig-common/data/bareos-database-common/upgrade/pgsql/2002
WILL INSTALL  /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/ddl/updates/postgresql.2002_2003.sql to /usr/local/share/dbconfig-common/data/bareos-database-common/upgrade/pgsql/2003
WILL INSTALL  /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/ddl/updates/postgresql.2003_2004.sql to /usr/local/share/dbconfig-common/data/bareos-database-common/upgrade/pgsql/2004
WILL INSTALL  /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/ddl/updates/postgresql.2004_2171.sql to /usr/local/share/dbconfig-common/data/bareos-database-common/upgrade/pgsql/2171
WILL INSTALL  /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/ddl/updates/postgresql.2171_2192.sql to /usr/local/share/dbconfig-common/data/bareos-database-common/upgrade/pgsql/2192
WILL INSTALL  /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/ddl/updates/postgresql.2192_2210.sql to /usr/local/share/dbconfig-common/data/bareos-database-common/upgrade/pgsql/2210
WILL INSTALL  /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats/ddl/updates/postgresql.bee.1017_2004.sql to /usr/local/share/dbconfig-common/data/bareos-database-common/upgrade/pgsql/2004
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/filed
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/tools
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/cats
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/ndmp
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/stored
Backends are now unix_tape_device.d;unix_fifo_device.d;droplet_device.d
-- BACKEND_OBJECTS ARE libbareossd-droplet;libbareossd-chunked
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/stored/backends
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/dird
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/dird/dbcopy
using BACKENDS: unix_tape_device.d;unix_fifo_device.d;droplet_device.d
using PLUGINS: python-ldap;python-ovirt
-- Skipping unit tests as gtest was not found
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/lmdb
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/lib
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/findlib
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/plugins
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/plugins/dird
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/plugins/stored
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/plugins/filed
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/fastlz
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/core/src/qt-tray-monitor
-- Found QT5Widgets
 
Configuration on 2021-11-13 21:49:11 : 
 
   Host:                         Linux-5.11.0-40-generic -- ubuntu Ubuntu 20.04.3 LTS 
   Hostname:                     consuela 
   Bareos version:               Bareos 21.0.0~pre993.4cfdab87b (12 November 2021) 
   Build platform:               ubuntu
   Source code location:         /home/hajer/Documents/Wks/bareos-local-tests/bareos/core 
   Modify Debian Control file:   OFF 
   Install binaries:             /usr/local/bin 
   Install system binaries:      /usr/local/sbin 
   Install libraries:            /usr/local/lib/bareos 
   Install system config files:  /usr/local/etc 
   Install Bareos config dir:    /usr/local/etc/bareos 
   Install Bareos config files:  /usr/local/etc/bareos 
   Log directory:                /usr/local/var/log/bareos 
   Scripts directory:            /usr/local/lib/bareos/scripts 
   Archive directory:            /usr/local/var/lib/bareos/storage 
   Working directory:            /usr/local/var/lib/bareos 
   BSR directory:                /usr/local/var/lib/bareos 
   PID directory:                /usr/local/var/lib/bareos 
   Subsys directory:             /usr/local/var/lib/bareos 
   Man directory:                /usr/local/share/man 
   Data directory:               /usr/local/share 
   Backend directory:            /usr/local/lib/bareos/backends 
   Plugin directory:             /usr/local/lib/bareos/plugins 
   C Compiler:                   /usr/bin/cc 9.3.0 
   C++ Compiler:                 /usr/bin/c++ 9.3.0 
   C Compiler flags:              -fdebug-prefix-map=/home/hajer/Documents/Wks/bareos-local-tests/bareos/core=. -fmacro-prefix-map=/home/hajer/Documents/Wks/bareos-local-tests/bareos/core=. -Werror -Wall 
   C++ Compiler flags:            -Wsuggest-override -Wformat -Werror=format-security -fdebug-prefix-map=/home/hajer/Documents/Wks/bareos-local-tests/bareos/core=. -fmacro-prefix-map=/home/hajer/Documents/Wks/bareos-local-tests/bareos/core=. -Wno-unknown-pragmas -Werror -Wall 
   Linker flags:                     
   Libraries:                     
   Statically Linked Tools:       
   Statically Linked FD:          
   Statically Linked SD:          
   Statically Linked DIR:         
   Statically Linked CONS:        
   Database backends:            postgresql 
   default_db_backend:           postgresql 
   db_backend_to_test:           postgresql 
   Database port:                 
   Database name:                bareos 
   Database user:                bareos 
   Database version:             2210 
 
   Job Output Email:             root 
   Traceback Email:              root 
   SMTP Host Address:            localhost 
 
   Director Port:                9101 
   File daemon Port:             9102 
   Storage daemon Port:          9103 
 
   Director User:                 
   Director Group:                
   Storage Daemon User:           
   Storage DaemonGroup:           
   File Daemon User:              
   File Daemon Group:             
 
   Large file support:           
   readline support:             ROOT_DIR:/usr INCLUDE_DIR:/usr/include LIBRARY:/usr/lib/x86_64-linux-gnu/libreadline.so
 
   TLS support:                  1 
   Encryption support:           1 
   OpenSSL support:              TRUE 1.1.1f /usr/include /usr/lib/x86_64-linux-gnu/libssl.so;/usr/lib/x86_64-linux-gnu/libcrypto.so 
   PAM support:                  FALSE   
   ZLIB support:                 TRUE /usr/include /usr/lib/x86_64-linux-gnu/libz.so 
   LZO2 support:                 TRUE /usr/include /usr/lib/x86_64-linux-gnu/liblzo2.so 
   JANSSON support:              TRUE 2.12 /usr/include /usr/lib/x86_64-linux-gnu/libjansson.so
   VIXDISKLIB support:           FALSE   
   LMDB support:                 ON 
   NDMP support:                 ON 
   Build ndmjob binary:          OFF 
   enable-lockmgr:               OFF 
   tray-monitor support:         1 
   test-plugin support:           
   client-only:                  OFF 
   build-dird:                    
   build-stored:                  
   Plugin support:                
   AFS support:                   
   ACL support:                  1 /usr/lib/x86_64-linux-gnu/libacl.so
   XATTR support:                YES 
   SCSI Crypto support:          OFF  
   GFAPI(GLUSTERFS) support:     FALSE   
   CEPH RADOS support:           FALSE   
   RADOS striping support:       FALSE   
   CEPHFS support:               FALSE   
   Python2 support:              TRUE 2.7.18 /usr/include/python2.7 /usr/bin/python2.7
   Python3 support:              FALSE 3.8.10 _Python3_INCLUDE_DIR-NOTFOUND /usr/bin/python3.8
   systemd support:              ON /lib/systemd/system
   Batch insert enabled:         1
   PostgreSQL Version:           12.9 
   GTest enabled:                0
   Intl support:                 TRUE  
   Dynamic cats backends:        ON 1 
   Dynamic storage backends:     ON 1 unix_tape_device.d;unix_fifo_device.d;droplet_device.d 
   PLUGINS:                      python-ldap;python-ovirt 
   Build for Test Coverage :     OFF 
   PSCMD:                        /usr/bin/ps -e
   PS:                           /usr/bin/ps
   PIDOF:                        /usr/bin/pidof
   PGREP:                        /usr/bin/pgrep
   AWK:                          /usr/bin/awk
   GAWK:                         GAWK-NOTFOUND
   GDB:                          /usr/bin/gdb
   RPCGEN:                       /usr/bin/rpcgen
   MTX:                          /usr/sbin/mtx
   MT:                           /usr/bin/mt
   MINIO:                        MINIO-NOTFOUND
   S3CMD:                        S3CMD-NOTFOUND
   XTRABACKUP:                   XTRABACKUP-NOTFOUND
   DEVELOPER:                    OFF
   LocalBuildDefinitionsFile:    NOTFOUND
   HAVE_IS_TRIVIALLY_COPYABLE:   TRUE
   do-static-code-checks:        OFF
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/webui
   PHP is /usr/bin/php 
   Install system config files:   /usr/local/etc 
   Install Bareos config dir:     /usr/local/etc/bareos 
   Install BareosWebui configdir: /usr/local/etc/bareos-webui 
   VERSION_STRING:                21.0.0~pre993.4cfdab87b 
   BAREOS_FULL_VERSION:           21.0.0~pre993.4cfdab87b 
   LocalBuildDefinitionsFile:     /home/hajer/Documents/Wks/bareos-local-tests/bareos/webui/cmake/BareosLocalBuildDefinitions.cmake
Entering /home/hajer/Documents/Wks/bareos-local-tests/bareos/systemtests
   SYSTEMTESTS_S3_USE_HTTPS: ON
-- POSTGRES_BIN_PATH is /usr/lib/postgresql/12/bin
-- RUN_SYSTEMTESTS_ON_INSTALLED_FILES="OFF"
-- Looking for binaries and paths...
   BAREOS_DIR_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/dird/bareos-dir
   BAREOS_DBCHECK_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/dird/bareos-dbcheck
   BAREOS_FD_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/filed/bareos-fd
   BAREOS_SD_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/stored/bareos-sd
   BAREOS_DBCOPY_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/dird/dbcopy/bareos-dbcopy
   BCOPY_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/stored/bcopy
   BTAPE_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/stored/btape
   BEXTRACT_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/stored/bextract
   BAREOS_SD_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/stored/bareos-sd
   BLS_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/stored/bls
   BSCAN_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/stored/bscan
   BCONSOLE_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/console/bconsole
   BSMTP_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/tools/bsmtp
   BWILD_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/tools/bwild
   BPLUGINFO_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/tools/bpluginfo
   BSMTP_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/tools/bsmtp
   BSCRYPTO_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/tools/bscrypto
   BTESTLS_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/tools/btestls
   DRIVETYPE_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/tools/drivetype
   FSTYPE_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/tools/fstype
   BREGEX_TO_TEST is /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/tools/bregex
-- FD_PLUGINS_DIR_TO_TEST="/home/hajer/Documents/Wks/bareos-local-tests/build/core/src/plugins/filed"
-- SD_PLUGINS_DIR_TO_TEST="/home/hajer/Documents/Wks/bareos-local-tests/build/core/src/plugins/stored"
-- DIR_PLUGINS_DIR_TO_TEST="/home/hajer/Documents/Wks/bareos-local-tests/build/core/src/plugins/dird"
-- BACKEND_DIR_TO_TEST="/home/hajer/Documents/Wks/bareos-local-tests/build/core/src/cats"
-- SD_BACKEND_DIR_TO_TEST="/home/hajer/Documents/Wks/bareos-local-tests/build/core/src/stored/backends"
-- WEBUI_PUBLIC_DIR_TO_TEST="/home/hajer/Documents/Wks/bareos-local-tests/bareos/systemtests/../webui/public"
-- db version from cats.h is 2210
-- DEFAULT_DB_TYPE="postgresql"
-- Looking for pam test requirements ...
-- checking for library pam and security/pam_appl.h header ...
--   HAVE_PAM=0
--               ERROR:  pam libraries NOT found. 
--               ERROR:  pam includes  NOT found. 
--   PAM_FOUND=FALSE
--   PAM_LIBRARIES=
--   PAM_INCLUDE_DIRS=
--   HAVE_PAM=0
-- checking for library pam_wrapper...
--               ERROR:  pam_wrapper libraries NOT found. 
--   PAM_WRAPPER_FOUND=FALSE
--   PAM_WRAPPER_LIBRARIES=
--   HAVE_LIBPAM_WRAPPER=0
   PAM_FOUND:                FALSE
   PAM_WRAPPER_LIBRARIES:    
   PAMTESTER:                PAMTESTER-NOTFOUND
   PAM_EXEC_AVAILABLE:       
-- NOT OK: disabling pam tests as not all requirements were found.
-- Looking for webui test requirements ...
-- python module pyversion 2 selenium NOT found
-- python module pyversion 3 selenium NOT found
   PHP:                    /usr/bin/php
   PYTHON_EXECUTABLE:      /usr/bin/python2.7
   PYMODULE_2_SELENIUM_FOUND:FALSE
   PYMODULE_3_SELENIUM_FOUND:FALSE
   CHROMEDRIVER:           CHROMEDRIVER-NOTFOUND
-- NOT OK: disabling webui tests as not all requirements were found.
-- Disabled test: webui:admin-client_disabling
-- Disabled test: webui:admin-rerun_job
-- Disabled test: webui:admin-restore
-- Disabled test: webui:admin-run_configured_job
-- Disabled test: webui:admin-run_default_job
-- Disabled test: webui:readonly-client_disabling
-- Disabled test: webui:readonly-rerun_job
-- Disabled test: webui:readonly-restore
-- Disabled test: webui:readonly-run_configured_job
-- Disabled test: webui:readonly-run_default_job
running cd "/home/hajer/Documents/Wks/bareos-local-tests/build/systemtests/tests/acl" && /usr/bin/setfacl -m user:0:rw- acl-test-file.txt  2>&1
-- Test: system:acl (baseport=30001)
--       * system:acl
-- Test: system:ai-consolidate-ignore-duplicate-job (baseport=30011)
--       * system:ai-consolidate-ignore-duplicate-job
-- Test: system:autochanger => disabled ()
-- Test: system:bareos (baseport=30021)
--       * system:bareos
-- Test: system:bareos-acl (baseport=30031)
--       * system:bareos-acl
-- Test: system:bconsole-pam => disabled ()
-- Test: system:bconsole-status-client (baseport=30041)
--       * system:bconsole-status-client
-- Test: system:block-size => disabled ()
-- Test: system:bscan-bextract-bls (baseport=30051)
--       * system:bscan-bextract-bls
-- Could NOT find GTest (missing: GTest_DIR)
-- Test: system:catalog => disabled (gtest not found or RUN_SYSTEMTESTS_ON_INSTALLED_FILES)
-- Test: system:chflags => disabled ()
-- Test: system:client-initiated (baseport=30061)
--       * system:client-initiated
-- Test: system:config-dump (baseport=30071)
--       * system:config-dump
-- Test: system:config-syntax-crash (baseport=30081)
--       * system:config-syntax-crash
-- Test: system:copy-archive-job (baseport=30091)
--       * system:copy-archive-job
-- Test: system:copy-bscan (baseport=30101)
--       * system:copy-bscan
-- Test: system:copy-remote-bscan (baseport=30111)
--       * system:copy-remote-bscan
-- Test: system:dbcopy-mysql-postgresql => disabled ()
-- Test: system:deprecation (baseport=30121)
--       * system:deprecation
-- Test: system:droplet-s3 => disabled ()
-- Test: system:encrypt-signature (baseport=30131)
--       * system:encrypt-signature
-- Test: system:encrypt-signature-tls-cert (baseport=30141)
--       * system:encrypt-signature-tls-cert
-- Test: system:fileset-multiple-blocks (baseport=30151)
--       * system:fileset-multiple-blocks:setup
--       * system:fileset-multiple-blocks:include-blocks
--       * system:fileset-multiple-blocks:options-blocks
--       * system:fileset-multiple-blocks:cleanup
-- Test: system:filesets (baseport=30161)
--       * system:filesets
-- Test: system:gfapi-fd => disabled ()
-- Test: system:glusterfs-backend => disabled ()
-- Test: system:list-backups (baseport=30171)
--       * system:list-backups
-- Test: system:messages => disabled ()
-- Test: system:multiplied-device (baseport=30181)
--       * system:multiplied-device
-- Test: system:ndmp => disabled (ndmp_data_agent_address not given)
-- Test: system:notls (baseport=30191)
--       * system:notls
-- Test: system:passive (baseport=30201)
--       * system:passive
-- Test: system:py2plug-dir (baseport=30211)
--       * system:py2plug-dir
-- Test: system:py3plug-dir => disabled (no python3-dir)
-- python module pyversion 2 ldap NOT found
-- Test: system:py2plug-fd-ldap => disabled ()
-- python module pyversion 2 libcloud NOT found
-- Test: system:py2plug-fd-libcloud => disabled (S3CMD, MINIO or LIBCLOUD MODULE not found)
-- Test: system:py3plug-fd-libcloud => disabled (requires python3-fd)
-- Test: system:py2plug-fd-local-fileset (baseport=30221)
--       * system:py2plug-fd-local-fileset
-- Test: system:py3plug-fd-local-fileset => disabled ()
-- Test: system:py2plug-fd-local-fileset-restoreobject (baseport=30231)
--       * system:py2plug-fd-local-fileset-restoreobject
-- Test: system:py3plug-fd-local-fileset-restoreobject => disabled ()
-- Test: system:py2plug-fd-ovirt => disabled (no python-fd or ovirt_server not set)
-- Test: system:py3plug-fd-ovirt => disabled (no python3-fd or ovirt_server not set)
-- Test: system:py2plug-fd-percona-xtrabackup => disabled (no python-fd or XTRABACKUP not set)
-- Test: system:py3plug-fd-percona-xtrabackup => disabled (no python3-fd or XTRABACKUP not set)
disabling py2plug-fd-postgres, Python2 not supported for this plugin
-- Test: system:py2plug-fd-postgres => disabled ()
-- Test: system:py2plug-fd-vmware => disabled (no python-fd or vmware_server not set)
-- Test: system:py3plug-fd-vmware => disabled (no python3-fd or vmware_server not set)
-- Test: system:py2plug-sd (baseport=30241)
--       * system:py2plug-sd
-- Test: system:py3plug-sd => disabled (no python3-sd)
-- Test: system:python-bareos (baseport=30251)
--       * system:python-bareos:setup
--       * system:python-bareos:acl
--       * system:python-bareos:bvfs
--       * system:python-bareos:delete
--       * system:python-bareos:filedaemon
--       * system:python-bareos:json
--       * system:python-bareos:json_config
--       * system:python-bareos:plaintext
--       * system:python-bareos:protocol_124
--       * system:python-bareos:python_bareos_module
--       * system:python-bareos:runscript
--       * system:python-bareos:show
--       * system:python-bareos:tlspsk
--       * system:python-bareos:cleanup
-- Test: system:python-pam => disabled ()
-- Test: system:quota-softquota (baseport=30261)
--       * system:quota-softquota
-- Test: system:reload (baseport=30271)
--       * system:reload:setup
--       * system:reload:add-client
--       * system:reload:add-duplicate-job
--       * system:reload:add-empty-job
--       * system:reload:add-second-director
--       * system:reload:add-uncommented-string
--       * system:reload:unchanged-config
--       * system:reload:cleanup
-- python module pyversion 3 fastapi NOT found
-- python module pyversion 3 uvicorn NOT found
-- Test: system:restapi => disabled ()
-- Test: system:scheduler-backup (baseport=30281)
--       * system:scheduler-backup
-- Test: system:sparse-file (baseport=30291)
--       * system:sparse-file
-- Test: system:spool (baseport=30301)
--       * system:spool
-- Test: system:truncate-command (baseport=30311)
--       * system:truncate-command
-- Test: system:upgrade-database (baseport=30321)
--       * system:upgrade-database
-- Test: system:virtualfull (baseport=30331)
--       * system:virtualfull
-- Test: system:virtualfull-bscan (baseport=30341)
--       * system:virtualfull-bscan
-- Test: system:volume-pruning (baseport=30351)
--       * system:volume-pruning
running cd "/home/hajer/Documents/Wks/bareos-local-tests/build/systemtests/tests/xattr" && /usr/bin/setfattr --name=user.cmake-check --value=xattr-value xattr-test-file.txt  2>&1
-- Test: system:xattr (baseport=30361)
--       * system:xattr
-- Configuring done
-- Generating done
-- Build files have been written to: /home/hajer/Documents/Wks/bareos-local-tests/build
```
- make_build_output
```
Scanning dependencies of target droplet
[  0%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/conn.c.o
[  0%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/converters.c.o
[  1%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/value.c.o
[  1%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/dict.c.o
[  1%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/droplet.c.o
[  1%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/httprequest.c.o
[  1%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/httpreply.c.o
[  2%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/pricing.c.o
[  2%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/profile.c.o
[  2%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/req.c.o
[  2%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/vec.c.o
[  2%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/sbuf.c.o
[  2%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/dbuf.c.o
[  3%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/ntinydb.c.o
[  3%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/utils.c.o
[  3%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/rest.c.o
[  3%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/sysmd.c.o
[  3%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/id_scheme.c.o
[  4%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/task.c.o
[  4%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/async.c.o
[  4%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/addrlist.c.o
[  4%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/getdate.c.o
[  4%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/vfs.c.o
[  5%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/uks.c.o
[  5%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/gc.c.o
[  5%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend.c.o
[  5%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/replyparser.c.o
[  5%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/reqbuilder.c.o
[  5%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/auth/v2.c.o
[  6%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/auth/v4.c.o
[  6%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/copy.c.o
[  6%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/delete.c.o
[  6%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/delete_all.c.o
[  6%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/delete_bucket.c.o
[  7%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/genurl.c.o
[  7%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/get.c.o
[  7%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/get_capabilities.c.o
[  7%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/head.c.o
[  7%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/head_raw.c.o
[  8%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/list_all_my_buckets.c.o
[  8%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/list_bucket.c.o
[  8%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/list_bucket_attrs.c.o
[  8%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/make_bucket.c.o
[  8%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/put.c.o
[  8%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/multipart.c.o
[  9%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/stream_resume.c.o
[  9%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/stream_get.c.o
[  9%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/stream_put.c.o
[  9%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/s3/backend/stream_flush.c.o
[  9%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/cdmi/backend.c.o
[ 10%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/cdmi/replyparser.c.o
[ 10%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/cdmi/reqbuilder.c.o
[ 10%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/cdmi/crcmodel.c.o
[ 10%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/cdmi/object_id.c.o
[ 10%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/swift/backend.c.o
[ 10%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/swift/replyparser.c.o
[ 11%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/swift/reqbuilder.c.o
[ 11%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/srws/backend.c.o
[ 11%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/srws/replyparser.c.o
[ 11%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/srws/reqbuilder.c.o
[ 11%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/srws/key.c.o
[ 12%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/sproxyd/backend.c.o
[ 12%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/sproxyd/replyparser.c.o
[ 12%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/sproxyd/reqbuilder.c.o
[ 12%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/sproxyd/backend/copy_id.c.o
[ 12%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/sproxyd/backend/delete_all_id.c.o
[ 13%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/sproxyd/backend/delete_id.c.o
[ 13%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/sproxyd/backend/get_id.c.o
[ 13%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/sproxyd/backend/head_id.c.o
[ 13%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/sproxyd/backend/put_id.c.o
[ 13%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/sproxyd/key.c.o
[ 13%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/posix/backend.c.o
[ 14%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/posix/reqbuilder.c.o
[ 14%] Building C object core/src/droplet/libdroplet/CMakeFiles/droplet.dir/src/backend/posix/replyparser.c.o
[ 14%] Linking C shared library libbareosdroplet.so
[ 14%] Built target droplet
Scanning dependencies of target bareosfastlz
[ 15%] Building C object core/src/fastlz/CMakeFiles/bareosfastlz.dir/src/fastlzlib.c.o
[ 15%] Building C object core/src/fastlz/CMakeFiles/bareosfastlz.dir/src/fastlz.c.o
[ 15%] Building C object core/src/fastlz/CMakeFiles/bareosfastlz.dir/src/lz4.c.o
[ 15%] Building C object core/src/fastlz/CMakeFiles/bareosfastlz.dir/src/lz4hc.c.o
[ 15%] Linking C shared library libbareosfastlz.so
[ 15%] Built target bareosfastlz
Scanning dependencies of target bareoslmdb
[ 15%] Building C object core/src/lmdb/CMakeFiles/bareoslmdb.dir/mdb.c.o
[ 15%] Building C object core/src/lmdb/CMakeFiles/bareoslmdb.dir/midl.c.o
[ 15%] Linking C shared library libbareoslmdb.so
[ 15%] Built target bareoslmdb
Scanning dependencies of target version-obj
[ 16%] Building CXX object core/src/lib/CMakeFiles/version-obj.dir/version.cc.o
[ 16%] Built target version-obj
Scanning dependencies of target bareos
[ 16%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/address_conf.cc.o
[ 16%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/alist.cc.o
[ 16%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/attr.cc.o
[ 17%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/attribs.cc.o
[ 17%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/backtrace.cc.o
[ 17%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/base64.cc.o
[ 17%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/berrno.cc.o
[ 17%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bget_msg.cc.o
[ 18%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/binflate.cc.o
[ 18%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bnet_server_tcp.cc.o
[ 18%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bnet.cc.o
[ 18%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bnet_network_dump.cc.o
[ 18%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bnet_network_dump_private.cc.o
[ 18%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bpipe.cc.o
[ 19%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/breg.cc.o
[ 19%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bregex.cc.o
[ 19%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bsnprintf.cc.o
[ 19%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bsock.cc.o
[ 19%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bsock_tcp.cc.o
[ 20%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bstringlist.cc.o
[ 20%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bsys.cc.o
[ 20%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/btime.cc.o
[ 20%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/btimers.cc.o
[ 20%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/cbuf.cc.o
[ 21%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/connection_pool.cc.o
[ 21%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/cram_md5.cc.o
[ 21%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/crypto.cc.o
[ 21%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/crypto_cache.cc.o
[ 21%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/crypto_none.cc.o
[ 21%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/crypto_nss.cc.o
[ 22%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/crypto_openssl.cc.o
[ 22%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/crypto_wrap.cc.o
[ 22%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/daemon.cc.o
[ 22%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/devlock.cc.o
[ 22%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/dlist.cc.o
[ 23%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/edit.cc.o
[ 23%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/fnmatch.cc.o
[ 23%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/guid_to_name.cc.o
[ 23%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/hmac.cc.o
[ 23%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/htable.cc.o
[ 23%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/jcr.cc.o
[ 24%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/lockmgr.cc.o
[ 24%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/mem_pool.cc.o
[ 24%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/message.cc.o
[ 24%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/messages_resource.cc.o
[ 24%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/mntent_cache.cc.o
[ 25%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/output_formatter.cc.o
[ 25%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/output_formatter_resource.cc.o
[ 25%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/passphrase.cc.o
[ 25%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/path_list.cc.o
[ 25%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/plugins.cc.o
[ 26%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bpoll.cc.o
[ 26%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/priv.cc.o
[ 26%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/recent_job_results_list.cc.o
[ 26%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/rblist.cc.o
[ 26%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/runscript.cc.o
[ 26%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/rwlock.cc.o
[ 27%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/scan.cc.o
[ 27%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/scsi_crypto.cc.o
[ 27%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/scsi_lli.cc.o
[ 27%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/serial.cc.o
[ 27%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/signal.cc.o
[ 28%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/status_packet.cc.o
[ 28%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/thread_list.cc.o
[ 28%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/thread_specific_data.cc.o
[ 28%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/timer_thread.cc.o
[ 28%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/tls.cc.o
[ 29%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/tls_conf.cc.o
[ 29%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/tls_openssl.cc.o
[ 29%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/tls_openssl_crl.cc.o
[ 29%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/tls_openssl_private.cc.o
[ 29%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/tree.cc.o
[ 29%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/try_tls_handshake_as_a_server.cc.o
[ 30%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/compression.cc.o
[ 30%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/util.cc.o
[ 30%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/var.cc.o
[ 30%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/watchdog.cc.o
[ 30%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/watchdog_timer.cc.o
[ 31%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/scsi_tapealert.cc.o
[ 31%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/bareos_resource.cc.o
[ 31%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/configured_tls_policy_getter.cc.o
[ 31%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/ini.cc.o
[ 31%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/lex.cc.o
[ 31%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/parse_bsr.cc.o
[ 32%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/res.cc.o
[ 32%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/parse_conf.cc.o
[ 32%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/parse_conf_init_resource.cc.o
[ 32%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/parse_conf_state_machine.cc.o
[ 32%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/qualified_resource_name_type_converter.cc.o
[ 33%] Building CXX object core/src/lib/CMakeFiles/bareos.dir/osinfo.cc.o
[ 33%] Linking CXX shared library libbareos.so
[ 33%] Built target bareos
Scanning dependencies of target fd_objects
[ 34%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/accurate.cc.o
[ 34%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/authenticate.cc.o
[ 34%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/crypto.cc.o
[ 34%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/evaluate_job_command.cc.o
[ 34%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/fd_plugins.cc.o
[ 35%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/fileset.cc.o
[ 35%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/sd_cmds.cc.o
[ 35%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/verify.cc.o
[ 35%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/accurate_htable.cc.o
[ 35%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/backup.cc.o
[ 35%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/dir_cmd.cc.o
[ 36%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/filed_globals.cc.o
[ 36%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/heartbeat.cc.o
[ 36%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/socket_server.cc.o
[ 36%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/verify_vol.cc.o
[ 36%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/accurate_lmdb.cc.o
[ 37%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/compression.cc.o
[ 37%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/estimate.cc.o
[ 37%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/filed_conf.cc.o
[ 37%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/restore.cc.o
[ 37%] Building CXX object core/src/filed/CMakeFiles/fd_objects.dir/status.cc.o
[ 37%] Linking CXX static library libfd_objects.a
[ 37%] Built target fd_objects
Scanning dependencies of target bareosfind
[ 37%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/acl.cc.o
[ 37%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/attribs.cc.o
[ 38%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/bfile.cc.o
[ 38%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/create_file.cc.o
[ 38%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/drivetype.cc.o
[ 38%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/enable_priv.cc.o
[ 38%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/find_one.cc.o
[ 38%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/find.cc.o
[ 39%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/fstype.cc.o
[ 39%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/hardlink.cc.o
[ 39%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/match.cc.o
[ 39%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/mkpath.cc.o
[ 39%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/shadowing.cc.o
[ 40%] Building CXX object core/src/findlib/CMakeFiles/bareosfind.dir/xattr.cc.o
[ 40%] Linking CXX shared library libbareosfind.so
[ 40%] Built target bareosfind
Scanning dependencies of target bareosfd-python2-module
[ 40%] Building CXX object core/src/plugins/filed/python/CMakeFiles/bareosfd-python2-module.dir/module/bareosfd.cc.o
[ 41%] Linking CXX shared module pythonmodules/bareosfd.so
[ 41%] Built target bareosfd-python2-module
Scanning dependencies of target python-fd
[ 41%] Building CXX object core/src/plugins/filed/python/CMakeFiles/python-fd.dir/python-fd.cc.o
[ 42%] Linking CXX shared module ../python-fd.so
[ 42%] Built target python-fd
Scanning dependencies of target bareos-fd
[ 42%] Building CXX object core/src/filed/CMakeFiles/bareos-fd.dir/filed.cc.o
[ 42%] Linking CXX executable bareos-fd
[ 42%] Built target bareos-fd
Scanning dependencies of target bsmtp
[ 42%] Building CXX object core/src/tools/CMakeFiles/bsmtp.dir/bsmtp.cc.o
[ 42%] Linking CXX executable bsmtp
[ 42%] Built target bsmtp
Scanning dependencies of target drivetype
[ 42%] Building CXX object core/src/tools/CMakeFiles/drivetype.dir/drivetype.cc.o
[ 42%] Linking CXX executable drivetype
[ 42%] Built target drivetype
Scanning dependencies of target fstype
[ 43%] Building CXX object core/src/tools/CMakeFiles/fstype.dir/fstype.cc.o
[ 43%] Linking CXX executable fstype
[ 43%] Built target fstype
Scanning dependencies of target bwild
[ 43%] Building CXX object core/src/tools/CMakeFiles/bwild.dir/bwild.cc.o
[ 43%] Linking CXX executable bwild
[ 43%] Built target bwild
Scanning dependencies of target bscrypto
[ 44%] Building CXX object core/src/tools/CMakeFiles/bscrypto.dir/bscrypto.cc.o
[ 44%] Linking CXX executable bscrypto
[ 44%] Built target bscrypto
Scanning dependencies of target btestls
[ 44%] Building CXX object core/src/tools/CMakeFiles/btestls.dir/btestls.cc.o
[ 44%] Linking CXX executable btestls
[ 44%] Built target btestls
Scanning dependencies of target bpluginfo
[ 44%] Building CXX object core/src/tools/CMakeFiles/bpluginfo.dir/bpluginfo.cc.o
[ 45%] Linking CXX executable bpluginfo
[ 45%] Built target bpluginfo
Scanning dependencies of target bregex
[ 45%] Building CXX object core/src/tools/CMakeFiles/bregex.dir/bregex.cc.o
[ 45%] Linking CXX executable bregex
[ 45%] Built target bregex
Scanning dependencies of target bareoscats
[ 45%] Building CXX object core/src/cats/CMakeFiles/bareoscats.dir/cats_backends.cc.o
[ 45%] Linking CXX shared library libbareoscats.so
[ 45%] Built target bareoscats
Scanning dependencies of target bareossql
[ 45%] Building CXX object core/src/cats/CMakeFiles/bareossql.dir/bvfs.cc.o
[ 46%] Building CXX object core/src/cats/CMakeFiles/bareossql.dir/cats.cc.o
[ 46%] Building CXX object core/src/cats/CMakeFiles/bareossql.dir/sql.cc.o
[ 46%] Building CXX object core/src/cats/CMakeFiles/bareossql.dir/sql_create.cc.o
[ 46%] Building CXX object core/src/cats/CMakeFiles/bareossql.dir/sql_delete.cc.o
[ 46%] Building CXX object core/src/cats/CMakeFiles/bareossql.dir/sql_find.cc.o
[ 47%] Building CXX object core/src/cats/CMakeFiles/bareossql.dir/sql_get.cc.o
[ 47%] Building CXX object core/src/cats/CMakeFiles/bareossql.dir/sql_list.cc.o
[ 47%] Building CXX object core/src/cats/CMakeFiles/bareossql.dir/sql_pooling.cc.o
[ 47%] Building CXX object core/src/cats/CMakeFiles/bareossql.dir/sql_query.cc.o
[ 47%] Building CXX object core/src/cats/CMakeFiles/bareossql.dir/sql_update.cc.o
[ 47%] Linking CXX shared library libbareossql.so
[ 47%] Built target bareossql
Scanning dependencies of target bareoscats-postgresql
[ 47%] Building CXX object core/src/cats/CMakeFiles/bareoscats-postgresql.dir/postgresql.cc.o
[ 48%] Building CXX object core/src/cats/CMakeFiles/bareoscats-postgresql.dir/postgresql_batch.cc.o
[ 48%] Building CXX object core/src/cats/CMakeFiles/bareoscats-postgresql.dir/cats_backends.cc.o
[ 48%] Linking CXX shared module libbareoscats-postgresql.so
[ 48%] Built target bareoscats-postgresql
[ 48%] Generating ndmp9.h, ndmp9_xdr.c
[ 49%] Generating ndmp0.h, ndmp0_xdr.c
[ 49%] Generating ndmp2.h, ndmp2_xdr.c
[ 49%] Generating ndmp3.h, ndmp3_xdr.c
[ 49%] Generating ndmp4.h, ndmp4_xdr.c
Scanning dependencies of target bareosndmp
[ 49%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp_translate.c.o
[ 50%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp0_enum_strs.c.o
[ 50%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp0_pp.c.o
[ 50%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp0_xdr.c.o
[ 50%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp0_xmt.c.o
[ 50%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp2_enum_strs.c.o
[ 51%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp2_pp.c.o
[ 51%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp2_translate.c.o
[ 51%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp2_xdr.c.o
[ 51%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp2_xmt.c.o
[ 51%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp3_enum_strs.c.o
[ 52%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp3_pp.c.o
[ 52%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp3_translate.c.o
[ 52%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp3_xdr.c.o
[ 52%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp3_xmt.c.o
[ 52%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp4_enum_strs.c.o
[ 52%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp4_pp.c.o
[ 53%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp4_translate.c.o
[ 53%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp4_xdr.c.o
[ 53%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp4_xmt.c.o
[ 53%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp9_enum_strs.c.o
[ 53%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp9_xdr.c.o
[ 54%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmp9_xmt.c.o
[ 54%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmprotocol.c.o
[ 54%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_agent.c.o
[ 54%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_bstf.c.o
[ 54%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_chan.c.o
[ 54%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_config.c.o
[ 55%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_conn.c.o
[ 55%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_cstr.c.o
[ 55%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_fhdb.c.o
[ 55%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_fhh.c.o
[ 55%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_log.c.o
[ 56%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_md5.c.o
[ 56%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_media.c.o
[ 56%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_nmb.c.o
[ 56%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_scsi.c.o
[ 56%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_stzf.c.o
[ 57%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndml_util.c.o
[ 57%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_comm_dispatch.c.o
[ 57%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_comm_job.c.o
[ 57%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_comm_session.c.o
[ 57%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_comm_subr.c.o
[ 57%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_control.c.o
[ 58%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_cops_backreco.c.o
[ 58%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_cops_labels.c.o
[ 58%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_cops_query.c.o
[ 58%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_cops_robot.c.o
[ 58%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_ctrl_calls.c.o
[ 59%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_ctrl_conn.c.o
[ 59%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_ctrl_media.c.o
[ 59%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_ctrl_robot.c.o
[ 59%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_ctst_data.c.o
[ 59%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_ctst_mover.c.o
[ 60%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_ctst_subr.c.o
[ 60%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_ctst_tape.c.o
[ 60%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_data_fh.c.o
[ 60%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_data_pfe.c.o
[ 60%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_data.c.o
[ 60%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_image_stream.c.o
[ 61%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_listmgmt.c.o
[ 61%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_noti_calls.c.o
[ 61%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_robot.c.o
[ 61%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_robot_simulator.c.o
[ 61%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_tape.c.o
[ 62%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndma_tape_simulator.c.o
[ 62%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/md5c.c.o
[ 62%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/ndmos.c.o
[ 62%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/smc_api.c.o
[ 62%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/smc_parse.c.o
[ 62%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/smc_pp.c.o
[ 63%] Building C object core/src/ndmp/CMakeFiles/bareosndmp.dir/wraplib.c.o
[ 63%] Linking C shared library libbareosndmp.so
[ 63%] Built target bareosndmp
Scanning dependencies of target bareossd
[ 63%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/acquire.cc.o
[ 63%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/ansi_label.cc.o
[ 63%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/askdir.cc.o
[ 64%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/autochanger.cc.o
[ 64%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/autochanger_resource.cc.o
[ 64%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/block.cc.o
[ 64%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/bsr.cc.o
[ 64%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/butil.cc.o
[ 65%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/crc32/crc32.cc.o
[ 65%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/dev.cc.o
[ 65%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/device.cc.o
[ 65%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/device_control_record.cc.o
[ 65%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/device_resource.cc.o
[ 65%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/ebcdic.cc.o
[ 66%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/label.cc.o
[ 66%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/lock.cc.o
[ 66%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/mount.cc.o
[ 66%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/read_record.cc.o
[ 66%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/record.cc.o
[ 67%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/reserve.cc.o
[ 67%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/scan.cc.o
[ 67%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/sd_backends.cc.o
[ 67%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/sd_device_control_record.cc.o
[ 67%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/sd_plugins.cc.o
[ 68%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/sd_stats.cc.o
[ 68%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/spool.cc.o
[ 68%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/stored_globals.cc.o
[ 68%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/stored_conf.cc.o
[ 68%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/vol_mgr.cc.o
[ 68%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/wait.cc.o
[ 69%] Building CXX object core/src/stored/CMakeFiles/bareossd.dir/backends/unix_file_device.cc.o
[ 69%] Linking CXX shared library libbareossd.so
[ 69%] Built target bareossd
Scanning dependencies of target stored_objects
[ 69%] Building CXX object core/src/stored/CMakeFiles/stored_objects.dir/append.cc.o
[ 69%] Building CXX object core/src/stored/CMakeFiles/stored_objects.dir/askdir.cc.o
[ 69%] Building CXX object core/src/stored/CMakeFiles/stored_objects.dir/authenticate.cc.o
[ 69%] Building CXX object core/src/stored/CMakeFiles/stored_objects.dir/dir_cmd.cc.o
[ 70%] Building CXX object core/src/stored/CMakeFiles/stored_objects.dir/fd_cmds.cc.o
[ 70%] Building CXX object core/src/stored/CMakeFiles/stored_objects.dir/job.cc.o
[ 70%] Building CXX object core/src/stored/CMakeFiles/stored_objects.dir/mac.cc.o
[ 70%] Building CXX object core/src/stored/CMakeFiles/stored_objects.dir/ndmp_tape.cc.o
[ 70%] Building CXX object core/src/stored/CMakeFiles/stored_objects.dir/read.cc.o
[ 71%] Building CXX object core/src/stored/CMakeFiles/stored_objects.dir/sd_cmds.cc.o
[ 71%] Building CXX object core/src/stored/CMakeFiles/stored_objects.dir/sd_stats.cc.o
[ 71%] Building CXX object core/src/stored/CMakeFiles/stored_objects.dir/socket_server.cc.o
[ 71%] Building CXX object core/src/stored/CMakeFiles/stored_objects.dir/status.cc.o
[ 71%] Linking CXX static library libstored_objects.a
[ 71%] Built target stored_objects
Scanning dependencies of target bareos-sd
[ 71%] Building CXX object core/src/stored/CMakeFiles/bareos-sd.dir/stored.cc.o
[ 71%] Linking CXX executable bareos-sd
[ 71%] Built target bareos-sd
Scanning dependencies of target bls
[ 71%] Building CXX object core/src/stored/CMakeFiles/bls.dir/bls.cc.o
[ 71%] Linking CXX executable bls
[ 71%] Built target bls
Scanning dependencies of target bextract
[ 71%] Building CXX object core/src/stored/CMakeFiles/bextract.dir/bextract.cc.o
[ 72%] Linking CXX executable bextract
[ 72%] Built target bextract
Scanning dependencies of target bscan
[ 72%] Building CXX object core/src/stored/CMakeFiles/bscan.dir/bscan.cc.o
[ 72%] Linking CXX executable bscan
[ 72%] Built target bscan
Scanning dependencies of target bcopy
[ 72%] Building CXX object core/src/stored/CMakeFiles/bcopy.dir/bcopy.cc.o
[ 72%] Linking CXX executable bcopy
[ 72%] Built target bcopy
Scanning dependencies of target btape
[ 72%] Building CXX object core/src/stored/CMakeFiles/btape.dir/btape.cc.o
[ 73%] Linking CXX executable btape
[ 73%] Built target btape
Scanning dependencies of target bareossd-gentape
[ 73%] Building CXX object core/src/stored/backends/CMakeFiles/bareossd-gentape.dir/generic_tape_device.cc.o
[ 73%] Linking CXX shared library libbareossd-gentape.so
[ 73%] Built target bareossd-gentape
Scanning dependencies of target bareossd-tape
[ 73%] Building CXX object core/src/stored/backends/CMakeFiles/bareossd-tape.dir/unix_tape_device.cc.o
[ 73%] Linking CXX shared module libbareossd-tape.so
[ 73%] Built target bareossd-tape
Scanning dependencies of target bareossd-fifo
[ 73%] Building CXX object core/src/stored/backends/CMakeFiles/bareossd-fifo.dir/unix_fifo_device.cc.o
[ 73%] Linking CXX shared module libbareossd-fifo.so
[ 73%] Built target bareossd-fifo
Scanning dependencies of target bareossd-chunked
[ 73%] Building CXX object core/src/stored/backends/CMakeFiles/bareossd-chunked.dir/ordered_cbuf.cc.o
[ 73%] Building CXX object core/src/stored/backends/CMakeFiles/bareossd-chunked.dir/chunked_device.cc.o
[ 73%] Linking CXX shared library libbareossd-chunked.so
[ 73%] Built target bareossd-chunked
Scanning dependencies of target bareossd-droplet
[ 74%] Building CXX object core/src/stored/backends/CMakeFiles/bareossd-droplet.dir/droplet_device.cc.o
[ 74%] Linking CXX shared module libbareossd-droplet.so
[ 74%] Built target bareossd-droplet
Scanning dependencies of target bareos-dbcheck
[ 74%] Building CXX object core/src/dird/CMakeFiles/bareos-dbcheck.dir/dbcheck.cc.o
[ 74%] Building CXX object core/src/dird/CMakeFiles/bareos-dbcheck.dir/dbcheck_utils.cc.o
[ 74%] Building CXX object core/src/dird/CMakeFiles/bareos-dbcheck.dir/dird_conf.cc.o
[ 75%] Building CXX object core/src/dird/CMakeFiles/bareos-dbcheck.dir/dird_globals.cc.o
[ 75%] Building CXX object core/src/dird/CMakeFiles/bareos-dbcheck.dir/ua_acl.cc.o
[ 75%] Building CXX object core/src/dird/CMakeFiles/bareos-dbcheck.dir/ua_audit.cc.o
[ 75%] Building CXX object core/src/dird/CMakeFiles/bareos-dbcheck.dir/run_conf.cc.o
[ 75%] Building CXX object core/src/dird/CMakeFiles/bareos-dbcheck.dir/inc_conf.cc.o
[ 75%] Linking CXX executable bareos-dbcheck
[ 75%] Built target bareos-dbcheck
Scanning dependencies of target dird_objects
[ 75%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/admin.cc.o
[ 76%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/archive.cc.o
[ 76%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/authenticate.cc.o
[ 76%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/authenticate_console.cc.o
[ 76%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/autoprune.cc.o
[ 76%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/backup.cc.o
[ 77%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/bsr.cc.o
[ 77%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/catreq.cc.o
[ 77%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/check_catalog.cc.o
[ 77%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/consolidate.cc.o
[ 77%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/dbcheck_utils.cc.o
[ 77%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/dird_globals.cc.o
[ 78%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/dir_plugins.cc.o
[ 78%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/dird_conf.cc.o
[ 78%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/expand.cc.o
[ 78%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/fd_cmds.cc.o
[ 78%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/get_database_connection.cc.o
[ 79%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/getmsg.cc.o
[ 79%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/inc_conf.cc.o
[ 79%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/job.cc.o
[ 79%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/jobq.cc.o
[ 79%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/job_trigger.cc.o
[ 79%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/migrate.cc.o
[ 80%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/msgchan.cc.o
[ 80%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_dma_storage.cc.o
[ 80%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_dma_backup_common.cc.o
[ 80%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_dma_backup_NDMP_BAREOS.cc.o
[ 80%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_dma_backup_NDMP_NATIVE.cc.o
[ 81%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_dma_generic.cc.o
[ 81%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_dma_restore_common.cc.o
[ 81%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_dma_restore_NDMP_BAREOS.cc.o
[ 81%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_dma_restore_NDMP_NATIVE.cc.o
[ 81%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_fhdb_common.cc.o
[ 82%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_fhdb_helpers.cc.o
[ 82%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_slot2elemaddr.cc.o
[ 82%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_fhdb_mem.cc.o
[ 82%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_fhdb_lmdb.cc.o
[ 82%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ndmp_ndmmedia_db_helpers.cc.o
[ 82%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/newvol.cc.o
[ 83%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/next_vol.cc.o
[ 83%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/quota.cc.o
[ 83%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/run_hour_validator.cc.o
[ 83%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/socket_server.cc.o
[ 83%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/recycle.cc.o
[ 84%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/restore.cc.o
[ 84%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/run_conf.cc.o
[ 84%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/run_on_incoming_connect_interval.cc.o
[ 84%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/sd_cmds.cc.o
[ 84%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/scheduler.cc.o
[ 85%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/scheduler_job_item_queue.cc.o
[ 85%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/scheduler_private.cc.o
[ 85%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/stats.cc.o
[ 85%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/storage.cc.o
[ 85%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua.cc.o
[ 85%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_acl.cc.o
[ 86%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_audit.cc.o
[ 86%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_cmds.cc.o
[ 86%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_configure.cc.o
[ 86%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_db.cc.o
[ 86%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_dotcmds.cc.o
[ 87%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_input.cc.o
[ 87%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_impexp.cc.o
[ 87%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_label.cc.o
[ 87%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_output.cc.o
[ 87%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_prune.cc.o
[ 87%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_purge.cc.o
[ 88%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_query.cc.o
[ 88%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_restore.cc.o
[ 88%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_run.cc.o
[ 88%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_select.cc.o
[ 88%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_server.cc.o
[ 89%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_status.cc.o
[ 89%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_tree.cc.o
[ 89%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/ua_update.cc.o
[ 89%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/vbackup.cc.o
[ 89%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/verify.cc.o
[ 90%] Building CXX object core/src/dird/CMakeFiles/dird_objects.dir/pthread_detach_if_not_detached.cc.o
[ 90%] Linking CXX static library libdird_objects.a
[ 90%] Built target dird_objects
Scanning dependencies of target bareos-dir
[ 91%] Building CXX object core/src/dird/CMakeFiles/bareos-dir.dir/dird.cc.o
[ 91%] Linking CXX executable bareos-dir
[ 91%] Built target bareos-dir
Scanning dependencies of target bareos-dbcopy
[ 92%] Building CXX object core/src/dird/dbcopy/CMakeFiles/bareos-dbcopy.dir/dbcopy.cc.o
[ 92%] Building CXX object core/src/dird/dbcopy/CMakeFiles/bareos-dbcopy.dir/database_table_descriptions.cc.o
[ 92%] Building CXX object core/src/dird/dbcopy/CMakeFiles/bareos-dbcopy.dir/database_column_descriptions.cc.o
[ 92%] Building CXX object core/src/dird/dbcopy/CMakeFiles/bareos-dbcopy.dir/column_description.cc.o
[ 92%] Building CXX object core/src/dird/dbcopy/CMakeFiles/bareos-dbcopy.dir/database_import.cc.o
[ 93%] Building CXX object core/src/dird/dbcopy/CMakeFiles/bareos-dbcopy.dir/database_import_mysql.cc.o
[ 93%] Building CXX object core/src/dird/dbcopy/CMakeFiles/bareos-dbcopy.dir/database_export.cc.o
[ 93%] Building CXX object core/src/dird/dbcopy/CMakeFiles/bareos-dbcopy.dir/database_export_postgresql.cc.o
[ 93%] Building CXX object core/src/dird/dbcopy/CMakeFiles/bareos-dbcopy.dir/progress.cc.o
[ 93%] Linking CXX executable bareos-dbcopy
[ 93%] Built target bareos-dbcopy
Scanning dependencies of target console_objects
[ 93%] Building CXX object core/src/console/CMakeFiles/console_objects.dir/connect_to_director.cc.o
[ 94%] Building CXX object core/src/console/CMakeFiles/console_objects.dir/console_conf.cc.o
[ 94%] Building CXX object core/src/console/CMakeFiles/console_objects.dir/console_globals.cc.o
[ 94%] Building CXX object core/src/console/CMakeFiles/console_objects.dir/console_output.cc.o
[ 94%] Linking CXX static library libconsole_objects.a
[ 94%] Built target console_objects
Scanning dependencies of target bconsole
[ 95%] Building CXX object core/src/console/CMakeFiles/bconsole.dir/console.cc.o
[ 95%] Linking CXX executable bconsole
[ 95%] Built target bconsole
Scanning dependencies of target bareosdir-python2-module
[ 95%] Building CXX object core/src/plugins/dird/python/CMakeFiles/bareosdir-python2-module.dir/module/bareosdir.cc.o
[ 95%] Linking CXX shared module pythonmodules/bareosdir.so
[ 95%] Built target bareosdir-python2-module
Scanning dependencies of target python-dir
[ 95%] Building CXX object core/src/plugins/dird/python/CMakeFiles/python-dir.dir/python-dir.cc.o
[ 95%] Linking CXX shared module ../python-dir.so
[ 95%] Built target python-dir
Scanning dependencies of target scsitapealert-sd
[ 96%] Building CXX object core/src/plugins/stored/CMakeFiles/scsitapealert-sd.dir/scsitapealert/scsitapealert-sd.cc.o
[ 96%] Linking CXX shared module scsitapealert-sd.so
[ 96%] Built target scsitapealert-sd
Scanning dependencies of target scsicrypto-sd
[ 96%] Building CXX object core/src/plugins/stored/CMakeFiles/scsicrypto-sd.dir/scsicrypto/scsicrypto-sd.cc.o
[ 96%] Linking CXX shared module scsicrypto-sd.so
[ 96%] Built target scsicrypto-sd
Scanning dependencies of target autoxflate-sd
[ 96%] Building CXX object core/src/plugins/stored/CMakeFiles/autoxflate-sd.dir/autoxflate/autoxflate-sd.cc.o
[ 96%] Linking CXX shared module autoxflate-sd.so
[ 96%] Built target autoxflate-sd
Scanning dependencies of target bareossd-python2-module
[ 97%] Building CXX object core/src/plugins/stored/python/CMakeFiles/bareossd-python2-module.dir/module/bareossd.cc.o
[ 97%] Linking CXX shared module pythonmodules/bareossd.so
[ 97%] Built target bareossd-python2-module
Scanning dependencies of target python-sd
[ 97%] Building CXX object core/src/plugins/stored/python/CMakeFiles/python-sd.dir/python-sd.cc.o
[ 97%] Linking CXX shared module ../python-sd.so
[ 97%] Built target python-sd
Scanning dependencies of target example-plugin-fd
[ 97%] Building CXX object core/src/plugins/filed/CMakeFiles/example-plugin-fd.dir/example/example-plugin-fd.cc.o
[ 97%] Linking CXX shared module example-plugin-fd.so
[ 97%] Built target example-plugin-fd
Scanning dependencies of target bpipe-fd
[ 97%] Building CXX object core/src/plugins/filed/CMakeFiles/bpipe-fd.dir/bpipe/bpipe-fd.cc.o
[ 97%] Linking CXX shared module bpipe-fd.so
[ 97%] Built target bpipe-fd
Scanning dependencies of target bareosfd-python2-module-tester
[ 97%] Building CXX object core/src/plugins/filed/python/CMakeFiles/bareosfd-python2-module-tester.dir/test/python-fd-module-tester.cc.o
[ 97%] Linking CXX executable bareosfd-python2-module-tester
[ 97%] Built target bareosfd-python2-module-tester
Scanning dependencies of target bareos-tray-monitor_autogen
[ 97%] Automatic MOC and UIC for target bareos-tray-monitor
[ 97%] Built target bareos-tray-monitor_autogen
[ 98%] Automatic RCC for main.qrc
Scanning dependencies of target bareos-tray-monitor
[ 98%] Building CXX object core/src/qt-tray-monitor/CMakeFiles/bareos-tray-monitor.dir/bareos-tray-monitor_autogen/mocs_compilation.cpp.o
[ 98%] Building CXX object core/src/qt-tray-monitor/CMakeFiles/bareos-tray-monitor.dir/tray-monitor.cc.o
[ 98%] Building CXX object core/src/qt-tray-monitor/CMakeFiles/bareos-tray-monitor.dir/authenticate.cc.o
[ 98%] Building CXX object core/src/qt-tray-monitor/CMakeFiles/bareos-tray-monitor.dir/tray_conf.cc.o
[ 99%] Building CXX object core/src/qt-tray-monitor/CMakeFiles/bareos-tray-monitor.dir/traymenu.cc.o
[ 99%] Building CXX object core/src/qt-tray-monitor/CMakeFiles/bareos-tray-monitor.dir/systemtrayicon.cc.o
[ 99%] Building CXX object core/src/qt-tray-monitor/CMakeFiles/bareos-tray-monitor.dir/mainwindow.cc.o
[ 99%] Building CXX object core/src/qt-tray-monitor/CMakeFiles/bareos-tray-monitor.dir/monitoritem.cc.o
[ 99%] Building CXX object core/src/qt-tray-monitor/CMakeFiles/bareos-tray-monitor.dir/monitoritemthread.cc.o
[ 99%] Building CXX object core/src/qt-tray-monitor/CMakeFiles/bareos-tray-monitor.dir/bareos-tray-monitor_autogen/EWIEGA46WW/qrc_main.cpp.o
[100%] Linking CXX executable bareos-tray-monitor
[100%] Built target bareos-tray-monitor
```
- test_output
```
Running tests...
Test project /home/hajer/Documents/Wks/bareos-local-tests/build
PRETEST: running /home/hajer/Documents/Wks/bareos-local-tests/build/systemtests/ctest_custom_pretest.sh script
PRETEST: running system tests on the sourcetree
PRETEST: executing /home/hajer/Documents/Wks/bareos-local-tests/build/core/src/dird/bareos-dir -?
PRETEST: checking configured hostname (consuela) ... OK
PRETEST: checking postgresql connection ... OK
        Start  15: system:acl
  1/101 Test  #15: system:acl ......................................   Passed   23.43 sec
        Start  16: system:ai-consolidate-ignore-duplicate-job
  2/101 Test  #16: system:ai-consolidate-ignore-duplicate-job ......   Passed   31.31 sec
        Start  18: system:bareos
  3/101 Test  #18: system:bareos ...................................   Passed   26.07 sec
        Start  19: system:bareos-acl
  4/101 Test  #19: system:bareos-acl ...............................   Passed    9.47 sec
        Start  21: system:bconsole-status-client
  5/101 Test  #21: system:bconsole-status-client ...................   Passed   23.51 sec
        Start  23: system:bscan-bextract-bls
  6/101 Test  #23: system:bscan-bextract-bls .......................   Passed   27.61 sec
        Start  26: system:client-initiated
  7/101 Test  #26: system:client-initiated .........................   Passed   23.29 sec
        Start  27: system:config-dump
  8/101 Test  #27: system:config-dump ..............................   Passed   16.99 sec
        Start  28: system:config-syntax-crash
  9/101 Test  #28: system:config-syntax-crash ......................   Passed    9.58 sec
        Start  29: system:copy-archive-job
 10/101 Test  #29: system:copy-archive-job .........................   Passed   27.80 sec
        Start  30: system:copy-bscan
 11/101 Test  #30: system:copy-bscan ...............................   Passed   30.16 sec
        Start  31: system:copy-remote-bscan
 12/101 Test  #31: system:copy-remote-bscan ........................   Passed   28.49 sec
        Start  33: system:deprecation
 13/101 Test  #33: system:deprecation ..............................   Passed    9.55 sec
        Start  35: system:encrypt-signature
 14/101 Test  #35: system:encrypt-signature ........................   Passed   23.68 sec
        Start  36: system:encrypt-signature-tls-cert
 15/101 Test  #36: system:encrypt-signature-tls-cert ...............   Passed   26.93 sec
        Start  37: system:fileset-multiple-blocks:setup
 16/101 Test  #37: system:fileset-multiple-blocks:setup ............   Passed   11.24 sec
        Start  38: system:fileset-multiple-blocks:include-blocks
 17/101 Test  #38: system:fileset-multiple-blocks:include-blocks ...   Passed    9.74 sec
        Start  39: system:fileset-multiple-blocks:options-blocks
 18/101 Test  #39: system:fileset-multiple-blocks:options-blocks ...   Passed    9.24 sec
        Start  40: system:fileset-multiple-blocks:cleanup
 19/101 Test  #40: system:fileset-multiple-blocks:cleanup ..........   Passed    3.47 sec
        Start  41: system:filesets
 20/101 Test  #41: system:filesets .................................   Passed   14.81 sec
        Start  44: system:list-backups
 21/101 Test  #44: system:list-backups .............................   Passed   13.51 sec
        Start  46: system:multiplied-device
 22/101 Test  #46: system:multiplied-device ........................   Passed   32.05 sec
        Start  48: system:notls
 23/101 Test  #48: system:notls ....................................   Passed   31.03 sec
        Start  49: system:passive
 24/101 Test  #49: system:passive ..................................   Passed   24.74 sec
        Start  50: system:py2plug-dir
 25/101 Test  #50: system:py2plug-dir ..............................   Passed   18.10 sec
        Start  55: system:py2plug-fd-local-fileset
 26/101 Test  #55: system:py2plug-fd-local-fileset .................   Passed   27.49 sec
        Start  57: system:py2plug-fd-local-fileset-restoreobject
 27/101 Test  #57: system:py2plug-fd-local-fileset-restoreobject ...   Passed   26.85 sec
        Start  66: system:py2plug-sd
 28/101 Test  #66: system:py2plug-sd ...............................   Passed   13.07 sec
        Start  68: system:python-bareos:setup
 29/101 Test  #68: system:python-bareos:setup ......................   Passed   11.72 sec
        Start  69: system:python-bareos:acl
 30/101 Test  #69: system:python-bareos:acl ........................   Passed   26.38 sec
        Start  70: system:python-bareos:bvfs
 31/101 Test  #70: system:python-bareos:bvfs .......................   Passed   19.69 sec
        Start  71: system:python-bareos:delete
 32/101 Test  #71: system:python-bareos:delete .....................   Passed   10.81 sec
        Start  72: system:python-bareos:filedaemon
 33/101 Test  #72: system:python-bareos:filedaemon .................   Passed    0.28 sec
        Start  73: system:python-bareos:json
 34/101 Test  #73: system:python-bareos:json .......................   Passed    0.25 sec
        Start  74: system:python-bareos:json_config
 35/101 Test  #74: system:python-bareos:json_config ................   Passed    0.60 sec
        Start  75: system:python-bareos:plaintext
 36/101 Test  #75: system:python-bareos:plaintext ..................   Passed    0.22 sec
        Start  76: system:python-bareos:protocol_124
 37/101 Test  #76: system:python-bareos:protocol_124 ...............   Passed    0.29 sec
        Start  77: system:python-bareos:python_bareos_module
 38/101 Test  #77: system:python-bareos:python_bareos_module .......   Passed    0.26 sec
        Start  78: system:python-bareos:runscript
 39/101 Test  #78: system:python-bareos:runscript ..................   Passed    9.23 sec
        Start  79: system:python-bareos:show
 40/101 Test  #79: system:python-bareos:show .......................   Passed    4.17 sec
        Start  80: system:python-bareos:tlspsk
 41/101 Test  #80: system:python-bareos:tlspsk .....................   Passed    0.31 sec
        Start  81: system:python-bareos:cleanup
 42/101 Test  #81: system:python-bareos:cleanup ....................   Passed    3.22 sec
        Start  83: system:quota-softquota
 43/101 Test  #83: system:quota-softquota ..........................   Passed   32.15 sec
        Start  84: system:reload:setup
 44/101 Test  #84: system:reload:setup .............................   Passed    6.15 sec
        Start  85: system:reload:add-client
 45/101 Test  #85: system:reload:add-client ........................   Passed    5.32 sec
        Start  86: system:reload:add-duplicate-job
 46/101 Test  #86: system:reload:add-duplicate-job .................   Passed    0.14 sec
        Start  87: system:reload:add-empty-job
 47/101 Test  #87: system:reload:add-empty-job .....................   Passed    0.15 sec
        Start  88: system:reload:add-second-director
 48/101 Test  #88: system:reload:add-second-director ...............   Passed    0.12 sec
        Start  89: system:reload:add-uncommented-string
 49/101 Test  #89: system:reload:add-uncommented-string ............   Passed    0.12 sec
        Start  90: system:reload:unchanged-config
 50/101 Test  #90: system:reload:unchanged-config ..................   Passed    0.24 sec
        Start  91: system:reload:cleanup
 51/101 Test  #91: system:reload:cleanup ...........................   Passed    1.75 sec
        Start  93: system:scheduler-backup
 52/101 Test  #93: system:scheduler-backup .........................   Passed   17.37 sec
        Start  94: system:sparse-file
 53/101 Test  #94: system:sparse-file ..............................   Passed   25.48 sec
        Start  95: system:spool
 54/101 Test  #95: system:spool ....................................   Passed   25.16 sec
        Start  96: system:truncate-command
 55/101 Test  #96: system:truncate-command .........................   Passed   13.13 sec
        Start  97: system:upgrade-database
 56/101 Test  #97: system:upgrade-database .........................   Passed    8.34 sec
        Start  98: system:virtualfull
 57/101 Test  #98: system:virtualfull ..............................   Passed   31.78 sec
        Start  99: system:virtualfull-bscan
 58/101 Test  #99: system:virtualfull-bscan ........................   Passed   33.10 sec
        Start 100: system:volume-pruning
 59/101 Test #100: system:volume-pruning ...........................   Passed   47.73 sec
        Start 101: system:xattr
 60/101 Test #101: system:xattr ....................................   Passed   25.31 sec
        Start   1: bareosdir-python2-module
 61/101 Test   #1: bareosdir-python2-module ........................   Passed    0.03 sec
        Start   2: bareossd-python2-module
 62/101 Test   #2: bareossd-python2-module .........................   Passed    0.11 sec
        Start   3: bareosfd-python2-module-tester
 63/101 Test   #3: bareosfd-python2-module-tester ..................   Passed    0.02 sec
        Start   4: bareosfd-python2-module
 64/101 Test   #4: bareosfd-python2-module .........................   Passed    0.03 sec
        Start   5: webui:admin-client_disabling
 65/101 Test   #5: webui:admin-client_disabling ....................***Not Run (Disabled)   0.00 sec
        Start   6: webui:admin-rerun_job
 66/101 Test   #6: webui:admin-rerun_job ...........................***Not Run (Disabled)   0.00 sec
        Start   7: webui:admin-restore
 67/101 Test   #7: webui:admin-restore .............................***Not Run (Disabled)   0.00 sec
        Start   8: webui:admin-run_configured_job
 68/101 Test   #8: webui:admin-run_configured_job ..................***Not Run (Disabled)   0.00 sec
        Start   9: webui:admin-run_default_job
 69/101 Test   #9: webui:admin-run_default_job .....................***Not Run (Disabled)   0.00 sec
        Start  10: webui:readonly-client_disabling
 70/101 Test  #10: webui:readonly-client_disabling .................***Not Run (Disabled)   0.00 sec
        Start  11: webui:readonly-rerun_job
 71/101 Test  #11: webui:readonly-rerun_job ........................***Not Run (Disabled)   0.00 sec
        Start  12: webui:readonly-restore
 72/101 Test  #12: webui:readonly-restore ..........................***Not Run (Disabled)   0.00 sec
        Start  13: webui:readonly-run_configured_job
 73/101 Test  #13: webui:readonly-run_configured_job ...............***Not Run (Disabled)   0.00 sec
        Start  14: webui:readonly-run_default_job
 74/101 Test  #14: webui:readonly-run_default_job ..................***Not Run (Disabled)   0.00 sec
        Start  17: system:autochanger
 75/101 Test  #17: system:autochanger ..............................***Not Run (Disabled)   0.00 sec
        Start  20: system:bconsole-pam
 76/101 Test  #20: system:bconsole-pam .............................***Not Run (Disabled)   0.00 sec
        Start  22: system:block-size
 77/101 Test  #22: system:block-size ...............................***Not Run (Disabled)   0.00 sec
        Start  24: system:catalog
 78/101 Test  #24: system:catalog ..................................***Not Run (Disabled)   0.00 sec
        Start  25: system:chflags
 79/101 Test  #25: system:chflags ..................................***Not Run (Disabled)   0.00 sec
        Start  32: system:dbcopy-mysql-postgresql
 80/101 Test  #32: system:dbcopy-mysql-postgresql ..................***Not Run (Disabled)   0.00 sec
        Start  34: system:droplet-s3
 81/101 Test  #34: system:droplet-s3 ...............................***Not Run (Disabled)   0.00 sec
        Start  42: system:gfapi-fd
 82/101 Test  #42: system:gfapi-fd .................................***Not Run (Disabled)   0.00 sec
        Start  43: system:glusterfs-backend
 83/101 Test  #43: system:glusterfs-backend ........................***Not Run (Disabled)   0.00 sec
        Start  45: system:messages
 84/101 Test  #45: system:messages .................................***Not Run (Disabled)   0.00 sec
        Start  47: system:ndmp
 85/101 Test  #47: system:ndmp .....................................***Not Run (Disabled)   0.00 sec
        Start  51: system:py3plug-dir
 86/101 Test  #51: system:py3plug-dir ..............................***Not Run (Disabled)   0.00 sec
        Start  52: system:py2plug-fd-ldap
 87/101 Test  #52: system:py2plug-fd-ldap ..........................***Not Run (Disabled)   0.00 sec
        Start  53: system:py2plug-fd-libcloud
 88/101 Test  #53: system:py2plug-fd-libcloud ......................***Not Run (Disabled)   0.00 sec
        Start  54: system:py3plug-fd-libcloud
 89/101 Test  #54: system:py3plug-fd-libcloud ......................***Not Run (Disabled)   0.00 sec
        Start  56: system:py3plug-fd-local-fileset
 90/101 Test  #56: system:py3plug-fd-local-fileset .................***Not Run (Disabled)   0.00 sec
        Start  58: system:py3plug-fd-local-fileset-restoreobject
 91/101 Test  #58: system:py3plug-fd-local-fileset-restoreobject ...***Not Run (Disabled)   0.00 sec
        Start  59: system:py2plug-fd-ovirt
 92/101 Test  #59: system:py2plug-fd-ovirt .........................***Not Run (Disabled)   0.00 sec
        Start  60: system:py3plug-fd-ovirt
 93/101 Test  #60: system:py3plug-fd-ovirt .........................***Not Run (Disabled)   0.00 sec
        Start  61: system:py2plug-fd-percona-xtrabackup
 94/101 Test  #61: system:py2plug-fd-percona-xtrabackup ............***Not Run (Disabled)   0.00 sec
        Start  62: system:py3plug-fd-percona-xtrabackup
 95/101 Test  #62: system:py3plug-fd-percona-xtrabackup ............***Not Run (Disabled)   0.00 sec
        Start  63: system:py2plug-fd-postgres
 96/101 Test  #63: system:py2plug-fd-postgres ......................***Not Run (Disabled)   0.00 sec
        Start  64: system:py2plug-fd-vmware
 97/101 Test  #64: system:py2plug-fd-vmware ........................***Not Run (Disabled)   0.00 sec
        Start  65: system:py3plug-fd-vmware
 98/101 Test  #65: system:py3plug-fd-vmware ........................***Not Run (Disabled)   0.00 sec
        Start  67: system:py3plug-sd
 99/101 Test  #67: system:py3plug-sd ...............................***Not Run (Disabled)   0.00 sec
        Start  82: system:python-pam
100/101 Test  #82: system:python-pam ...............................***Not Run (Disabled)   0.00 sec
        Start  92: system:restapi
101/101 Test  #92: system:restapi ..................................***Not Run (Disabled)   0.00 sec

100% tests passed, 0 tests failed out of 64

Label Time Summary:
broken    =   0.03 sec*proc (1 test)

Total Test time (real) = 934.60 sec

The following tests did not run:
	  5 - webui:admin-client_disabling (Disabled)
	  6 - webui:admin-rerun_job (Disabled)
	  7 - webui:admin-restore (Disabled)
	  8 - webui:admin-run_configured_job (Disabled)
	  9 - webui:admin-run_default_job (Disabled)
	 10 - webui:readonly-client_disabling (Disabled)
	 11 - webui:readonly-rerun_job (Disabled)
	 12 - webui:readonly-restore (Disabled)
	 13 - webui:readonly-run_configured_job (Disabled)
	 14 - webui:readonly-run_default_job (Disabled)
	 17 - system:autochanger (Disabled)
	 20 - system:bconsole-pam (Disabled)
	 22 - system:block-size (Disabled)
	 24 - system:catalog (Disabled)
	 25 - system:chflags (Disabled)
	 32 - system:dbcopy-mysql-postgresql (Disabled)
	 34 - system:droplet-s3 (Disabled)
	 42 - system:gfapi-fd (Disabled)
	 43 - system:glusterfs-backend (Disabled)
	 45 - system:messages (Disabled)
	 47 - system:ndmp (Disabled)
	 51 - system:py3plug-dir (Disabled)
	 52 - system:py2plug-fd-ldap (Disabled)
	 53 - system:py2plug-fd-libcloud (Disabled)
	 54 - system:py3plug-fd-libcloud (Disabled)
	 56 - system:py3plug-fd-local-fileset (Disabled)
	 58 - system:py3plug-fd-local-fileset-restoreobject (Disabled)
	 59 - system:py2plug-fd-ovirt (Disabled)
	 60 - system:py3plug-fd-ovirt (Disabled)
	 61 - system:py2plug-fd-percona-xtrabackup (Disabled)
	 62 - system:py3plug-fd-percona-xtrabackup (Disabled)
	 63 - system:py2plug-fd-postgres (Disabled)
	 64 - system:py2plug-fd-vmware (Disabled)
	 65 - system:py3plug-fd-vmware (Disabled)
	 67 - system:py3plug-sd (Disabled)
	 82 - system:python-pam (Disabled)
	 92 - system:restapi (Disabled)
```

### Extras 
The bconsole command offers a setdebug command. When specifying a the full command like setdebug level=100 dir trace=1 it works as expected and a trace will be be created.

- bareos offers two programs : bareos and bconsole

- debug = console write text, this text can be specified by 
- levels = WARN, ERROR, TRACE, ...
if we specified a level we can get text at this level