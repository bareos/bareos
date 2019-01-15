Seldom Used Text Formats
########################

Bnfvar Command
--------------

A bnfvar command should be formatted as:

.. \newcommand{\bnfvar}[1]{\lt#1\gt}

   Used only in one tex file. So, it can be ignored.


CommandPrompt
-------------

If you want to display the linux command prompt, the following formatting should be used:

.. \newcommand{\commandPrompt}{root{\at}linux:\~{}\# }

.. code-block:: sh

   root@linux:~#


Configresource
--------------

If you want to display a configured resource name, the following formatting should be used:

.. \newcommand{\configresource}[1]{\path|#1|}

   Post Conversion Changes:
   ${PERL} 's#:raw-latex:`\\configresource\{(.*?)\}`#:strong:`\1`#g'  ${DESTFILE}

   Only used 4 times. So, it can be ignored.

.. code-block:: sh

   :strong:`Media`


Dbcolumn
--------

If you want to display a database column name, this formatting should be used.

.. \newcommand{\dbcolumn}[1]{\path|#1|}

   Only used 2 times. So, it can be ignored.


Group
-----

If you want to display the name of a user group, this formatting should be used.

.. \newcommand{\group}[1]{\path|#1|}

   Only used 3 times. So, it can be ignored.


Sql Command
-----------

If you want to display a sql command, this formatting should be used.

.. \newcommand{\sqlcommand}[1]{\path|#1|}

   Only used in 1 file. So, it can be ignored.


Volume
------

If you want to display a volumne name, this formatting should be used.


Email
-----

If you want to display an email address, this formatting should be used:

.. \newcommand{\email}[1]{\url{#1}}


Paragraph
---------

If you want to write a paragraph heading, this formatting should be used:

.. \newcommand{\subsubsubsection}[1]{\paragraph{#1}}


Storage Backend
---------------

If you want to display the name of a storage backend, the following formatting should be used:

.. \newcommand{\sdBackend}[2]{%
   \ifthenelse{\isempty{#2}}{%
    \path|#1|%
    \index[general]{#1}%
    \index[sd]{Backend!#1}%
   }{%
    \path|#1| (#2)%
    \index[general]{#1 (#2)}%
    \index[general]{#2!#1}%
    \index[sd]{Backend!#1 (#2)}%
   }%
   }

   Post Conversion Changes
