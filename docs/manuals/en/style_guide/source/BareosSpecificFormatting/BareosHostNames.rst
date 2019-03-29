Bareos Host Names
=================

All host names in example should use the :strong:`example.com` domain.

Also in all examples, the same names for the |bareosDir|, Bareos Storage and Bareos File Daemons should be used.


If you want to display a hostname, the following formatting should be followed:

.. \newcommand{\host}[1]{\path|#1|}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\host\{(.*?)\}`#:strong:`\1`#g' ${DESTFILE}

.. code-block:: sh

   :strong:`client1.example.com`

The output should look like this:

:strong:`client1.example.com`


.. csv-table:: Host Names
   :header: "Host name", "Description"

   ":strong:`bareos-dir.example.com`",     "Bareos Director host"
   ":strong:`bareos-sd.example.com`",      "Bareos Storage Daemon host, if only one Storage Daemon is used."
   ":strong:`bareos-sd1.example.com`, :strong:`bareos-sd2.example.com`, ...", "Bareos Storage Daemon host, if multiple Storage Daemons are used."
   ":strong:`bareos-sd-tape.example.com`", "Bareos Storage Daemon with a specific backend."
   ":strong:`client1.example.com`, :strong:`client2.example.com`, ...", "Bareos File Daemon"
