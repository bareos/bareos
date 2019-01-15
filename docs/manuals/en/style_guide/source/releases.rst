Releases
########

Release Notes - Stable Version
------------------------------

If you want to display the release notes of a bareos stable release, the following formatting should be used:

.. \newcommand{\releasenote}[2]{
    \subsection*{bareos-#1}
    \addcontentsline{toc}{subsection}{\protect\numberline{}#1}%
    \label{bareos-#1}
    \index[general]{bareos-#1!Release Notes}%
    #2
   }

.. code-block:: sh

   **bareos-15.2.3**

         +----------------------+-------------------------------------------------------------+
         | Code Release         |                     2016-03-11                              |
         +----------------------+-------------------------------------------------------------+
         | Database Version     |                     2004 (unchanged)                        |
         +----------------------+-------------------------------------------------------------+
         | Release Ticket       |                     :issue:`625`                            |
         +----------------------+-------------------------------------------------------------+
         | Download URL         |     https://download.bareos.com/bareos/release/15.2/        |
         +----------------------+-------------------------------------------------------------+

         This release contains several bugﬁxes and enhancements. Excerpt:

            * VMWare plugin can now restore to VMDK ﬁle
            * Ceph support for SLES12 included
            * Multiple gfapi and ceph enhancements
            * NDMP enhancements and bugﬁxes
            * Windows: multiple VSS Jobs can now run concurrently in one FD, installer ﬁxes
            * bpipe: ﬁx stderr/stdout problems
            * reload command enhancements (limitations eliminated)
            * label barcodes now can run without interaction

The output should look like this:

**bareos-15.2.3**

         +----------------------+-------------------------------------------------------------+
         | Code Release         |                     2016-03-11                              |
         +----------------------+-------------------------------------------------------------+
         | Database Version     |                     2004 (unchanged)                        |
         +----------------------+-------------------------------------------------------------+
         | Release Ticket       |                     :issue:`625`                            |
         +----------------------+-------------------------------------------------------------+
         | Download URL         |     https://download.bareos.com/bareos/release/15.2/        |
         +----------------------+-------------------------------------------------------------+

         This release contains several bugﬁxes and enhancements. Excerpt:

            * VMWare plugin can now restore to VMDK ﬁle
            * Ceph support for SLES12 included
            * Multiple gfapi and ceph enhancements
            * NDMP enhancements and bugﬁxes
            * Windows: multiple VSS Jobs can now run concurrently in one FD, installer ﬁxes
            * bpipe: ﬁx stderr/stdout problems
            * reload command enhancements (limitations eliminated)
            * label barcodes now can run without interaction


Release Notes - Unstable Version
--------------------------------

If you want to display the release notes of a bareos unstable release, the following formatting should be used:

.. \newcommand{\releasenoteUnstable}[2]{
    \subsection*{\textit{bareos-#1 (unstable)}}
    %\addcontentsline{toc}{subsection}{\protect\numberline{}#1}%
    %\index[general]{bareos-#1!Release Notes}%
    #2
   }

* Heading: ``bareos-<release_version> (unstable)``
* Mandatory information to be provided in release notes:
   1. Code Release
   2. Database Version
   3. Release Ticket
   4. Download URL
   5. Beta Release


Ticket
------

If you want to display a ticket number, the following formatting should be used:

.. \newcommand{\ticket}[1]{%
    \href{https://bugs.bareos.org/view.php?id=#1}{Ticket \##1}%
   }

.. code-block:: sh

   :issue:`845`

The output should look like this:

:issue:`845`


Release URL - Bareos.org
------------------------

If you want to display the release download URL from the Bareos.org site, the following formatting should be used:

.. \newcommand{\releaseUrlDownloadBareosOrg}[1]{%
   \url{http://download.bareos.org/bareos/release/#1/}%
   }

   Post Conversion Changes
   ${PERL} 's#\\releaseUrlDownloadBareosOrg\{(.*?)\}#//http://download.bareos.org/bareos/release/\1/#g' ${DESTFILE}

.. code-block:: sh

   http://download.bareos.org/bareos/release/18.2/


Release URL - Bareos.com
------------------------

If you want to display the release download URL from the Bareos.com site, the following formatting should be used:

.. \newcommand{\releaseUrlDownloadBareosOrg}[1]{%
   \url{http://download.bareos.org/bareos/release/#1/}%
   }

   Post Conversion Changes
   ${PERL} 's#\\releaseUrlDownloadBareosCom\{(.*?)\}#//https://download.bareos.com/bareos/release/\1/#g' ${DESTFILE}

.. code-block:: sh

   https://download.bareos.com/bareos/release/18.2/


Plugin Download URL
-------------------

If you want to display the download URL of plugins available on Bareos.org, the following formatting should be used:

.. \newcommand{\contribDownloadBareosOrg}{%
   \url{http://download.bareos.org/bareos/contrib/}%
   }

   Post Conversion Changes
   ${PERL} 's#\\contribDownloadBareosOrg\{(.*?)\}#//http://download.bareos.org/bareos/contrib/#g' ${DESTFILE}

.. code-block:: sh

   http://download.bareos.org/bareos/contrib/
