python-bareos
=============

Python module to access a http://www.bareos.org backup system.

`python-bareos` packages are included in the Bareos core distribution since Bareos >= 17.2.

TLS-PSK
-------

Since Bareos >= 18.2.4 can secure its connections via TLS-PSK (Transport-Layer-Security Pre-Shared-Key).

This subset of TLS is currently not supported by the Python SSL module.
To enable this feature in `python-bareos` the Python module 'sslpsk` must be installed:

.. code::

  pip install sslpsk





Calling bareos-director user agent commands
-------------------------------------------

.. code:: python

   import bareos.bsock

   password=bareos.bsock.Password('secret')
   directorconsole=bareos.bsock.DirectorConsole(address='localhost', port=9101, password=password)
   print directorconsole.call('help')
   ...

To connected to a named console instead, use the `name` parameter:

.. code:: python

   password=bareos.bsock.Password('secret')
   directorconsole=bareos.bsock.DirectorConsole(address='localhost', port=9101, name='user1', password=password)



Simple version of the bconsole in Python
----------------------------------------

.. code:: python

   import bareos.bsock
   password=bareos.bsock.Password('secret')
   directorconsole=bareos.bsock.DirectorConsole(address='localhost', port=9101, password=password)
   directorconsole.interactive()
   ...

or use the included bconsole.py script:

..

   bconsole.py --debug --name=user1 --password=secret --port 9101 localhost


Use JSON objects of API mode 2
----------------------------------

Requires: Bareos >= 15.2

.. code:: python

   import bareos.bsock
   password=bareos.bsock.Password('secret')
   directorconsole=bareos.bsock.DirectorConsoleJson(address='localhost', port=9101, password=password)
   directorconsole.call('list pools')
   ...
