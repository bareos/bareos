python-bareos
=============

Python libraries to access Bareos


preperation
---------------

.. code:: python

  export PYTHONPATH=.
  python

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

.. code:: python

  import bareos.bsock
  password=bareos.bsock.Password("secret")
  bconsole=bareos.bsock.BSockJson(address="localhost", port=9001, password=password)
  bconsole.call("list pools")
  ...

