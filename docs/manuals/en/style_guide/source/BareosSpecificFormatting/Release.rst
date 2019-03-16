Releases
########

Release Notes - Stable Version
------------------------------

If you want to display the release notes of a Bareos stable release, use the following template:

.. \newcommand{\releasenote}[2]{
    \subsection*{bareos-#1}
    \addcontentsline{toc}{subsection}{\protect\numberline{}#1}%
    \label{bareos-#1}
    \index[general]{bareos-#1!Release Notes}%
    #2
   }

.. literalinclude:: /example/releasenotes-template.rst.inc
   
Example:

.. literalinclude:: /example/releasenotes.rst.inc


Output:

.. include:: /example/releasenotes.rst.inc



Reference to a Release
----------------------

To refer to a release, use::

    :ref:`bareos-<VERSION>`

Example:

  This refers to :ref:`bareos-17.2.7`\ .
  
Output:

This refers to :ref:`bareos-17.2.7`\ .


Release Notes - Unstable Version
--------------------------------

TODO: required?

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



URLs
####

Release URL - Bareos.org
------------------------

If you want to display the release download URL from the Bareos.org site, the following formatting should be used:

.. \newcommand{\releaseUrlDownloadBareosOrg}[1]{%
   \url{http://download.bareos.org/bareos/release/#1/}%
   }

   Post Conversion Changes
   ${PERL} 's#\\releaseUrlDownloadBareosOrg\{(.*?)\}#//http://download.bareos.org/bareos/release/\1/#g' ${DESTFILE}

http://download.bareos.org/bareos/release/18.2/


Release URL - Bareos.com
------------------------

If you want to display the release download URL from the Bareos.com site, the following formatting should be used:

.. \newcommand{\releaseUrlDownloadBareosOrg}[1]{%
   \url{http://download.bareos.org/bareos/release/#1/}%
   }

   Post Conversion Changes
   ${PERL} 's#\\releaseUrlDownloadBareosCom\{(.*?)\}#//https://download.bareos.com/bareos/release/\1/#g' ${DESTFILE}


https://download.bareos.com/bareos/release/18.2/


Contributions Download URL
--------------------------

If you want to display the download URL of plugins available on Bareos.org, the following formatting should be used:

.. \newcommand{\contribDownloadBareosOrg}{%
   \url{http://download.bareos.org/bareos/contrib/}%
   }

   Post Conversion Changes
   ${PERL} 's#\\contribDownloadBareosOrg\{(.*?)\}#//http://download.bareos.org/bareos/contrib/#g' ${DESTFILE}

http://download.bareos.org/bareos/contrib/
