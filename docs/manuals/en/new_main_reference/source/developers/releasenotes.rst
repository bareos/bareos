Bareos 18.2.4rc2 Release Notes
============================

General Release Information
---------------------------

.. list-table:: Bareos 18.2.4rc2 Release Information
   :header-rows: 0

   * - **Release Date**
     - December 2018
   * - **Database Version**
     -  2171
   * - **Release Ticket**
     -
   * - **URL**
     - http://download.bareos.org/release/18.2/

..
   * - **LOC**
     - 123456+ 12345-

.. csv-table:: binary package availablility
   :header: "Distribution", "Architecture"
   :widths: auto

   CentOS_6, "x86_64"
   CentOS_7, "x86_64"
   Debian_8.0, "i586,x86_64"
   Debian_9.0, "i586,x86_64"
   Fedora_28, "x86_64"
   Fedora_29, "x86_64"
   openSUSE_Leap_15.0, "x86_64"
   RHEL_6, "x86_64"
   RHEL_7, "x86_64"
   SLE_12_SP3, "x86_64"
   SLE_12_SP4, "x86_64"
   SLE_15, "x86_64"
   Univention_4.3, "x86_64"
   win_cross, "x86_64"
   xUbuntu_14.04, "i586,x86_64"
   xUbuntu_16.04, "i586,x86_64"
   xUbuntu_18.04, "x86_64"

New Features
------------

* New network Protocol using immediately TLS

  * TLS is immediately used on all network connections
  * Support for TLS-PSK in all daemons
  * Automatic encryption of all network traffic with TLS-PSK
  * Full Compatibility with old |bareosFd|

    * Old |bareosFd| speaking the old protocol are automatically detected
      and daemons switch to the old protocol

  * Easy update without configuration changes

    * Existing Bareos installations can be upgraded without configuration changes

* PAM Support

  * Detailed information follows
  * Introduction of new "User" Resource
  * The |bareosDir| supports PAM for user authentication
  * The Bareos WebUI supports PAM user authentication against the |bareosDir|

Changed Features
----------------
* Bandwidth limiting now also works in TLS encrypted connections. Before, bandwidth limiting
  was ignored when the connections were TLS encrypted

* droplet (S3): multiple enhancements

* bconsole: added "whoami" command to show currently associated user

* xattr and acl support now are enabled by default

  * both features were disabled by default and needed to be enabled in the fileset options
  * now both are enabled by default and can be disabled in the fileset options
  * new |bareosFd| logs the current status of both options in job log

Backward compatibiliy
---------------------
  * |bareosDir| >= 18.2 can work with all |bareosFd| versions. However, all other components need to be updated to Bareos version >= 18.2

..  * |bconsole| < 18.2 can be used with minor drawbacks (no PAM authentication, no TLS-PSK)

  * To maintain |bareosWebui| access to the |bareosDir|, it depends on the current configuration. 1. TLS certificates: Nothing to do. 2. No TLS configured: Set TlsEnable=false in the respective console config of the |bareosWebui| in the |bareosDir|
  * The local |bareosFd| on the |bareosDir| will be renamed (also his jobs?). What to do to make it work again?

Deprecated and Removed Features
-------------------------------
* removed Bareos conio option, as the standard library readline is used instead

Bugs Fixed
----------
* #805 (bugs.bareos.org): can't restore vmware-plugin assisted backups via |bareosWebui|
* Windows Installer: Fixed infinite install dialog for VC 2012 checks on x86 windows
* Fixed memory leaks in the |bareosDir| when using bconsole or |bareosWebui|
* Fixed a bug in the debug message handler on the |bareosDir| when debuglevel is >= 900
* Improved shutdown of |bareosDir|


Internal Project Changes
------------------------
* reorganized the whole git repository and merged sub repositories into main repository
* changed the build system from autoconf/automake to cmake
* switched from cmocka to google test framework for unit tests
* introduced namespaces to avoid name clashes when parts of different daemons are tested in one test
* switched to use c++11 standard, start to refactor using standard library instead of legacy features
* use google c++ style guide

  * refactored variable names

* refactored configuration parser
* tls implementation has now a base class interface instead of compile time switched behaviour
* library cleanup and reorganization

  * library does not use main program variables anymore
  * removed libbareoscfg
  * enhanced windows cross building

* renamed c++ files to use .cc file extension
* cleanup of header files

  * removed "protos.h"
  * introduced individual header file for each c++ file
  * each header file has own google c++ standard header guard
  * explicitly declare functions override where applicable

* |bareosTraymonitor|: allows compiling against to qt4 and qt5
* switch the documentation from LaTeX to Sphinx (work in progress)
* |bareosWebui|: enhances Selenium tests to be used on https://saucelabs.com/u/bareossaucelabs
* clang: massively reduced number of warnings
* FreeBSD: added start scripts, fixed buggy cmake detection of ACL support
* regression tests

  * automatically build |bareosTraymonitor|
  * preconfigure |bareosWebui| to run in php's own webserver for easy testing

