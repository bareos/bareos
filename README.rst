python-bareos
=============

Python module to access a http://www.bareos.org backup system.

* prebuild packages available at http://download.bareos.org/bareos/contrib/


calling bareos-director user agent commands
-----------------------------------------------

.. code:: python

  import bareos.bsock

  password=bareos.bsock.Password("secret")
  directorconsole=bareos.bsock.DirectorConsole(address="localhost", port=9101, password=password)
  print directorconsole.call("help")
  ...


simple version of the bconsole in Python
--------------------------------------------

.. code:: python

  import bareos.bsock
  password=bareos.bsock.Password("secret")
  directorconsole=bareos.bsock.DirectorConsole(address="localhost", port=9101, password=password)
  directorconsole.interactive()
  ...

use JSON objects of API mode 2
----------------------------------

Requires: bareos >= 15.2

.. code:: python

  import bareos.bsock
  password=bareos.bsock.Password("secret")
  directorconsole=bareos.bsock.DirectorConsoleJson(address="localhost", port=9101, password=password)
  directorconsole.call("list pools")
  ...

