Build the documentation
=======================

This following shell script will show how to build the **Bareos documentation** from source.

.. code-block:: bash
  :caption: Example shell script

  #!/bin/sh

  mkdir bareos-local-tests
  cd bareos-local-tests
  git clone https://github.com/bareos/bareos.git

  mkdir build-docs
  cd build-docs

  cmake -Ddocs-only=yes ../bareos
  make
