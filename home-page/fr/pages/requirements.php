<? require_once("inc/header.php"); ?>
<table>
<tr>
        <td class="contentTopic">
                System Requirements
        </td>
</tr>
<tr>
   <td class="content">

   <ul class="hardware">
      <li><b>Bacula</b> has been compiled and run on Linux RedHat, FreeBSD,
              and Solaris systems.</li>
      <li>It requires GNU C++ version 2.95 or higher to compile.  You can try
              with other compilers and older versions, but you are on your
              own.  We have successfully compiled and used Bacula on
              RH8.0/RH9/RHEL 3.0 with GCC 3.2.  Note, in general GNU C++ is a
              separate package (e.g.  RPM) from GNU C, so you need them both
              loaded.  On RedHat systems, the C++ compiler is part of the
              <b>gcc-c++</b> rpm package.  </li>

      <li>There are certain third party packages that Bacula needs.
              Except for MySQL and PostgreSQL, they can all be found in the
              <b>depkgs</b> and <b>depkgs1</b> releases.</li>
      <li>If you want to build the Win32 binaries, you will need a
              Microsoft Visual C++ compiler (or Visual Studio).
              Although all components build (console has
              some warnings), only the File daemon has been tested. </li>
      <li><b>Bacula</b> requires a good implementation of pthreads to work.
              This is not the case on some of the BSD systems.</li>
      <li>The source code has been written with portability in mind and is
              mostly POSIX compatible. Thus porting to any POSIX compatible
              operating system should be relatively easy.</li>
      <li>The GNOME Console program is developed and tested under GNOME 2.x.
              It also runs under GNOME 1.4 but this version is deprecated and
              thus no longer maintained.</li>

      <li>The wxWidgets Console program is developed and tested with the
              latest stable version of <a
              href="http://www.wxwidgets.org/">wxWidgets</a> (2.6).  It
              works fine with the Windows and GTK+-1.x version of wxWidgets,
              and should also works on other platforms supported by
              wxWidgets.</li>
      <li>The Tray Monitor program is developed for GTK+-2.x. It needs
              Gnome &gt;=2.2, KDE &gt;=3.1 or any window manager supporting the
              <a href="http://www.freedesktop.org/Standards/systemtray-spec">
              FreeDesktop system tray standard</a>.</li>
      <li>If you want to enable command line editing and history, you will
              need to have /usr/include/termcap.h and either the termcap or the
              ncurses library loaded (libtermcap-devel or ncurses-devel).</li>
   </ul>

   </td>
</tr>
</table>
<? require_once("inc/footer.php"); ?>
