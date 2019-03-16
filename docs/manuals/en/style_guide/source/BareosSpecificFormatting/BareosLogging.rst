Bareos Logging
==============

If you want to display Bareos specific logs, use following formatting:

.. ${PERL} 's#\{logging\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}

.. literalinclude:: /example/code-block-bareoslog.rst.inc

The output will look like this:

.. include:: /example/code-block-bareoslog.rst.inc
