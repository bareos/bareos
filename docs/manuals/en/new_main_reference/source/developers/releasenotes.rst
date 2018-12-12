Bareos 18.2.4rc2 Release Notes
============================

General Release Information
---------------------------

.. list-table:: Bareos 18.2.4rc2 Release Information
   :header-rows: 0

   * - **Release Date**
     - 12 December 2018
   * - **Database Version**
     -  2171
   * - **Release Ticket**
     - #123
   * - **URL**
     - http://

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
  * Compatibility with old clients
  * Full compatibility with old clients

    * Old clients speaking the old protocol are automatically detected
      and daemons switch to the old protocol

  * Easy update without configuration changes

    * Existing Bareos installations can be upgraded without configuration changes

* PAM Support

  * Introduction of new "User" Resource
  * The Bareos Director supports PAM for user authentication
  * The Bareos WebUI supports PAM user authentication against the Bareos Director

Changed Features
----------------
* Bandwidth limiting now also works in TLS encrypted connections. Before, bandwidth limiting
  was ignored when the connections were TLS encrypted

* droplet: multiple enhancements

* bconsole: added "whoami" command to show currently associated user

* xattr and acl support now are enabled by default

  * both features were disabled by default and needed to be enabled in the fileset options
  * now both are enabled by default and can be disabled in the fileset options
  * new filedaemon logs the current status of both options in job log



Deprecated and Removed Features
-------------------------------

* removed bareos conio and console command line argument -n


Bugs Fixed
----------
* #805: can't restore vmware-plugin assisted backups via webui
* Windows Installer: Fixed infinite install dialog for VC 2012 checks on x86 windows
* Fixed memory leaks in the director when using bconsole or webui
* Fixed a bug in the debug message handler on the director when debuglevel is >= 900


Internal Project Changes
------------------------
* reorganized the whole git repository and merged sub repositories into main repository
* changed the build system from autoconf/automake to cmake
* switched from cmocka to google test framework for unit tests
* introduced first tests based on google test
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

* tray monitor: switched to qt5
* tray monitor: enable testing in binary and created regression test
* switch the documentation from LaTeX to Sphinx (work in progress)
* webui: enabled automatic tests in saucelabs
* clang: massively reduced number of warnings
* FreeBSD: added start scripts, fixed buggy cmake detection of ACL support
* regression tests

  * automatically build traymonitor
  * preconfigure webui to run in php's own webserver for easy testing

