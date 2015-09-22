python-bareos
=============

Python module to access a http://www.bareos.org backup system.

  * prebuild packages available at http://download.bareos.org/bareos/contrib/


calling bareos-director user agent commands
-----------------------------------------------

.. code:: python

  import bareos.bsock

  password=bareos.bsock.Password("secret")
  bsock=bareos.bsock.BSock(address="localhost", port=9001, password=password)
  print bsock.call("help")
  ...


simple version of the bconsole in Python
--------------------------------------------

.. code:: python

  import bareos.bsock
  password=bareos.bsock.Password("secret")
  bconsole=bareos.BSock(address="localhost", port=9001, password=password)
  bconsole.interactive()
  ...

use JSON objects of API mode 2
----------------------------------

Requires: bareos >= 15.2

.. code:: python

  import bareos.bsock
  password=bareos.bsock.Password("secret")
  bconsole=bareos.bsock.BSockJson(address="localhost", port=9001, password=password)
  bconsole.call("list pools")
  ...

